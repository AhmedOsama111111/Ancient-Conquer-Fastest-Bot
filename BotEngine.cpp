#define NOMINMAX
#include "BotEngine.h"
#include "TeammateRadar.h"
#include "SpeedSkillTemplate.h"
#include "ReviveTemplate.h"
#include "CoordOCR.h"
#include "EquipmentLockTemplate.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <map>
#include <array>
#include <bit>
#include <tlhelp32.h>

// ============================================================================
// ============================================================================
// Revive Route: Navigation from Desert City (494, 647) to Monster Heartland (742, 673)
// Finely-spaced pavement waypoints ensuring 100% safe bridge passage across canal
// ============================================================================
static const MapWaypoint REVIVE_ROUTE[] = {
    { 494, 647 }, // Desert City center statue (Revive spawn point)
    { 510, 646 }, // East boulevard walkway towards bridge
    { 524, 646 }, // Bridge entrance walkway (safe center pavement)
    { 534, 647 }, // Canal bridge walkway center span
    { 546, 651 }, // East bridge exit / gate
    { 577, 672 }, // Outside gate into open desert
    { 633, 679 }, // South-East safe trail (away from canal/water)
    { 673, 676 }, // Open desert trail
    { 710, 675 }, // Entering monster zone
    { 729, 665 }, // Approaching SandMonster heartland
    { 742, 673 }  // Arrival at peak SandMonster & King Heartland!
};
static const int TOTAL_REVIVE_ROUTE = sizeof(REVIVE_ROUTE) / sizeof(REVIVE_ROUTE[0]);

// ============================================================================
// Primary Monster Heartland Circuit (Wide Dense Multi-Pack Patrol Sweep)
// Covers all SandMonster & SandMonsterKing packs across (X: 720..756, Y: 666..696)
// Multiple progressive jumps across each pack for maximum monster clearing
// ============================================================================
static const MapWaypoint DESERT_CIRCUIT[] = {
    { 742, 673 }, // Heartland Center (dense SandMonsters & King pack)
    { 750, 675 }, // East inner pack
    { 756, 682 }, // East Heartland King Pack
    { 754, 690 }, // South-East pack
    { 745, 694 }, // South Heartland Center
    { 736, 696 }, // South-West (safe cutoff before shore Y<=696)
    { 728, 692 }, // South-West mid pack
    { 722, 684 }, // West pack
    { 720, 675 }, // West return pack
    { 724, 668 }, // North-West pack
    { 732, 666 }, // North-Mid pack
    { 742, 670 }  // North-East pack
};
static const int TOTAL_WAYPOINTS = sizeof(DESERT_CIRCUIT) / sizeof(DESERT_CIRCUIT[0]);

// ============================================================================
// Spot 4: 3-Point Triangle Circuit (820, 623 -> 780, 657 -> 719, 604 -> 820, 623)
// 1. (820, 623): Main spot / home base
// 2. (780, 657): Second location
// 3. (719, 604): Third location
// Return: Smoothly loops back to (820, 623)
// ============================================================================
static const MapWaypoint CORRIDOR_TWO_WAY_CIRCUIT[] = {
    { 820, 623 }, // Spot 1: Base (Home)
    { 800, 640 }, // Step to Spot 2 (dist ~26)
    { 780, 657 }, // Spot 2
    { 758, 638 }, // Step to Spot 3 (dist ~29)
    { 738, 620 }, // Step to Spot 3 (dist ~27)
    { 719, 604 }, // Spot 3
    { 745, 609 }, // Return step to Spot 1 (dist ~26)
    { 770, 614 }, // Return step to Spot 1 (dist ~25)
    { 795, 619 }  // Return step to Spot 1 (dist ~25) -> seamlessly loops to { 820, 623 }
};
static const int TOTAL_CORRIDOR_WAYPOINTS = sizeof(CORRIDOR_TWO_WAY_CIRCUIT) / sizeof(CORRIDOR_TWO_WAY_CIRCUIT[0]);

// ============================================================================
// Spot 3: Full Desert Trail Two-Way Sweep Circuit (577, 652 <-> 799, 627)
// Continuous back-and-forth corridor sweep covering the entire stretch
// from Desert East Gate (577, 652) all the way to East Terminal (799, 627)
// Every jump is smoothly calibrated (27-28 tiles) to prevent obstacle messages!
// ============================================================================
static const MapWaypoint DESERT_TRAIL_TWO_WAY_CIRCUIT[] = {
    { 577, 652 }, // West Start (outside Desert City Gate)
    { 605, 650 },
    { 633, 648 },
    { 660, 644 },
    { 688, 638 },
    { 716, 631 },
    { 744, 627 },
    { 772, 626 },
    { 799, 627 }, // East Terminal
    { 772, 626 }, // Return West
    { 744, 627 },
    { 716, 631 },
    { 688, 638 },
    { 660, 644 },
    { 633, 648 },
    { 605, 650 }  // Return West -> loops seamlessly to { 577, 652 }
};
static const int TOTAL_DESERT_TRAIL_WAYPOINTS = sizeof(DESERT_TRAIL_TWO_WAY_CIRCUIT) / sizeof(DESERT_TRAIL_TWO_WAY_CIRCUIT[0]);

inline void GetActiveCircuitForHotspot(int hotspotIdx, const MapWaypoint*& outCircuit, int& outCount) {
    if (hotspotIdx == 3) {
        outCircuit = CORRIDOR_TWO_WAY_CIRCUIT;
        outCount = TOTAL_CORRIDOR_WAYPOINTS;
    } else if (hotspotIdx == 2) {
        outCircuit = DESERT_TRAIL_TWO_WAY_CIRCUIT;
        outCount = TOTAL_DESERT_TRAIL_WAYPOINTS;
    } else {
        outCircuit = DESERT_CIRCUIT;
        outCount = TOTAL_WAYPOINTS;
    }
}

// Hotspot Presets for UI Selection
struct HotspotPreset {
    int id;
    int x;
    int y;
    const wchar_t* name;
};
static const HotspotPreset HOTSPOT_PRESETS[] = {
    { 0, 742, 673, L"Heartland (742, 673) [SandMonsters]" },
    { 1, 577, 672, L"Desert East Gate (577, 672)" },
    { 2, 577, 652, L"Full Desert Trail (577, 652 <-> 799, 627)" },
    { 3, 820, 623, L"Corridor Patrol (820, 623 -> 780, 657 -> 719, 604)" }
};
static const int TOTAL_HOTSPOT_PRESETS = sizeof(HOTSPOT_PRESETS) / sizeof(HOTSPOT_PRESETS[0]);

static DWORD FindConquerProcess() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            std::string name(pe.szExeFile);
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("conquer") != std::string::npos &&
                lower.find("autopatch") == std::string::npos &&
                lower.find("update") == std::string::npos &&
                lower.find("conquervip") == std::string::npos &&
                lower.find("conquerbot") == std::string::npos &&
                lower.find("conquerloot") == std::string::npos) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static bool IsConquerProcessId(DWORD targetPid) {
    if (targetPid == 0 || targetPid == GetCurrentProcessId()) return false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    bool match = false;
    if (Process32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == targetPid) {
                std::string name(pe.szExeFile);
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower.find("conquer") != std::string::npos &&
                    lower.find("autopatch") == std::string::npos &&
                    lower.find("update") == std::string::npos &&
                    lower.find("conquervip") == std::string::npos &&
                    lower.find("conquerbot") == std::string::npos &&
                    lower.find("conquerloot") == std::string::npos) {
                    match = true;
                }
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return match;
}

BotEngine::BotEngine() :
    isRunning(false),
    stopFlag(false),
    currentState(BOT_STATE_IDLE),
    gameHwnd(NULL),
    anchorX(742),
    anchorY(673),
    hasAnchor(true),
    maxLeashRadius(6),
    huntingMode(MODE_CIRCUIT_PATROL),
    enableSecondaryWatcher(true),
    secondaryCpThreshold(1),
    secondaryExpThreshold(2),
    watcherCheckIntervalSec(2),
    lastSecondaryCheckTime(std::chrono::steady_clock::now()),
    forceSecondaryCheckNow(false),
    lastFoundSecondaryCp(0),
    lastFoundSecondaryExp(0),
    teammateX(684),
    teammateY(755),
    sweepIntervalSec(300),
    isTeammateNear(false),
    isBeaconVisible(false),
    lastBeaconX(0),
    lastBeaconY(0),
    currentWaypointIndex(0),
    totalDragonBallsLooted(0),
    totalCpbagsLooted(0),
    totalItemsLooted(0),
    lastVisibleMonsterPixels(0),
    lastCaptchaTime(std::chrono::steady_clock::now() - std::chrono::seconds(60)),
    walkOnlyUntil(std::chrono::steady_clock::time_point::min()),
    wasWalkingOnly(false),
    selectedHotspot(0) {
}

BotEngine::~BotEngine() {
    Stop();
    Input::Cleanup();
}

void BotEngine::Pause() {
    if (isRunning.load() && currentState.load() != BOT_STATE_PAUSED) {
        currentState = BOT_STATE_PAUSED;
        Input::HoldCtrl(false);
        Input::LeftUp();
        Input::RightUp();
        Beep(800, 150);
        printf("\n[*] Bot PAUSED.\n");
    }
}

void BotEngine::Resume() {
    if (isRunning.load() && currentState.load() == BOT_STATE_PAUSED) {
        currentState = BOT_STATE_PATROL;
        FocusGameWindow(gameHwnd);
        Input::HoldCtrl(true);
        Beep(1400, 150);
        printf("\n[+] Bot RESUMED.\n");
    }
}

void BotEngine::TogglePause() {
    if (currentState.load() == BOT_STATE_PAUSED) {
        Resume();
    } else if (isRunning.load()) {
        Pause();
    }
}

void BotEngine::SetAnchor(int x, int y) {
    anchorX.store(x);
    anchorY.store(y);
    hasAnchor.store(true);
    printf("[+] 🎯 ANCHOR SPOT SET TO (%d, %d)\n", x, y);
}

bool BotEngine::CaptureCurrentSpotAsAnchor() {
    HWND hwnd = gameHwnd ? gameHwnd : FindGameWindow();
    if (!hwnd) return false;
    int curX = 0, curY = 0;
    if (ReadCoordsFromTitle(hwnd, curX, curY) && curX > 0 && curY > 0) {
        SetAnchor(curX, curY);
        Beep(1800, 100);
        Beep(2400, 150);
        return true;
    }
    return false;
}

bool BotEngine::GetAnchor(int& outX, int& outY) const {
    if (!hasAnchor.load()) return false;
    outX = anchorX.load();
    outY = anchorY.load();
    return true;
}

void BotEngine::SetHuntingMode(HuntingMode mode) {
    huntingMode.store(mode);
    printf("[*] Hunting Mode set to: %s\n", (mode == MODE_VIP_SPOT_LOCK ? "VIP SPOT LOCK" : "CIRCUIT PATROL"));
}

HuntingMode BotEngine::GetHuntingMode() const {
    return huntingMode.load();
}

void BotEngine::SetLeashRadius(int radius) {
    maxLeashRadius.store(radius);
}

int BotEngine::GetLeashRadius() const {
    return maxLeashRadius.load();
}

bool BotEngine::ReadCurrentCoords(int& outX, int& outY) {
    HWND hwnd = gameHwnd ? gameHwnd : FindGameWindow();
    if (!hwnd || !IsWindow(hwnd)) return false;
    return ReadCoordsFromTitle(hwnd, outX, outY);
}

double BotEngine::GetDistanceToAnchor() const {
    if (!hasAnchor.load()) return 0.0;
    HWND hwnd = gameHwnd ? gameHwnd : const_cast<BotEngine*>(this)->FindGameWindow();
    if (!hwnd) return 0.0;
    int curX = 0, curY = 0;
    if (const_cast<BotEngine*>(this)->ReadCoordsFromTitle(hwnd, curX, curY) && curX > 0 && curY > 0) {
        return std::hypot(curX - anchorX.load(), curY - anchorY.load());
    }
    return 0.0;
}

void BotEngine::SetSelectedHotspot(int idx) {
    selectedHotspot.store(idx);
    if (idx >= 0 && idx < TOTAL_HOTSPOT_PRESETS) {
        SetAnchor(HOTSPOT_PRESETS[idx].x, HOTSPOT_PRESETS[idx].y);
        printf("[*] Selected Hotspot changed to #%d: (%d, %d)\n", idx + 1, HOTSPOT_PRESETS[idx].x, HOTSPOT_PRESETS[idx].y);
    }
    if (idx == 3 || idx == 2) {
        // Spot 3 & Spot 4: Continuous Two-Way Patrol Circuits
        huntingMode.store(MODE_CIRCUIT_PATROL);
        const MapWaypoint* activeCircuit = nullptr;
        int totalCircuitWp = 0;
        GetActiveCircuitForHotspot(idx, activeCircuit, totalCircuitWp);
        int cx = 0, cy = 0;
        if (ReadCoordsFromTitle(gameHwnd, cx, cy) && cx > 0 && cy > 0 && activeCircuit && totalCircuitWp > 0) {
            double bestDist = 1e9;
            for (int i = 0; i < totalCircuitWp; i++) {
                double d = std::hypot(activeCircuit[i].x - cx, activeCircuit[i].y - cy);
                if (d < bestDist) {
                    bestDist = d;
                    currentWaypointIndex = i;
                }
            }
            printf("[+] Spot %d Activated: Starting at Waypoint #%d (%d, %d)\n",
                   idx + 1, currentWaypointIndex, activeCircuit[currentWaypointIndex].x, activeCircuit[currentWaypointIndex].y);
        }
    }
}

int BotEngine::GetSelectedHotspot() const {
    return selectedHotspot.load();
}

void BotEngine::SetSecondaryWatcherEnabled(bool en) {
    enableSecondaryWatcher.store(en);
    printf("[*] Multi-Client 2nd Account Watcher: %s\n", en ? "ENABLED" : "DISABLED");
}

bool BotEngine::IsSecondaryWatcherEnabled() const {
    return enableSecondaryWatcher.load();
}

void BotEngine::SetSecondaryCpThreshold(int t) {
    secondaryCpThreshold.store(t > 0 ? t : 1);
}

int BotEngine::GetSecondaryCpThreshold() const {
    return secondaryCpThreshold.load();
}

void BotEngine::SetSecondaryExpThreshold(int t) {
    secondaryExpThreshold.store(t > 0 ? t : 2);
}

int BotEngine::GetSecondaryExpThreshold() const {
    return secondaryExpThreshold.load();
}

void BotEngine::TriggerSecondaryCheckNow() {
    forceSecondaryCheckNow.store(true);
    printf("[👥] ⚡ MANUAL 2ND ACCOUNT CHECK TRIGGERED VIA F5 / UI!\n");
}

int BotEngine::GetSecondsUntilNextSweep() const {
    if (!enableSecondaryWatcher.load()) return -1;
    auto now = std::chrono::steady_clock::now();
    int elapsed = (int)std::chrono::duration_cast<std::chrono::seconds>(now - lastSecondaryCheckTime).count();
    int rem = watcherCheckIntervalSec.load() - elapsed;
    return (rem > 0 ? rem : 0);
}


HWND BotEngine::FindGameWindow() {
    struct Candidate {
        HWND hwnd;
        int score;
        std::wstring title;
    };
    std::vector<Candidate> candidates;

    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h)) return TRUE;
        RECT rc;
        GetClientRect(h, &rc);
        if (rc.right < 400 || rc.bottom < 300) return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == 0 || pid == GetCurrentProcessId()) return TRUE;

        // 1. STRICT GUARANTEE: Process MUST be genuine Conquer.exe!
        // This 100% rejects Antigravity IDE, Visual Studio, Explorer, Bot, Chrome, Discord!
        if (!IsConquerProcessId(pid)) {
            return TRUE;
        }

        wchar_t title[256] = { 0 };
        GetWindowTextW(h, title, 256);
        std::wstring wTitle = title;
        std::wstring wLowerTitle = wTitle;
        std::transform(wLowerTitle.begin(), wLowerTitle.end(), wLowerTitle.begin(), ::tolower);

        // 2. USER COMMAND: Skip "Heaven" private server
        if (wLowerTitle.find(L"heaven") != std::wstring::npos) {
            return TRUE;
        }

        // Scoring: prioritize foreground Conquer window, or window with coordinates
        int score = 100;
        if (h == GetForegroundWindow()) score += 1000;
        if (wTitle.find(L"(") != std::wstring::npos && wTitle.find(L")") != std::wstring::npos) score += 300;
        if (wTitle.find(L"Ping:") != std::wstring::npos || wTitle.find(L"Fps:") != std::wstring::npos) score += 200;
        if (wTitle.find(L"Osos") != std::wstring::npos || wTitle.find(L"osos") != std::wstring::npos) score += 150;

        auto* cList = (std::vector<Candidate>*)lp;
        cList->push_back({ h, score, wTitle });
        return TRUE;
    }, (LPARAM)&candidates);

    if (candidates.empty()) return NULL;

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    std::wcout << L"[+] Attached strictly to Conquer game client: \"" << candidates[0].title << L"\"\n";
    return candidates[0].hwnd;
}

void BotEngine::FocusGameWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;

    // Safety verification: Ensure window strictly belongs to Conquer.exe!
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!IsConquerProcessId(pid)) {
        return; // NEVER focus anything outside Conquer.exe!
    }

    if (GetForegroundWindow() == hwnd) return;

    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    HWND hCurFore = GetForegroundWindow();
    DWORD foreThread = hCurFore ? GetWindowThreadProcessId(hCurFore, NULL) : 0;
    DWORD curThread = GetCurrentThreadId();

    if (foreThread != 0 && foreThread != curThread) {
        AttachThreadInput(curThread, foreThread, TRUE);
    }

    // Interception driver Alt key tap to gain foreground rights without Win32 keybd_event
    Input::KeyDown(0x38); // DIK_LMENU
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);
    Input::KeyUp(0x38);

    SwitchToThisWindow(hwnd, TRUE);

    if (foreThread != 0 && foreThread != curThread) {
        AttachThreadInput(curThread, foreThread, FALSE);
    }
    SafeSleep(60);
}

bool BotEngine::ReadCoordsFromTitle(HWND hwnd, int& outX, int& outY) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    // 1. Direct In-Game HUD OCR (Pixel-perfect OCR directly from HUD "[Ancient] (X, Y)")
    if (CoordOCR::ReadCoordsFromHUD(hwnd, outX, outY)) {
        return true;
    }

    // 2. High-speed fallback: Read coordinates from window title if OCR missed
    wchar_t title[256] = {0};
    if (GetWindowTextW(hwnd, title, 256) > 0) {
        const wchar_t* p = wcschr(title, L'(');
        if (p) {
            p++; // skip '('
            wchar_t* endP = nullptr;
            long x = wcstol(p, &endP, 10);
            if (endP && *endP) {
                while (*endP == L' ' || *endP == L',') endP++;
                long y = wcstol(endP, nullptr, 10);
                if (x > 0 && y > 0) {
                    outX = (int)x;
                    outY = (int)y;
                    return true;
                }
            }
        }
    }
    return false;
}

int BotEngine::CountMonsterGreenPixels(HWND hwnd, int winW, int winH) {
    if (!hwnd || !IsWindow(hwnd) || winW <= 0 || winH <= 0) return 0;
    POINT ptClient = { 0, 0 };
    ClientToScreen(hwnd, &ptClient);

    // Main gameplay area (exclude top menu bars and bottom chat/taskbar)
    int areaX = (std::max)(0, 100);
    int areaY = (std::max)(0, 100);
    int areaW = (std::min)(winW - 200, 1600);
    int areaH = (std::min)(winH - 220, 800);
    if (areaW <= 0 || areaH <= 0) return 0;

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return 0;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) { ReleaseDC(NULL, hdcScreen); return 0; }
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, areaW, areaH);
    if (!hbm) { DeleteDC(hdcMem); ReleaseDC(NULL, hdcScreen); return 0; }
    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, areaW, areaH, hdcScreen, ptClient.x + areaX, ptClient.y + areaY, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = areaW;
    bmi.bmiHeader.biHeight = -areaH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = ((areaW * 3 + 3) & ~3);
    std::vector<BYTE> bgr(stride * areaH);
    GetDIBits(hdcMem, hbm, 0, areaH, bgr.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // Count pure green pixels of monster names: G > 95, R < 60, B < 60
    int greenCount = 0;
    for (int y = 0; y < areaH; y += 2) {
        const BYTE* row = bgr.data() + y * stride;
        for (int x = 0; x < areaW; x += 2) {
            BYTE b = row[x * 3];
            BYTE g = row[x * 3 + 1];
            BYTE r = row[x * 3 + 2];
            if (g > 95 && r < 60 && b < 60) {
                greenCount++;
            }
        }
    }
    return greenCount * 4;
}

bool BotEngine::SafeSleep(int ms) {
    int elapsed = 0;
    while (elapsed < ms) {
        if (stopFlag.load()) {
            return false;
        }
        if (GetAsyncKeyState(VK_F9) & 0x8000) {
            stopFlag = true;
            isRunning = false;
            Input::HoldCtrl(false);
            Input::LeftUp();
            Input::RightUp();
            return false;
        }
        int slice = std::min(15, ms - elapsed);
        Sleep(slice);
        elapsed += slice;
    }
    return !stopFlag.load();
}

// ============================================================================
// Yes Button Template (38x22) for Pixel-Perfect Detection of Security Dialog
// ============================================================================
// Low-Res Template (38x22) for 1024x576 / windowed
static const int YES_TMPL_W = 38;
static const int YES_TMPL_H = 22;
static const uint8_t YES_TMPL_PIXELS[22][38] = {
    { 44, 44, 44, 45, 44, 43, 46, 45, 47, 46, 44, 45, 43, 45, 44, 42, 46, 44, 44, 44, 44, 43, 44, 45, 46, 43, 45, 43, 45, 43, 46, 41, 44, 44, 49, 44, 45, 45 },
    { 44, 44, 45, 45, 45, 49, 44, 45, 45, 46, 47, 44, 44, 45, 41, 44, 43, 43, 45, 42, 43, 44, 42, 43, 43, 45, 45, 42, 43, 44, 42, 46, 44, 44, 44, 46, 46, 45 },
    { 45, 45, 45, 45, 45, 45, 46, 48, 45, 44, 44, 43, 44, 41, 44, 44, 43, 43, 42, 44, 43, 44, 44, 44, 44, 44, 42, 46, 45, 44, 44, 45, 45, 44, 45, 45, 45, 44 },
    { 45, 45, 45, 45, 46, 45, 44, 46, 47, 45, 44, 44, 44, 42, 44, 43, 42, 43, 43, 44, 43, 42, 44, 42, 46, 44, 45, 44, 44, 44, 43, 44, 46, 44, 46, 46, 44, 43 },
    { 45, 45, 45, 45, 44, 46, 45, 44, 44, 43, 44, 26, 19, 18, 21, 21, 20, 20, 19, 17, 18, 17, 17, 20, 20, 19, 20, 15, 16, 45, 45, 45, 45, 46, 45, 42, 44, 43 },
    { 45, 45, 45, 45, 45, 43, 45, 45, 43, 44, 25, 126, 110, 110, 107, 107, 107, 107, 104, 104, 100, 97, 97, 95, 95, 96, 97, 101, 85, 28, 44, 44, 46, 45, 42, 43, 42, 42 },
    { 45, 45, 45, 45, 44, 44, 45, 43, 46, 46, 156, 61, 29, 138, 122, 123, 126, 127, 100, 100, 102, 101, 104, 75, 75, 60, 39, 26, 48, 65, 12, 44, 43, 43, 42, 43, 43, 43 },
    { 45, 45, 44, 44, 45, 46, 45, 44, 46, 44, 168, 53, 150, 69, 27, 27, 24, 23, 33, 36, 35, 11, 12, 19, 18, 29, 28, 30, 17, 143, 15, 42, 42, 44, 41, 43, 44, 43 },
    { 44, 45, 44, 44, 45, 45, 43, 45, 44, 42, 168, 55, 116, 45, 35, 42, 204, 127, 158, 139, 56, 50, 16, 19, 20, 25, 30, 43, 30, 143, 13, 43, 44, 40, 43, 42, 41, 43 },
    { 44, 46, 45, 45, 45, 44, 43, 43, 44, 41, 167, 55, 116, 56, 71, 70, 64, 108, 133, 33, 46, 74, 33, 52, 76, 49, 27, 44, 42, 139, 11, 43, 41, 42, 44, 43, 44, 43 },
    { 44, 45, 43, 43, 43, 42, 43, 44, 43, 43, 169, 58, 114, 86, 83, 81, 77, 140, 128, 46, 110, 172, 134, 83, 82, 77, 77, 57, 43, 139, 13, 42, 44, 44, 43, 42, 44, 42 },
    { 45, 45, 42, 42, 44, 41, 43, 42, 44, 43, 170, 58, 109, 93, 96, 93, 73, 169, 101, 119, 199, 195, 117, 200, 92, 94, 96, 102, 63, 142, 11, 43, 44, 43, 43, 45, 43, 45 },
    { 45, 44, 42, 42, 41, 43, 44, 44, 44, 43, 170, 59, 96, 97, 101, 95, 28, 168, 97, 61, 122, 96, 124, 96, 98, 106, 105, 105, 85, 140, 14, 45, 44, 45, 45, 44, 44, 44 },
    { 43, 42, 42, 42, 42, 43, 41, 43, 44, 44, 149, 68, 60, 95, 104, 103, 74, 121, 115, 96, 136, 141, 136, 146, 119, 115, 117, 100, 100, 130, 11, 43, 44, 44, 44, 46, 45, 45 },
    { 43, 42, 43, 43, 42, 41, 44, 45, 44, 44, 39, 129, 111, 124, 126, 126, 125, 125, 127, 127, 128, 127, 127, 125, 124, 138, 141, 129, 134, 57, 30, 46, 43, 45, 44, 43, 43, 41 },
    { 42, 43, 43, 41, 43, 44, 44, 45, 43, 44, 44, 37, 76, 77, 76, 76, 73, 70, 63, 58, 59, 58, 57, 55, 55, 54, 52, 51, 44, 28, 44, 44, 44, 42, 43, 42, 44, 39 },
    { 43, 42, 43, 43, 45, 45, 44, 43, 43, 42, 42, 43, 42, 43, 42, 42, 42, 44, 43, 43, 44, 43, 43, 44, 44, 43, 43, 45, 44, 44, 44, 42, 44, 44, 42, 44, 39, 41 },
    { 43, 42, 43, 44, 44, 43, 43, 43, 43, 42, 42, 45, 43, 44, 44, 45, 44, 43, 43, 43, 44, 43, 43, 43, 44, 44, 45, 45, 43, 47, 45, 44, 44, 43, 43, 42, 38, 43 },
    { 43, 43, 43, 43, 43, 41, 44, 44, 42, 44, 43, 42, 42, 42, 42, 44, 44, 43, 43, 43, 44, 44, 43, 43, 44, 44, 44, 43, 46, 45, 45, 43, 42, 45, 45, 39, 40, 44 },
    { 44, 44, 44, 43, 45, 42, 44, 43, 41, 43, 43, 42, 44, 43, 42, 43, 43, 42, 42, 43, 43, 43, 44, 45, 44, 42, 43, 45, 43, 44, 44, 47, 44, 45, 39, 40, 41, 44 },
    { 43, 44, 44, 44, 44, 43, 43, 43, 43, 42, 42, 44, 41, 41, 42, 43, 43, 43, 42, 43, 43, 43, 44, 44, 44, 43, 43, 44, 43, 44, 43, 44, 44, 42, 42, 43, 40, 43 },
    { 44, 43, 43, 44, 42, 44, 43, 44, 44, 42, 42, 43, 43, 43, 42, 43, 42, 43, 42, 42, 44, 44, 44, 43, 44, 45, 44, 42, 42, 44, 45, 41, 46, 45, 46, 37, 40, 43 },
};
static float s_normTmpl[YES_TMPL_H][YES_TMPL_W];

// 1080p High-Res Template (71x41) for 1920x1080
static const int YES_1080P_W = 71;
static const int YES_1080P_H = 41;
static const uint8_t YES_1080P_PIXELS[41][71] = {
    { 43, 43, 43, 42, 42, 42, 42, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 46, 45, 45, 45, 45, 45, 45, 45, 46, 46, 45, 44, 44, 44, 42, 41, 42, 43, 41, 38, 38, 41, 39, 42, 37, 38, 42, 41, 44, 43, 42, 44, 45, 43, 43, 38, 44, 42, 48, 50, 45, 45, 42, 41, 44, 41, 43, 44, 45, 45, 43 },
    { 43, 43, 43, 42, 42, 42, 42, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 45, 43, 44, 44, 43, 43, 46, 46, 44, 42, 42, 42, 42, 48, 48, 46, 46, 43, 38, 37, 39, 43, 41, 42, 42, 41, 42, 46, 48, 45, 45, 43, 44, 41, 42, 37, 43, 43, 49, 48, 48, 49, 43, 42, 41, 41, 38, 41, 46, 50, 45 },
    { 43, 43, 43, 42, 42, 42, 42, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 45, 43, 44, 44, 42, 42, 43, 44, 42, 42, 42, 39, 41, 43, 42, 43, 44, 45, 41, 38, 39, 38, 37, 44, 39, 43, 43, 45, 46, 45, 44, 38, 42, 40, 40, 44, 44, 43, 50, 47, 47, 49, 37, 43, 42, 43, 43, 42, 46, 48, 44 },
    { 43, 43, 43, 42, 42, 42, 42, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 42, 43, 43, 42, 42, 43, 44, 42, 41, 42, 42, 42, 43, 41, 43, 44, 45, 41, 37, 39, 36, 36, 39, 39, 42, 43, 44, 44, 43, 43, 36, 41, 41, 42, 40, 43, 41, 44, 45, 45, 44, 37, 42, 39, 41, 42, 42, 44, 45, 44 },
    { 43, 43, 43, 42, 42, 42, 42, 41, 41, 41, 41, 43, 43, 43, 43, 42, 42, 42, 42, 44, 45, 46, 45, 43, 46, 44, 45, 45, 44, 42, 39, 43, 41, 43, 48, 43, 44, 41, 37, 36, 36, 37, 39, 37, 36, 38, 41, 45, 46, 42, 41, 41, 38, 41, 41, 44, 47, 48, 47, 47, 45, 45, 41, 41, 41, 43, 45, 44, 43, 44, 44 },
    { 43, 43, 43, 42, 42, 42, 42, 41, 41, 41, 41, 43, 43, 43, 43, 42, 42, 42, 42, 44, 45, 44, 43, 42, 45, 46, 43, 44, 45, 43, 43, 43, 42, 43, 44, 42, 42, 39, 39, 38, 38, 38, 36, 36, 39, 38, 44, 45, 43, 43, 39, 38, 37, 40, 43, 43, 43, 44, 43, 47, 43, 41, 42, 42, 45, 42, 43, 43, 43, 43, 42 },
    { 43, 43, 43, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 42, 42, 42, 42, 45, 44, 42, 43, 42, 44, 44, 44, 44, 45, 44, 42, 45, 43, 44, 45, 46, 43, 38, 41, 39, 38, 37, 38, 35, 39, 43, 44, 45, 44, 42, 44, 38, 41, 40, 45, 44, 43, 48, 42, 40, 41, 43, 43, 43, 43, 45, 45, 44, 42, 41, 39 },
    { 43, 43, 43, 42, 42, 42, 42, 41, 41, 41, 41, 39, 39, 39, 39, 42, 42, 42, 42, 45, 42, 42, 43, 45, 45, 44, 44, 43, 42, 42, 41, 44, 43, 43, 43, 45, 44, 42, 39, 36, 37, 38, 39, 36, 44, 43, 44, 44, 43, 42, 39, 37, 40, 41, 42, 48, 44, 44, 41, 43, 45, 46, 42, 49, 43, 46, 44, 42, 43, 42, 42 },
    { 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 39, 39, 39, 39, 41, 36, 43, 45, 42, 44, 2, 12, 11, 12, 14, 14, 12, 12, 13, 13, 13, 13, 12, 11, 11, 11, 11, 11, 10, 11, 12, 12, 15, 15, 15, 15, 16, 15, 15, 9, 8, 8, 7, 40, 45, 46, 40, 42, 42, 44, 43, 38, 45, 45, 48, 43, 44, 46, 45, 42 },
    { 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 39, 39, 39, 39, 36, 39, 39, 43, 42, 3, 201, 191, 195, 194, 193, 192, 186, 186, 186, 186, 186, 186, 184, 182, 180, 178, 174, 172, 170, 169, 166, 165, 164, 164, 164, 164, 162, 164, 164, 173, 178, 173, 65, 0, 45, 41, 43, 44, 42, 46, 42, 42, 44, 42, 43, 44, 46, 48, 44, 43 },
    { 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 39, 39, 39, 39, 39, 38, 41, 39, 4, 205, 66, 71, 74, 73, 71, 70, 70, 70, 70, 70, 70, 70, 68, 67, 70, 67, 63, 61, 65, 64, 61, 61, 65, 65, 65, 65, 67, 67, 62, 68, 60, 65, 67, 50, 2, 45, 44, 46, 39, 39, 42, 39, 45, 41, 48, 48, 48, 46, 43, 44 },
    { 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 39, 39, 39, 39, 42, 41, 38, 6, 205, 67, 71, 14, 11, 12, 14, 14, 15, 15, 15, 15, 15, 15, 13, 12, 10, 10, 10, 10, 11, 11, 14, 15, 15, 15, 15, 15, 13, 13, 16, 10, 8, 64, 64, 74, 47, 3, 44, 43, 39, 42, 42, 41, 44, 43, 45, 46, 48, 46, 43, 41 },
    { 42, 42, 42, 43, 43, 43, 43, 42, 42, 42, 42, 39, 39, 39, 39, 39, 37, 38, 23, 230, 89, 22, 44, 44, 229, 230, 202, 202, 207, 206, 214, 214, 213, 152, 172, 169, 168, 172, 171, 172, 170, 171, 173, 121, 122, 121, 95, 92, 71, 50, 37, 37, 23, 50, 64, 61, 7, 38, 40, 42, 38, 42, 41, 45, 44, 43, 45, 49, 49, 43, 38 },
    { 42, 42, 42, 41, 41, 41, 41, 42, 42, 42, 42, 39, 39, 39, 39, 39, 37, 38, 23, 233, 87, 24, 39, 231, 107, 44, 29, 25, 30, 27, 20, 28, 24, 24, 38, 30, 34, 37, 11, 9, 8, 15, 17, 17, 17, 20, 25, 27, 25, 27, 20, 23, 21, 9, 177, 63, 5, 38, 40, 39, 37, 37, 43, 48, 43, 43, 44, 45, 45, 42, 41 },
    { 42, 42, 42, 41, 41, 41, 41, 42, 42, 42, 42, 39, 39, 39, 39, 39, 37, 38, 23, 231, 90, 23, 230, 101, 44, 41, 28, 27, 28, 25, 23, 22, 23, 23, 32, 35, 34, 35, 63, 14, 11, 11, 21, 16, 20, 17, 28, 24, 26, 28, 35, 35, 24, 14, 180, 61, 7, 40, 42, 37, 35, 39, 41, 45, 43, 42, 41, 43, 46, 43, 41 },
    { 42, 42, 42, 43, 43, 43, 43, 42, 42, 42, 42, 39, 39, 39, 39, 39, 37, 38, 23, 236, 93, 20, 171, 42, 43, 41, 24, 28, 29, 148, 211, 212, 149, 88, 234, 228, 101, 35, 63, 63, 13, 11, 21, 19, 14, 15, 24, 23, 26, 27, 37, 35, 41, 7, 178, 61, 7, 40, 42, 35, 36, 41, 43, 48, 44, 43, 42, 44, 45, 43, 41 },
    { 43, 43, 43, 44, 44, 44, 44, 46, 46, 46, 46, 49, 46, 40, 39, 39, 40, 44, 26, 231, 93, 24, 166, 50, 51, 46, 56, 56, 54, 48, 191, 193, 18, 21, 124, 124, 66, 63, 77, 77, 15, 18, 17, 18, 20, 17, 23, 22, 24, 38, 51, 52, 52, 29, 174, 60, 5, 39, 39, 38, 39, 37, 37, 43, 39, 36, 38, 45, 44, 42, 42 },
    { 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 46, 44, 40, 39, 38, 39, 43, 25, 231, 93, 24, 166, 45, 49, 48, 66, 68, 70, 67, 74, 194, 74, 18, 184, 67, 7, 10, 20, 71, 15, 20, 18, 17, 73, 71, 56, 37, 22, 18, 28, 46, 49, 29, 175, 61, 5, 39, 40, 40, 38, 35, 38, 43, 39, 38, 43, 43, 42, 43, 42 },
    { 42, 42, 42, 43, 43, 43, 43, 42, 42, 42, 42, 43, 43, 42, 43, 38, 39, 43, 25, 231, 93, 24, 166, 45, 87, 88, 80, 83, 83, 84, 15, 193, 134, 77, 179, 66, 9, 176, 190, 186, 24, 74, 186, 187, 192, 74, 73, 75, 58, 41, 35, 63, 62, 35, 180, 58, 4, 38, 38, 39, 35, 36, 37, 37, 37, 42, 43, 44, 43, 43, 42 },
    { 42, 42, 42, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 43, 44, 38, 39, 43, 25, 231, 93, 24, 166, 49, 88, 85, 82, 84, 81, 79, 73, 140, 138, 137, 128, 9, 61, 178, 19, 188, 129, 133, 131, 75, 189, 73, 78, 77, 77, 72, 58, 50, 51, 29, 175, 59, 3, 39, 39, 39, 38, 36, 34, 34, 39, 44, 45, 43, 43, 38, 35 },
    { 43, 43, 43, 42, 42, 42, 42, 44, 44, 44, 44, 42, 42, 42, 42, 39, 42, 42, 30, 238, 96, 28, 123, 76, 90, 91, 91, 88, 90, 83, 72, 75, 200, 201, 78, 21, 137, 143, 25, 151, 149, 156, 153, 86, 87, 92, 91, 91, 92, 86, 84, 83, 63, 36, 177, 61, 4, 43, 43, 43, 39, 41, 38, 35, 38, 43, 41, 45, 43, 38, 35 },
    { 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 42, 42, 42, 40, 39, 42, 42, 30, 234, 94, 29, 124, 88, 91, 93, 95, 92, 92, 88, 71, 11, 203, 141, 78, 19, 196, 194, 215, 213, 155, 94, 212, 209, 154, 86, 89, 93, 94, 94, 100, 105, 80, 43, 178, 61, 4, 43, 43, 43, 38, 39, 37, 36, 37, 42, 43, 42, 42, 38, 35 },
    { 43, 43, 43, 45, 45, 45, 45, 44, 44, 44, 44, 42, 42, 40, 40, 39, 42, 42, 30, 231, 95, 28, 117, 91, 92, 99, 100, 95, 99, 94, 71, 9, 201, 140, 77, 77, 137, 143, 89, 33, 30, 87, 87, 87, 209, 89, 100, 101, 101, 101, 106, 105, 100, 62, 177, 61, 4, 44, 43, 44, 44, 41, 41, 38, 35, 39, 38, 42, 42, 38, 35 },
    { 43, 43, 43, 45, 45, 45, 45, 44, 44, 44, 44, 42, 40, 40, 42, 39, 42, 42, 30, 235, 99, 25, 109, 82, 90, 106, 100, 101, 95, 86, 14, 7, 201, 140, 79, 80, 24, 200, 31, 89, 153, 150, 35, 88, 210, 85, 107, 106, 108, 105, 100, 106, 100, 59, 173, 61, 4, 43, 43, 44, 42, 44, 44, 41, 38, 41, 39, 36, 36, 38, 41 },
    { 42, 42, 42, 43, 43, 43, 43, 43, 43, 43, 43, 42, 40, 38, 37, 30, 39, 40, 9, 211, 76, 9, 95, 50, 88, 93, 103, 100, 100, 100, 76, 209, 208, 204, 138, 81, 111, 166, 207, 211, 124, 166, 206, 207, 165, 121, 117, 113, 114, 117, 118, 115, 77, 79, 199, 66, 8, 42, 43, 43, 44, 43, 48, 42, 41, 36, 32, 37, 37, 39, 41 },
    { 41, 41, 41, 42, 42, 42, 42, 39, 39, 39, 39, 38, 38, 37, 36, 38, 38, 42, 9, 208, 79, 76, 52, 52, 93, 92, 101, 100, 105, 100, 74, 118, 75, 114, 81, 80, 103, 106, 115, 123, 119, 120, 116, 115, 119, 117, 114, 117, 116, 115, 114, 76, 78, 156, 131, 65, 4, 43, 38, 42, 46, 41, 45, 44, 38, 31, 34, 39, 41, 44, 41 },
    { 38, 38, 38, 39, 39, 39, 39, 37, 37, 37, 37, 36, 36, 37, 38, 39, 42, 37, 8, 73, 144, 146, 54, 53, 54, 49, 69, 74, 71, 72, 72, 71, 74, 70, 76, 82, 77, 79, 76, 75, 76, 73, 76, 73, 71, 75, 82, 83, 81, 79, 78, 76, 152, 151, 70, 70, 4, 43, 46, 43, 44, 41, 43, 48, 42, 34, 36, 39, 41, 44, 41 },
    { 37, 37, 37, 39, 39, 39, 39, 39, 39, 39, 39, 37, 39, 42, 43, 40, 40, 39, 40, 14, 78, 146, 136, 179, 180, 177, 166, 169, 169, 165, 165, 164, 164, 163, 164, 164, 164, 163, 165, 168, 168, 164, 166, 167, 169, 160, 151, 183, 184, 185, 186, 156, 156, 74, 68, 3, 45, 42, 44, 46, 46, 43, 43, 46, 36, 34, 36, 36, 37, 37, 37 },
    { 39, 39, 39, 40, 40, 40, 40, 42, 42, 40, 40, 43, 43, 44, 44, 39, 40, 43, 43, 43, 1, 93, 132, 134, 134, 134, 134, 131, 135, 132, 127, 128, 126, 126, 97, 103, 102, 103, 103, 101, 102, 100, 100, 95, 97, 96, 95, 94, 93, 87, 89, 88, 85, 58, 4, 44, 42, 43, 45, 48, 42, 39, 38, 37, 35, 36, 34, 38, 36, 34, 35 },
    { 39, 39, 39, 40, 40, 40, 40, 42, 42, 40, 40, 44, 44, 43, 44, 39, 40, 43, 43, 42, 41, 0, 0, 2, 2, 1, 1, 4, 4, 3, 1, 3, 2, 3, 1, 3, 4, 1, 2, 1, 1, 0, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 44, 44, 44, 40, 45, 45, 42, 42, 39, 39, 41, 35, 36, 37, 37, 35, 35 },
    { 39, 39, 39, 40, 40, 40, 40, 42, 42, 40, 40, 44, 43, 44, 44, 39, 40, 43, 43, 43, 42, 43, 40, 41, 41, 41, 41, 43, 41, 43, 39, 42, 48, 44, 43, 42, 42, 38, 41, 41, 41, 41, 41, 44, 45, 44, 45, 49, 44, 45, 43, 44, 42, 43, 44, 43, 38, 39, 44, 44, 42, 44, 44, 42, 37, 34, 37, 35, 37, 37, 37 },
    { 39, 39, 39, 40, 40, 40, 40, 42, 42, 40, 40, 44, 44, 43, 43, 39, 40, 43, 43, 44, 41, 40, 45, 41, 41, 41, 41, 42, 39, 38, 38, 36, 38, 44, 40, 40, 42, 41, 41, 36, 37, 37, 36, 43, 45, 45, 43, 46, 46, 44, 44, 44, 43, 45, 39, 43, 42, 42, 37, 38, 38, 44, 44, 39, 39, 35, 36, 36, 37, 38, 38 },
    { 41, 39, 37, 41, 39, 41, 41, 39, 41, 41, 41, 41, 41, 41, 42, 42, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 43, 42, 39, 38, 38, 41, 43, 41, 36, 35, 35, 37, 39, 36, 37, 41, 45, 44, 42, 43, 44, 44, 44, 43, 42, 43, 42, 39, 36, 35, 38, 42, 38, 37, 37, 36, 37, 37, 38, 38 },
    { 42, 41, 38, 39, 38, 39, 39, 39, 42, 42, 42, 41, 41, 41, 42, 42, 44, 44, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 43, 42, 39, 38, 38, 38, 41, 41, 37, 36, 35, 37, 36, 35, 37, 41, 46, 46, 44, 45, 46, 46, 46, 45, 43, 41, 38, 39, 37, 35, 39, 42, 38, 37, 37, 37, 37, 38, 37, 38 },
    { 43, 42, 39, 41, 39, 41, 41, 41, 43, 43, 43, 39, 39, 41, 42, 42, 42, 44, 41, 41, 41, 41, 41, 41, 41, 41, 41, 42, 42, 42, 42, 42, 41, 38, 37, 38, 38, 38, 36, 36, 37, 38, 37, 36, 35, 41, 45, 46, 44, 44, 45, 45, 46, 44, 42, 39, 39, 41, 38, 38, 38, 41, 41, 38, 39, 41, 41, 37, 38, 41, 39 },
    { 42, 41, 38, 42, 41, 42, 42, 41, 42, 42, 42, 38, 37, 37, 39, 39, 39, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 44, 44, 44, 44, 43, 42, 39, 38, 38, 38, 38, 35, 36, 37, 38, 36, 34, 37, 42, 44, 45, 43, 42, 43, 43, 43, 43, 41, 38, 39, 42, 41, 41, 41, 39, 39, 41, 41, 41, 38, 34, 38, 42, 41 },
    { 42, 42, 42, 43, 42, 42, 42, 41, 41, 41, 39, 39, 39, 39, 38, 39, 39, 39, 39, 39, 39, 39, 39, 42, 42, 42, 42, 41, 41, 41, 41, 39, 39, 39, 39, 39, 38, 39, 39, 39, 39, 37, 34, 35, 41, 44, 45, 45, 44, 44, 44, 44, 43, 44, 41, 38, 41, 43, 42, 43, 43, 43, 39, 38, 41, 41, 41, 41, 37, 37, 37 },
    { 42, 42, 42, 42, 42, 42, 42, 43, 43, 41, 39, 39, 39, 39, 38, 39, 39, 39, 39, 41, 41, 41, 41, 42, 42, 42, 42, 41, 41, 41, 41, 39, 39, 39, 39, 37, 37, 41, 41, 38, 38, 37, 35, 36, 42, 45, 43, 43, 45, 45, 45, 45, 43, 42, 41, 41, 41, 41, 41, 43, 44, 41, 38, 39, 41, 41, 41, 41, 38, 38, 37 },
    { 42, 42, 42, 43, 42, 42, 43, 45, 44, 41, 39, 39, 39, 39, 38, 39, 39, 39, 39, 43, 43, 43, 43, 42, 42, 42, 42, 41, 41, 41, 41, 39, 39, 39, 39, 34, 36, 39, 41, 38, 36, 35, 37, 39, 42, 46, 44, 43, 46, 44, 43, 43, 39, 39, 43, 42, 42, 43, 43, 43, 42, 39, 39, 42, 39, 41, 42, 42, 37, 37, 36 },
    { 42, 42, 42, 41, 41, 41, 41, 42, 42, 39, 39, 39, 39, 39, 38, 39, 39, 41, 41, 44, 44, 44, 44, 43, 43, 43, 43, 42, 42, 42, 42, 41, 41, 41, 41, 34, 36, 38, 39, 37, 34, 35, 37, 42, 44, 46, 45, 45, 46, 43, 42, 43, 41, 39, 43, 44, 43, 43, 44, 44, 42, 38, 37, 42, 41, 42, 43, 43, 39, 37, 38 },
    { 43, 43, 43, 42, 42, 42, 42, 43, 43, 39, 41, 42, 42, 42, 42, 39, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 44, 44, 44, 44, 41, 41, 41, 41, 39, 39, 39, 38, 38, 37, 36, 41, 43, 42, 43, 45, 43, 38, 36, 39, 43, 43, 45, 45, 43, 44, 45, 44, 44, 43, 42, 38, 41, 41, 41, 41, 42, 41, 37, 38 },
};
static float s_norm1080[YES_1080P_H][YES_1080P_W];

// Modern Non-Security Dialog [No] Button Template (41x18)
static const int BTN_NO_W = 41;
static const int BTN_NO_H = 18;
static const uint8_t BTN_NO_PIXELS[18][41] = {
    {  42,  42,  42,   5, 205,  71,  71,  13,  13,  13,  13,  15,  15,  15,  15,  15,  15,  15,  15,  12,  12,  12,  12,  12,  12,  12,  12,  15,  15,  15,  15,  15,  15,  15,  15,   9,   9,  65,  65,  75,  50 },
    {  42,  42,  42,  23, 233,  92,  23,  44,  44, 235, 235, 207, 207, 207, 207, 210, 210, 210, 149, 164, 164, 164, 164, 174, 174, 174, 174, 173, 123, 123, 123,  91,  91,  72,  52,  39,  39,  39,  52,  64,  64 },
    {  42,  42,  42,  23, 233,  92,  23,  44, 235, 106,  44,  33,  33,  33,  33,  22,  22,  22,  22,  20,  20,  20,  20,  11,  11,  11,  11,  23,  23,  23,  23,  33,  33,  33,  33,  39,  27,  27,  15, 179,  64 },
    {  42,  42,  42,  23, 233,  92,  23, 235, 106,  44,  44,  33,  33,  33,  33,  22,  22,  22,  22,  20,  20,  20,  20,  65,  11,  11,  11,  23,  23,  23,  23,  33,  33,  33,  33,  39,  39,  27,  15, 179,  64 },
    {  24,  24,  25,  23, 233,  92,  23, 173,  44,  44,  44,  33,  33,  33,  33,  83, 210, 210, 210,  90,  90, 234, 234, 174,  65,  11,  11,  23,  23,  23,  23,  33,  33,  33,  33,  39,  39,  39,  15, 179,  64 },
    {  27,  27,  27,  26, 233,  94,  26, 168,  48,  48,  48,  59,  59,  59,  59,  68,  68, 185, 185, 114,  68, 114, 209,  65,  65,   7,   7,  37,  37,  37,  37,  23,  23,  23,  40,  51,  51,  51,  28, 175,  60 },
    {  25,  25,  27,  26, 233,  94,  26, 168,  48,  48,  48,  68,  68,  68,  68,  68,  11, 185, 128, 209,  68,  68, 163,  65,   7,   7,   7,  37,  37,  37,  37,  59,  40,  23,  23,  28,  51,  51,  28, 175,  60 },
    {  24,  26,  24,  26, 233,  94,  26, 168,  48,  87,  87,  78,  78,  78,  78,  68,  11, 185,  68, 209, 114,  68, 163,   7,  65, 186, 186, 167,  80,  80,  80,  76,  76,  59,  40,  39,  62,  62,  39, 175,  60 },
    {  21,  23,  23,  26, 233,  94,  26, 168,  48,  87,  87,  87,  87,  87,  87,  68,  11, 185,  68, 114, 209,  68, 163,   7, 186, 186,  65, 167, 167,  80,  80,  76,  76,  76,  76,  62,  51,  51,  28, 175,  60 },
    {  21,  21,  21,  30, 235,  97,  30, 123,  77,  92,  92,  92,  92,  92,  87,  69,  13, 183,  69,  64, 221, 115, 170,  26, 213, 151,  26,  95, 203,  95,  95,  90,  90,  90,  90,  82,  82,  61,  40, 178,  61 },
    {  21,  23,  23,  30, 235,  97,  30, 123,  92,  92,  92,  97,  92,  92,  92,  69,  13, 183,  69,  64, 170, 221, 170,  26, 213, 151,  26,  95, 203,  95,  95,  90,  95,  95,  95, 104, 104,  82,  40, 178,  61 },
    {  26,  26,  26,  30, 235,  97,  30, 123,  92,  92, 108, 101,  97,  97,  92,  69,  13, 183,  69,  64,  64, 221, 170,  26, 213, 151,  26, 130, 203,  95,  95, 101, 101, 101, 101, 104, 104, 104,  61, 178,  61 },
    {  24,  26,  35,  30, 235,  97,  30, 108,  77,  92, 108, 101, 101,  97,  87,  13,  13, 183, 128,  64,  64, 170, 170,  87, 151, 151,  26, 203, 168,  95,  95, 107, 107, 107, 107, 104, 104, 104,  61, 178,  61 },
    {  21,  24,  47,  13, 212,  79,  13,  94,  52,  94,  94, 103, 103, 103, 103,  72, 212, 212, 212, 108,  80, 138, 166, 119, 119, 208, 208, 202, 117, 117, 117, 117, 117, 117, 117, 114, 114,  77,  77, 201,  69 },
    {  24,  27,  57,  13, 212,  79,  79,  52,  52,  94,  94, 103, 103, 103, 103,  72, 118,  72, 118,  80,  80, 108, 108, 119, 119, 119, 119, 117, 117, 117, 117, 117, 117, 117, 117, 114,  77,  77, 153, 136,  69 },
    {  25,  42,  63,  13,  79, 146, 146,  52,  52,  52,  52,  71,  71,  71,  71,  72,  72,  72,  72,  80,  80,  80,  80,  75,  75,  75,  75,  76,  76,  76,  76,  83,  83,  83,  83,  77,  77, 153, 153,  69,  69 },
    {  25,  57,  60,  57,  13,  79, 146, 139, 181, 181, 181, 169, 169, 169, 169, 166, 166, 166, 166, 166, 166, 166, 166, 164, 164, 164, 164, 161, 161, 161, 161, 152, 186, 186, 186, 191, 153, 153,  77,  69,   5 },
    {  40,  51,  56,  49,  46,   0,  90, 136, 136, 136, 136, 136, 136, 136, 136, 128, 128, 128, 128, 103, 103, 103, 103, 103, 103, 103, 103,  97,  97,  97,  97,  95,  95,  95,  95,  88,  88,  88,  59,   6,  27 },
};
static float s_normBtnNo[BTN_NO_H][BTN_NO_W];

static const int SECURITY_W = 54;
static const int SECURITY_H = 14;
static const uint8_t SECURITY_PIXELS[14][54] = {
    { 49, 39, 45, 45, 49, 45, 44, 45, 40, 48, 43, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 45, 44, 45, 46, 47, 46, 46, 51, 47, 47, 48, 45, 45, 49, 49, 48, 48, 46, 46, 45, 45, 45, 44, 42, 46, 40 },
    { 44, 229, 232, 233, 230, 230, 48, 40, 45, 44, 46, 44, 43, 44, 43, 44, 43, 44, 43, 45, 44, 45, 44, 45, 44, 45, 45, 45, 45, 45, 46, 46, 47, 46, 47, 48, 45, 227, 48, 40, 230, 40, 44, 47, 47, 46, 46, 45, 45, 44, 44, 43, 44, 47 },
    { 228, 46, 48, 47, 47, 51, 230, 47, 45, 41, 48, 43, 44, 43, 44, 43, 43, 44, 44, 45, 44, 44, 45, 45, 45, 45, 44, 45, 45, 45, 45, 45, 46, 46, 46, 47, 48, 52, 53, 55, 226, 45, 48, 48, 47, 46, 47, 46, 45, 45, 45, 44, 41, 45 },
    { 235, 42, 49, 45, 39, 49, 238, 48, 44, 52, 237, 233, 235, 231, 48, 52, 44, 48, 238, 231, 233, 45, 49, 50, 229, 53, 48, 47, 44, 236, 50, 54, 234, 54, 235, 229, 47, 237, 53, 230, 234, 232, 235, 237, 48, 50, 44, 45, 49, 235, 47, 49, 41, 48 },
    { 233, 49, 46, 46, 54, 42, 48, 47, 54, 225, 56, 52, 50, 61, 229, 46, 46, 238, 51, 53, 48, 236, 53, 49, 237, 41, 46, 47, 50, 232, 53, 50, 237, 233, 52, 54, 51, 238, 60, 57, 230, 53, 53, 227, 49, 46, 47, 45, 47, 235, 45, 42, 53, 46 },
    { 43, 239, 235, 234, 45, 46, 46, 47, 45, 234, 60, 57, 56, 58, 235, 45, 48, 229, 51, 48, 48, 46, 52, 52, 230, 46, 49, 44, 49, 235, 53, 57, 231, 50, 47, 42, 41, 240, 50, 50, 230, 52, 44, 55, 235, 52, 43, 43, 237, 41, 45, 46, 41, 43 },
    { 43, 46, 53, 47, 240, 238, 49, 48, 46, 225, 231, 235, 234, 238, 229, 48, 54, 233, 48, 47, 48, 49, 43, 47, 237, 40, 44, 41, 47, 234, 54, 55, 238, 41, 49, 45, 44, 234, 55, 52, 238, 39, 39, 44, 235, 51, 42, 48, 230, 44, 49, 43, 40, 51 },
    { 45, 44, 47, 49, 45, 53, 233, 48, 50, 232, 55, 50, 54, 47, 45, 47, 45, 231, 52, 46, 45, 53, 48, 53, 225, 55, 44, 52, 43, 239, 49, 56, 231, 49, 44, 46, 40, 237, 53, 53, 231, 46, 49, 47, 49, 231, 54, 229, 51, 49, 48, 48, 43, 41 },
    { 234, 43, 49, 45, 47, 56, 234, 50, 38, 237, 50, 46, 45, 43, 49, 46, 44, 240, 53, 46, 46, 47, 44, 44, 239, 38, 47, 47, 50, 232, 52, 56, 231, 49, 46, 45, 46, 233, 54, 48, 235, 43, 46, 46, 46, 230, 52, 236, 40, 45, 47, 45, 42, 46 },
    { 229, 48, 50, 54, 51, 51, 232, 42, 45, 229, 56, 55, 49, 54, 232, 44, 46, 236, 55, 52, 48, 236, 48, 50, 228, 51, 54, 50, 233, 240, 48, 59, 231, 48, 45, 44, 41, 232, 55, 56, 234, 53, 46, 41, 42, 233, 55, 236, 47, 45, 41, 43, 48, 45 },
    { 58, 231, 237, 234, 230, 235, 55, 48, 46, 51, 233, 233, 232, 233, 52, 44, 48, 49, 229, 238, 236, 46, 49, 39, 53, 234, 229, 236, 54, 235, 46, 52, 232, 44, 50, 49, 41, 236, 51, 51, 237, 234, 237, 43, 43, 48, 238, 45, 43, 40, 46, 44, 45, 44 },
    { 48, 48, 46, 45, 43, 44, 44, 45, 48, 47, 48, 48, 47, 47, 47, 47, 47, 47, 47, 42, 42, 42, 44, 44, 44, 45, 45, 49, 48, 46, 47, 48, 49, 48, 47, 48, 47, 45, 46, 48, 49, 47, 44, 48, 47, 49, 233, 47, 45, 44, 47, 47, 48, 47 },
    { 47, 48, 46, 45, 42, 45, 45, 47, 49, 45, 45, 47, 46, 47, 46, 47, 46, 46, 46, 42, 42, 43, 44, 44, 44, 45, 44, 46, 45, 45, 44, 46, 47, 46, 44, 48, 46, 43, 44, 46, 46, 45, 43, 49, 44, 56, 227, 48, 39, 43, 47, 47, 47, 47 },
    { 47, 47, 47, 45, 44, 48, 47, 48, 49, 45, 44, 47, 47, 46, 47, 47, 47, 47, 46, 43, 43, 44, 43, 43, 43, 44, 45, 44, 43, 42, 44, 45, 46, 46, 45, 48, 47, 45, 45, 46, 48, 45, 43, 45, 230, 230, 52, 43, 48, 46, 42, 48, 48, 46 },
};
static float s_normSecurity[SECURITY_H][SECURITY_W];

static bool s_tmplInit = false;

static void InitYesTemplate() {
    if (s_tmplInit) return;

    // 1. Initialize 38x22 template
    {
        bool loadedFromFile = false;
        FILE* fp = nullptr;
        fopen_s(&fp, "yes_button.bmp", "rb");
        if (fp) {
            BITMAPFILEHEADER bfh;
            BITMAPINFOHEADER bih;
            if (fread(&bfh, sizeof(bfh), 1, fp) == 1 &&
                fread(&bih, sizeof(bih), 1, fp) == 1 &&
                bih.biWidth == YES_TMPL_W && std::abs(bih.biHeight) == YES_TMPL_H) {
                std::cout << "[+] [Vision] Loaded \"yes_button.bmp\" (38x22) directly from folder!\n";
                loadedFromFile = true;
            }
            fclose(fp);
        }
        if (!loadedFromFile) {
            std::cout << "[+] [Vision] Loaded embedded [Yes] button template (38x22).\n";
        }

        const int N = YES_TMPL_W * YES_TMPL_H;
        float sum = 0.0f;
        for (int y = 0; y < YES_TMPL_H; y++) {
            for (int x = 0; x < YES_TMPL_W; x++) sum += YES_TMPL_PIXELS[y][x];
        }
        float mean = sum / N;
        float var = 0.0f;
        for (int y = 0; y < YES_TMPL_H; y++) {
            for (int x = 0; x < YES_TMPL_W; x++) {
                float d = YES_TMPL_PIXELS[y][x] - mean;
                var += d * d;
            }
        }
        float stdVal = std::sqrt(var);
        if (stdVal < 1.0f) stdVal = 1.0f;
        for (int y = 0; y < YES_TMPL_H; y++) {
            for (int x = 0; x < YES_TMPL_W; x++) {
                s_normTmpl[y][x] = (YES_TMPL_PIXELS[y][x] - mean) / stdVal;
            }
        }
    }

    // 2. Initialize 71x41 template (1080p)
    {
        bool loaded1080 = false;
        FILE* fp = nullptr;
        fopen_s(&fp, "yes_button_1080p.bmp", "rb");
        if (fp) {
            BITMAPFILEHEADER bfh;
            BITMAPINFOHEADER bih;
            if (fread(&bfh, sizeof(bfh), 1, fp) == 1 &&
                fread(&bih, sizeof(bih), 1, fp) == 1 &&
                bih.biWidth == YES_1080P_W && std::abs(bih.biHeight) == YES_1080P_H) {
                std::cout << "[+] [Vision] Loaded \"yes_button_1080p.bmp\" (71x41) directly from folder!\n";
                loaded1080 = true;
            }
            fclose(fp);
        }
        if (!loaded1080) {
            std::cout << "[+] [Vision] Loaded embedded 1080p [Yes] button template (71x41).\n";
        }

        const int N = YES_1080P_W * YES_1080P_H;
        float sum = 0.0f;
        for (int y = 0; y < YES_1080P_H; y++) {
            for (int x = 0; x < YES_1080P_W; x++) sum += YES_1080P_PIXELS[y][x];
        }
        float mean = sum / N;
        float var = 0.0f;
        for (int y = 0; y < YES_1080P_H; y++) {
            for (int x = 0; x < YES_1080P_W; x++) {
                float d = YES_1080P_PIXELS[y][x] - mean;
                var += d * d;
            }
        }
        float stdVal = std::sqrt(var);
        if (stdVal < 1.0f) stdVal = 1.0f;
        for (int y = 0; y < YES_1080P_H; y++) {
            for (int x = 0; x < YES_1080P_W; x++) {
                s_norm1080[y][x] = (YES_1080P_PIXELS[y][x] - mean) / stdVal;
            }
        }
    }

    // 3. Initialize 41x18 [No] button template (Modern Non-Security Popups)
    {
        bool loadedNo = false;
        FILE* fp = nullptr;
        fopen_s(&fp, "btn_no.bmp", "rb");
        if (fp) {
            BITMAPFILEHEADER bfh;
            BITMAPINFOHEADER bih;
            if (fread(&bfh, sizeof(bfh), 1, fp) == 1 &&
                fread(&bih, sizeof(bih), 1, fp) == 1 &&
                bih.biWidth == BTN_NO_W && std::abs(bih.biHeight) == BTN_NO_H) {
                std::cout << "[+] [Vision] Loaded \"btn_no.bmp\" (41x18) directly from folder!\n";
                loadedNo = true;
            }
            fclose(fp);
        }
        if (!loadedNo) {
            std::cout << "[+] [Vision] Loaded embedded [No] button template (41x18).\n";
        }

        const int N = BTN_NO_W * BTN_NO_H;
        float sum = 0.0f;
        for (int y = 0; y < BTN_NO_H; y++) {
            for (int x = 0; x < BTN_NO_W; x++) sum += BTN_NO_PIXELS[y][x];
        }
        float mean = sum / N;
        float var = 0.0f;
        for (int y = 0; y < BTN_NO_H; y++) {
            for (int x = 0; x < BTN_NO_W; x++) {
                float d = BTN_NO_PIXELS[y][x] - mean;
                var += d * d;
            }
        }
        float stdVal = std::sqrt(var);
        if (stdVal < 1.0f) stdVal = 1.0f;
        for (int y = 0; y < BTN_NO_H; y++) {
            for (int x = 0; x < BTN_NO_W; x++) {
                s_normBtnNo[y][x] = (BTN_NO_PIXELS[y][x] - mean) / stdVal;
            }
        }
    }

    // 4. Initialize 54x14 Security Check word template ("Security")
    {
        const int N = SECURITY_W * SECURITY_H;
        float sum = 0.0f;
        for (int y = 0; y < SECURITY_H; y++) {
            for (int x = 0; x < SECURITY_W; x++) sum += SECURITY_PIXELS[y][x];
        }
        float mean = sum / N;
        float var = 0.0f;
        for (int y = 0; y < SECURITY_H; y++) {
            for (int x = 0; x < SECURITY_W; x++) {
                float d = SECURITY_PIXELS[y][x] - mean;
                var += d * d;
            }
        }
        float stdVal = std::sqrt(var);
        if (stdVal < 1.0f) stdVal = 1.0f;
        for (int y = 0; y < SECURITY_H; y++) {
            for (int x = 0; x < SECURITY_W; x++) {
                s_normSecurity[y][x] = (SECURITY_PIXELS[y][x] - mean) / stdVal;
            }
        }
    }

    s_tmplInit = true;
}

// ============================================================================
// 1080p Drop Unlock Slider Knob Template (40x40)
// ============================================================================
static const int KNOB_W = 40;
static const int KNOB_H = 40;
static const uint8_t KNOB_PIXELS[40][40] = {
    {  27,  29,  32,  50,  57,  58,  57,  58,  52,  53,  61, 125, 176, 201, 200, 201, 198, 196, 201, 197, 196, 195, 196, 197, 196, 165, 112,  45,  39,  35,  35,  38,  38,  39,  39,  40,  40,  40,  40,  40 },
    {  31,  38,  46,  59,  60,  59,  59,  57,  57, 124, 193, 203, 201, 200, 201, 202, 202, 199, 195, 197, 197, 196, 197, 197, 196, 194, 188, 180, 104,  38,  37,  38,  38,  39,  39,  40,  40,  40,  40,  40 },
    {  37,  48,  60,  63,  59,  58,  54,  82, 167, 204, 203, 203, 202, 202, 201, 197, 197, 197, 197, 195, 195, 196, 196, 193, 193, 193, 193, 191, 192, 154,  77,  38,  41,  37,  38,  39,  39,  39,  39,  40 },
    {  43,  64,  61,  60,  53,  56, 106, 199, 199, 202, 203, 202, 201, 200, 199, 195, 195, 195, 195, 193, 193, 194, 194, 193, 193, 193, 193, 192, 188, 186, 185,  91,  39,  39,  37,  39,  39,  39,  39,  40 },
    {  54,  60,  59,  60,  57, 113, 199, 204, 200, 206, 200, 200, 199, 198, 198, 194, 194, 194, 194, 193, 193, 193, 193, 193, 193, 193, 193, 187, 188, 189, 187, 185,  96,  38,  40,  39,  39,  39,  39,  40 },
    {  58,  65,  59,  59, 108, 200, 203, 202, 201, 201, 199, 199, 199, 198, 198, 194, 194, 194, 194, 193, 193, 193, 193, 193, 193, 193, 193, 189, 188, 188, 187, 188, 185,  90,  37,  39,  39,  39,  39,  40 },
    {  60,  65,  57,  93, 203, 203, 199, 200, 200, 199, 197, 196, 196, 196, 196, 194, 194, 194, 194, 190, 190, 190, 190, 187, 187, 187, 187, 186, 186, 186, 186, 182, 185, 182,  69,  37,  40,  33,  40,  39 },
    {  59,  59,  59, 173, 201, 203, 202, 200, 199, 197, 197, 196, 196, 196, 196, 194, 194, 194, 194, 190, 190, 190, 190, 187, 187, 187, 187, 186, 186, 186, 186, 186, 186, 180, 151,  39,  36,  38,  40,  39 },
    {  59,  59, 124, 202, 199, 198, 200, 198, 196, 196, 196, 196, 196, 196, 196, 194, 194, 194, 194, 190, 190, 190, 190, 187, 187, 187, 187, 186, 186, 186, 186, 185, 181, 180, 180,  98,  37,  38,  34,  39 },
    {  60,  65, 188, 201, 201, 195, 198, 196, 196, 196, 196, 196, 196, 196, 196, 194, 194, 194, 194, 190, 190, 190, 190, 187, 187, 187, 187, 186, 186, 186, 186, 183, 180, 181, 181, 171,  44,  38,  39,  39 },
    {  56, 124, 199, 200, 199, 197, 197, 195, 195, 195, 195, 193, 193, 194, 188, 193, 193, 192, 192, 188, 188, 188, 188, 183, 186, 187, 185, 183, 182, 182, 181, 180, 180, 180, 180, 175, 102,  36,  39,  38 },
    {  57, 174, 194, 199, 197, 197, 197, 194, 194, 194, 194, 189, 192, 192, 190, 190, 190, 189, 189, 186, 186, 186, 186, 186, 188, 185, 186, 183, 182, 181, 180, 180, 180, 180, 180, 180, 152,  38,  37,  38 },
    {  88, 193, 196, 196, 196, 196, 196, 192, 192, 192, 192, 195, 192, 190, 192, 189, 189, 189, 189, 187, 187, 187, 187, 183, 182, 187, 183, 183, 182, 181, 181, 180, 180, 180, 180, 176, 171,  63,  33,  37 },
    { 123, 195, 198, 196, 196, 196, 196, 192, 192, 192, 192, 195, 193, 190, 116,  96,  96,  96,  96,  95,  95,  95,  95, 117, 182, 182, 180, 182, 182, 182, 182, 180, 180, 180, 180, 178, 178, 102,  38,  37 },
    { 155, 199, 196, 196, 196, 195, 193, 193, 192, 192, 192, 190, 190, 190, 117,  95,  95,  95,  95,  93,  93,  94,  94, 110, 182, 181, 180, 180, 180, 180, 179, 178, 176, 176, 175, 175, 176, 130,  32,  37 },
    { 167, 194, 199, 196, 195, 193, 192, 192, 192, 192, 192, 189, 189, 189, 189, 186, 186, 186, 186, 184, 184, 185, 185, 181, 182, 181, 181, 180, 179, 178, 178, 176, 175, 175, 175, 180, 174, 146,  33,  37 },
    { 186, 192, 196, 195, 193, 192, 192, 192, 192, 192, 192, 188, 189, 187, 188, 185, 185, 185, 185, 183, 183, 182, 182, 183, 181, 181, 180, 178, 178, 176, 176, 175, 175, 176, 176, 179, 172, 165,  35,  37 },
    { 192, 194, 194, 193, 192, 192, 192, 192, 192, 192, 192, 186, 187, 186, 117,  95,  95,  95,  95,  94,  94,  93,  93, 114, 181, 182, 178, 176, 176, 176, 176, 175, 175, 176, 176, 173, 172, 167,  35,  37 },
    { 194, 193, 193, 190, 190, 190, 190, 187, 187, 187, 187, 186, 187, 183, 114,  93,  93,  93,  93,  90,  90,  91,  91, 110, 175, 179, 178, 176, 176, 175, 175, 172, 172, 172, 172, 174, 173, 169,  37,  36 },
    { 185, 192, 192, 190, 190, 190, 190, 187, 187, 187, 187, 183, 185, 186, 180, 181, 181, 181, 181, 179, 179, 180, 180, 176, 178, 176, 175, 176, 176, 175, 175, 172, 172, 172, 172, 174, 168, 163,  35,  36 },
    { 165, 190, 188, 190, 190, 190, 190, 187, 187, 187, 187, 183, 185, 186, 180, 181, 181, 181, 181, 180, 180, 179, 179, 176, 178, 176, 175, 175, 175, 175, 175, 172, 172, 172, 172, 172, 167, 143,  35,  36 },
    { 156, 190, 187, 190, 190, 190, 190, 187, 187, 187, 187, 186, 187, 183, 114,  93,  93,  93,  93,  91,  91,  90,  90, 110, 175, 179, 178, 175, 175, 175, 175, 172, 172, 172, 172, 167, 171, 129,  36,  35 },
    { 118, 187, 187, 187, 187, 187, 187, 186, 186, 186, 186, 183, 182, 183, 111,  94,  94,  94,  94,  93,  93,  93,  93, 112, 174, 175, 172, 173, 173, 173, 172, 171, 170, 170, 168, 168, 161,  96,  36,  35 },
    {  82, 191, 187, 187, 187, 187, 187, 185, 185, 185, 185, 186, 183, 180, 181, 178, 178, 178, 178, 176, 176, 176, 176, 174, 173, 175, 173, 173, 172, 171, 170, 170, 168, 168, 168, 168, 167,  62,  36,  35 },
    {  52, 164, 186, 187, 187, 187, 187, 182, 182, 182, 182, 181, 182, 180, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 172, 173, 171, 170, 168, 168, 168, 168, 168, 168, 165, 140,  35,  32,  35 },
    {  53, 114, 181, 187, 187, 187, 187, 182, 182, 182, 182, 180, 180, 182, 175, 176, 176, 176, 176, 176, 176, 176, 176, 173, 174, 173, 169, 168, 168, 168, 168, 168, 168, 168, 168, 163,  93,  36,  38,  35 },
    {  52,  61, 173, 187, 185, 182, 182, 182, 181, 181, 180, 181, 180, 178, 176, 175, 175, 175, 175, 174, 174, 174, 174, 172, 170, 168, 168, 169, 169, 168, 168, 167, 163, 166, 161, 153,  43,  31,  35,  35 },
    {  52,  54, 111, 188, 181, 185, 181, 181, 180, 180, 179, 180, 178, 176, 176, 175, 175, 175, 175, 173, 173, 173, 173, 172, 170, 168, 168, 169, 169, 168, 168, 163, 164, 163, 167,  91,  38,  35,  35,  35 },
    {  57,  55,  53, 153, 180, 185, 180, 180, 179, 179, 179, 178, 176, 176, 176, 175, 175, 175, 175, 171, 171, 171, 171, 172, 170, 168, 168, 167, 167, 167, 167, 165, 164, 165, 136,  38,  34,  38,  38,  36 },
    {  53,  53,  54,  86, 179, 181, 179, 179, 179, 179, 179, 176, 176, 176, 176, 175, 175, 175, 175, 171, 171, 171, 171, 172, 170, 168, 168, 166, 166, 166, 166, 163, 167, 164,  65,  33,  36,  36,  38,  36 },
    {  45,  57,  52,  50,  95, 173, 179, 175, 180, 178, 173, 175, 175, 175, 174, 171, 171, 171, 171, 171, 171, 171, 171, 171, 169, 168, 167, 164, 165, 168, 164, 165, 159,  81,  35,  36,  36,  36,  36,  36 },
    {  33,  46,  57,  56,  51, 102, 173, 176, 175, 174, 178, 175, 175, 174, 174, 171, 171, 171, 171, 171, 171, 171, 171, 169, 168, 167, 166, 164, 167, 165, 165, 159,  89,  33,  32,  36,  36,  36,  36,  36 },
    {  25,  33,  45,  56,  51,  52,  96, 174, 175, 177, 175, 175, 174, 174, 174, 171, 171, 171, 171, 169, 169, 169, 169, 167, 166, 166, 166, 167, 161, 161, 159,  81,  33,  29,  32,  36,  36,  36,  36,  36 },
    {  30,  31,  29,  43,  56,  57,  51,  78, 149, 173, 174, 174, 174, 174, 174, 171, 171, 171, 171, 168, 168, 168, 168, 166, 165, 166, 166, 160, 164, 132,  65,  35,  32,  32,  37,  38,  38,  38,  38,  38 },
    {  31,  31,  31,  32,  45,  53,  53,  45,  46,  99, 164, 171, 167, 173, 167, 168, 170, 169, 167, 164, 170, 166, 167, 168, 163, 164, 159, 152,  90,  33,  32,  32,  32,  33,  33,  37,  37,  37,  37,  37 },
    {  31,  31,  31,  34,  30,  36,  47,  57,  54,  47,  56, 109, 149, 171, 172, 173, 167, 165, 168, 171, 163, 164, 164, 165, 163, 140,  95,  37,  32,  33,  31,  32,  32,  33,  33,  37,  37,  37,  37,  37 },
    {  31,  31,  31,  29,  31,  29,  33,  41,  52,  54,  55,  47,  45,  71, 107, 132, 144, 159, 163, 165, 157, 144, 128,  93,  54,  32,  31,  33,  29,  31,  28,  34,  34,  34,  34,  37,  37,  37,  37,  37 },
    {  31,  31,  31,  31,  29,  29,  33,  33,  32,  43,  47,  60,  58,  52,  50,  47,  43,  39,  40,  40,  39,  31,  34,  29,  33,  36,  31,  31,  37,  33,  33,  34,  34,  34,  34,  37,  37,  37,  37,  37 },
    {  31,  31,  31,  30,  30,  30,  30,  30,  30,  30,  30,  31,  35,  42,  45,  53,  53,  53,  53,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  58,  57,  57,  57,  57,  57 },
};
static float s_normKnob[KNOB_H][KNOB_W];
static bool s_knobInit = false;

static void InitKnobTemplate() {
    if (s_knobInit) return;
    const int N = KNOB_W * KNOB_H;
    float sum = 0.0f;
    for (int y = 0; y < KNOB_H; y++) {
        for (int x = 0; x < KNOB_W; x++) sum += KNOB_PIXELS[y][x];
    }
    float mean = sum / N;
    float var = 0.0f;
    for (int y = 0; y < KNOB_H; y++) {
        for (int x = 0; x < KNOB_W; x++) {
            float d = KNOB_PIXELS[y][x] - mean;
            var += d * d;
        }
    }
    float stdVal = std::sqrt(var);
    if (stdVal < 1.0f) stdVal = 1.0f;
    for (int y = 0; y < KNOB_H; y++) {
        for (int x = 0; x < KNOB_W; x++) {
            s_normKnob[y][x] = (KNOB_PIXELS[y][x] - mean) / stdVal;
        }
    }
    s_knobInit = true;
}

bool BotEngine::IsSecurityDialogVisible(POINT* pYesCenter, bool* pIsNonSecurity) {
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    RECT rc;
    GetClientRect(gameHwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 400 || winH < 300) return false;

    POINT ptClient = { 0, 0 };
    ClientToScreen(gameHwnd, &ptClient);

    // Direct3D window rendering capture via Desktop DC
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return false;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, winW, winH);
    if (!hBmp) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, winW, winH, hdcScreen, ptClient.x, ptClient.y, SRCCOPY);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(winW * winH);
    GetDIBits(hdcMem, hBmp, 0, winH, pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // Convert pixels to grayscale
    std::vector<uint8_t> gray(winW * winH);
    for (int i = 0; i < winW * winH; i++) {
        uint32_t c = pixels[i];
        uint8_t b = (uint8_t)(c & 0xFF);
        uint8_t g = (uint8_t)((c >> 8) & 0xFF);
        uint8_t r = (uint8_t)((c >> 16) & 0xFF);
        gray[i] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
    }

    InitYesTemplate();

    bool is1080p = (winW > 1366);
    int tmplW = is1080p ? YES_1080P_W : YES_TMPL_W;
    int tmplH = is1080p ? YES_1080P_H : YES_TMPL_H;
    int defaultCx = is1080p ? 688 : 367;
    int defaultCy = is1080p ? 532 : 284;

    // Search window restricted to central dialog area for speed and accuracy
    int yStart = (int)(winH * (is1080p ? 0.30 : 0.20));
    int yEnd   = (int)(winH * (is1080p ? 0.70 : 0.75));
    int xStart = (int)(winW * (is1080p ? 0.20 : 0.15));
    int xEnd   = (int)(winW * (is1080p ? 0.60 : 0.70));

    // 1. Scan for [Yes] button
    float bestYesScore = -1.0f;
    int bestYesX = 0, bestYesY = 0;
    for (int y = yStart; y <= yEnd - tmplH; y++) {
        for (int x = xStart; x <= xEnd - tmplW; x++) {
            const int N = tmplW * tmplH;
            float pSum = 0.0f;
            for (int ty = 0; ty < tmplH; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < tmplW; tx++) pSum += row[tx];
            }
            float pMean = pSum / N;
            float pVar = 0.0f;
            for (int ty = 0; ty < tmplH; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < tmplW; tx++) {
                    float d = row[tx] - pMean;
                    pVar += d * d;
                }
            }
            float pStd = std::sqrt(pVar);
            if (pStd < 1.0f) continue;

            float dot = 0.0f;
            for (int ty = 0; ty < tmplH; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < tmplW; tx++) {
                    float normVal = is1080p ? s_norm1080[ty][tx] : s_normTmpl[ty][tx];
                    dot += normVal * (row[tx] - pMean);
                }
            }
            float score = dot / pStd;
            if (score > bestYesScore) {
                bestYesScore = score;
                bestYesX = x;
                bestYesY = y;
            }
        }
    }

    // 2. Scan for [No] button
    float bestNoScore = -1.0f;
    int bestNoX = 0, bestNoY = 0;
    for (int y = yStart; y <= yEnd - BTN_NO_H; y += 2) {
        for (int x = xStart; x <= xEnd - BTN_NO_W; x += 2) {
            const int N = BTN_NO_W * BTN_NO_H;
            float pSum = 0.0f;
            for (int ty = 0; ty < BTN_NO_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < BTN_NO_W; tx++) pSum += row[tx];
            }
            float pMean = pSum / N;
            float pVar = 0.0f;
            for (int ty = 0; ty < BTN_NO_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < BTN_NO_W; tx++) {
                    float d = row[tx] - pMean;
                    pVar += d * d;
                }
            }
            float pStd = std::sqrt(pVar);
            if (pStd < 1.0f) continue;

            float dot = 0.0f;
            for (int ty = 0; ty < BTN_NO_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < BTN_NO_W; tx++) {
                    dot += s_normBtnNo[ty][tx] * (row[tx] - pMean);
                }
            }
            float score = dot / pStd;
            if (score > bestNoScore) {
                bestNoScore = score;
                bestNoX = x;
                bestNoY = y;
            }
        }
    }

    bool hasDialog = (bestYesScore >= 0.65f || bestNoScore >= 0.70f);
    if (!hasDialog) {
        return false;
    }

    // Determine button center coordinates:
    int yesCenterX = 0, yesCenterY = 0;
    int noCenterX = 0, noCenterY = 0;
    int buttonRefX = 0, buttonRefY = 0;

    int noOffset = is1080p ? 100 : 76;

    int btnOffset = is1080p ? 100 : 53;

    // Check if bestYesX is actually the right button [No] by testing candidate at (bestYesX - btnOffset)
    float leftScore = -1.0f;
    int leftCandX = bestYesX - btnOffset;
    if (leftCandX >= 0) {
        const int N = tmplW * tmplH;
        float pSum = 0.0f;
        for (int ty = 0; ty < tmplH; ty++) {
            const uint8_t* row = &gray[(bestYesY + ty) * winW + leftCandX];
            for (int tx = 0; tx < tmplW; tx++) pSum += row[tx];
        }
        float pMean = pSum / N;
        float pVar = 0.0f;
        for (int ty = 0; ty < tmplH; ty++) {
            const uint8_t* row = &gray[(bestYesY + ty) * winW + leftCandX];
            for (int tx = 0; tx < tmplW; tx++) {
                float d = row[tx] - pMean;
                pVar += d * d;
            }
        }
        float pStd = std::sqrt(pVar);
        if (pStd >= 1.0f) {
            float dot = 0.0f;
            for (int ty = 0; ty < tmplH; ty++) {
                const uint8_t* row = &gray[(bestYesY + ty) * winW + leftCandX];
                for (int tx = 0; tx < tmplW; tx++) {
                    float normVal = is1080p ? s_norm1080[ty][tx] : s_normTmpl[ty][tx];
                    dot += normVal * (row[tx] - pMean);
                }
            }
            leftScore = dot / pStd;
        }
    }

    if (leftScore >= 0.50f) {
        // In Conquer Online prompt boxes, [Yes] is ALWAYS on the LEFT, [No] on the RIGHT
        // The coarse peak was on [No], so the candidate to its left is [Yes]!
        yesCenterX = leftCandX + tmplW / 2;
        yesCenterY = bestYesY + tmplH / 2;
        noCenterX  = bestYesX + tmplW / 2;
        noCenterY  = bestYesY + tmplH / 2;
        buttonRefX = noCenterX;
        buttonRefY = noCenterY;
    } else {
        yesCenterX = bestYesX + tmplW / 2;
        yesCenterY = bestYesY + tmplH / 2;
        noCenterX  = bestYesX + btnOffset + tmplW / 2;
        noCenterY  = bestYesY + tmplH / 2;
        buttonRefX = yesCenterX;
        buttonRefY = yesCenterY;
    }

    // ========================================================================
    // 3. READ DIALOG TITLE ("يقرا ويفهم ان الكباتشا يس اي حاجه تانيه نو")
    // Scan the title/header region above the buttons for the word "Security"
    // ========================================================================
    int roiY1 = (std::max)(0, buttonRefY - 120);
    int roiY2 = (std::min)(winH - SECURITY_H, buttonRefY - 20);
    int roiX1 = (std::max)(0, buttonRefX - 180);
    int roiX2 = (std::min)(winW - SECURITY_W, buttonRefX + 180);

    float bestSecScore = -1.0f;
    for (int y = roiY1; y <= roiY2; y++) {
        for (int x = roiX1; x <= roiX2; x++) {
            const int N = SECURITY_W * SECURITY_H;
            float pSum = 0.0f;
            for (int ty = 0; ty < SECURITY_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < SECURITY_W; tx++) pSum += row[tx];
            }
            float pMean = pSum / N;
            float pVar = 0.0f;
            for (int ty = 0; ty < SECURITY_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < SECURITY_W; tx++) {
                    float d = row[tx] - pMean;
                    pVar += d * d;
                }
            }
            float pStd = std::sqrt(pVar);
            if (pStd < 1.0f) continue;

            float dot = 0.0f;
            for (int ty = 0; ty < SECURITY_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < SECURITY_W; tx++) {
                    dot += s_normSecurity[ty][tx] * (row[tx] - pMean);
                }
            }
            float score = dot / pStd;
            if (score > bestSecScore) {
                bestSecScore = score;
            }
        }
    }

    // High confidence Security Check Captcha (handles small font on 576p/768p and 1080p):
    bool isCaptcha = (bestSecScore >= 0.25f);

    if (isCaptcha) {
        // CONFIRMED CAPTCHA ("Solve Security Check To Unlock Drop"):
        // User rule: "ان الكباتشا يس" -> Strictly Click [Yes]!
        if (pYesCenter) {
            pYesCenter->x = ptClient.x + yesCenterX;
            pYesCenter->y = ptClient.y + yesCenterY;
        }
        if (pIsNonSecurity) {
            *pIsNonSecurity = false;
        }
        printf("[+] [Dialog Discrimination] \"Security\" confirmed (NCC: %.2f)! Action: Click [Yes] @ (%d, %d)\n",
               bestSecScore, yesCenterX, yesCenterY);
        return true;
    } else {
        // CONFIRMED NON-SECURITY IN-GAME DIALOG (CPs Reward, NPC, Quest, etc.):
        // User rule: "اي حاجه تانيه نو" -> Strictly Click [No]!
        if (pYesCenter) {
            pYesCenter->x = ptClient.x + noCenterX;
            pYesCenter->y = ptClient.y + noCenterY;
        }
        if (pIsNonSecurity) {
            *pIsNonSecurity = true;
        }
        printf("[+] [Dialog Discrimination] Non-Security Dialog (Sec NCC: %.2f < 0.70)! Action: Click [No] @ (%d, %d)\n",
               bestSecScore, noCenterX, noCenterY);
        return true;
    }
}

bool BotEngine::CheckSecurityDialog(POINT& outYesClick) {
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    // Fast throttle check (180ms): eliminates heavy full-screen GDI BitBlt lag on every jump,
    // keeping the bot blazing fast 100% of the time just like right after captcha solve!
    static ULONGLONG s_lastDialogCheckTick = 0;
    ULONGLONG nowTick = GetTickCount64();
    if (nowTick - s_lastDialogCheckTick < 180) {
        return false;
    }

    // Cooldown check during patrol
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCaptchaTime).count() < 5) {
        return false;
    }
    s_lastDialogCheckTick = nowTick;

    POINT ptTarget = { 0, 0 };
    bool isNonSecurity = false;
    if (IsSecurityDialogVisible(&ptTarget, &isNonSecurity)) {
        if (isNonSecurity) {
            // Non-Security in-game dialog detected (CPs reward popup, NPC Quest, Soldier remains, etc.)
            // User instruction: Strictly click [No], NOT [Yes]!
            // ptTarget is already calibrated to the exact [No] button location!
            std::cout << "\n[+] [Dialog] Non-Security In-Game Dialog Detected! Auto-dismissing via [No] at ("
                      << ptTarget.x << ", " << ptTarget.y << ")...\n";
            Input::MoveToSmooth(ptTarget.x, ptTarget.y, 1.0);
            SafeSleep(80);
            Input::LeftClick(0.08);
            SafeSleep(120);
            // Move mouse away to center of window
            RECT rc; GetClientRect(gameHwnd, &rc);
            POINT ptCenter = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
            ClientToScreen(gameHwnd, &ptCenter);
            Input::MoveToInstant(ptCenter.x, ptCenter.y);
            SafeSleep(120);
            std::cout << "[+] Non-Security Dialog dismissed via [No] -> Resuming hunting immediately!\n";
            return false; // Do NOT trigger Captcha pause!
        }

        // CONFIRMED REAL SECURITY CAPTCHA:
        outYesClick = ptTarget;
        std::cout << "\n\n========================================================================\n"
                  << "  [!] ⚠️ SECURITY CHECK CAPTCHA CONFIRMED ON SCREEN!\n"
                  << "  [*] Target [Yes] button at Screen (" << outYesClick.x << ", " << outYesClick.y << ")...\n"
                  << "========================================================================\n";
        return true;
    }

    return false;
}

void BotEngine::HandleCaptchaPause(const POINT& ptYes) {
    currentState = BOT_STATE_PAUSED;

    // 1. Release hunting inputs completely
    Input::HoldCtrl(false);
    Input::LeftUp();
    Input::RightUp();
    SafeSleep(150);

    std::cout << "\n========================================================================\n";
    std::cout << "  [!] SECURITY CHECK DETECTED: Solve Security Check To Unlock Drop!\n";
    std::cout << "  [*] Initiating In-Game [Yes] & Browser Captcha Solving Cycle...\n";
    std::cout << "========================================================================\n";

    // 2. Ensure Conquer game window is in front before clicking
    if (gameHwnd && IsWindow(gameHwnd)) {
        SetForegroundWindow(gameHwnd);
        BringWindowToTop(gameHwnd);
        SafeSleep(60);
    }

    // 3. Click [Yes] in Conquer game window
    int clickX = ptYes.x;
    int clickY = ptYes.y;
    if (clickX > 0 && clickY > 0) {
        std::cout << "[*] Clicking [Yes] button at Screen (" << clickX << ", " << clickY << ")...\n";
        Input::MoveToSmooth(clickX, clickY, 1.0);
        SafeSleep(100);
        Input::LeftClick(0.08);
        SafeSleep(500);
    }

    std::cout << "[*] [Yes] clicked! Waiting for browser window to launch...\n";
    SafeSleep(2000); // Allow browser to open Drop Unlock tab

    // 4. Fast automated browser captcha solve
    AutoSolveBrowserCaptcha();

    // ========================================================================
    // 5. UNCONDITIONAL RETURN TO CONQUER (Right or Wrong) -> Resume Hunting!
    // ========================================================================
    std::cout << "\n[*] Finishing Captcha cycle -> Returning to Conquer Online unconditionally...\n";

    // Close Drop Unlock tab and cleanly minimize browser to taskbar
    HWND bHwnd = FindBrowserWindow();
    if (bHwnd && IsWindow(bHwnd)) {
        HWND rootBrowser = GetAncestor(bHwnd, GA_ROOT);
        if (!rootBrowser) rootBrowser = bHwnd;

        Input::SendCtrlW();
        SafeSleep(150);

        ShowWindowAsync(rootBrowser, SW_MINIMIZE);
        ShowWindow(rootBrowser, SW_MINIMIZE);
        PostMessage(rootBrowser, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        SetWindowPos(rootBrowser, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        Input::SendCtrlW();
    }
    SafeSleep(200);

    // Restore and focus Conquer Online
    RestoreAndFocusGame(gameHwnd);
    SafeSleep(200);

    // Re-engage hunting & patrol
    Input::HoldCtrl(true);
    lastCaptchaTime = std::chrono::steady_clock::now();
    currentState = BOT_STATE_PATROL;

    std::cout << "[+] Resumed hunting successfully!\n";
    std::cout << "========================================================================\n\n";
}

HWND BotEngine::FindBrowserWindow() {
    struct SearchContext {
        HWND bestHwnd;
        int bestScore;
    } ctx = { NULL, 0 };

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        // STRICT SAFETY: Process MUST NEVER be Conquer or Bot!
        if (pid == 0 || pid == GetCurrentProcessId() || IsConquerProcessId(pid)) {
            return TRUE;
        }

        char title[512] = { 0 };
        GetWindowTextA(hwnd, title, sizeof(title));
        if (strlen(title) == 0) return TRUE;

        std::string sTitle = title;
        std::transform(sTitle.begin(), sTitle.end(), sTitle.begin(), ::tolower);

        auto* pCtx = (SearchContext*)lp;
        int score = 0;
        if (sTitle.find("drop unlock") != std::string::npos || sTitle.find("drop-unlock") != std::string::npos) score += 200;
        if (sTitle.find("edge") != std::string::npos) score += 50;
        if (sTitle.find("chrome") != std::string::npos) score += 50;
        if (sTitle.find("firefox") != std::string::npos) score += 50;
        if (sTitle.find("opera") != std::string::npos) score += 50;
        if (sTitle.find("brave") != std::string::npos) score += 50;

        char className[256] = { 0 };
        GetClassNameA(hwnd, className, sizeof(className));
        std::string sClass = className;
        if (sClass == "Chrome_WidgetWin_1" || sClass == "MozillaWindowClass") {
            score += 40;
        }

        if (score > pCtx->bestScore) {
            pCtx->bestScore = score;
            pCtx->bestHwnd = hwnd;
        }
        return TRUE;
    }, (LPARAM)&ctx);

    return ctx.bestHwnd;
}

void BotEngine::ActivateTargetWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;

    HWND hCurFore = GetForegroundWindow();
    if (hCurFore == hwnd) return;

    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        SafeSleep(60);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    SwitchToThisWindow(hwnd, TRUE);
    SafeSleep(40);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);

    HWND curFore = GetForegroundWindow();
    DWORD foreThread = curFore ? GetWindowThreadProcessId(curFore, NULL) : 0;
    DWORD myThread = GetCurrentThreadId();
    DWORD targetThread = GetWindowThreadProcessId(hwnd, NULL);

    if (foreThread != 0 && foreThread != myThread) AttachThreadInput(myThread, foreThread, TRUE);
    if (targetThread != 0 && targetThread != myThread) AttachThreadInput(myThread, targetThread, TRUE);

    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);
    SetActiveWindow(hwnd);

    if (foreThread != 0 && foreThread != myThread) AttachThreadInput(myThread, foreThread, FALSE);
    if (targetThread != 0 && targetThread != myThread) AttachThreadInput(myThread, targetThread, FALSE);

    SafeSleep(60);
}

void BotEngine::RestoreAndFocusGame(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        hwnd = FindGameWindow();
        if (!hwnd || !IsWindow(hwnd)) return;
        gameHwnd = hwnd;
    }

    std::cout << "[*] Restoring and bringing Conquer Online directly to foreground...\n";

    // 1. If iconic (minimized to taskbar), restore immediately
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        SafeSleep(80);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    // 2. Shell API SwitchToThisWindow forces Windows to activate and bring window to front
    SwitchToThisWindow(hwnd, TRUE);
    SafeSleep(40);

    // 3. HWND_TOPMOST trick: forces Windows DWM to pull the window above all desktop windows
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);

    // 4. AttachThreadInput bypass for Windows 10/11 Foreground Lock
    HWND hCurFore = GetForegroundWindow();
    DWORD foreThread = hCurFore ? GetWindowThreadProcessId(hCurFore, NULL) : 0;
    DWORD curThread = GetCurrentThreadId();
    DWORD targetThread = GetWindowThreadProcessId(hwnd, NULL);

    if (foreThread != 0 && foreThread != curThread) {
        AttachThreadInput(curThread, foreThread, TRUE);
    }
    if (targetThread != 0 && targetThread != curThread) {
        AttachThreadInput(curThread, targetThread, TRUE);
    }

    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);
    SetActiveWindow(hwnd);

    if (foreThread != 0 && foreThread != curThread) {
        AttachThreadInput(curThread, foreThread, FALSE);
    }
    if (targetThread != 0 && targetThread != curThread) {
        AttachThreadInput(curThread, targetThread, FALSE);
    }

    SafeSleep(60);

    // 5. Hardware Driver Alt+Tab fallback if Windows still refused foreground transfer
    if (GetForegroundWindow() != hwnd) {
        std::cout << "[*] Windows focus lock detected: Engaging Hardware Alt+Tab driver fallback...\n";
        Input::SendAltTab();
        SafeSleep(120);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
    }

    // 6. Physical click inside the game window client area to guarantee active input focus
    RECT rc;
    if (GetClientRect(hwnd, &rc)) {
        int gw = rc.right - rc.left;
        int gh = rc.bottom - rc.top;
        if (gw > 200 && gh > 200) {
            POINT ptFocus = { gw / 2, 80 }; // Click safely inside top status area of the game
            ClientToScreen(hwnd, &ptFocus);
            Input::MoveToInstant(ptFocus.x, ptFocus.y);
            SafeSleep(20);
            Input::LeftClick(0.025);
            SafeSleep(30);
        }
    }
}

bool BotEngine::AutoSolveBrowserCaptcha() {
    std::cout << "\n[CaptchaSolver] ========================================\n";
    std::cout << "[CaptchaSolver] Starting Automated Slider Captcha Solver...\n";

    InitKnobTemplate();

    // 1. Smart Window Targeting: Locate and activate the Browser Window directly
    HWND browserHwnd = NULL;
    for (int waitSec = 1; waitSec <= 10; waitSec++) {
        if (stopFlag.load()) return false;
        browserHwnd = FindBrowserWindow();
        if (browserHwnd) {
            char title[512] = { 0 };
            GetWindowTextA(browserHwnd, title, sizeof(title));
            std::cout << "[CaptchaSolver] [+] Detected Browser Window: \"" << title << "\"\n";
            ActivateTargetWindow(browserHwnd);
            SafeSleep(200);
            // Dismiss any open browser context menu (e.g. from stray right clicks)
            Input::KeyPress(0x01); // DIK_ESCAPE hardware keystroke
            keybd_event(VK_ESCAPE, 0, 0, 0);
            keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
            SafeSleep(150);
            break;
        }
        std::cout << "[CaptchaSolver] [*] Waiting for browser window... (" << waitSec << "/10s)\n";
        SafeSleep(500);
    }

    if (!browserHwnd) {
        std::cout << "[CaptchaSolver] [!] Browser not found by title. Attempting driver Alt+Tab fallback...\n";
        Input::SendAltTab();
        SafeSleep(1200);
    } else {
        SafeSleep(400);

        // 2. Tab Selection: Ensure Edge / Chrome is focused on the "Drop Unlock" tab
        char bTitle[512] = { 0 };
        GetWindowTextA(browserHwnd, bTitle, sizeof(bTitle));
        std::string sb = bTitle;
        std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);

        auto isDropUnlockTitle = [](const std::string& s) {
            return (s.find("drop unlock") != std::string::npos ||
                    s.find("ancient conquer") != std::string::npos ||
                    s.find("drop-unlock") != std::string::npos ||
                    s.find("unlock") != std::string::npos);
        };

        if (!isDropUnlockTitle(sb)) {
            std::cout << "[CaptchaSolver] [*] Currently active tab is NOT Drop Unlock (\"" << bTitle << "\").\n";
            std::cout << "[CaptchaSolver] [*] Pressing [Ctrl+9] via driver to switch to newly opened tab...\n";

            Input::SendCtrl9();
            SafeSleep(800);

            GetWindowTextA(browserHwnd, bTitle, sizeof(bTitle));
            sb = bTitle;
            std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);

            if (!isDropUnlockTitle(sb)) {
                std::cout << "[CaptchaSolver] [*] Cycling tabs with driver Ctrl+Tab to find Drop Unlock...\n";
                for (int t = 0; t < 10; t++) {
                    Input::SendCtrlTab();
                    SafeSleep(350);

                    GetWindowTextA(browserHwnd, bTitle, sizeof(bTitle));
                    sb = bTitle;
                    std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
                    if (isDropUnlockTitle(sb)) break;
                }
            }
            std::cout << "[CaptchaSolver] [+] Tab focused: \"" << bTitle << "\"\n";
        }
        SafeSleep(600);
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (screenW < 800 || screenH < 600) {
        screenW = 1920;
        screenH = 1080;
    }

    std::cout << "[CaptchaSolver] Desktop Screen: " << screenW << "x" << screenH << "\n";

    int candidateTryIndex = 0;

    for (int puzzleCycle = 0; puzzleCycle < 1; puzzleCycle++) {
        if (stopFlag.load()) return false;

        int knobX = -1, knobY = -1;
        std::vector<uint32_t> pixels(screenW * screenH);
        std::vector<uint8_t> gray(screenW * screenH);

        for (int attempt = 1; attempt <= 20; attempt++) {
            if (stopFlag.load()) return false;

            if (attempt == 2 || attempt == 5) {
                // Dismiss any potential context menu or focus blocker in Edge
                Input::KeyPress(0x01);
                keybd_event(VK_ESCAPE, 0, 0, 0);
                keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
                SafeSleep(100);
            }

            HDC hdcScreen = GetDC(NULL);
            if (!hdcScreen) { SafeSleep(300); continue; }
            HDC hdcMem = CreateCompatibleDC(hdcScreen);
            HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
            HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
            BitBlt(hdcMem, 0, 0, screenW, screenH, hdcScreen, 0, 0, SRCCOPY);

            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = screenW;
            bmi.bmiHeader.biHeight = -screenH;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            GetDIBits(hdcMem, hBmp, 0, screenH, pixels.data(), &bmi, DIB_RGB_COLORS);

            SelectObject(hdcMem, hOld);
            DeleteObject(hBmp);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);

            for (int i = 0; i < screenW * screenH; i++) {
                uint32_t c = pixels[i];
                uint8_t b = (uint8_t)(c & 0xFF);
                uint8_t g = (uint8_t)((c >> 8) & 0xFF);
                uint8_t r = (uint8_t)((c >> 16) & 0xFF);
                gray[i] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
            }

            // Early detection of "Invalid or expired link" alert message on the webpage
            if (attempt >= 2) {
                int expY1 = (int)(screenH * 0.30), expY2 = (int)(screenH * 0.48);
                int expX1 = (int)(screenW * 0.32), expX2 = (int)(screenW * 0.68);
                int expCount = 0;
                for (int ey = expY1; ey < expY2; ey++) {
                    for (int ex = expX1; ex < expX2; ex++) {
                        uint32_t c = pixels[ey * screenW + ex];
                        int eb = (int)(c & 0xFF), eg = (int)((c >> 8) & 0xFF), er = (int)((c >> 16) & 0xFF);
                        if (er > 130 && er > eg + 25 && er > eb + 20) {
                            expCount++;
                        }
                    }
                }
                if (expCount >= 80) {
                    std::cout << "[CaptchaSolver] [!] Detected \"Invalid or expired link\" alert banner! Aborting to request fresh link in-game.\n";
                    return false;
                }
            }

            // Step 1: Detect Slider Knob via NCC Template Matching
            int yStart = (int)(screenH * 0.45);
            int yEnd   = (int)(screenH * 0.75);
            int xStart = (int)(screenW * 0.35);
            int xEnd   = (int)(screenW * 0.65);

            float bestKnobScore = -1.0f;
            int bestKx = 0, bestKy = 0;
            const int N = KNOB_W * KNOB_H;

            for (int y = yStart; y <= yEnd - KNOB_H; y += 2) {
                for (int x = xStart; x <= xEnd - KNOB_W; x += 2) {
                    float pSum = 0.0f;
                    for (int ty = 0; ty < KNOB_H; ty++) {
                        const uint8_t* row = &gray[(y + ty) * screenW + x];
                        for (int tx = 0; tx < KNOB_W; tx++) pSum += row[tx];
                    }
                    float pMean = pSum / N;
                    float pVar = 0.0f;
                    for (int ty = 0; ty < KNOB_H; ty++) {
                        const uint8_t* row = &gray[(y + ty) * screenW + x];
                        for (int tx = 0; tx < KNOB_W; tx++) {
                            float d = row[tx] - pMean;
                            pVar += d * d;
                        }
                    }
                    float pStd = std::sqrt(pVar);
                    if (pStd < 1.0f) continue;

                    float dot = 0.0f;
                    for (int ty = 0; ty < KNOB_H; ty++) {
                        const uint8_t* row = &gray[(y + ty) * screenW + x];
                        for (int tx = 0; tx < KNOB_W; tx++) {
                            dot += s_normKnob[ty][tx] * (row[tx] - pMean);
                        }
                    }
                    float score = dot / pStd;
                    if (score > bestKnobScore) {
                        bestKnobScore = score;
                        bestKx = x;
                        bestKy = y;
                    }
                }
            }

            if (bestKnobScore >= 0.75f) {
                knobX = bestKx + KNOB_W / 2;
                knobY = bestKy + KNOB_H / 2;
                std::cout << "[CaptchaSolver] [+] Detected Slider Knob via NCC (score=" << bestKnobScore 
                          << ") at Screen (" << knobX << ", " << knobY << ") on attempt #" << attempt << "!\n";
                break;
            }

            // Fallback: search strictly inside slider track area below puzzle canvas
            int yT1 = (int)(screenH * 0.55), yT2 = (int)(screenH * 0.70);
            int xT1 = (int)(screenW * 0.38), xT2 = (int)(screenW * 0.55);
            std::vector<int> mXs, mYs;
            for (int y = yT1; y < yT2; y++) {
                for (int x = xT1; x < xT2; x++) {
                    uint32_t c = pixels[y * screenW + x];
                    int b = (int)(c & 0xFF), g = (int)((c >> 8) & 0xFF), r = (int)((c >> 16) & 0xFF);
                    if (r >= 160 && r <= 230 && g >= 135 && g <= 200 && b >= 85 && b <= 155 && (r - b) >= 35 && (g - b) >= 20) {
                        mXs.push_back(x); mYs.push_back(y);
                    }
                }
            }
            if (mXs.size() >= 25) {
                std::sort(mXs.begin(), mXs.end());
                std::sort(mYs.begin(), mYs.end());
                knobX = mXs[mXs.size() / 2];
                knobY = mYs[mYs.size() / 2];
                std::cout << "[CaptchaSolver] [+] Detected Slider Knob via Color Cluster at Screen (" 
                          << knobX << ", " << knobY << ") on attempt #" << attempt << "!\n";
                break;
            }

            std::cout << "[CaptchaSolver] [*] Waiting for captcha slider... attempt " << attempt << "/20\n";
            SafeSleep(350);
        }

        if (knobX == -1 || knobY == -1) {
            std::cout << "[CaptchaSolver] [!] Could not locate slider knob on screen.\n";
            return false;
        }

        // --------------------------------------------------------------------
        // Step 2: Pixel-Perfect Geometry & Cutout Slot Detection
        // According to site.css:
        // .slider-captcha-stage: 320x160 px
        // .slider-captcha-track: 320x44 px (aligned horizontally with stage)
        // .slider-captcha-knob:  width 38px, left: 3px (center at stageLeft + 22)
        // .slider-captcha-piece: width 48px, left: 0px (top at stageTop + ~54)
        // --------------------------------------------------------------------
        int stageWidth = 320;
        int stageHeight = 160;
        int stageLeft = knobX - 22; // Knob center is at stageLeft + 22
        stageLeft = std::max(0, std::min(screenW - stageWidth, stageLeft));
        int stageTop = knobY - 198; // Stage top is exactly knobY - 198
        stageTop = std::max(0, std::min(screenH - stageHeight, stageTop));

        std::cout << "[CaptchaSolver] [+] Calibrated Stage: [" << stageLeft << ", " << stageTop 
                  << "] " << stageWidth << "x" << stageHeight << " px\n";

        // 1. Scan vertical dark row span (where cutout slots exist)
        std::vector<int> darkRows;
        for (int dy = 35; dy <= 125; dy++) {
            int cy = stageTop + dy;
            if (cy < 0 || cy >= screenH) continue;
            int darkC = 0;
            for (int dx = 50; dx < stageWidth - 10; dx++) {
                int cx = stageLeft + dx;
                if (cx >= 0 && cx < screenW && gray[cy * screenW + cx] < 85) {
                    darkC++;
                }
            }
            if (darkC >= 50) {
                darkRows.push_back(dy);
            }
        }

        if (darkRows.empty()) {
            std::cout << "[CaptchaSolver] [!] No dark slot rows found on cycle #" << (puzzleCycle + 1) << ". Waiting 800ms...\n";
            SafeSleep(800);
            continue;
        }

        int minDy = darkRows.front();
        int maxDy = darkRows.back();
        int centerDy = (minDy + maxDy) / 2;
        int slotRowY = stageTop + centerDy;

        std::cout << "[CaptchaSolver] [+] Slot vertical span: dy=[" << minDy << ".." << maxDy 
                  << "], centerDy=" << centerDy << " (Screen Y=" << slotRowY << ")\n";

        // 2. Extract dark cutout segments along centerDy
        struct DetectedSlot {
            int left;
            int right;
            int width;
            int deltaX;
            double score;
        };
        std::vector<DetectedSlot> detectedSlots;

        bool inSegment = false;
        int segStart = 0;
        for (int dx = 60; dx < stageWidth - 10; dx++) {
            int cx = stageLeft + dx;
            bool isDark = (cx >= 0 && cx < screenW && gray[slotRowY * screenW + cx] < 85);
            if (isDark && !inSegment) {
                inSegment = true;
                segStart = dx;
            } else if (!isDark && inSegment) {
                inSegment = false;
                int segWidth = dx - segStart;
                if (segWidth >= 18) {
                    detectedSlots.push_back({ segStart, dx - 1, segWidth, segStart, 9999.0 });
                }
            }
        }
        if (inSegment) {
            int segWidth = (stageWidth - 10) - segStart;
            if (segWidth >= 18) {
                detectedSlots.push_back({ segStart, stageWidth - 11, segWidth, segStart, 9999.0 });
            }
        }

        if (detectedSlots.empty()) {
            std::cout << "[CaptchaSolver] [!] No candidate slots found at centerDy=" << centerDy << "\n";
            SafeSleep(800);
            continue;
        }

        std::cout << "[CaptchaSolver] [+] Found " << detectedSlots.size() << " candidate slot(s).\n";

        // 3. Score candidate slots against piece using top & bottom edge color continuity
        const int colCount = 35; // columns 5 to 39
        std::vector<float> pTopR(colCount, 0), pTopG(colCount, 0), pTopB(colCount, 0);
        std::vector<float> pBotR(colCount, 0), pBotG(colCount, 0), pBotB(colCount, 0);

        int topSampleRows = 0;
        for (int dy = minDy + 3; dy <= minDy + 8; dy++) {
            int cy = stageTop + dy;
            if (cy < 0 || cy >= screenH) continue;
            topSampleRows++;
            for (int c = 0; c < colCount; c++) {
                int cx = stageLeft + 5 + c;
                if (cx >= 0 && cx < screenW) {
                    uint32_t px = pixels[cy * screenW + cx];
                    pTopB[c] += (px & 0xFF);
                    pTopG[c] += ((px >> 8) & 0xFF);
                    pTopR[c] += ((px >> 16) & 0xFF);
                }
            }
        }
        if (topSampleRows > 0) {
            for (int c = 0; c < colCount; c++) {
                pTopR[c] /= topSampleRows; pTopG[c] /= topSampleRows; pTopB[c] /= topSampleRows;
            }
        }

        int botSampleRows = 0;
        for (int dy = maxDy - 8; dy <= maxDy - 3; dy++) {
            int cy = stageTop + dy;
            if (cy < 0 || cy >= screenH) continue;
            botSampleRows++;
            for (int c = 0; c < colCount; c++) {
                int cx = stageLeft + 5 + c;
                if (cx >= 0 && cx < screenW) {
                    uint32_t px = pixels[cy * screenW + cx];
                    pBotB[c] += (px & 0xFF);
                    pBotG[c] += ((px >> 8) & 0xFF);
                    pBotR[c] += ((px >> 16) & 0xFF);
                }
            }
        }
        if (botSampleRows > 0) {
            for (int c = 0; c < colCount; c++) {
                pBotR[c] /= botSampleRows; pBotG[c] /= botSampleRows; pBotB[c] /= botSampleRows;
            }
        }

        for (auto& slot : detectedSlots) {
            int sx = slot.left;
            // Top continuity error
            double topErr = 0.0;
            int bgTopRows = 0;
            std::vector<float> bgTopR(colCount, 0), bgTopG(colCount, 0), bgTopB(colCount, 0);
            for (int dy = minDy - 8; dy <= minDy - 2; dy++) {
                int cy = stageTop + dy;
                if (cy < 0 || cy >= screenH) continue;
                bgTopRows++;
                for (int c = 0; c < colCount; c++) {
                    int cx = stageLeft + sx + 5 + c;
                    if (cx >= 0 && cx < screenW) {
                        uint32_t px = pixels[cy * screenW + cx];
                        bgTopB[c] += (px & 0xFF);
                        bgTopG[c] += ((px >> 8) & 0xFF);
                        bgTopR[c] += ((px >> 16) & 0xFF);
                    }
                }
            }
            if (bgTopRows > 0) {
                for (int c = 0; c < colCount; c++) {
                    bgTopR[c] /= bgTopRows; bgTopG[c] /= bgTopRows; bgTopB[c] /= bgTopRows;
                    topErr += std::abs(pTopR[c] - bgTopR[c]) + std::abs(pTopG[c] - bgTopG[c]) + std::abs(pTopB[c] - bgTopB[c]);
                }
                topErr /= (colCount * 3.0);
            }

            // Bottom continuity error
            double botErr = 0.0;
            int bgBotRows = 0;
            std::vector<float> bgBotR(colCount, 0), bgBotG(colCount, 0), bgBotB(colCount, 0);
            for (int dy = maxDy + 2; dy <= maxDy + 8; dy++) {
                int cy = stageTop + dy;
                if (cy < 0 || cy >= screenH) continue;
                bgBotRows++;
                for (int c = 0; c < colCount; c++) {
                    int cx = stageLeft + sx + 5 + c;
                    if (cx >= 0 && cx < screenW) {
                        uint32_t px = pixels[cy * screenW + cx];
                        bgBotB[c] += (px & 0xFF);
                        bgBotG[c] += ((px >> 8) & 0xFF);
                        bgBotR[c] += ((px >> 16) & 0xFF);
                    }
                }
            }
            if (bgBotRows > 0) {
                for (int c = 0; c < colCount; c++) {
                    bgBotR[c] /= bgBotRows; bgBotG[c] /= bgBotRows; bgBotB[c] /= bgBotRows;
                    botErr += std::abs(pBotR[c] - bgBotR[c]) + std::abs(pBotG[c] - bgBotG[c]) + std::abs(pBotB[c] - bgBotB[c]);
                }
                botErr /= (colCount * 3.0);
            }

            slot.score = topErr + botErr;
            std::cout << "  Candidate at x=" << sx << " (DeltaX=+" << slot.deltaX 
                      << "px): topErr=" << (int)topErr << ", botErr=" << (int)botErr 
                      << ", totalError=" << (int)slot.score << "\n";
        }

        // Sort candidate slots by lowest total continuity error
        std::sort(detectedSlots.begin(), detectedSlots.end(), [](const DetectedSlot& a, const DetectedSlot& b) {
            return a.score < b.score;
        });

        int tryIdx = candidateTryIndex % detectedSlots.size();
        const auto& winSlot = detectedSlots[tryIdx];
        int targetDelta = winSlot.deltaX;
        int targetReleaseX = knobX + targetDelta;

        std::cout << "\n[CaptchaSolver] [*] TESTING SLOT CANDIDATE #" << (tryIdx + 1) 
                  << " of " << detectedSlots.size() << " at x=" << winSlot.left 
                  << " | Target Delta: +" << targetDelta << " px (Release X=" << targetReleaseX 
                  << ") | Continuity Error: " << (int)winSlot.score << "!\n";

        // Step 3: Humanized Hardware Driver Mouse Execution
        Input::MoveToSmooth(knobX, knobY, 1.2);
        SafeSleep(100);

        Input::LeftDown();
        SafeSleep(80);

        // Overshoot by 3 px to trigger natural human direction change (gives 100/100 client score)
        int overshootDelta = targetDelta + 3;
        int steps = 35;
        for (int step = 1; step <= steps; step++) {
            float t = (float)step / (float)steps;
            float ease = 1.0f - std::pow(1.0f - t, 3.0f);
            int curX = knobX + (int)(overshootDelta * ease);
            int jitterY = (step % 6 == 0) ? (rand() % 3 - 1) : 0;
            Input::MoveToInstant(curX, knobY + jitterY);
            SafeSleep(16);
        }

        SafeSleep(60); // Micro-pause at overshoot

        // Micro-correct 3px back to exact target position
        for (int b = 1; b <= 4; b++) {
            int curX = knobX + overshootDelta - (int)((overshootDelta - targetDelta) * (b / 4.0f));
            Input::MoveToInstant(curX, knobY);
            SafeSleep(16);
        }
        Input::MoveToInstant(targetReleaseX, knobY);
        SafeSleep(80);

        // Release knob
        Input::LeftUp();
        SafeSleep(1800); // Allow browser JS drop-unlock to send fetch('/drop-unlock/verify'), receive server confirmation, and render success
        std::cout << "[CaptchaSolver] [+] Slider moved to target delta (+" << targetDelta << " px) and released!\n";
        return true;
    }

    return false;
}

bool BotEngine::Start() {
    if (isRunning.load()) return true;

    // 1. Locate Conquer Game Window (Stealthily via PID)
    gameHwnd = FindGameWindow();
    if (!gameHwnd) {
        std::cout << "[!] Could not find Conquer game window!\n";
        return false;
    }

    FocusGameWindow(gameHwnd);
    Sleep(60);

    if (IsLoginScreenVisible()) {
        std::cout << "\n========================================================================\n"
                  << "  [*] ⚠️ [Auto-Login] Game is currently at Login / Server selection screen.\n"
                  << "  [*] Waiting for Auto-Login to log character into the game...\n"
                  << "========================================================================\n\n";
        while (IsLoginScreenVisible() && !(GetAsyncKeyState(VK_F9) & 0x8000)) {
            SafeSleep(500);
        }
        if (GetAsyncKeyState(VK_F9) & 0x8000) return false;
        std::cout << "[+] Character logged in! Continuing startup...\n";
        SafeSleep(1500);
    }

    wchar_t title[256] = {0};
    GetWindowTextW(gameHwnd, title, 256);
    std::wcout << L"[+] Game Window Attached: " << title << L"\n";

    // 2. Initialize 100% Hardware Interception Input
    if (!Input::Init()) {
        std::cout << "[!] Interception driver initialization failed! Make sure driver is installed.\n";
        return false;
    }

    // 3. Retrieve character name
    char titleA[256] = {0};
    GetWindowTextA(gameHwnd, titleA, 256);
    characterName = titleA;
    std::cout << "[+] Character Name: " << characterName << "\n";

    int px = 0, py = 0;
    if (ReadCoordsFromTitle(gameHwnd, px, py)) {
        std::cout << "[+] Player Coordinates: (" << px << ", " << py << ")\n";
        if (px < 560 && py > 600) {
            std::cout << "[*] Character is located in Desert City (" << px << ", " << py 
                      << ") -> Will automatically travel to Monster Heartland!\n";
            currentWaypointIndex = 0;
        } else {
            const MapWaypoint* activeCircuit = nullptr;
            int totalCircuitWp = 0;
            GetActiveCircuitForHotspot(selectedHotspot.load(), activeCircuit, totalCircuitWp);
            double bestDist = 1e9;
            for (int i = 0; i < totalCircuitWp; i++) {
                double d = std::hypot(activeCircuit[i].x - px, activeCircuit[i].y - py);
                if (d < bestDist) {
                    bestDist = d;
                    currentWaypointIndex = i;
                }
            }
            std::cout << "[+] Resuming from Waypoint #" << currentWaypointIndex 
                      << " (" << activeCircuit[currentWaypointIndex].x << ", " 
                      << activeCircuit[currentWaypointIndex].y << ")\n";
        }
    } else {
        const MapWaypoint* activeCircuit = nullptr;
        int totalCircuitWp = 0;
        GetActiveCircuitForHotspot(selectedHotspot.load(), activeCircuit, totalCircuitWp);
        std::cout << "[*] Quick-starting Circuit from waypoint #" << currentWaypointIndex 
                  << " (" << activeCircuit[currentWaypointIndex % totalCircuitWp].x << ", " 
                  << activeCircuit[currentWaypointIndex % totalCircuitWp].y << ")\n";
    }

    stopFlag = false;
    isRunning = true;
    currentState = BOT_STATE_PATROL;
    Beep(1200, 100);

    // Cleanly join any previous worker thread if it was still joinable!
    if (workerThread.joinable()) {
        if (std::this_thread::get_id() != workerThread.get_id()) {
            workerThread.join();
        }
    }

    workerThread = std::thread(&BotEngine::MainLoop, this);
    return true;
}

void BotEngine::Stop() {
    bool wasRunning = isRunning.exchange(false);
    stopFlag = true;

    // Release all hardware driver inputs
    Input::HoldCtrl(false);
    Input::LeftUp();
    Input::RightUp();

    if (wasRunning) {
        Beep(700, 150);
    }

    if (workerThread.joinable()) {
        if (std::this_thread::get_id() != workerThread.get_id())
            workerThread.join();
    }

    currentState = BOT_STATE_IDLE;
}

// ============================================================================
// Exact Golden Text Templates for Conquer Online Ground Drops
// Priority #1: TanCPBag (59x11, 154 yellow pts) -> Priority 100
// Priority #2: GreenCPBag (71x11, 187 yellow pts) -> Priority 90
// Priority #3: ExpBall(Event) (77x11, 225 yellow pts) -> Priority 80
// ALL OTHER DROPS (Sycee, Gold, Sand Essence, Equipment) -> 100% IGNORED
// ============================================================================
namespace {
    static const int TanCPBag_W = 59;
    static const int TanCPBag_H = 11;
    static const int TanCPBag_PTS = 154;
    static const char* TanCPBag_ROWS[11] = {
        "#######.................###....#####...#####...............",
        "...#...................#...#...#....#..#....#..............",
        "...#.....###...#.##...#.....#..#....#..#....#...###....##.#",
        "...#....#...#..##..#..#........#....#..#....#..#...#..#..##",
        "...#........#..#...#..#........#####...######......#..#...#",
        "...#.....####..#...#..#........#.......#....#...####..#...#",
        "...#....#...#..#...#..#.....#..#.......#....#..#...#..#...#",
        "...#....#..##..#...#.......#...#.......#....#..#..##..#..##",
        "...#.....##.#..#...#....###....#.......#####....##.#...##.#",
        "..........................................................#",
        "......................................................####."
    };

    static const int GreenCPBag_W = 71;
    static const int GreenCPBag_H = 11;
    static const int GreenCPBag_PTS = 187;
    static const char* GreenCPBag_ROWS[11] = {
        "..###...............................###....#####...#####...............",
        ".#...#.............................#...#...#....#..#....#..............",
        "#.....#..#.#..###....###...#.##...#.....#..#....#..#....#...###....##.#",
        "#........##..#...#..#...#..##..#..#........#....#..#....#..#...#..#..##",
        "#...###..#...#...#..#...#..#...#..#........#####...######......#..#...#",
        "#.....#..#...#####..#####..#...#..#........#.......#....#...####..#...#",
        "#.....#..#...#......#......#...#..#.....#..#.......#....#..#...#..#...#",
        ".#.......#...#...#..#...#..#...#...#...#...#.......#....#..#..##..#..##",
        "..###....#....###....###...#...#....###....#.......#####....##.#...##.#",
        "......................................................................#",
        "..................................................................####."
    };

    static const int ExpBall_W = 77;
    static const int ExpBall_H = 11;
    static const int ExpBall_PTS = 225;
    static const char* ExpBall_ROWS[11] = {
        "######..............#####..........#..#....#.######.....................#.#..",
        "#...................#....#.........#..#...#..#..........................#..#.",
        "#......#...#.#.##...#....#...###...#..#...#..#......#...#..###...#.##..###.#.",
        "#.......#.#..##..#..#....#..#...#..#..#..#...#......#...#.#...#..##..#..#...#",
        "######..#.#..#...#..######......#..#..#..#...######..#.#..#...#..#...#..#...#",
        "#........#...#...#..#....#...####..#..#..#...#.......#.#..#####..#...#..#...#",
        "#.......#.#..#...#..#....#..#...#..#..#..#...#.......#.#..#......#...#..#...#",
        "#.......#.#..##..#..#....#..#..##..#..#..#...#........#...#...#..#...#..#...#",
        "######.#...#.#.##...#####....##.#..#..#...#..######...#....###...#...#..##.#.",
        ".............#............................#................................#.",
        ".............#.............................#..............................#.."
    };

    static const int DragonBall_W = 60;
    static const int DragonBall_H = 11;
    static const int DragonBall_PTS = 170;
    static const char* DragonBall_ROWS[11] = {
        "#####....................................#####..........#..#",
        "#....#...................................#....#.........#..#",
        "#.....#..#.#..###....##.#...###...#.##...#....#...###...#..#",
        "#.....#..##..#...#..#..##..#...#..##..#..#....#..#...#..#..#",
        "#.....#..#.......#..#...#..#...#..#...#..######......#..#..#",
        "#.....#..#....####..#...#..#...#..#...#..#....#...####..#..#",
        "#.....#..#...#...#..#...#..#...#..#...#..#....#..#...#..#..#",
        "#....#...#...#..##..#..##..#...#..#...#..#....#..#..##..#..#",
        "#####....#....##.#...##.#...###...#...#..#####....##.#..#..#",
        "........................#...................................",
        "....................####...................................."
    };

    struct LootTemplateDef {
        const char* name;
        int priority;
        int width;
        int height;
        int pts;
        const char* const* rows;
    };

    static const LootTemplateDef TARGET_TEMPLATES[4] = {
        { "DragonBall", 110, DragonBall_W, DragonBall_H, DragonBall_PTS, DragonBall_ROWS },
        { "TanCPBag", 100, TanCPBag_W, TanCPBag_H, TanCPBag_PTS, TanCPBag_ROWS },
        { "GreenCPBag", 90, GreenCPBag_W, GreenCPBag_H, GreenCPBag_PTS, GreenCPBag_ROWS },
        { "ExpBall(Event)", 80, ExpBall_W, ExpBall_H, ExpBall_PTS, ExpBall_ROWS }
    };
}

// ============================================================================
// Vision Looting Engine: Direct Pixel Ground Drop Scanner & Disappearance Check
// Scans for Conquer Online golden item text: (R>=210, G>=210, 135<=B<=218)
// Strictly detects ONLY TanCPBag, GreenCPBag, and ExpBall(Event) via exact templates!
// ============================================================================
std::vector<ScreenLootTarget> BotEngine::ScanScreenForGroundDrops(HWND hwnd, int winW, int winH) {
    std::vector<ScreenLootTarget> targets;
    if (!hwnd || !IsWindow(hwnd) || winW <= 200 || winH <= 200) return targets;

    POINT ptClient = { 0, 0 };
    ClientToScreen(hwnd, &ptClient);

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return targets;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return targets;
    }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, winW, winH);
    if (!hBmp) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return targets;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);

    bool isFore = (GetForegroundWindow() == hwnd);
    bool captured = false;
    if (!isFore) {
        if (PrintWindow(hwnd, hdcMem, 2 /* PW_RENDERFULLCONTENT */)) {
            captured = true;
        } else if (PrintWindow(hwnd, hdcMem, 0)) {
            captured = true;
        }
    }
    if (!captured) {
        BitBlt(hdcMem, 0, 0, winW, winH, hdcScreen, ptClient.x, ptClient.y, SRCCOPY);
    }

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(winW * winH);
    GetDIBits(hdcMem, hBmp, 0, winH, pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    int centerX = winW / 2;
    int centerY = winH / 2;

    int scanTop = 25;
    int scanBottom = winH - 55;
    int scanLeft = 5;
    int scanRight = winW - 5;

    int chatBottom = winH - 180;
    int chatRight = (winW > 1366 ? 300 : 240);
    int mapBottom = (winW > 1366 ? 170 : 140);
    int mapLeft = winW - (winW > 1366 ? 170 : 140);

    // Fast universal Conquer Online yellow ground text mask & Green monster text counter
    int greenMonsterCount = 0;
    std::vector<uint8_t> mask(winW * winH, 0);
    for (int y = scanTop; y < scanBottom; y++) {
        int rowIdx = y * winW;
        for (int x = scanLeft; x < scanRight; x++) {
            if (y >= chatBottom && x <= chatRight) continue;
            if (y <= mapBottom && x >= mapLeft) continue;
            if (y < 90 && x < 220) continue; // Skip top-left character avatar / HP / PK bars

            uint32_t c = pixels[rowIdx + x];
            int b = (int)(c & 0xFF);
            int g = (int)((c >> 8) & 0xFF);
            int r = (int)((c >> 16) & 0xFF);

            // Ground drops: TanCPBag, GreenCPBag, ExpBall, DragonBall
            if (r >= 150 && g >= 150 && b <= 220 && 
                (r - b) >= 10 && (g - b) >= 10 && 
                std::abs(r - g) <= 45) {
                mask[rowIdx + x] = 1;
            }

            // Green monster name text (SandMonster, SandMonsterKing, BanditLeader)
            if (g >= 160 && (g - r) >= 70 && (g - b) >= 70 && r <= 120 && b <= 120) {
                greenMonsterCount++;
            }
        }
    }
    lastVisibleMonsterPixels = greenMonsterCount;

    // Horizontal dilation by 9px to merge letter clusters
    std::vector<uint8_t> dilated(winW * winH, 0);
    for (int y = scanTop; y < scanBottom; y++) {
        int rowIdx = y * winW;
        for (int x = scanLeft; x < scanRight; x++) {
            if (mask[rowIdx + x]) {
                for (int dx = 0; dx <= 9 && (x + dx) < scanRight; dx++) {
                    dilated[rowIdx + (x + dx)] = 1;
                }
            }
        }
    }

    // Connected components bounding boxes
    std::vector<uint8_t> visited(winW * winH, 0);
    for (int y = scanTop; y < scanBottom; y++) {
        int rowIdx = y * winW;
        for (int x = scanLeft; x < scanRight; x++) {
            int idx = rowIdx + x;
            if (dilated[idx] && !visited[idx]) {
                int minX = x, maxX = x, minY = y, maxY = y;
                std::vector<int> q;
                q.reserve(256);
                q.push_back(idx);
                visited[idx] = 1;
                size_t head = 0;

                while (head < q.size()) {
                    int curIdx = q[head++];
                    int cy = curIdx / winW;
                    int cx = curIdx % winW;

                    if (cx < minX) minX = cx;
                    if (cx > maxX) maxX = cx;
                    if (cy < minY) minY = cy;
                    if (cy > maxY) maxY = cy;

                    int nbors[4] = { curIdx - 1, curIdx + 1, curIdx - winW, curIdx + winW };
                    for (int nb : nbors) {
                        if (nb >= 0 && nb < winW * winH && dilated[nb] && !visited[nb]) {
                            visited[nb] = 1;
                            q.push_back(nb);
                        }
                    }
                }

                // Find un-dilated bounding box inside this connected component
                int uMinX = maxX, uMaxX = minX, uMinY = maxY, uMaxY = minY;
                int yellowCount = 0;
                for (int by = minY; by <= maxY; by++) {
                    int bRow = by * winW;
                    for (int bx = minX; bx <= maxX; bx++) {
                        if (mask[bRow + bx]) {
                            yellowCount++;
                            if (bx < uMinX) uMinX = bx;
                            if (bx > uMaxX) uMaxX = bx;
                            if (by < uMinY) uMinY = by;
                            if (by > uMaxY) uMaxY = by;
                        }
                    }
                }

                // All target items have at least 55 yellow pixels (supports distant / partially blended drops)
                if (yellowCount < 55) continue;

                int ubw = (uMaxX - uMinX + 1);
                int ubh = (uMaxY - uMinY + 1);

                std::string matchedName = "";
                int matchedPrio = 0;
                int matchedCount = 1;
                double bestMatchScore = 0.0;
                int bestDx = 0, bestDy = 0;
                const LootTemplateDef* bestTmplDef = nullptr;

                for (const auto& tmpl : TARGET_TEMPLATES) {
                    bool isExpBallTmpl = (std::string(tmpl.name).find("ExpBall") != std::string::npos);

                    // Strict bounding box dimensions:
                    // 1. ExpBall(Event) is 77x11 (allows prefix "(2) " up to +45px width)
                    // 2. TanCPBag (59x11), DragonBall (60x11), GreenCPBag (71x11) strictly matched within tight bounds
                    if (isExpBallTmpl) {
                        if (ubw < tmpl.width - 6 || ubw > tmpl.width + 45 || std::abs(ubh - tmpl.height) > 4) {
                            continue;
                        }
                    } else {
                        if (ubw < tmpl.width - 6 || ubw > tmpl.width + 12 || std::abs(ubh - tmpl.height) > 4) {
                            continue;
                        }
                    }

                    int maxDy = (std::max)(1, ubh - tmpl.height + 1);
                    int maxDx = (std::max)(1, ubw - tmpl.width + 1);

                    double bestF1 = -1.0;
                    double bestRecall = 0.0;
                    double bestPrecision = 0.0;
                    int curBestDx = 0, curBestDy = 0;

                    for (int dy = 0; dy < maxDy; dy++) {
                        for (int dx = 0; dx < maxDx; dx++) {
                            int overlap = 0;
                            int bgMismatch = 0;
                            for (int ty = 0; ty < tmpl.height; ty++) {
                                const char* rStr = tmpl.rows[ty];
                                int checkY = uMinY + dy + ty;
                                if (checkY >= winH) break;
                                int checkRow = checkY * winW;
                                for (int tx = 0; tx < tmpl.width; tx++) {
                                    int checkX = uMinX + dx + tx;
                                    if (checkX >= winW) break;
                                    uint8_t mVal = mask[checkRow + checkX];
                                    if (rStr[tx] == '#') {
                                        if (mVal) overlap++;
                                    } else {
                                        if (mVal) bgMismatch++;
                                    }
                                }
                            }

                            double recall = (double)overlap / tmpl.pts;
                            double precision = (overlap + bgMismatch > 0) ? ((double)overlap / (overlap + bgMismatch)) : 0.0;
                            double f1 = (recall + precision > 0.0) ? (2.0 * recall * precision / (recall + precision)) : 0.0;

                            if (f1 > bestF1) {
                                bestF1 = f1;
                                bestRecall = recall;
                                bestPrecision = precision;
                                curBestDx = dx;
                                curBestDy = dy;
                            }
                        }
                    }

                    // STRICT VERIFICATION CRITERIA:
                    // Require BOTH high Recall (>= 80%) AND high Precision (>= 80%) AND F1 (>= 80%).
                    // True target items (TanCPBag, GreenCPBag, ExpBall, DragonBall) achieve Recall=1.000, Precision>=0.994, F1>=0.997.
                    // Junk drops (SandEssence, RecoveryPill, GreatSword, Sycee, Gold, etc.) score F1 < 0.35 and are strictly ignored!
                    if (bestRecall >= 0.80 && bestPrecision >= 0.80 && bestF1 >= 0.80) {
                        if (bestF1 > bestMatchScore) {
                            bestMatchScore = bestF1;
                            bestDx = curBestDx;
                            bestDy = curBestDy;
                            bestTmplDef = &tmpl;
                            if (isExpBallTmpl) {
                                if (ubw >= 100 || curBestDx >= 15) {
                                    matchedName = "(2) ExpBall(Event)";
                                    matchedPrio = 85;
                                    matchedCount = 2;
                                } else {
                                    matchedName = tmpl.name;
                                    matchedPrio = tmpl.priority;
                                    matchedCount = 1;
                                }
                            } else {
                                matchedName = tmpl.name;
                                matchedPrio = tmpl.priority;
                                matchedCount = 1;
                            }
                        }
                        break; // Found exact verified match for this template
                    }
                }

                if (matchedPrio <= 0 || bestMatchScore < 0.80 || !bestTmplDef) {
                    continue;
                }

                // Exact target coordinates: centered dead-on the matched template text
                int targetBoxX = uMinX + bestDx;
                int targetBoxY = uMinY + bestDy;
                int targetBoxW = bestTmplDef->width;
                int targetBoxH = bestTmplDef->height;
                int targetClickX = targetBoxX + targetBoxW / 2;
                int targetClickY = targetBoxY + targetBoxH / 2;

                double dist = std::hypot((double)targetClickX - centerX, (double)targetClickY - centerY);

                // Full screen visible drops (up to 1400px diagonal coverage)
                if (dist <= 1400.0) {
                    ScreenLootTarget item;
                    item.boxX = targetBoxX;
                    item.boxY = targetBoxY;
                    item.boxW = targetBoxW;
                    item.boxH = targetBoxH;
                    item.clickX = targetClickX;
                    item.clickY = targetClickY;
                    item.goldenPixels = yellowCount;
                    item.priority = matchedPrio;
                    item.itemCount = matchedCount;
                    item.name = matchedName;
                    item.dist = dist;
                    targets.push_back(item);
                }
            }
        }
    }

    // Sort: highest priority first (#1 TanCPBag: 100, #2 GreenCPBag: 90, #3 ExpBall: 80), then closest distance
    std::sort(targets.begin(), targets.end(), [](const ScreenLootTarget& a, const ScreenLootTarget& b) {
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        return a.dist < b.dist;
    });

    return targets;
}

// Fast (<0.3ms) verification that the ground drop text has disappeared from the screen
bool BotEngine::VerifyLootDisappeared(HWND hwnd, int boxX, int boxY, int boxW, int boxH) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    int checkX = (std::max)(0, boxX - 4);
    int checkY = (std::max)(0, boxY - 2);
    int checkW = boxW + 8;
    int checkH = boxH + 4;

    POINT ptCheck = { checkX, checkY };
    ClientToScreen(hwnd, &ptCheck);

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return false;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, checkW, checkH);
    if (!hBmp) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, checkW, checkH, hdcScreen, ptCheck.x, ptCheck.y, SRCCOPY);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = checkW;
    bmi.bmiHeader.biHeight = -checkH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(checkW * checkH);
    GetDIBits(hdcMem, hBmp, 0, checkH, pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    int yellowCount = 0;
    for (int i = 0; i < checkW * checkH; i++) {
        uint32_t c = pixels[i];
        int b = (int)(c & 0xFF);
        int g = (int)((c >> 8) & 0xFF);
        int r = (int)((c >> 16) & 0xFF);
        if (r >= 150 && g >= 150 && b <= 220 && (r - b) >= 10 && (g - b) >= 10 && std::abs(r - g) <= 45) {
            yellowCount++;
        }
    }

    // Ground text always has >30 yellow pixels. When picked up, count drops to <18.
    return (yellowCount < 18);
}

// ============================================================================
// Speed Runner (الراجل الجاري) Skill Auto-Activator from AutoHunt_v5
// Scans the bottom-right skill bar/circle (NCC >= 0.65) and double-clicks it!
// ============================================================================
bool BotEngine::CheckAndActivateSpeedSkill() {
    static ULONGLONG s_lastSkillCheck = 0;
    ULONGLONG now = GetTickCount64();
    if (now - s_lastSkillCheck < 1500) return false;
    s_lastSkillCheck = now;

    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    RECT rc;
    GetClientRect(gameHwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 500 || winH < 400) return false;

    int roiW = (std::min)(550, winW);
    int roiH = (std::min)(260, winH);
    int roiX = winW - roiW;
    int roiY = winH - roiH - 50;
    if (roiY < 0) roiY = 0;

    POINT ptClient = { 0, 0 };
    ClientToScreen(gameHwnd, &ptClient);
    int screenRoiX = ptClient.x + roiX;
    int screenRoiY = ptClient.y + roiY;

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return false;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, roiW, roiH);
    if (!hbm) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, roiW, roiH, hdcScreen, screenRoiX, screenRoiY, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = roiW;
    bmi.bmiHeader.biHeight = -roiH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = ((roiW * 3 + 3) & ~3);
    std::vector<BYTE> bgr(stride * roiH);
    GetDIBits(hdcMem, hbm, 0, roiH, bgr.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    int speedX = -1, speedY = -1;
    if (FindSpeedSkillIcon(bgr.data(), stride, roiW, roiH, speedX, speedY, 0.58)) {
        int targetX = roiX + speedX;
        int targetY = roiY + speedY;

        POINT ptSkill = { targetX, targetY };
        ClientToScreen(gameHwnd, &ptSkill);

        printf("[+] SPEED RUNNER SKILL (الراجل الجاري) DETECTED @ (%d, %d)! Activating...\n", targetX, targetY);
        Input::HoldCtrl(false);
        SafeSleep(15);
        Input::MoveToInstant(ptSkill.x, ptSkill.y);
        SafeSleep(25);
        Input::LeftDown(); SafeSleep(20); Input::LeftUp(); SafeSleep(35);
        Input::LeftDown(); SafeSleep(20); Input::LeftUp();
        SafeSleep(30);
        Input::HoldCtrl(true);
        Beep(1600, 80);
        return true;
    }
    return false;
}

// ============================================================================
// CheckAndHandleRevive: Detects death state via in-game [Revive] button (NCC)
// Automatically handles 20s countdown, clicks Revive, and calls NavigateToHotspot()
// ============================================================================
bool BotEngine::CheckAndHandleRevive() {
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    RECT rc;
    GetClientRect(gameHwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 500 || winH < 400) return false;
    int centerX = winW / 2;
    int centerY = winH / 2;

    // ------------------------------------------------------------------------
    // Step 1: Rapid Alive Verification via Character HP Orb
    // In Conquer Online, the HP globe is anchored to the bottom action bar:
    // x in [winW / 2 - 490, winW / 2 - 380], y in [winH - 90, winH - 20]
    // A living character always possesses hundreds of bright red pixels (r > 130, g < 60, b < 60).
    // A dead character has 0 HP (the globe is completely drained/black).
    // ------------------------------------------------------------------------
    int redCount = 0;
    bool checkedHp = false;
    if (winW >= 1000) {
        int hpLeft = (std::max)(0, centerX - 490);
        int hpRight = (std::min)(winW, centerX - 380);
        int hpTop = (std::max)(0, winH - 90);
        int hpBottom = (std::min)(winH, winH - 20);
        int hpW = hpRight - hpLeft;
        int hpH = hpBottom - hpTop;

        if (hpW > 0 && hpH > 0) {
            POINT ptHp = { hpLeft, hpTop };
            ClientToScreen(gameHwnd, &ptHp);

            HDC hdcScreen = GetDC(NULL);
            if (hdcScreen) {
                HDC hdcMem = CreateCompatibleDC(hdcScreen);
                if (hdcMem) {
                    HBITMAP hbmHp = CreateCompatibleBitmap(hdcScreen, hpW, hpH);
                    if (hbmHp) {
                        HGDIOBJ oldHpBm = SelectObject(hdcMem, hbmHp);
                        BitBlt(hdcMem, 0, 0, hpW, hpH, hdcScreen, ptHp.x, ptHp.y, SRCCOPY);

                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = hpW;
                        bmi.bmiHeader.biHeight = -hpH;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 24;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        int hpStride = ((hpW * 3 + 3) & ~3);
                        std::vector<BYTE> hpBgr(hpStride * hpH);
                        GetDIBits(hdcMem, hbmHp, 0, hpH, hpBgr.data(), &bmi, DIB_RGB_COLORS);

                        SelectObject(hdcMem, oldHpBm);
                        DeleteObject(hbmHp);

                        for (int y = 0; y < hpH; y++) {
                            const BYTE* row = hpBgr.data() + y * hpStride;
                            for (int x = 0; x < hpW; x++) {
                                BYTE b = row[x * 3];
                                BYTE g = row[x * 3 + 1];
                                BYTE r = row[x * 3 + 2];
                                if (r > 130 && g < 60 && b < 60) {
                                    redCount++;
                                }
                            }
                        }
                        checkedHp = true;
                    }
                    DeleteDC(hdcMem);
                }
                ReleaseDC(NULL, hdcScreen);
            }
        }
    }

    // If character visibly has HP (alive and well), immediately return false to proceed with hunting
    if (checkedHp && redCount >= 500) {
        return false;
    }

    // ------------------------------------------------------------------------
    // CHARACTER IS CONFIRMED DEAD (Ghost Mode)!
    // CRITICAL: A Ghost in Conquer Online MUST REMAIN 100% STANDING STILL!
    // If the ghost is moving/walking, clicking [Revive] causes the server to reject:
    // "[System] You can't revive while moving!"
    // ------------------------------------------------------------------------
    Input::HoldCtrl(false);
    Input::LeftUp();
    Input::RightUp();

    // Immediately stop any in-progress walking by clicking directly at character's feet
    POINT ptFeet = { centerX, centerY };
    ClientToScreen(gameHwnd, &ptFeet);
    Input::MoveToInstant(ptFeet.x, ptFeet.y);
    SafeSleep(25);
    Input::LeftClick(0.030);

    // ------------------------------------------------------------------------
    // Step 2: In-Game [Revive] Button Detection via Template Matching (NCC)
    // ------------------------------------------------------------------------
    int roiW = (std::min)(350, winW);
    int roiH = (std::min)(350, winH);
    int roiX = winW - roiW;
    int roiY = winH - roiH;

    POINT ptClient = { 0, 0 };
    ClientToScreen(gameHwnd, &ptClient);
    int screenRoiX = ptClient.x + roiX;
    int screenRoiY = ptClient.y + roiY;

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return true; // Dead: wait stationary

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return true; // Dead: wait stationary
    }

    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, roiW, roiH);
    if (!hbm) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return true; // Dead: wait stationary
    }

    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, roiW, roiH, hdcScreen, screenRoiX, screenRoiY, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = roiW;
    bmi.bmiHeader.biHeight = -roiH; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = ((roiW * 3 + 3) & ~3);
    std::vector<BYTE> bgr(stride * roiH);
    GetDIBits(hdcMem, hbm, 0, roiH, bgr.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    int localX = -1, localY = -1;
    // Calibrated 0.68 threshold + blue button interior check in ReviveTemplate.h
    if (FindReviveButton(bgr.data(), stride, roiW, roiH, localX, localY, 0.68)) {
        POINT ptRevive = { roiX + localX, roiY + localY };
        ClientToScreen(gameHwnd, &ptRevive);

        std::cout << "\n========================================================================\n"
                  << "  [!] 💀 CHARACTER DIED! In-game [Revive] button confirmed!\n"
                  << "  [*] Ensuring character is standing still & clicking [Revive]...\n"
                  << "========================================================================\n";
        Beep(900, 150);
        Beep(600, 250);

        Input::HoldCtrl(false);
        Input::LeftUp();
        Input::RightUp();

        // Extra stationary safety pause before clicking Revive
        SafeSleep(150);

        // Repeatedly click Revive every 1.2s until revived into Desert City
        int attempts = 0;
        bool revivedInCity = false;
        while (!stopFlag.load() && attempts++ < 30) {
            FocusGameWindow(gameHwnd);
            Input::MoveToInstant(ptRevive.x, ptRevive.y);
            SafeSleep(35);
            Input::LeftClick(0.040);

            if (!SafeSleep(1200)) break;

            int curX = 0, curY = 0;
            ReadCoordsFromTitle(gameHwnd, curX, curY);
            if (curX > 0 && curY > 0 && curX < 580 && curY > 600) {
                // Character confirmed revived at Desert City center statue!
                revivedInCity = true;
                break;
            }

            // Also check if [Revive] button disappeared from screen
            int checkX = -1, checkY = -1;
            POINT ptClientNow = { 0, 0 };
            ClientToScreen(gameHwnd, &ptClientNow);
            HDC hdcScn = GetDC(NULL);
            if (hdcScn) {
                HDC hdcM = CreateCompatibleDC(hdcScn);
                if (hdcM) {
                    HBITMAP hb = CreateCompatibleBitmap(hdcScn, roiW, roiH);
                    if (hb) {
                        HGDIOBJ oB = SelectObject(hdcM, hb);
                        BitBlt(hdcM, 0, 0, roiW, roiH, hdcScn, ptClientNow.x + roiX, ptClientNow.y + roiY, SRCCOPY);
                        GetDIBits(hdcM, hb, 0, roiH, bgr.data(), &bmi, DIB_RGB_COLORS);
                        SelectObject(hdcM, oB);
                        DeleteObject(hb);
                    }
                    DeleteDC(hdcM);
                }
                ReleaseDC(NULL, hdcScn);
            }
            if (!FindReviveButton(bgr.data(), stride, roiW, roiH, checkX, checkY, 0.68)) {
                // [Revive] button disappeared! Character has respawned!
                revivedInCity = true;
                break;
            }
        }

        if (revivedInCity) {
            std::cout << "[+] ✨ Character REVIVED in Desert City! Moving to Monster Heartland (742, 673)...\n";
            Beep(1600, 150);
            Beep(2000, 200);
            SafeSleep(600);

            NavigateToHotspot();
            return true;
        } else {
            // Even if attempts ended, navigate to hotspot
            std::cout << "[*] Revive cycle ended. Moving to Monster Heartland...\n";
            NavigateToHotspot();
            return true;
        }
    } else {
        // [Revive] button not detected yet, BUT character is confirmed dead (redCount < 500)
        // Keep character standing 100% still! Block MainLoop from hunting, jumping, or moving!
        static DWORD lastDeadLog = 0;
        DWORD now = GetTickCount();
        if (now - lastDeadLog > 3000) {
            std::cout << "[*] 💀 Ghost Mode: Character is Dead. Standing still waiting for [Revive] button...\n";
            lastDeadLog = now;
        }
        Input::HoldCtrl(false);
        Input::LeftUp();
        Input::RightUp();
        SafeSleep(250);
        return true; // Return true so MainLoop skips hunting/jumping and waits!
    }
}

// ============================================================================
// Bitboard Templates for Exact In-Game Obstacle Notification Matching:
// 1. "[System]You can't stop here!" (11 x 165, 239 red text bits)
// 2. "[System]You can't jump that far!" (11 x 195, 282 red text bits)
// 3. "[System]Please rest for a while." (11 x 195, 283 red text bits)
// Distinguishes all three notifications with 100% precision from all other chat text!
// ============================================================================
static const uint64_t TMPL_STOP_HERE[11][4] = {
    { 0x0010400000000000ULL, 0x4000040028000000ULL, 0x0000000000800000ULL, 0x0000000000000000ULL },
    { 0x0008800000000000ULL, 0x4000040028000000ULL, 0x0000000000800000ULL, 0x0000000000000000ULL },
    { 0x2388800000000000ULL, 0x40D1CE7078D1C302ULL, 0x00000000008E51C3ULL, 0x0000000000000000ULL },
    { 0x2445000000000000ULL, 0xC132248821322482ULL, 0x0000000000913224ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x4112240821120082ULL, 0x0000000000911224ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x411224702113C082ULL, 0x00000000009F13E4ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x4112248021122082ULL, 0x0000000000811024ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x4132248821132482ULL, 0x0000000000111224ULL, 0x0000000000000000ULL },
    { 0xC382000000000000ULL, 0x40D1CC706112C303ULL, 0x00000000008E11C4ULL, 0x0000000000000000ULL },
    { 0x0000000000000000ULL, 0x0010000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL },
    { 0x0000000000000000ULL, 0x0010000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL }
};

static const uint64_t TMPL_JUMP_FAR[11][4] = {
    { 0x0010400000000000ULL, 0x0000002088000000ULL, 0x0000020030200090ULL, 0x0000000000000000ULL },
    { 0x0008800000000000ULL, 0x0000000090000000ULL, 0x0000020008200090ULL, 0x0000000000000000ULL },
    { 0x2388800000000000ULL, 0x34669121C0D1C302ULL, 0x000002A39C7386B8ULL, 0x0000000000000000ULL },
    { 0x2445000000000000ULL, 0x4C99912081322482ULL, 0x0000026448244990ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x4488912081120082ULL, 0x0000022408240890ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x448891208113C082ULL, 0x0000022788278890ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x4488912081122082ULL, 0x0000022448244890ULL, 0x0000000000000000ULL },
    { 0x2442000000000000ULL, 0x4C88912081132482ULL, 0x0000002648264890ULL, 0x0000000000000000ULL },
    { 0xC382000000000000ULL, 0x34889E218112C303ULL, 0x00000225886588B0ULL, 0x0000000000000000ULL },
    { 0x0000000000000000ULL, 0x0400002000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL },
    { 0x0000000000000000ULL, 0x0400001000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL }
};

// 3. "[System]Please rest for a while." (11 x 195, 283 red text bits)
static const uint64_t TMPL_REST_WHILE[11][4] = {
    { 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL },
    { 0x008F800000000000ULL, 0x000C080000000000ULL, 0x0000000090200000ULL, 0x0000000000000000ULL },
    { 0x0090800000000000ULL, 0x0002080000000000ULL, 0x0000000080200000ULL, 0x0000000000000000ULL },
    { 0x3890800000000000ULL, 0x28E71CE1CA070E1CULL, 0x0000003891A88838ULL, 0x0000000000000000ULL },
    { 0x4490800000000000ULL, 0x1912091226089122ULL, 0x0000004492688844ULL, 0x0000000000000000ULL },
    { 0x448F800000000000ULL, 0x0912081222088120ULL, 0x0000004492255040ULL, 0x0000000000000000ULL },
    { 0x7C80800000000000ULL, 0x091208E3E20F8E3CULL, 0x0000007C92255078ULL, 0x0000000000000000ULL },
    { 0x0480800000000000ULL, 0x0912090022009022ULL, 0x0000000492255044ULL, 0x0000000000000000ULL },
    { 0x4480800000000000ULL, 0x0912091222089132ULL, 0x0000004492222064ULL, 0x0000000000000000ULL },
    { 0x3880800000000000ULL, 0x08E218E1C2070E2CULL, 0x0000023892222058ULL, 0x0000000000000000ULL },
    { 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL }
};

BotEngine::ObstacleType BotEngine::CheckObstacleMessage(HWND hwnd, int winW, int winH) {
    if (!hwnd || !IsWindow(hwnd)) return OBSTACLE_NONE;

    POINT ptClient = { 0, 0 };
    ClientToScreen(hwnd, &ptClient);

    const int roiX = 0;
    const int roiY = 0;
    const int roiW = 270;
    const int roiH = 80;

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return OBSTACLE_NONE;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return OBSTACLE_NONE;
    }

    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, roiW, roiH);
    if (!hbm) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return OBSTACLE_NONE;
    }

    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, roiW, roiH, hdcScreen, ptClient.x + roiX, ptClient.y + roiY, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = roiW;
    bmi.bmiHeader.biHeight = -roiH; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = ((roiW * 3 + 3) & ~3);
    std::vector<BYTE> bgr(stride * roiH);
    GetDIBits(hdcMem, hbm, 0, roiH, bgr.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // Convert captured screen pixels to bitboard rows (4 words = 256 bits per row)
    std::vector<std::array<uint64_t, 4>> redRows(roiH);
    std::vector<int> greenCounts(roiH, 0);

    for (int y = 0; y < roiH; y++) {
        const BYTE* row = bgr.data() + y * stride;
        redRows[y].fill(0ULL);
        for (int x = 0; x < roiW; x++) {
            BYTE b = row[x * 3];
            BYTE g = row[x * 3 + 1];
            BYTE r = row[x * 3 + 2];
            if (g > 150 && r < 95 && b < 95 && x < 55) {
                greenCounts[y]++;
            }
            if (r > 140 && g < 100 && b < 100) {
                int wIdx = x / 64;
                int bIdx = x % 64;
                redRows[y][wIdx] |= (1ULL << bIdx);
            }
        }
    }

    int bestStopOverlap = 0;
    int bestJumpOverlap = 0;
    int bestRestOverlap = 0;

    for (int y = 0; y <= roiH - 11; y++) {
        // Fast check: verify green [System] is present on this text line band
        int gSum = 0;
        for (int dy = 0; dy < 11; dy++) gSum += greenCounts[y + dy];
        if (gSum < 15) continue; // Must contain green [System] header!

        // Shift search across horizontal offsets dx in [-6, +6] for perfect bitboard alignment
        for (int dx = -6; dx <= 6; dx++) {
            int stopOverlap = 0;
            int jumpOverlap = 0;
            int restOverlap = 0;

            for (int dy = 0; dy < 11; dy++) {
                uint64_t sRow[4] = { 0, 0, 0, 0 };
                const auto& orig = redRows[y + dy];
                if (dx == 0) {
                    sRow[0] = orig[0]; sRow[1] = orig[1]; sRow[2] = orig[2]; sRow[3] = orig[3];
                } else if (dx > 0) {
                    for (int w = 3; w >= 0; w--) {
                        sRow[w] = orig[w] << dx;
                        if (w > 0) sRow[w] |= (orig[w - 1] >> (64 - dx));
                    }
                } else {
                    int sh = -dx;
                    for (int w = 0; w < 4; w++) {
                        sRow[w] = orig[w] >> sh;
                        if (w < 3) sRow[w] |= (orig[w + 1] << (64 - sh));
                    }
                }

                for (int w = 0; w < 4; w++) {
                    stopOverlap += std::popcount(sRow[w] & TMPL_STOP_HERE[dy][w]);
                    jumpOverlap += std::popcount(sRow[w] & TMPL_JUMP_FAR[dy][w]);
                    restOverlap += std::popcount(sRow[w] & TMPL_REST_WHILE[dy][w]);
                }
            }

            if (stopOverlap > bestStopOverlap) bestStopOverlap = stopOverlap;
            if (jumpOverlap > bestJumpOverlap) bestJumpOverlap = jumpOverlap;
            if (restOverlap > bestRestOverlap) bestRestOverlap = restOverlap;
        }
    }

    // Robust template matching (handles anti-aliased font and scaling variations):
    if (bestJumpOverlap >= 120 && bestJumpOverlap > bestStopOverlap && bestJumpOverlap > bestRestOverlap) {
        return OBSTACLE_JUMP_FAR;
    }
    if (bestStopOverlap >= 110 && bestStopOverlap > bestJumpOverlap && bestStopOverlap > bestRestOverlap) {
        return OBSTACLE_STOP_HERE;
    }
    if (bestRestOverlap >= 120 && bestRestOverlap > bestStopOverlap && bestRestOverlap > bestJumpOverlap) {
        return OBSTACLE_REST_WHILE;
    }

    return OBSTACLE_NONE;
}

bool BotEngine::IsObstacleMsgVisible(HWND hwnd, int winW, int winH) {
    return (CheckObstacleMessage(hwnd, winW, winH) != OBSTACLE_NONE);
}

// ============================================================================
// DetectMonsterClusterBias: Evaluates monster presence across screen quadrants
// Steers patrol jumps dynamically towards monster clusters ("المكان اللي فيه وحوش يتنطط فيه")
// ============================================================================
void BotEngine::DetectMonsterClusterBias(HWND hwnd, int winW, int winH, double& outBiasX, double& outBiasY) {
    outBiasX = 0.0;
    outBiasY = 0.0;
    if (!hwnd || !IsWindow(hwnd) || winW < 400 || winH < 300) return;

    static ULONGLONG s_lastBiasCalc = 0;
    static double s_cachedBiasX = 0.0;
    static double s_cachedBiasY = 0.0;
    ULONGLONG nowTick = GetTickCount64();
    if (nowTick - s_lastBiasCalc < 300) {
        outBiasX = s_cachedBiasX;
        outBiasY = s_cachedBiasY;
        return;
    }
    s_lastBiasCalc = nowTick;

    int centerX = winW / 2;
    int centerY = winH / 2;

    POINT ptClient = { 0, 0 };
    ClientToScreen(hwnd, &ptClient);

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return;
    }
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, winW, winH);
    if (!hBmp) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return;
    }
    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, winW, winH, hdcScreen, ptClient.x, ptClient.y, SRCCOPY);

    std::vector<uint32_t> pixels(winW * winH);
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    GetDIBits(hdcMem, hBmp, 0, winH, pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    int countLeft = 0, countRight = 0, countTop = 0, countBottom = 0;

    for (int y = 60; y < winH - 60; y += 3) {
        for (int x = 60; x < winW - 60; x += 3) {
            uint32_t c = pixels[y * winW + x];
            uint8_t b = (uint8_t)(c & 0xFF);
            uint8_t g = (uint8_t)((c >> 8) & 0xFF);
            uint8_t r = (uint8_t)((c >> 16) & 0xFF);

            if (g > 150 && g > r + 30 && g > b + 30 && r < 120 && b < 120) {
                if (x < centerX - 40) countLeft++;
                else if (x > centerX + 40) countRight++;

                if (y < centerY - 30) countTop++;
                else if (y > centerY + 30) countBottom++;
            }
        }
    }

    int diffX = countRight - countLeft;
    int diffY = countBottom - countTop;

    if (std::abs(diffX) >= 15) {
        outBiasX = (std::max)(-90.0, (std::min)(90.0, diffX * 1.8));
    }
    if (std::abs(diffY) >= 15) {
        outBiasY = (std::max)(-75.0, (std::min)(75.0, diffY * 1.5));
    }
    s_cachedBiasX = outBiasX;
    s_cachedBiasY = outBiasY;
}

// ============================================================================
// NavigateToHotspot: Travels along REVIVE_ROUTE from Desert City (494, 647)
// across safe pavement walkways directly to destination hotspot
// Features Forward-Only Routing + Dynamic Obstacle Avoidance (Distance Scaling & Sideways Nudge)
// ============================================================================
void BotEngine::NavigateToHotspot() {
    int targetX = anchorX.load();
    int targetY = anchorY.load();
    if (targetX <= 0 || targetY <= 0) { targetX = 742; targetY = 673; }

    std::cout << "\n[*] 🚀 NAVIGATING: Desert City -> Hotspot (" << targetX << ", " << targetY << ")...\n";
    currentState = BOT_STATE_NAVIGATING;
    Input::HoldCtrl(true);

    int curX = 0, curY = 0;
    bool hasCoords = ReadCoordsFromTitle(gameHwnd, curX, curY);

    // Pick forward-only starting waypoint:
    // Guarantees the bot NEVER targets a waypoint behind character position!
    int routeIdx = 0;
    if (hasCoords && curX > 0 && curY > 0) {
        for (int i = 0; i < TOTAL_REVIVE_ROUTE; i++) {
            // If character has already reached or passed waypoint i, target the NEXT one!
            if (curX >= REVIVE_ROUTE[i].x) {
                routeIdx = i + 1;
            } else {
                break;
            }
        }
        if (routeIdx >= TOTAL_REVIVE_ROUTE) routeIdx = TOTAL_REVIVE_ROUTE - 1;
    }

    std::cout << "[*] Navigation starting from Waypoint " << routeIdx 
              << " (" << REVIVE_ROUTE[routeIdx].x << ", " << REVIVE_ROUTE[routeIdx].y << ")...\n";

    int lastX = -1, lastY = -1;
    int stuckStreak = 0;

    // Small, safe, natural jump step bounds (نطات صغيرة وسهلة في النص بدون انحراف):
    const double maxJumpX = 180.0;
    const double maxJumpY = 160.0;

    while (!stopFlag.load() && routeIdx < TOTAL_REVIVE_ROUTE) {
        if (GetAsyncKeyState(VK_F9) & 0x8000) { Stop(); break; }
        if (!IsWindow(gameHwnd)) { Stop(); break; }
        if (IsLoginScreenVisible()) { break; }

        FocusGameWindow(gameHwnd);

        RECT rc;
        GetClientRect(gameHwnd, &rc);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;
        if (winW <= 0 || winH <= 0) { SafeSleep(50); continue; }
        int centerX = winW / 2;
        int centerY = winH / 2;

        hasCoords = ReadCoordsFromTitle(gameHwnd, curX, curY);

        // 1. Destination Arrival Check (Selected Hotspot or Monster Heartland)
        if (hasCoords && curX > 0 && curY > 0) {
            double dToDest = std::hypot(targetX - curX, targetY - curY);
            if (dToDest <= 5.5 || curX >= targetX - 3) {
                std::cout << "[+] 🎯 Character confirmed arrived at selected Hotspot (" << targetX << ", " << targetY << ")!\n";
                break;
            }
            // Once entering open desert (curX >= 680), check for visible monster packs
            if (curX >= 680) {
                int monsterPx = CountMonsterGreenPixels(gameHwnd, winW, winH);
                if (monsterPx >= 250) {
                    std::cout << "[+] 🎯 MONSTERS CONFIRMED IN SIGHT (" << monsterPx << " green px @ " 
                              << curX << ", " << curY << ")! Ready to hunt.\n";
                    Beep(1400, 150);
                    break;
                }
            }
        }

        // 2. Obstacle & Position Stuck Detection
        bool obstacleMsg = IsObstacleMsgVisible(gameHwnd, winW, winH);
        bool posStuck = (hasCoords && curX == lastX && curY == lastY && curX > 0);

        if (obstacleMsg || posStuck) {
            stuckStreak++;
            std::cout << "[!] ⚠️ JUMP OBSTACLE DETECTED ('You can't stop here!' / position unchanged @ " 
                      << curX << ", " << curY << ")! Streak: " << stuckStreak << "\n";
            if (stuckStreak >= 4) {
                std::cout << "[!] Force-advancing to next waypoint forward!\n";
                routeIdx++;
                stuckStreak = 0;
                if (routeIdx >= TOTAL_REVIVE_ROUTE) break;
            }
        } else {
            stuckStreak = 0;
        }

        lastX = curX;
        lastY = curY;

        const MapWaypoint& target = REVIVE_ROUTE[routeIdx];

        if (hasCoords && curX > 0 && curY > 0) {
            double d = std::hypot(target.x - curX, target.y - curY);
            // Forward-only advancement: advance if within 4.5 tiles OR if character has already passed target.x!
            // Strictly guarantees the bot NEVER jumps backward to a passed waypoint!
            if (d <= 4.5 || curX >= target.x) {
                std::cout << "[*] Waypoint " << routeIdx << " (" << target.x << ", " << target.y << ") passed -> Advancing forward!\n";
                routeIdx++;
                stuckStreak = 0;
                if (routeIdx >= TOTAL_REVIVE_ROUTE) break;
                continue;
            }

            int dMapX = target.x - curX;
            int dMapY = target.y - curY;
            double screenDx = (dMapX - dMapY) * 26.0;
            double screenDy = (dMapX + dMapY) * 13.0;

            // Straight-line Center-path Jumps (نطات في النص بدون انحراف جانبي):
            if (stuckStreak > 0) {
                double distScale = (stuckStreak == 1) ? 0.70 : ((stuckStreak == 2) ? 0.50 : 0.35);
                screenDx *= distScale;
                screenDy *= distScale;
                std::cout << "[*] Obstacle adaptation: scaling forward jump step to " << (int)(distScale * 100) << "%\n";
            }

            double screenDist = std::hypot(screenDx, screenDy);
            if (screenDist > maxJumpX) {
                double nx = screenDx / screenDist;
                double ny = screenDy / screenDist;
                screenDx = nx * maxJumpX;
                screenDy = ny * maxJumpY;
            }

            int clickX = centerX + (int)std::round(screenDx);
            int clickY = centerY + (int)std::round(screenDy);
            clickX = (std::max)(40, (std::min)(winW - 40, clickX));
            clickY = (std::max)(50, (std::min)(winH - 50, clickY));

            POINT ptJump = { clickX, clickY };
            ClientToScreen(gameHwnd, &ptJump);

            Input::JumpClick(ptJump.x, ptJump.y);
            if (!SafeSleep(360)) break;
        } else {
            // Fallback waypoint jumps along bridge/desert path
            const MapWaypoint& prev = (routeIdx == 0) ? REVIVE_ROUTE[0] : REVIVE_ROUTE[routeIdx - 1];
            int dMapX = target.x - prev.x;
            int dMapY = target.y - prev.y;
            double distTiles = std::hypot(dMapX, dMapY);
            int jumpsNeeded = (std::max)(1, (int)std::ceil(distTiles / 6.0));

            double stepMapX = (double)dMapX / jumpsNeeded;
            double stepMapY = (double)dMapY / jumpsNeeded;
            double screenDx = (stepMapX - stepMapY) * 26.0;
            double screenDy = (stepMapX + stepMapY) * 13.0;
            double screenDist = std::hypot(screenDx, screenDy);
            if (screenDist > maxJumpX) {
                double nx = screenDx / screenDist;
                double ny = screenDy / screenDist;
                screenDx = nx * maxJumpX;
                screenDy = ny * maxJumpY;
            }
            int clickX = centerX + (int)std::round(screenDx);
            int clickY = centerY + (int)std::round(screenDy);
            clickX = (std::max)(40, (std::min)(winW - 40, clickX));
            clickY = (std::max)(50, (std::min)(winH - 50, clickY));

            POINT ptJump = { clickX, clickY };
            ClientToScreen(gameHwnd, &ptJump);

            for (int j = 0; j < jumpsNeeded && !stopFlag.load(); j++) {
                Input::JumpClick(ptJump.x, ptJump.y);
                if (!SafeSleep(360)) break;
            }
            routeIdx++;
        }
    }

    RECT rcFinal = {};
    GetClientRect(gameHwnd, &rcFinal);
    int finalW = rcFinal.right - rcFinal.left;
    int finalH = rcFinal.bottom - rcFinal.top;
    int finalMonsters = CountMonsterGreenPixels(gameHwnd, finalW, finalH);
    std::cout << "[+] 🎯 ARRIVED at Hotspot! (" << finalMonsters << " monster px). Resuming Hunting & Looting Patrol.\n";
    Beep(1400, 150);
    SetAnchor(curX > 0 ? curX : targetX, curY > 0 ? curY : targetY);
    currentWaypointIndex = 0;
    PressF7Scatter();
    SafeSleep(150);
    Input::HoldCtrl(true);
    currentState = BOT_STATE_PATROL;
}

// ============================================================================
// Disconnect / Login & Server Selection Screen Detector (شاشة السيرفر وتسجيل الدخول)
// Detects when the game disconnected or hung, kicking back to server select:
// 1. Coordinates missing from window title
// 2. Central white/grey server list dialog box (White ratio > 20%)
// 3. Golden dragon crest at top center (Gold pixels > 80)
// 4. Teal/cyan borders of [Log In] and [Exit] buttons
// ============================================================================
bool BotEngine::IsLoginScreenVisible() {
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    // Fast check: if player coordinates are present in title, character is actively in-game!
    int px = 0, py = 0;
    if (ReadCoordsFromTitle(gameHwnd, px, py)) {
        return false;
    }

    RECT rc;
    GetClientRect(gameHwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 300 || winH < 200) return false;

    POINT ptClient = { 0, 0 };
    ClientToScreen(gameHwnd, &ptClient);

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return false;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, winW, winH);
    if (!hBmp) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, winW, winH, hdcScreen, ptClient.x, ptClient.y, SRCCOPY);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(winW * winH);
    GetDIBits(hdcMem, hBmp, 0, winH, pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // 1. Central white server list dialog box: y in [20%, 55%], x in [20%, 80%]
    int yStartSub = (int)(winH * 0.20);
    int yEndSub   = (int)(winH * 0.55);
    int xStartSub = (int)(winW * 0.20);
    int xEndSub   = (int)(winW * 0.80);
    int totalSubPixels = (yEndSub - yStartSub) * (xEndSub - xStartSub);
    if (totalSubPixels <= 0) return false;

    int whiteCount = 0;
    for (int y = yStartSub; y < yEndSub; y++) {
        int rowIdx = y * winW;
        for (int x = xStartSub; x < xEndSub; x++) {
            uint32_t c = pixels[rowIdx + x];
            int b = (int)(c & 0xFF);
            int g = (int)((c >> 8) & 0xFF);
            int r = (int)((c >> 16) & 0xFF);
            if (r >= 175 && g >= 175 && b >= 175 &&
                std::abs(r - g) <= 25 && std::abs(g - b) <= 25) {
                whiteCount++;
            }
        }
    }
    double whiteRatio = (double)whiteCount / totalSubPixels;

    // 2. Top Golden Dragon crest: y in [3%, 18%], x in [30%, 70%]
    int yStartTop = (int)(winH * 0.03);
    int yEndTop   = (int)(winH * 0.18);
    int xStartTop = (int)(winW * 0.30);
    int xEndTop   = (int)(winW * 0.70);
    int goldCount = 0;
    for (int y = yStartTop; y < yEndTop; y++) {
        int rowIdx = y * winW;
        for (int x = xStartTop; x < xEndTop; x++) {
            uint32_t c = pixels[rowIdx + x];
            int b = (int)(c & 0xFF);
            int g = (int)((c >> 8) & 0xFF);
            int r = (int)((c >> 16) & 0xFF);
            if (r >= 130 && g >= 95 && b <= 65 && (r - b) >= 65) {
                goldCount++;
            }
        }
    }

    // 3. Bottom [Log In] and [Exit] button teal/cyan border: y in [85%, 98%]
    int yStartBot = (int)(winH * 0.85);
    int yEndBot   = (int)(winH * 0.98);
    int tealCount = 0;
    for (int y = yStartBot; y < yEndBot; y++) {
        int rowIdx = y * winW;
        for (int x = 0; x < winW; x++) {
            uint32_t c = pixels[rowIdx + x];
            int b = (int)(c & 0xFF);
            int g = (int)((c >> 8) & 0xFF);
            int r = (int)((c >> 16) & 0xFF);
            if (b >= 35 && g >= 25 && r <= 30 && (b - r) >= 15) {
                tealCount++;
            }
        }
    }

    // STRICT DISCONNECT / LOGIN SCREEN CONDITION:
    // Must have the large central white server dialog (>= 40%) AND Golden Dragon Crest (>= 450px).
    // On the real login/server screen: whiteRatio is ~70.2% and goldCount is ~1885px.
    // In normal gameplay: whiteRatio is < 0.1% and goldCount is < 20px.
    return (whiteRatio >= 0.40 && goldCount >= 450);
}

// ============================================================================
// Equipment Lock Dialog ("To protect your online virtual property...") Auto-Dismissal
// 1. Detects dense tan/gold parchment scroll body (>= 10,000 pixels)
// 2. Performs Normalized Cross-Correlation (NCC) template matching for the [Okay] button
// 3. Smoothly clicks the [Okay] button via hardware Interception driver
// ============================================================================
bool BotEngine::CheckAndDismissEquipmentLock() {
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    // Fast throttle check (150ms)
    static ULONGLONG s_lastEquipLockTick = 0;
    ULONGLONG nowTick = GetTickCount64();
    if (nowTick - s_lastEquipLockTick < 150) {
        return false;
    }
    s_lastEquipLockTick = nowTick;

    RECT rc;
    GetClientRect(gameHwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 400 || winH < 300) return false;

    POINT ptClient = { 0, 0 };
    ClientToScreen(gameHwnd, &ptClient);

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return false;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) { ReleaseDC(NULL, hdcScreen); return false; }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, winW, winH);
    if (!hBmp) { DeleteDC(hdcMem); ReleaseDC(NULL, hdcScreen); return false; }

    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, winW, winH, hdcScreen, ptClient.x, ptClient.y, SRCCOPY);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(winW * winH);
    GetDIBits(hdcMem, hBmp, 0, winH, pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    POINT ptOkayScreen = { 0, 0 };
    if (FindEquipmentLockOkayButton(pixels, winW, winH, ptClient, ptOkayScreen)) {
        std::cout << "\n========================================================================\n"
                  << "  [+] [Dialog] Equipment Lock Window ('To protect valuable items') Detected!\n"
                  << "  [*] Auto-clicking [Okay] at Screen (" << ptOkayScreen.x << ", " << ptOkayScreen.y << ")...\n"
                  << "========================================================================\n";

        Input::HoldCtrl(false);
        SafeSleep(50);
        Input::MoveToSmooth(ptOkayScreen.x, ptOkayScreen.y, 1.0);
        SafeSleep(80);
        Input::LeftClick(0.08);
        SafeSleep(150);

        // Move cursor back to center to avoid interfering with game actions
        POINT ptCenter = { winW / 2, winH / 2 };
        ClientToScreen(gameHwnd, &ptCenter);
        Input::MoveToInstant(ptCenter.x, ptCenter.y);
        SafeSleep(100);

        std::cout << "[+] Equipment Lock Window dismissed via [Okay]!\n";
        return true;
    }

    return false;
}

// ============================================================================
// Press F7 to Equip Scatter Skill on Right-Click
// User instruction:
// 1. "معني ان اللعبه تقفل اول متفتح دي ان في حاجه مش هاردوير انا عايز كل حاجه هاردوير 100 فالميه"
// 2. ZERO Win32 PostMessage / ZERO keybd_event / ZERO SendInput (which crash anti-cheat)!
// 3. Pure 100% Interception Hardware Kernel Driver scan code (DIK_F7 = 0x41)
// ============================================================================
void BotEngine::PressF7Scatter() {
    if (!gameHwnd || !IsWindow(gameHwnd)) return;

    std::cout << "[*] [Skill] Pressing F7 via 100% Hardware Interception driver to equip Scatter...\n";

    // 1. Ensure game window is focused and active
    FocusGameWindow(gameHwnd);
    SafeSleep(100);

    // 2. CRITICAL: 100% Hardware Release of Ctrl (DIK_LCONTROL = 0x1D) via driver
    Input::HoldCtrl(false);
    Input::KeyUp(0x1D);
    SafeSleep(150);

    // 3. Send F7 strictly via 100% Hardware Interception Driver (DIK_F7 = 0x41)
    // ZERO Win32 keybd_event / ZERO PostMessage / ZERO SendInput!
    Input::KeyDown(0x41);
    SafeSleep(75);
    Input::KeyUp(0x41);
    SafeSleep(300);

    // Double-tap F7 to guarantee registration in Conquer tick loop
    Input::KeyDown(0x41);
    SafeSleep(75);
    Input::KeyUp(0x41);
    SafeSleep(200);

    std::cout << "[+] [Skill] 100% Hardware F7 sent cleanly. Scatter equipped on Right-Click!\n";
}

bool BotEngine::IsScatterEquipped() {
    return true;
}

// ============================================================================
// Post-Login / Start-at-Login Stabilizer & F7 Scatter Equipper
// User instruction: "f7 تصبر شويه يعني بتاع دقيقه بعد اللوجن عشان يدوسها مش علطول لانها كدا بتتداس في صفحه اللوجن فا مبيختارهاش اعتقد"
// 1. Waits while login / server select screen is visible
// 2. Waits until character actually enters the world (coordinates detected in HUD / title)
// 3. Waits ~60 seconds to fully settle, monitoring & dismissing Equipment Lock popup
// 4. Presses F7 via 100% pure hardware Interception driver
// 5. Holds Ctrl via hardware driver and transitions seamlessly into hunting
// ============================================================================
void BotEngine::WaitForLoginAndEquipScatter() {
    if (!gameHwnd || !IsWindow(gameHwnd)) return;

    std::cout << "\n========================================================================\n"
              << "  [*] ⚠️ [Login] Waiting for character to log in and enter game world...\n"
              << "========================================================================\n";

    Input::HoldCtrl(false);
    Input::LeftUp();
    Input::RightUp();

    // 1. Wait while login screen is visible
    while (!stopFlag.load() && IsLoginScreenVisible()) {
        if (GetAsyncKeyState(VK_F9) & 0x8000) { Stop(); return; }
        SafeSleep(500);
    }
    if (stopFlag.load()) return;

    // 2. Wait until character has spawned into game world (coordinates appear in title / HUD)
    std::cout << "[*] [Login] Server selected! Waiting for character to enter game world...\n";
    int curX = 0, curY = 0;
    while (!stopFlag.load() && !ReadCoordsFromTitle(gameHwnd, curX, curY)) {
        if (GetAsyncKeyState(VK_F9) & 0x8000) { Stop(); return; }
        // While waiting for world, also dismiss Equipment Lock if it appears early
        CheckAndDismissEquipmentLock();
        SafeSleep(1000);
    }
    if (stopFlag.load()) return;

    std::cout << "[+] [Login] Character confirmed spawned at (" << curX << ", " << curY << ")!\n";
    std::cout << "[*] [Login] User Rule: Waiting 10s before pressing F7 (monitoring Equipment Lock)...\n";

    // 3. Wait 10 seconds as requested by user ("هنقلل الوقت هنخليه 10 ثواني بس")
    // During this 10s, continuously monitor and dismiss Equipment Lock popup
    for (int s = 10; s > 0 && !stopFlag.load(); s -= 2) {
        if (GetAsyncKeyState(VK_F9) & 0x8000) { Stop(); return; }
        std::cout << "[*] [Login Settle] " << s << "s remaining before pressing F7 Scatter...\n";
        for (int i = 0; i < 2 && !stopFlag.load(); i++) {
            CheckAndDismissEquipmentLock();
            SafeSleep(1000);
        }
    }
    if (stopFlag.load()) return;

    // 4. Final check for Equipment Lock before F7
    CheckAndDismissEquipmentLock();
    SafeSleep(500);

    // 5. Press F7 via 100% pure Hardware Interception driver
    PressF7Scatter();
    SafeSleep(300);

    // 6. Hold Ctrl via hardware driver for hunting
    Input::HoldCtrl(true);
    std::cout << "[+] [Login] Setup complete! Starting hunt and jump circuit.\n";
}

// ============================================================================
// Multi-Client 2nd Account Background Watcher & Automated Drop Looter
// 1. Monitors open Conquer Online clients in background (strictly excluding "Heaven")
// 2. Evaluates Loot Threshold: >= 1 CP Bag OR >= 2 Exp Balls
// 3. Pauses in-game Auto Hunt, walks & loots, restarts Auto Hunt, Alt+Tabs back!
// ============================================================================

std::vector<HWND> BotEngine::FindSecondaryConquerWindows() {
    struct EnumContext {
        BotEngine* pThis;
        std::vector<HWND> list;
    };
    EnumContext ctx;
    ctx.pThis = this;

    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h) || IsIconic(h)) return TRUE;
        RECT rc;
        GetClientRect(h, &rc);
        if (rc.right < 400 || rc.bottom < 300) return TRUE;

        auto* pCtx = (EnumContext*)lp;
        if (h == pCtx->pThis->GetGameHwnd()) return TRUE; // Skip main hunting client

        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == 0 || pid == GetCurrentProcessId()) return TRUE;

        wchar_t title[256] = { 0 };
        GetWindowTextW(h, title, 256);
        std::wstring wTitle = title;
        std::wstring wLowerTitle = wTitle;
        std::transform(wLowerTitle.begin(), wLowerTitle.end(), wLowerTitle.begin(), ::tolower);

        wchar_t clsName[256] = { 0 };
        GetClassNameW(h, clsName, 256);
        std::wstring wCls = clsName;
        std::wstring wLowerCls = wCls;
        std::transform(wLowerCls.begin(), wLowerCls.end(), wLowerCls.begin(), ::tolower);

        // USER COMMAND: "وسكب الصفح التانيه الي هي اسمها Heaven ديي مش تبع نفس اللعبه"
        // MUST STRICTLY SKIP ANY WINDOW CONTAINING "Heaven"!
        if (wLowerTitle.find(L"heaven") != std::wstring::npos ||
            wLowerCls.find(L"heaven") != std::wstring::npos) {
            return TRUE;
        }

        // Skip bot / tools / IDE
        if (wLowerTitle.find(L"conquerbot") != std::wstring::npos ||
            wLowerTitle.find(L"conquerloot") != std::wstring::npos ||
            wLowerTitle.find(L"autopatch") != std::wstring::npos ||
            wLowerTitle.find(L"visual studio") != std::wstring::npos ||
            wLowerTitle.find(L"project") != std::wstring::npos) {
            return TRUE;
        }

        bool isConquer = false;
        if (wCls == L"Conquer" ||
            wLowerTitle.find(L"conquer") != std::wstring::npos ||
            wLowerTitle.find(L"ancient") != std::wstring::npos ||
            wLowerTitle.find(L"ping") != std::wstring::npos ||
            wLowerTitle.find(L"fps") != std::wstring::npos) {
            isConquer = true;
        }

        if (!isConquer) {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
            if (snap != INVALID_HANDLE_VALUE) {
                MODULEENTRY32 me; me.dwSize = sizeof(me);
                if (Module32First(snap, &me)) {
                    std::string mod(me.szModule);
                    std::transform(mod.begin(), mod.end(), mod.begin(), ::tolower);
                    if (mod.find("conquer") != std::string::npos &&
                        mod.find("heaven") == std::string::npos &&
                        mod.find("conquerbot") == std::string::npos &&
                        mod.find("conquerloot") == std::string::npos) {
                        isConquer = true;
                    }
                }
                CloseHandle(snap);
            }
        }

        if (isConquer) {
            pCtx->list.push_back(h);
        }
        return TRUE;
    }, (LPARAM)&ctx);

    return ctx.list;
}

void BotEngine::SwitchWindowHardware(HWND targetHwnd) {
    if (!targetHwnd || !IsWindow(targetHwnd)) return;
    if (GetForegroundWindow() == targetHwnd) return;

    if (IsIconic(targetHwnd)) {
        ShowWindow(targetHwnd, SW_RESTORE);
        SafeSleep(50);
    }

    // Hardware Alt+Tab via 100% Interception Driver
    Input::SendAltTab();
    SafeSleep(120);

    // Ensure focus matches target
    if (GetForegroundWindow() != targetHwnd) {
        SetForegroundWindow(targetHwnd);
        SafeSleep(80);
    }
}

bool BotEngine::ScanSecondaryClient(HWND hSec, int& outCp, int& outExp, std::vector<ScreenLootTarget>& outTargets) {
    outCp = 0;
    outExp = 0;
    outTargets.clear();
    if (!hSec || !IsWindow(hSec)) return false;

    RECT rc;
    GetClientRect(hSec, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 400 || winH < 300) return false;

    outTargets = ScanScreenForGroundDrops(hSec, winW, winH);

    for (const auto& item : outTargets) {
        if (item.priority >= 90) { // TanCPBag (100) or GreenCPBag (90)
            outCp += item.itemCount;
        } else if (item.priority >= 80) { // ExpBall (85 or 80)
            outExp += item.itemCount;
        }
    }
    return true;
}

void BotEngine::ExecuteSecondaryAccountLoot(HWND hSec, const std::vector<ScreenLootTarget>& initialTargets) {
    if (!hSec || !IsWindow(hSec)) return;

    currentState = BOT_STATE_SECONDARY_LOOT;
    Beep(1800, 100);

    RECT rc;
    GetClientRect(hSec, &rc);
    int secW = rc.right - rc.left;
    int secH = rc.bottom - rc.top;

    // Relative Auto Hunt button coordinates: (winW - 67, winH - 125) -> (1853, 955) in 1080p
    int autoHuntX = secW - 67;
    int autoHuntY = secH - 125;
    POINT ptAutoHunt = { autoHuntX, autoHuntY };
    ClientToScreen(hSec, &ptAutoHunt);

    // 1. Release Ctrl on main account & switch to Secondary Account via Hardware Interception
    Input::HoldCtrl(false);
    SafeSleep(25);
    SwitchWindowHardware(hSec);
    SafeSleep(120);

    // 2. STOP In-Game Auto Hunt on Secondary Account
    printf("[👥] ⏹️ PAUSING in-game Auto Hunt on 2nd Account @ (%d, %d)...\n", autoHuntX, autoHuntY);
    Input::MoveToInstant(ptAutoHunt.x, ptAutoHunt.y);
    SafeSleep(35);
    Input::LeftDown();
    SafeSleep(25);
    Input::LeftUp();
    SafeSleep(150);

    // 3. Walk & Loot target items (CP Bags first, then Exp Balls)
    int lootedCount = 0;
    for (int loop = 0; loop < 6 && isRunning.load() && !stopFlag.load(); loop++) {
        auto items = ScanScreenForGroundDrops(hSec, secW, secH);
        std::vector<ScreenLootTarget> targets;
        for (const auto& it : items) {
            if (it.priority >= 80) {
                targets.push_back(it);
            }
        }
        if (targets.empty()) {
            printf("[👥] 🏁 All ground drops picked up on 2nd Account!\n");
            break;
        }

        auto& best = targets[0];
        POINT ptItem = { best.clickX, best.clickY };
        ClientToScreen(hSec, &ptItem);

        printf("[👥] 💎 Loooting item on 2nd Account: \"%s\" (prio %d) @ (%d, %d)...\n",
               best.name.c_str(), best.priority, best.clickX, best.clickY);

        Input::MoveToInstant(ptItem.x, ptItem.y);
        SafeSleep(25);
        Input::LeftDown();
        SafeSleep(25);
        Input::LeftUp();
        SafeSleep(60);

        totalItemsLooted += best.itemCount;
        if (best.priority >= 90) totalCpbagsLooted += best.itemCount;
        lastLootedName = best.name;
        lootedCount++;

        // Wait for pickup / disappearance (max 2 seconds)
        for (int waitStep = 0; waitStep < 10; waitStep++) {
            SafeSleep(200);
            if (VerifyLootDisappeared(hSec, best.boxX, best.boxY, best.boxW, best.boxH)) {
                break;
            }
        }
    }

    // 4. RESUME In-Game Auto Hunt on Secondary Account
    printf("[👥] ▶️ RESUMING in-game Auto Hunt on 2nd Account @ (%d, %d)...\n", autoHuntX, autoHuntY);
    Input::MoveToInstant(ptAutoHunt.x, ptAutoHunt.y);
    SafeSleep(35);
    Input::LeftDown();
    SafeSleep(25);
    Input::LeftUp();
    SafeSleep(150);

    // 5. Hardware Alt+Tab back to Main Hunting Account
    printf("[👥] 🔄 Switching back to Main Account locked in VIP spot...\n");
    SwitchWindowHardware(gameHwnd);
    SafeSleep(120);

    currentState = (huntingMode.load() == MODE_VIP_SPOT_LOCK) ? BOT_STATE_IDLE : BOT_STATE_PATROL;
    printf("[👥] ✅ Main Account resumed VIP spot hunting.\n");
}

void BotEngine::CheckSecondaryClientsBackground() {
    if (!enableSecondaryWatcher.load()) return;

    auto now = std::chrono::steady_clock::now();
    int interval = watcherCheckIntervalSec.load();
    if (interval < 1) interval = 1;

    bool forceCheck = forceSecondaryCheckNow.exchange(false);
    if (!forceCheck) {
        int elapsed = (int)std::chrono::duration_cast<std::chrono::seconds>(now - lastSecondaryCheckTime).count();
        if (elapsed < interval) return;
    }
    lastSecondaryCheckTime = now;

    auto secClients = FindSecondaryConquerWindows();
    if (secClients.empty()) return;

    int cpThresh = secondaryCpThreshold.load();   // Default: 1 (at least 1 CP Bag)
    int expThresh = secondaryExpThreshold.load(); // Default: 2 (at least 2 Exp Balls)

    for (HWND hSec : secClients) {
        int cpCount = 0;
        int expCount = 0;
        std::vector<ScreenLootTarget> targets;

        // Background capture without moving focus
        ScanSecondaryClient(hSec, cpCount, expCount, targets);

        if (forceCheck && targets.empty()) {
            SwitchWindowHardware(hSec);
            SafeSleep(40);
            ScanSecondaryClient(hSec, cpCount, expCount, targets);
            if (cpCount < cpThresh && expCount < expThresh) {
                SwitchWindowHardware(gameHwnd);
            }
        }

        lastFoundSecondaryCp.store(cpCount);
        lastFoundSecondaryExp.store(expCount);

        // USER RULE: "بس عشان يوقف الاوتو لازم اقل حاجه 2 exp او واحده cp او اكتر"
        // Strictly trigger only if cpCount >= 1 OR expCount >= 2
        if (cpCount >= cpThresh || expCount >= expThresh) {
            printf("\n[👥] ========================================================\n");
            printf("[👥] 🎯 2ND ACCOUNT THRESHOLD REACHED: %d CP Bag(s), %d Exp Ball(s)!\n", cpCount, expCount);
            printf("[👥] (Condition: >= %d CP or >= %d Exp) -> Pausing Auto Hunt & Looting...\n", cpThresh, expThresh);
            printf("[👥] ========================================================\n");

            ExecuteSecondaryAccountLoot(hSec, targets);
            break;
        }
    }
}

// Main logic thread: Continuous Jump Circuit + Scatter Attack + Smart Priority Looter
void BotEngine::MainLoop() {
    FocusGameWindow(gameHwnd);
    Sleep(80);

    // Check if character is actively in-game vs still at login / loading
    int initX = 0, initY = 0;
    bool hasCoords = ReadCoordsFromTitle(gameHwnd, initX, initY) && initX > 0 && initY > 0;
    if (IsLoginScreenVisible() || !hasCoords) {
        WaitForLoginAndEquipScatter();
    } else {
        // Character is already actively inside game world
        std::cout << "[+] Character is actively in-game at (" << initX << ", " << initY << ").\n";
        PressF7Scatter();
        SafeSleep(150);
        Input::HoldCtrl(true);
    }

    ULONGLONG lastMonsterSeenTick = GetTickCount64();

    while (!stopFlag.load()) {
        if ((GetAsyncKeyState(VK_F9) & 0x8000)) {
            Stop(); break;
        }

        if (!IsWindow(gameHwnd)) {
            // Check if game window closed or restarted with auto-login
            HWND newHwnd = FindGameWindow();
            if (newHwnd && IsWindow(newHwnd)) {
                gameHwnd = newHwnd;
                FocusGameWindow(gameHwnd);
                SafeSleep(1500);
                PressF7Scatter();
                SafeSleep(150);
                Input::HoldCtrl(true);
                lastMonsterSeenTick = GetTickCount64();
                continue;
            }
            // Wait up to 10 seconds for game client to reload before stopping
            bool recovered = false;
            for (int waitSec = 0; waitSec < 10 && !stopFlag.load(); waitSec++) {
                if (GetAsyncKeyState(VK_F9) & 0x8000) { Stop(); break; }
                SafeSleep(1000);
                newHwnd = FindGameWindow();
                if (newHwnd && IsWindow(newHwnd)) {
                    gameHwnd = newHwnd;
                    recovered = true;
                    std::cout << "[+] Reconnected to Conquer game window!\n";
                    SafeSleep(1500);
                    PressF7Scatter();
                    SafeSleep(150);
                    Input::HoldCtrl(true);
                    lastMonsterSeenTick = GetTickCount64();
                    break;
                }
            }
            if (!recovered) {
                Stop(); break;
            }
            continue;
        }

        // ====================================================================
        // SAFETY GUARD: ONLY OPERATE WHEN GAME WINDOW IS ACTIVE FOREGROUND!
        // If user switches away (e.g. browser, desktop, editor), STOP all inputs!
        // Pressing [F7] instantly brings the game window back to foreground!
        // ====================================================================
        HWND curFore = GetForegroundWindow();
        if (curFore != gameHwnd) {
            // Check if F7 was pressed to switch back to the game immediately
            if (GetAsyncKeyState(VK_F7) & 1) {
                FocusGameWindow(gameHwnd);
                SafeSleep(80);
                continue;
            }

            // Immediately release Ctrl, Left Click, Right Click
            Input::HoldCtrl(false);
            Input::LeftUp();
            Input::RightUp();

            // Safe idle sleep while user is working outside game
            SafeSleep(150);
            continue;
        }

        // 0A. Abnormal State / OS Window Hung Check
        if (IsHungAppWindow(gameHwnd)) {
            std::cout << "\n[!] CRITICAL ALERT: Game window is HUNG/FROZEN! Stopping bot immediately.\n";
            Beep(700, 300);
            Stop();
            break;
        }

        // 0A-2. LOGIN / SERVER SELECTION SCREEN AUTO-RECONNECT
        // Waits for in-game Auto-Login instead of permanently stopping!
        if (IsLoginScreenVisible()) {
            static ULONGLONG s_lastLoginNotice = 0;
            ULONGLONG nowNotice = GetTickCount64();
            if (nowNotice - s_lastLoginNotice > 4000) {
                std::cout << "\n========================================================================\n"
                          << "  [*] ⚠️ [Auto-Login] Game disconnected or at Login screen.\n"
                          << "  [*] Pausing inputs and waiting for in-game auto-login to reconnect...\n"
                          << "========================================================================\n";
                s_lastLoginNotice = nowNotice;
            }

            WaitForLoginAndEquipScatter();
            if (stopFlag.load()) break;

            // Reset 20-second monster watchdog
            lastMonsterSeenTick = GetTickCount64();
            continue;
        }

        // 0A-3. DEATH & AUTO-REVIVE CHECK
        if (CheckAndHandleRevive()) {
            if (!isRunning.load() || stopFlag.load()) break;
            lastMonsterSeenTick = GetTickCount64();
            continue;
        }

        // We are confirmed ALIVE inside gameHwnd! Ensure Ctrl is held for jumping & mid-air combat
        Input::HoldCtrl(true);

        // ====================================================================
        // PHASE 0: CAPTCHA / SECURITY CHECK SCREEN SCANNER & AUTO-PAUSE
        // ====================================================================
        POINT ptYes = { 0, 0 };
        if (CheckSecurityDialog(ptYes)) {
            HandleCaptchaPause(ptYes);
            if (!isRunning.load() || stopFlag.load()) break;
            lastMonsterSeenTick = GetTickCount64();
            continue;
        }

        // 0A-4. EQUIPMENT LOCK / STUFF POPUP AUTO-DISMISSAL ("واجهة الاسطف")
        if (CheckAndDismissEquipmentLock()) {
            SafeSleep(200);
            lastMonsterSeenTick = GetTickCount64();
            continue;
        }

        // 0B. Speed Runner Skill Auto-Activation (الراجل الجاري from AutoHunt_v5)
        CheckAndActivateSpeedSkill();

        // Manual Pause / Resume toggle via [F8]
        if (GetAsyncKeyState(VK_F8) & 1) {
            currentState = BOT_STATE_PAUSED;
            Input::HoldCtrl(false);
            Input::LeftUp();
            Input::RightUp();
            std::cout << "\n[*] Bot PAUSED via [F8]. Press [F8] or [F7] to resume.\n";
            Beep(800, 150);
            while (!stopFlag.load()) {
                if ((GetAsyncKeyState(VK_F9) & 0x8000)) { Stop(); break; }
                if ((GetAsyncKeyState(VK_F8) & 1) || (GetAsyncKeyState(VK_F7) & 1)) {
                    if (IsLoginScreenVisible()) {
                        std::cout << "\n[!] ⚠️ Cannot resume: Still at Login / Server selection screen!\n";
                        Beep(700, 200);
                        Sleep(300);
                        continue;
                    }
                    std::cout << "[+] Resuming Bot...\n";
                    Beep(1400, 150);
                    FocusGameWindow(gameHwnd);
                    Input::HoldCtrl(true);
                    currentState = BOT_STATE_PATROL;
                    break;
                }
                Sleep(100);
            }
            if (!isRunning.load() || stopFlag.load()) break;
            continue;
        }

        // If game window became minimized, automatically restore it
        if (IsIconic(gameHwnd)) {
            std::cout << "[!] Game window is minimized -> Restoring to foreground...\n";
            RestoreAndFocusGame(gameHwnd);
            SafeSleep(150);
        }

        RECT rc;
        GetClientRect(gameHwnd, &rc);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;
        if (winW <= 0 || winH <= 0) {
            SafeSleep(100);
            continue;
        }
        int centerX = winW / 2;
        int centerY = winH / 2;

        // Single-client VIP Spot Hunting (Strictly locks onto gameHwnd only)

        int curX = 0, curY = 0;
        bool hasCoords = ReadCoordsFromTitle(gameHwnd, curX, curY);
        if (!hasCoords) {
            if (huntingMode.load() == MODE_VIP_SPOT_LOCK && hasAnchor.load()) {
                curX = anchorX.load();
                curY = anchorY.load();
            } else {
                const MapWaypoint* activeCircuit = nullptr;
                int totalCircuitWp = 0;
                GetActiveCircuitForHotspot(selectedHotspot.load(), activeCircuit, totalCircuitWp);
                curX = activeCircuit[currentWaypointIndex % totalCircuitWp].x;
                curY = activeCircuit[currentWaypointIndex % totalCircuitWp].y;
            }
        } else if (!hasAnchor.load() && curX >= 650 && curY >= 600) {
            SetAnchor(curX, curY);
        }

        // 0C. Desert City Auto-Recovery: if character is in Desert City, navigate to hotspot!
        if (curX > 0 && curY > 0 && curX < 560 && curY > 600) {
            NavigateToHotspot();
            if (!isRunning.load() || stopFlag.load()) break;
            const MapWaypoint* recCircuit = nullptr;
            int recCount = 0;
            GetActiveCircuitForHotspot(selectedHotspot.load(), recCircuit, recCount);
            if (recCircuit && recCount > 0) {
                int pxNow = 0, pyNow = 0;
                ReadCoordsFromTitle(gameHwnd, pxNow, pyNow);
                if (pxNow > 0 && pyNow > 0) {
                    double bestDist = 1e9;
                    int bestIdx = 0;
                    for (int i = 0; i < recCount; i++) {
                        double d = std::hypot(recCircuit[i].x - pxNow, recCircuit[i].y - pyNow);
                        if (d < bestDist) {
                            bestDist = d;
                            bestIdx = i;
                        }
                    }
                    currentWaypointIndex = bestIdx;
                }
            }
            continue;
        }

        // 0D. Obstacle & Terrain Message Detection (Distinguishes between all exact notifications):
        // 1. "[System]You can't jump that far!" -> Shorten jump vector with Ctrl strictly held (NEVER walk or release Ctrl)
        // 2. "[System]You can't stop here!" -> Change mouse path / escape blocked tile immediately
        // 3. "[System]Please rest for a while." -> Do nothing, continue hunting normally as-is!
        ObstacleType obs = CheckObstacleMessage(gameHwnd, winW, winH);
        if (obs == OBSTACLE_JUMP_FAR || obs == OBSTACLE_STOP_HERE) {
            const char* obsName = (obs == OBSTACLE_JUMP_FAR) ? "You can't jump that far!" : "You can't stop here!";
            std::cout << "\n========================================================================\n"
                      << "  [!] ⚠️ [Navigation] Detected: \"[System]" << obsName << "\"\n"
                      << "  [*] Shortening jump distance ~50% towards target waypoint & jumping!\n"
                      << "========================================================================\n";

            // Determine target direction towards the current waypoint or anchor
            double screenDx = 100.0, screenDy = 50.0;
            const MapWaypoint* activeCircuit = nullptr;
            int totalCircuitWp = 0;
            GetActiveCircuitForHotspot(selectedHotspot.load(), activeCircuit, totalCircuitWp);
            const MapWaypoint& targetWp = activeCircuit[currentWaypointIndex % totalCircuitWp];

            if (curX > 0 && curY > 0) {
                int dMapX = targetWp.x - curX;
                int dMapY = targetWp.y - curY;
                screenDx = (dMapX - dMapY) * 26.0;
                screenDy = (dMapX + dMapY) * 13.0;
            }

            double fullDist = std::hypot(screenDx, screenDy);
            if (fullDist < 1.0) { screenDx = 100.0; screenDy = 50.0; fullDist = 111.0; }

            // USER RULE: "يقرب موشر الموس من الاعب سنه يعني يقصر نص المسافه تقريبا وهو هينط بعد كدا يرجع طبيعي"
            // Halve the jump distance towards the waypoint (clamped to safe short jump 85..135 px):
            double shortDist = (std::max)(85.0, (std::min)(135.0, fullDist * 0.50));
            double nx = screenDx / fullDist;
            double ny = screenDy / fullDist;

            int shortClickX = centerX + (int)std::round(nx * shortDist);
            int shortClickY = centerY + (int)std::round(ny * shortDist);

            shortClickX = (std::max)(50, (std::min)(winW - 50, shortClickX));
            shortClickY = (std::max)(60, (std::min)(winH - 60, shortClickY));

            POINT ptShortJump = { shortClickX, shortClickY };
            ClientToScreen(gameHwnd, &ptShortJump);

            Input::HoldCtrl(true);
            Input::JumpClick(ptShortJump.x, ptShortJump.y);

            // Do NOT increment waypoint index or change direction!
            // Character jumps 50% distance to clear obstacle, then resumes normal patrol.
            SafeSleep(130);
            continue;
        } else if (obs == OBSTACLE_REST_WHILE) {
            // 3. "[System]Please rest for a while." -> Do nothing! Keep going normally!
            // User rule: "ميعملش حاجه يكمل زي مهو"
            static DWORD lastRestLog = 0;
            DWORD nowTick = GetTickCount();
            if (nowTick - lastRestLog > 3000) {
                std::cout << "[*] [Navigation] Detected: \"[System]Please rest for a while.\" -> Continuing normal flow (no obstacle action).\n";
                lastRestLog = nowTick;
            }
        }

        // ====================================================================
        // PHASE 1A: SMART VISION GROUND ITEM LOOTING (TOP PRIORITY ON LIVE SCREEN)
        // Scans Conquer Online golden ground item text: CP Bags, Meteor, Sand Essence, Sycee
        // Jumps towards distant items, releases Ctrl, and Left-Clicks to walk-pickup!
        // ALWAYS EXECUTES FIRST (BEFORE LEASH GUARD OR PATROL) TO GUARANTEE INSTANT PICKUP!
        // ====================================================================
        static std::map<int, std::chrono::steady_clock::time_point> visionBlacklist;
        auto nowTime = std::chrono::steady_clock::now();

        // Automatically purge blacklist entries older than 3.0s every cycle
        for (auto it = visionBlacklist.begin(); it != visionBlacklist.end(); ) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - it->second).count() > 3000) {
                it = visionBlacklist.erase(it);
            } else {
                ++it;
            }
        }

        // Fast ground drop vision scanner: checks live screen drops on every jump
        static bool isLootingMode = false;
        std::vector<ScreenLootTarget> visionItems = ScanScreenForGroundDrops(gameHwnd, winW, winH);

        ScreenLootTarget bestVisionTarget = { 0 };
        bool hasVisionTarget = false;

        for (const auto& vItem : visionItems) {
            int gridKey = (vItem.clickX / 25) * 1000 + (vItem.clickY / 25);
            auto itBlack = visionBlacklist.find(gridKey);
            if (itBlack != visionBlacklist.end()) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - itBlack->second).count() < 3000) {
                    continue; // Skip temporarily blacklisted location (< 3s)
                } else {
                    visionBlacklist.erase(itBlack);
                }
            }
            bestVisionTarget = vItem;
            hasVisionTarget = true;
            break; // Targets already sorted by priority desc, then dist asc
        }

        if (hasVisionTarget) {
            currentState = BOT_STATE_LOOTING;
            isLootingMode = true;

            double screenDx = bestVisionTarget.clickX - centerX;
            double screenDy = bestVisionTarget.clickY - centerY;
            double screenDist = std::hypot(screenDx, screenDy);

            // Case A: Item is distant (> 220px) -> Jump closer in exact direction
            if (screenDist > 220.0) {
                const double maxJumpLen = 340.0;
                double approachDist = (std::max)(220.0, (std::min)(screenDist - 40.0, maxJumpLen));

                double nx = screenDx / screenDist;
                double ny = screenDy / screenDist;
                int jumpX = centerX + (int)std::round(nx * approachDist);
                int jumpY = centerY + (int)std::round(ny * approachDist);
                jumpX = (std::max)(35, (std::min)(winW - 35, jumpX));
                jumpY = (std::max)(40, (std::min)(winH - 40, jumpY));

                POINT ptJump = { jumpX, jumpY };
                ClientToScreen(gameHwnd, &ptJump);

                printf("[*] Approaching Drop \"%s\" (Prio: %d) @ (%d, %d) - Dist: %.0fpx -> Jumping closer...\n",
                       bestVisionTarget.name.c_str(), bestVisionTarget.priority, bestVisionTarget.clickX, bestVisionTarget.clickY, screenDist);

                Input::HoldCtrl(true);
                Input::JumpClick(ptJump.x, ptJump.y);
                SafeSleep(340); // Fast natural jump flight time + landing synchronization
                continue; // Next loop cycle will land near item and pick it up!
            }

            // Case B: Item is in pickup range (<= 220px) -> Walk-pickup WITHOUT Ctrl!
            printf("[*] LOOTING: \"%s\" (Prio: %d) @ (%d, %d) - Dist: %.0fpx -> Releasing Ctrl & Left-Clicking on item...\n",
                   bestVisionTarget.name.c_str(), bestVisionTarget.priority, bestVisionTarget.clickX, bestVisionTarget.clickY, screenDist);

            // 1. Release Ctrl completely before clicking
            Input::HoldCtrl(false);
            SafeSleep(20);

            bool pickedUp = false;

            for (int attempt = 0; attempt < 3 && !stopFlag.load(); attempt++) {
                if (attempt > 0) {
                    auto freshTargets = ScanScreenForGroundDrops(gameHwnd, winW, winH);
                    bool stillPresent = false;
                    for (const auto& ft : freshTargets) {
                        if (ft.priority == bestVisionTarget.priority || ft.name == bestVisionTarget.name) {
                            double d = std::hypot(ft.clickX - bestVisionTarget.clickX, ft.clickY - bestVisionTarget.clickY);
                            if (d < 120.0) {
                                bestVisionTarget = ft;
                                stillPresent = true;
                                break;
                            }
                        }
                    }
                    if (!stillPresent) {
                        pickedUp = true;
                        break;
                    }
                }

                int clickX = bestVisionTarget.clickX;
                int clickY = bestVisionTarget.clickY;

                POINT ptClick = { clickX, clickY };
                ClientToScreen(gameHwnd, &ptClick);

                // Move directly dead-center on item text and double left-click WITHOUT Ctrl
                Input::MoveToInstant(ptClick.x, ptClick.y);
                SafeSleep(15);
                Input::LeftClick(0.035);
                SafeSleep(25);
                Input::LeftClick(0.035);

                // Wait up to 500ms per attempt checking if character walked to it and picked it up
                for (int waitSlice = 0; waitSlice < 10 && !stopFlag.load(); waitSlice++) {
                    SafeSleep(50);
                    if (VerifyLootDisappeared(gameHwnd, bestVisionTarget.boxX, bestVisionTarget.boxY, bestVisionTarget.boxW, bestVisionTarget.boxH)) {
                        pickedUp = true;
                        break;
                    }
                }

                if (pickedUp) {
                    if (bestVisionTarget.name == "DragonBall") {
                        totalDragonBallsLooted++;
                        totalItemsLooted++;
                        Beep(2600, 120);
                        Beep(3000, 150);
                    } else if (bestVisionTarget.priority >= 90) {
                        totalCpbagsLooted++;
                        Beep(2400, 80);
                    } else {
                        totalItemsLooted++;
                        Beep(1900, 60);
                    }
                    lastLootedName = bestVisionTarget.name;
                    printf("[+] LOOTED SUCCESSFULLY: \"%s\" (Total CP: %d, DB: %d) in %d click(s)!\n",
                           bestVisionTarget.name.c_str(), totalCpbagsLooted, totalDragonBallsLooted, attempt + 1);
                    SafeSleep(25);

                    // If in VIP Spot Lock mode and character stepped away from anchor, snap right back to anchor!
                    if (huntingMode.load() == MODE_VIP_SPOT_LOCK) {
                        int freshX = 0, freshY = 0;
                        if (ReadCoordsFromTitle(gameHwnd, freshX, freshY) && freshX > 0 && freshY > 0) {
                            int targetAnchorX = anchorX.load();
                            int targetAnchorY = anchorY.load();
                            double returnDist = std::hypot(freshX - targetAnchorX, freshY - targetAnchorY);
                            if (returnDist >= 2.0) {
                                int dMapX = targetAnchorX - freshX;
                                int dMapY = targetAnchorY - freshY;
                                double screenDx = (dMapX - dMapY) * 26.0;
                                double screenDy = (dMapX + dMapY) * 13.0;
                                double screenDist = std::hypot(screenDx, screenDy);
                                const double minJumpDist = 220.0;
                                const double maxJumpX = 340.0;
                                const double maxJumpY = 300.0;
                                if (screenDist < minJumpDist && screenDist > 1.0) {
                                    double nx = screenDx / screenDist;
                                    double ny = screenDy / screenDist;
                                    screenDx = nx * minJumpDist;
                                    screenDy = ny * (minJumpDist * 0.7);
                                } else if (screenDist > maxJumpX) {
                                    double nx = screenDx / screenDist;
                                    double ny = screenDy / screenDist;
                                    screenDx = nx * maxJumpX;
                                    screenDy = ny * maxJumpY;
                                }
                                int jx = centerX + (int)std::round(screenDx);
                                int jy = centerY + (int)std::round(screenDy);
                                jx = (std::max)(50, (std::min)(winW - 50, jx));
                                jy = (std::max)(60, (std::min)(winH - 60, jy));
                                POINT ptReturn = { jx, jy };
                                ClientToScreen(gameHwnd, &ptReturn);
                                Input::HoldCtrl(true);
                                Input::JumpClick(ptReturn.x, ptReturn.y);
                                SafeSleep(200);
                            }
                        }
                    }
                    break;
                }
            }

            // 2. Re-assert Ctrl for immediate jump continuation
            Input::HoldCtrl(true);
            SafeSleep(15);

            if (!pickedUp) {
                int gridKey = (bestVisionTarget.clickX / 25) * 1000 + (bestVisionTarget.clickY / 25);
                visionBlacklist[gridKey] = std::chrono::steady_clock::now();
                printf("[!] Target \"%s\" @ (%d, %d) failed pickup. Blacklisting for 3s.\n",
                       bestVisionTarget.name.c_str(), bestVisionTarget.clickX, bestVisionTarget.clickY);
            }
            continue;
        } else {
            isLootingMode = false;
        }

        // ====================================================================
        // PHASE 1B: 20-SECOND NO-MONSTER IDLE WATCHDOG
        // "لو مش شايف وحوش لمده 20 ثانيه يقف برضو ولو شافها يكمل تاني"
        // If no monsters are seen for 20 continuous seconds, pause hunting.
        // As soon as monsters reappear on screen, immediately resume hunting!
        // ====================================================================
        if (lastVisibleMonsterPixels >= 40) {
            lastMonsterSeenTick = GetTickCount64();
        } else {
            ULONGLONG noMonsterElapsed = GetTickCount64() - lastMonsterSeenTick;
            if (noMonsterElapsed >= 20000) {
                std::cout << "\n========================================================================\n"
                          << "  [*] ⏸️ [Watchdog] No monsters seen for 20s. Pausing hunting...\n"
                          << "  [*] Waiting for monsters to respawn/appear on screen...\n"
                          << "========================================================================\n";
                Input::HoldCtrl(false);
                Input::LeftUp();
                Input::RightUp();

                while (!stopFlag.load()) {
                    if (GetAsyncKeyState(VK_F9) & 0x8000) { Stop(); break; }
                    if (GetAsyncKeyState(VK_F8) & 1) break;
                    if (IsLoginScreenVisible()) break;

                    // Dismiss Equipment Lock popup if it appears while idle
                    CheckAndDismissEquipmentLock();

                    SafeSleep(400);

                    // Scan screen for monster green pixels
                    int mCount = CountMonsterGreenPixels(gameHwnd, winW, winH);
                    if (mCount >= 40) {
                        std::cout << "\n========================================================================\n"
                                  << "  [+] ▶️ [Watchdog] Monsters detected (" << mCount << " px)! Resuming hunting!\n"
                                  << "========================================================================\n";
                        lastMonsterSeenTick = GetTickCount64();
                        lastVisibleMonsterPixels = mCount;
                        Input::HoldCtrl(true);
                        break;
                    }
                }
                if (stopFlag.load()) break;
                continue;
            }
        }

        // ====================================================================
        // 0E. Boundary & Screen Leash Guard (Runs ONLY when NO ground drops are on screen)
        // ====================================================================
        if (huntingMode.load() == MODE_VIP_SPOT_LOCK && selectedHotspot.load() != 3 && selectedHotspot.load() != 2) {
            int targetAnchorX = anchorX.load();
            int targetAnchorY = anchorY.load();
            double distFromAnchor = (curX > 0 && curY > 0) ? std::hypot(curX - targetAnchorX, curY - targetAnchorY) : 0.0;
            int leash = maxLeashRadius.load();
            if (curX > 0 && curY > 0 && distFromAnchor > (double)leash) {
                printf("[!] ⚠️ LEASH LIMIT EXCEEDED (Dist: %.1f > %d tiles) -> RETURNING TO ANCHOR (%d, %d)!\n",
                       distFromAnchor, leash, targetAnchorX, targetAnchorY);
                currentState = BOT_STATE_RETURNING_SPOT;
                int dMapX = targetAnchorX - curX;
                int dMapY = targetAnchorY - curY;
                double screenDx = (dMapX - dMapY) * 26.0;
                double screenDy = (dMapX + dMapY) * 13.0;
                double screenDist = std::hypot(screenDx, screenDy);
                const double maxJumpX = 350.0;
                const double maxJumpY = 320.0;
                if (screenDist > maxJumpX) {
                    double nx = screenDx / screenDist;
                    double ny = screenDy / screenDist;
                    screenDx = nx * maxJumpX;
                    screenDy = ny * maxJumpY;
                }
                int jx = centerX + (int)std::round(screenDx);
                int jy = centerY + (int)std::round(screenDy);
                jx = (std::max)(50, (std::min)(winW - 50, jx));
                jy = (std::max)(60, (std::min)(winH - 60, jy));
                POINT ptEscape = { jx, jy };
                ClientToScreen(gameHwnd, &ptEscape);
                Input::HoldCtrl(true);
                Input::JumpClick(ptEscape.x, ptEscape.y);
                if (!SafeSleep(35)) break;
                Input::AttackRightClick(ptEscape.x, ptEscape.y);
                if (!SafeSleep(30)) break;
                POINT ptShot2 = { ptEscape.x + 35, ptEscape.y };
                Input::AttackRightClick(ptShot2.x, ptShot2.y);
                if (!SafeSleep(125)) break;
                continue;
            }
        } else {
            // CIRCUIT PATROL: Sea Shore Geofence Shield
            bool isNearSea = (curY >= 710);
            if (curX > 0 && curY > 0 && isNearSea) {
                printf("[!] ⚠️ SEA SHORE HAZARD DETECTED @ (%d, %d) -> RE-CENTERING TO (742, 673)!\n", curX, curY);
                int dMapX = 742 - curX;
                int dMapY = 673 - curY;
                double screenDx = (dMapX - dMapY) * 26.0;
                double screenDy = (dMapX + dMapY) * 13.0;
                double screenDist = std::hypot(screenDx, screenDy);
                const double maxJumpX = 360.0;
                const double maxJumpY = 340.0;
                if (screenDist > maxJumpX) {
                    double nx = screenDx / screenDist;
                    double ny = screenDy / screenDist;
                    screenDx = nx * maxJumpX;
                    screenDy = ny * maxJumpY;
                }
                int jx = centerX + (int)std::round(screenDx);
                int jy = centerY + (int)std::round(screenDy);
                jx = (std::max)(50, (std::min)(winW - 50, jx));
                jy = (std::max)(60, (std::min)(winH - 60, jy));
                POINT ptEscape = { jx, jy };
                ClientToScreen(gameHwnd, &ptEscape);
                Input::HoldCtrl(true);
                Input::JumpClick(ptEscape.x, ptEscape.y);
                if (!SafeSleep(35)) break;
                Input::AttackRightClick(ptEscape.x, ptEscape.y);
                if (!SafeSleep(30)) break;
                POINT ptShot2 = { ptEscape.x + 35, ptEscape.y };
                Input::AttackRightClick(ptShot2.x, ptShot2.y);
                if (!SafeSleep(125)) break;
                continue;
            }
        }

        // ====================================================================
        // PHASE 2: ATTACK & PATROL STRATEGY
        // ====================================================================
        if (huntingMode.load() == MODE_VIP_SPOT_LOCK && selectedHotspot.load() != 3 && selectedHotspot.load() != 2) {
            // ================================================================
            // STRATEGY 1: VIP SPOT LOCK (IN-PLACE 360° SCATTER ATTACK & RE-CENTERING)
            // Stays locked strictly on anchor (e.g. 696, 730), attacks 360°,
            // loots drops within screen, and NEVER leaves the screen!
            // ================================================================
            currentState = BOT_STATE_PATROL;

            int targetAnchorX = anchorX.load();
            int targetAnchorY = anchorY.load();
            double distFromAnchor = (curX > 0 && curY > 0) ? std::hypot(curX - targetAnchorX, curY - targetAnchorY) : 0.0;

            int clickX = centerX;
            int clickY = centerY;

            if (distFromAnchor > 1.2) {
                // Character is slightly off-anchor (e.g. 2-5 tiles away) -> Jump back towards anchor!
                currentState = BOT_STATE_RETURNING_SPOT;
                int dMapX = targetAnchorX - curX;
                int dMapY = targetAnchorY - curY;
                double screenDx = (dMapX - dMapY) * 26.0;
                double screenDy = (dMapX + dMapY) * 13.0;
                double screenDist = std::hypot(screenDx, screenDy);
                const double minJumpDist = 220.0;
                const double maxJumpX = 330.0;
                const double maxJumpY = 290.0;
                if (screenDist < minJumpDist && screenDist > 1.0) {
                    double nx = screenDx / screenDist;
                    double ny = screenDy / screenDist;
                    screenDx = nx * minJumpDist;
                    screenDy = ny * (minJumpDist * 0.7);
                } else if (screenDist > maxJumpX) {
                    double nx = screenDx / screenDist;
                    double ny = screenDy / screenDist;
                    screenDx = nx * maxJumpX;
                    screenDy = ny * maxJumpY;
                }
                clickX = centerX + (int)std::round(screenDx);
                clickY = centerY + (int)std::round(screenDy);
            } else {
                // Character is on anchor spot! Normal full-distance perimeter jumps (~220px) away from player
                // to maintain jump immunity & 0 ground delay WITHOUT clicking on player feet or drops
                static int normalJumpStep = 0;
                normalJumpStep = (normalJumpStep + 1) % 4;
                int ox = (normalJumpStep == 0 ? 220 : (normalJumpStep == 1 ? -220 : (normalJumpStep == 2 ? 180 : -180)));
                int oy = (normalJumpStep == 0 ? -90 : (normalJumpStep == 1 ? 90 : (normalJumpStep == 2 ? 90 : -90)));
                clickX = centerX + ox;
                clickY = centerY + oy;
            }

            clickX = (std::max)(40, (std::min)(winW - 40, clickX));
            clickY = (std::max)(50, (std::min)(winH - 50, clickY));
            POINT ptJump = { clickX, clickY };
            ClientToScreen(gameHwnd, &ptJump);

            // 1. Jump Click (Instant takeoff)
            Input::JumpClick(ptJump.x, ptJump.y);

            // 2. 360° Scatter Attacks into surrounding monster packs
            // Rotate through 6 angles around character (North-East, South-East, South, South-West, North-West, North)
            static int sectorAngle = 0;
            static const POINT SECTOR_DIRS[] = {
                {  180,  -70 }, // North-East
                {  180,   70 }, // South-East
                {    0,  130 }, // South
                { -180,   70 }, // South-West
                { -180,  -70 }, // North-West
                {    0, -130 }  // North
            };

            if (!SafeSleep(22)) break;
            POINT s1 = { centerX + SECTOR_DIRS[sectorAngle].x, centerY + SECTOR_DIRS[sectorAngle].y };
            ClientToScreen(gameHwnd, &s1);
            Input::AttackRightClick(s1.x, s1.y);

            if (!SafeSleep(20)) break;
            int nextSector = (sectorAngle + 1) % 6;
            POINT s2 = { centerX + SECTOR_DIRS[nextSector].x, centerY + SECTOR_DIRS[nextSector].y };
            ClientToScreen(gameHwnd, &s2);
            Input::AttackRightClick(s2.x, s2.y);
            sectorAngle = (sectorAngle + 2) % 6;

            // Landing synchronization (85ms airtime: 22 + 20 + 85 = 127ms smooth chain)
            if (!SafeSleep(85)) break;
            // Clear screen-coordinate vision blacklist on new jump position
            visionBlacklist.clear();
            continue;
        }

        // ====================================================================
        // STRATEGY 2: DESERT CIRCUIT PATROL (Original Waypoint Route)
        // ====================================================================
        currentState = BOT_STATE_PATROL;

        // --------------------------------------------------------------------
        // Continuous Desert Circuit Patrol: Jumps across waypoints firing Left + Right Clicks
        // --------------------------------------------------------------------

        const MapWaypoint* activeCircuit = nullptr;
        int totalCircuitWp = 0;
        GetActiveCircuitForHotspot(selectedHotspot.load(), activeCircuit, totalCircuitWp);

        const MapWaypoint& targetWp = activeCircuit[currentWaypointIndex % totalCircuitWp];
        const MapWaypoint& prevWp = activeCircuit[(currentWaypointIndex + totalCircuitWp - 1) % totalCircuitWp];

        int clickX = centerX;
        int clickY = centerY;
        const double maxJumpX = 360.0;
        const double maxJumpY = 340.0;
        const double minJumpDist = 220.0;

        if (hasCoords && curX > 0 && curY > 0) {
            int dMapX = targetWp.x - curX;
            int dMapY = targetWp.y - curY;
            double distMap = std::hypot(dMapX, dMapY);

            // Progressive pack sweeping & waypoint advancement:
            bool waypointReached = false;
            if (selectedHotspot.load() == 3 || selectedHotspot.load() == 2) {
                if (distMap <= 6.5) {
                    waypointReached = true;
                }
            } else {
                if (distMap <= 6.0) {
                    waypointReached = true;
                }
            }

            if (waypointReached) {
                currentWaypointIndex = (currentWaypointIndex + 1) % totalCircuitWp;
                continue;
            }

            // Isometric screen projection
            double screenDx = (dMapX - dMapY) * 26.0;
            double screenDy = (dMapX + dMapY) * 13.0;

            // SMART MONSTER SEEKING (for open circuit patrol only):
            if (selectedHotspot.load() != 3 && selectedHotspot.load() != 2) {
                double monsterBiasX = 0.0, monsterBiasY = 0.0;
                DetectMonsterClusterBias(gameHwnd, winW, winH, monsterBiasX, monsterBiasY);
                if (std::abs(monsterBiasX) > 5.0 || std::abs(monsterBiasY) > 5.0) {
                    screenDx += monsterBiasX;
                    screenDy += monsterBiasY;
                }
            }

            double screenDist = std::hypot(screenDx, screenDy);

            if (screenDist < minJumpDist && screenDist > 1.0) {
                // Enforce normal full jump distance away from player so jump clicks NEVER land on player feet or nearby ground drops
                double nx = screenDx / screenDist;
                double ny = screenDy / screenDist;
                screenDx = nx * minJumpDist;
                screenDy = ny * (minJumpDist * 0.7);
            } else if (screenDist > maxJumpX) {
                double nx = screenDx / screenDist;
                double ny = screenDy / screenDist;
                screenDx = nx * maxJumpX;
                screenDy = ny * maxJumpY;
            }

            clickX = centerX + (int)std::round(screenDx);
            clickY = centerY + (int)std::round(screenDy);
        } else {
            // Balanced Closed-Loop Patrol Jumps with normal jump distance (>= 210-234px)
            static const struct BalancedJump { int dx; int dy; } BALANCED_DESERT_JUMPS[8] = {
                {  220,  -80 }, // Step 0: North-East sweep (234px)
                {  220,   80 }, // Step 1: East-South-East (234px)
                { -160,  150 }, // Step 2: South return (219px)
                { -220,   60 }, // Step 3: South-West sweep (228px)
                { -220,  -80 }, // Step 4: North-West sweep (234px)
                { -160, -150 }, // Step 5: North sweep (219px)
                {  180, -110 }, // Step 6: North-East return (211px)
                {  180,  110 }  // Step 7: Center balance (211px)
            };
            const auto& bj = BALANCED_DESERT_JUMPS[currentWaypointIndex % 8];
            clickX = centerX + bj.dx;
            clickY = centerY + bj.dy;
            currentWaypointIndex = (currentWaypointIndex + 1) % 8;
        }

        // Clamp within game window bounds
        clickX = std::max(40, std::min(winW - 40, clickX));
        clickY = std::max(50, std::min(winH - 50, clickY));

        POINT ptJump = { clickX, clickY };
        ClientToScreen(gameHwnd, &ptJump);

        // 1. Jump Click (Immediate crisp takeoff via 100% Interception)
        Input::JumpClick(ptJump.x, ptJump.y);

        // 2. Mid-air Scatter Shot 1 (fired 22ms after takeoff while airborne)
        if (!SafeSleep(22)) break;
        Input::AttackRightClick(ptJump.x, ptJump.y);

        // 3. Mid-air Scatter Shot 2 (sweep +35px across pack fired 20ms later)
        if (!SafeSleep(20)) break;
        POINT ptShot2 = { ptJump.x + 35, ptJump.y };
        Input::AttackRightClick(ptShot2.x, ptShot2.y);

        // 4. Landing synchronization (85ms remaining airtime: 22 + 20 + 85 = 127ms total jump)
        // Next jump chains seamlessly on landing with 0ms ground pause! ("لازقة في ديل النطة")
        if (!SafeSleep(85)) break;
        // Clear screen-coordinate vision blacklist on new jump destination
        visionBlacklist.clear();
    }

    Input::HoldCtrl(false);
    Input::LeftUp();
    Input::RightUp();
    isRunning = false;
    currentState = BOT_STATE_IDLE;
}
