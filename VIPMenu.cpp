#include "VIPMenu.h"
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")

// Control IDs
#define IDC_BTN_LOCK_SPOT       1001
#define IDC_BTN_START           1002
#define IDC_BTN_PAUSE           1003
#define IDC_BTN_STOP            1004
#define IDC_RADIO_VIP_SPOT      1005
#define IDC_RADIO_PATROL        1006
#define IDC_EDIT_MANUAL_X       1007
#define IDC_EDIT_MANUAL_Y       1008
#define IDC_BTN_APPLY_COORDS    1009
#define IDC_EDIT_LEASH          1010
#define IDC_CHK_TOPMOST         1011
#define IDC_BTN_LOCK_TEAMMATE   1012
#define IDC_EDIT_TEAM_X         1013
#define IDC_EDIT_TEAM_Y         1014
#define IDC_BTN_APPLY_TEAM      1015
#define IDC_CHK_TEAM_SWEEP      1016
#define IDC_EDIT_SWEEP_INT      1017
#define IDC_BTN_SWEEP_NOW       1018
#define IDC_RADIO_HOTSPOT_1     1020
#define IDC_RADIO_HOTSPOT_2     1021
#define IDC_RADIO_HOTSPOT_3     1022
#define IDC_RADIO_HOTSPOT_4     1023

static BotEngine* g_pBot = nullptr;
static HWND g_hMenuWnd = nullptr;

// Dark Palette Colors
static const COLORREF CLR_BG         = RGB(20, 22, 31);
static const COLORREF CLR_PANEL      = RGB(28, 31, 44);
static const COLORREF CLR_BORDER     = RGB(46, 52, 74);
static const COLORREF CLR_TEXT       = RGB(245, 247, 252);
static const COLORREF CLR_MUTED      = RGB(148, 163, 184);
static const COLORREF CLR_CYAN       = RGB(0, 229, 255);
static const COLORREF CLR_GREEN      = RGB(16, 185, 129);
static const COLORREF CLR_AMBER      = RGB(245, 158, 11);
static const COLORREF CLR_RED        = RGB(239, 68, 68);
static const COLORREF CLR_PURPLE     = RGB(168, 85, 247);
static const COLORREF CLR_BTN_BG     = RGB(36, 40, 58);
static const COLORREF CLR_BTN_HOVER  = RGB(48, 54, 78);

static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushPanel = NULL;
static HFONT  g_hFontTitle = NULL;
static HFONT  g_hFontBold = NULL;
static HFONT  g_hFontNormal = NULL;
static HFONT  g_hFontMono = NULL;

// Child controls
static HWND g_hEditX = NULL;
static HWND g_hEditY = NULL;
static HWND g_hEditTeamX = NULL;
static HWND g_hEditTeamY = NULL;
static HWND g_hEditLeash = NULL;
static HWND g_hEditSweepInt = NULL;
static HWND g_hRadioVip = NULL;
static HWND g_hRadioPatrol = NULL;
static HWND g_hChkTeamSweep = NULL;
static HWND g_hChkTopmost = NULL;
static HWND g_hRadioHotspot1 = NULL;
static HWND g_hRadioHotspot2 = NULL;
static HWND g_hRadioHotspot3 = NULL;
static HWND g_hRadioHotspot4 = NULL;

static void DrawRoundedCard(HDC hdc, int x, int y, int w, int h, const wchar_t* title) {
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, g_hBrushPanel);
    HPEN hPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + w, y + h, 8, 8);

    if (title && wcslen(title) > 0) {
        SelectObject(hdc, g_hFontBold);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_CYAN);
        TextOutW(hdc, x + 12, y + 10, title, (int)wcslen(title));

        // Separator line
        HPEN hLinePen = CreatePen(PS_SOLID, 1, RGB(38, 43, 62));
        SelectObject(hdc, hLinePen);
        MoveToEx(hdc, x + 10, y + 30, NULL);
        LineTo(hdc, x + w - 10, y + 30);
        DeleteObject(hLinePen);
    }

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}

static void DrawCustomButton(LPDRAWITEMSTRUCT pDIS, COLORREF accentColor, const wchar_t* text, HFONT hFont) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;
    bool isDown = (pDIS->itemState & ODS_SELECTED);

    COLORREF fillCol = isDown ? RGB(25, 28, 40) : CLR_BTN_BG;
    HBRUSH hBr = CreateSolidBrush(fillCol);
    HPEN hPen = CreatePen(PS_SOLID, isDown ? 2 : 1, accentColor);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

    SelectObject(hdc, hFont ? hFont : g_hFontBold);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isDown ? CLR_MUTED : CLR_TEXT);

    RECT rcText = rc;
    if (isDown) { rcText.left += 1; rcText.top += 1; }
    DrawTextW(hdc, text, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static LRESULT CALLBACK VIPMenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hBrushBg = CreateSolidBrush(CLR_BG);
        g_hBrushPanel = CreateSolidBrush(CLR_PANEL);

        g_hFontTitle = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontNormal = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontMono = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // --- Card 1: Spots & Coordinates (Y: 55 to 215) ---
        // Row 1: Anchor Spot
        CreateWindowW(L"BUTTON", L"🎯 LOCK ANCHOR [F6]",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            24, 108, 195, 30, hwnd, (HMENU)IDC_BTN_LOCK_SPOT, hInst, NULL);

        g_hEditX = CreateWindowW(L"EDIT", L"742",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            255, 110, 50, 24, hwnd, (HMENU)IDC_EDIT_MANUAL_X, hInst, NULL);
        g_hEditY = CreateWindowW(L"EDIT", L"673",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            335, 110, 50, 24, hwnd, (HMENU)IDC_EDIT_MANUAL_Y, hInst, NULL);
        CreateWindowW(L"BUTTON", L"Set",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            395, 110, 45, 24, hwnd, (HMENU)IDC_BTN_APPLY_COORDS, hInst, NULL);

        // Row 2: Secondary Account Watcher Trigger (Manual [F5] or Background)
        CreateWindowW(L"BUTTON", L"⚡ CHECK 2ND CLIENT [F5]",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            24, 144, 195, 30, hwnd, (HMENU)IDC_BTN_LOCK_TEAMMATE, hInst, NULL);

        g_hEditTeamX = CreateWindowW(L"EDIT", L"684",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            255, 146, 50, 24, hwnd, (HMENU)IDC_EDIT_TEAM_X, hInst, NULL);
        g_hEditTeamY = CreateWindowW(L"EDIT", L"755",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            335, 146, 50, 24, hwnd, (HMENU)IDC_EDIT_TEAM_Y, hInst, NULL);
        CreateWindowW(L"BUTTON", L"Set",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            395, 146, 45, 24, hwnd, (HMENU)IDC_BTN_APPLY_TEAM, hInst, NULL);

        SendMessage(g_hEditX, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
        SendMessage(g_hEditY, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
        SendMessage(g_hEditTeamX, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
        SendMessage(g_hEditTeamY, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        // --- Card 2: Hotspot Presets (Y: 225 to 345) ---
        g_hRadioHotspot1 = CreateWindowW(L"BUTTON", L"Spot 1: Heartland (742, 673) [Peak SandMonsters & King]",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            26, 252, 410, 20, hwnd, (HMENU)IDC_RADIO_HOTSPOT_1, hInst, NULL);
        g_hRadioHotspot2 = CreateWindowW(L"BUTTON", L"Spot 2: Desert East Gate (577, 672) [Revive Outpost]",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            26, 274, 410, 20, hwnd, (HMENU)IDC_RADIO_HOTSPOT_2, hInst, NULL);
        g_hRadioHotspot3 = CreateWindowW(L"BUTTON", L"Spot 3: Full Desert Trail (577, 652 <-> 799, 627) [Two-Way Sweep]",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            26, 296, 410, 20, hwnd, (HMENU)IDC_RADIO_HOTSPOT_3, hInst, NULL);
        g_hRadioHotspot4 = CreateWindowW(L"BUTTON", L"Spot 4: Corridor (820, 623 -> 780, 657 -> 719, 604)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            26, 318, 410, 20, hwnd, (HMENU)IDC_RADIO_HOTSPOT_4, hInst, NULL);

        SendMessage(g_hRadioHotspot1, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hRadioHotspot2, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hRadioHotspot3, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hRadioHotspot4, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hRadioHotspot1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessage(g_hRadioHotspot2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessage(g_hRadioHotspot3, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessage(g_hRadioHotspot4, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // --- Card 3: Strategy Configuration (Y: 355 to 518) ---
        g_hRadioVip = CreateWindowW(L"BUTTON", L"VIP Spot Lock (Stay in spot, 360 attack, screen guard)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            26, 380, 410, 20, hwnd, (HMENU)IDC_RADIO_VIP_SPOT, hInst, NULL);
        g_hRadioPatrol = CreateWindowW(L"BUTTON", L"Desert Circuit Patrol (Original multi-waypoint loop)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            26, 402, 410, 20, hwnd, (HMENU)IDC_RADIO_PATROL, hInst, NULL);

        SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hRadioVip, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessage(g_hRadioPatrol, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Screen Leash Radius
        g_hEditLeash = CreateWindowW(L"EDIT", L"6",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER | WS_GROUP,
            165, 428, 42, 22, hwnd, (HMENU)IDC_EDIT_LEASH, hInst, NULL);
        SendMessage(g_hEditLeash, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        // 2nd Account Background Watcher Checkbox & Status
        g_hChkTeamSweep = CreateWindowW(L"BUTTON", L"Background 2nd Account Watcher (1 CP / >= 2 Exp)",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            26, 454, 410, 22, hwnd, (HMENU)IDC_CHK_TEAM_SWEEP, hInst, NULL);
        SendMessage(g_hChkTeamSweep, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkTeamSweep, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hEditSweepInt = CreateWindowW(L"EDIT", L"2",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            145, 482, 42, 22, hwnd, (HMENU)IDC_EDIT_SWEEP_INT, hInst, NULL);
        SendMessage(g_hEditSweepInt, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        CreateWindowW(L"BUTTON", L"⚡ Check Now",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            335, 480, 105, 26, hwnd, (HMENU)IDC_BTN_SWEEP_NOW, hInst, NULL);

        // --- Main Action Buttons (Y: 526 to 566) ---
        CreateWindowW(L"BUTTON", L"▶ START [F7]",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            24, 526, 130, 40, hwnd, (HMENU)IDC_BTN_START, hInst, NULL);
        CreateWindowW(L"BUTTON", L"⏸ PAUSE [F8]",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            168, 526, 130, 40, hwnd, (HMENU)IDC_BTN_PAUSE, hInst, NULL);
        CreateWindowW(L"BUTTON", L"⏹ STOP [F9]",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            312, 526, 130, 40, hwnd, (HMENU)IDC_BTN_STOP, hInst, NULL);

        // --- Topmost Checkbox (Y: 742) ---
        g_hChkTopmost = CreateWindowW(L"BUTTON", L"Keep Menu Always On Top",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            26, 742, 220, 22, hwnd, (HMENU)IDC_CHK_TOPMOST, hInst, NULL);
        SendMessage(g_hChkTopmost, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkTopmost, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // 80ms Fast Refresh Timer
        SetTimer(hwnd, 1, 80, NULL);
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == IDC_BTN_LOCK_SPOT) {
            DrawCustomButton(pDIS, CLR_CYAN, L"🎯 LOCK ANCHOR [F6]", g_hFontBold);
            return TRUE;
        } else if (pDIS->CtlID == IDC_BTN_LOCK_TEAMMATE) {
            DrawCustomButton(pDIS, CLR_PURPLE, L"⚡ CHECK 2ND CLIENT [F5]", g_hFontBold);
            return TRUE;
        } else if (pDIS->CtlID == IDC_BTN_START) {
            DrawCustomButton(pDIS, CLR_GREEN, L"▶ START [F7]", g_hFontBold);
            return TRUE;
        } else if (pDIS->CtlID == IDC_BTN_PAUSE) {
            DrawCustomButton(pDIS, CLR_AMBER, L"⏸ PAUSE [F8]", g_hFontBold);
            return TRUE;
        } else if (pDIS->CtlID == IDC_BTN_STOP) {
            DrawCustomButton(pDIS, CLR_RED, L"⏹ STOP [F9]", g_hFontBold);
            return TRUE;
        } else if (pDIS->CtlID == IDC_BTN_APPLY_COORDS || pDIS->CtlID == IDC_BTN_APPLY_TEAM) {
            DrawCustomButton(pDIS, CLR_MUTED, L"Set", g_hFontNormal);
            return TRUE;
        } else if (pDIS->CtlID == IDC_BTN_SWEEP_NOW) {
            DrawCustomButton(pDIS, CLR_PURPLE, L"⚡ Sweep Now", g_hFontBold);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, CLR_PANEL);
        SetTextColor(hdc, CLR_TEXT);
        return (INT_PTR)g_hBrushPanel;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(35, 38, 52));
        SetTextColor(hdc, CLR_CYAN);
        return (INT_PTR)CreateSolidBrush(RGB(35, 38, 52));
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_LOCK_SPOT) {
            if (g_pBot) {
                if (g_pBot->CaptureCurrentSpotAsAnchor()) {
                    int ax = 0, ay = 0;
                    g_pBot->GetAnchor(ax, ay);
                    SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                    SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                } else {
                    Beep(600, 200);
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_LOCK_TEAMMATE) {
            if (g_pBot) {
                g_pBot->TriggerTeammateSweepNow();
                Beep(2100, 150);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_START) {
            if (g_pBot) {
                HWND hGame = g_pBot->FindGameWindow();
                if (hGame && IsWindow(hGame)) {
                    g_pBot->FocusGameWindow(hGame);
                }
                if (!g_pBot->IsRunning()) {
                    g_pBot->Start();
                } else if (g_pBot->GetState() == BOT_STATE_PAUSED) {
                    g_pBot->Resume();
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_PAUSE) {
            if (g_pBot) g_pBot->TogglePause();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_STOP) {
            if (g_pBot) g_pBot->Stop();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_APPLY_COORDS) {
            wchar_t bx[32] = {0}, by[32] = {0};
            GetWindowTextW(g_hEditX, bx, 32);
            GetWindowTextW(g_hEditY, by, 32);
            int nx = _wtoi(bx);
            int ny = _wtoi(by);
            if (nx > 0 && ny > 0 && g_pBot) {
                g_pBot->SetAnchor(nx, ny);
                if (nx == 742 && ny == 673) g_pBot->SetSelectedHotspot(0);
                else if (nx == 577 && ny == 672) g_pBot->SetSelectedHotspot(1);
                else if (nx == 577 && ny == 652) {
                    g_pBot->SetSelectedHotspot(2);
                    g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
                    SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                else if (nx == 820 && ny == 623) {
                    g_pBot->SetSelectedHotspot(3);
                    g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
                    SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                else g_pBot->SetSelectedHotspot(-1);
                Beep(1800, 100);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_APPLY_TEAM) {
            wchar_t bx[32] = {0}, by[32] = {0};
            GetWindowTextW(g_hEditTeamX, bx, 32);
            GetWindowTextW(g_hEditTeamY, by, 32);
            int nx = _wtoi(bx);
            int ny = _wtoi(by);
            if (nx > 0 && ny > 0 && g_pBot) {
                g_pBot->SetTeammateSpot(nx, ny);
                Beep(1800, 100);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_CHK_TEAM_SWEEP) {
            bool isChecked = (SendMessage(g_hChkTeamSweep, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (g_pBot) g_pBot->SetTeammateSweepEnabled(isChecked);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_BTN_SWEEP_NOW) {
            if (g_pBot) {
                g_pBot->TriggerTeammateSweepNow();
                Beep(2000, 100);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_RADIO_HOTSPOT_1) {
            if (g_pBot) {
                g_pBot->SetSelectedHotspot(0);
                g_pBot->SetHuntingMode(MODE_VIP_SPOT_LOCK);
                SendMessage(g_hRadioVip, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_UNCHECKED, 0);
                int ax = 0, ay = 0;
                g_pBot->GetAnchor(ax, ay);
                SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                Beep(1800, 80);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_RADIO_HOTSPOT_2) {
            if (g_pBot) {
                g_pBot->SetSelectedHotspot(1);
                g_pBot->SetHuntingMode(MODE_VIP_SPOT_LOCK);
                SendMessage(g_hRadioVip, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_UNCHECKED, 0);
                int ax = 0, ay = 0;
                g_pBot->GetAnchor(ax, ay);
                SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                Beep(1800, 80);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_RADIO_HOTSPOT_3) {
            if (g_pBot) {
                g_pBot->SetSelectedHotspot(2);
                g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
                SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
                int ax = 0, ay = 0;
                g_pBot->GetAnchor(ax, ay);
                SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                Beep(1800, 80);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_RADIO_HOTSPOT_4) {
            if (g_pBot) {
                g_pBot->SetSelectedHotspot(3);
                g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
                SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
                int ax = 0, ay = 0;
                g_pBot->GetAnchor(ax, ay);
                SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                Beep(1800, 80);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDC_RADIO_VIP_SPOT) {
            if (g_pBot) {
                g_pBot->SetHuntingMode(MODE_VIP_SPOT_LOCK);
                if (g_pBot->GetSelectedHotspot() == 3 || g_pBot->GetSelectedHotspot() == 2) {
                    g_pBot->SetSelectedHotspot(0);
                    SendMessage(g_hRadioHotspot1, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(g_hRadioHotspot3, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendMessage(g_hRadioHotspot4, BM_SETCHECK, BST_UNCHECKED, 0);
                    int ax = 0, ay = 0;
                    g_pBot->GetAnchor(ax, ay);
                    SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                    SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                }
            }
        } else if (id == IDC_RADIO_PATROL) {
            if (g_pBot) g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
        } else if (id == IDC_CHK_TOPMOST) {
            bool isChecked = (SendMessage(g_hChkTopmost, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetWindowPos(hwnd, isChecked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        break;
    }

    case WM_TIMER: {
        // Global Hotkeys Check (works even when Conquer window has focus!)
        if (GetAsyncKeyState(VK_F5) & 1) {
            if (g_pBot) {
                g_pBot->TriggerTeammateSweepNow();
                Beep(2100, 150);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (GetAsyncKeyState(VK_F6) & 1) {
            if (g_pBot) {
                g_pBot->CaptureCurrentSpotAsAnchor();
                int ax = 0, ay = 0;
                g_pBot->GetAnchor(ax, ay);
                SetWindowTextW(g_hEditX, std::to_wstring(ax).c_str());
                SetWindowTextW(g_hEditY, std::to_wstring(ay).c_str());
                if (ax == 742 && ay == 673) g_pBot->SetSelectedHotspot(0);
                else if (ax == 577 && ay == 672) g_pBot->SetSelectedHotspot(1);
                else if (ax == 577 && ay == 652) {
                    g_pBot->SetSelectedHotspot(2);
                    g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
                    SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                else if (ax == 820 && ay == 623) {
                    g_pBot->SetSelectedHotspot(3);
                    g_pBot->SetHuntingMode(MODE_CIRCUIT_PATROL);
                    SendMessage(g_hRadioPatrol, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(g_hRadioVip, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                else g_pBot->SetSelectedHotspot(-1);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (GetAsyncKeyState(VK_F7) & 1) {
            if (g_pBot) {
                HWND hGame = g_pBot->FindGameWindow();
                if (hGame && IsWindow(hGame)) {
                    g_pBot->FocusGameWindow(hGame);
                }
                if (!g_pBot->IsRunning()) {
                    g_pBot->Start();
                } else if (g_pBot->GetState() == BOT_STATE_PAUSED) {
                    g_pBot->Resume();
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (GetAsyncKeyState(VK_F8) & 1) {
            if (g_pBot) g_pBot->TogglePause();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (GetAsyncKeyState(VK_F9) & 1) {
            if (g_pBot) g_pBot->Stop();
            InvalidateRect(hwnd, NULL, FALSE);
        }

        // Read leash radius if modified
        if (g_hEditLeash) {
            wchar_t bl[32] = {0};
            GetWindowTextW(g_hEditLeash, bl, 32);
            int lr = _wtoi(bl);
            if (lr >= 3 && lr <= 30 && g_pBot && g_pBot->GetLeashRadius() != lr) {
                g_pBot->SetLeashRadius(lr);
            }
        }

        // Read sweep interval in minutes if modified
        if (g_hEditSweepInt) {
            wchar_t bi[32] = {0};
            GetWindowTextW(g_hEditSweepInt, bi, 32);
            int mi = _wtoi(bi);
            if (mi >= 1 && mi <= 60 && g_pBot && g_pBot->GetSweepInterval() != (mi * 60)) {
                g_pBot->SetSweepInterval(mi * 60);
            }
        }

        // Sync hotspot radio selection if modified externally
        if (g_pBot) {
            static int s_lastCheckedHotspot = -1;
            int curHotspot = g_pBot->GetSelectedHotspot();
            if (curHotspot != s_lastCheckedHotspot) {
                s_lastCheckedHotspot = curHotspot;
                SendMessage(g_hRadioHotspot1, BM_SETCHECK, (curHotspot == 0) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(g_hRadioHotspot2, BM_SETCHECK, (curHotspot == 1) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(g_hRadioHotspot3, BM_SETCHECK, (curHotspot == 2) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(g_hRadioHotspot4, BM_SETCHECK, (curHotspot == 3) ? BST_CHECKED : BST_UNCHECKED, 0);
            }
        }

        // Repaint live labels
        RECT rcUpdate = { 10, 50, 460, 755 };
        InvalidateRect(hwnd, &rcUpdate, FALSE);
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Double buffer to eliminate flicker
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int w = rcClient.right - rcClient.left;
        int h = rcClient.bottom - rcClient.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Fill background
        FillRect(memDC, &rcClient, g_hBrushBg);

        // 1. Header Bar
        SelectObject(memDC, g_hFontTitle);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, CLR_CYAN);
        TextOutW(memDC, 16, 12, L"⚡ CONQUER BOT NEW 1", 20);

        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, CLR_MUTED);
        TextOutW(memDC, 16, 32, L"100% Hardware Driver • Screen-Locked Guard • 2nd Client Watcher", 61);

        // Live Status Badge in Header
        BotState st = g_pBot ? g_pBot->GetState() : BOT_STATE_IDLE;
        bool running = g_pBot ? g_pBot->IsRunning() : false;
        HWND curFore = GetForegroundWindow();
        HWND hGame = g_pBot ? g_pBot->GetGameHwnd() : NULL;
        bool isOutside = (running && hGame && curFore != hGame && st != BOT_STATE_PAUSED && st != BOT_STATE_SECONDARY_LOOT);

        const wchar_t* stText = L"[ IDLE ]";
        COLORREF stColor = CLR_MUTED;
        if (running) {
            if (st == BOT_STATE_PAUSED) { stText = L"[ ⏸ PAUSED ]"; stColor = CLR_AMBER; }
            else if (isOutside) { stText = L"[ 💤 OUTSIDE (SAFE) ]"; stColor = CLR_AMBER; }
            else if (st == BOT_STATE_LOOTING) { stText = L"[ 💎 LOOTING ]"; stColor = CLR_CYAN; }
            else if (st == BOT_STATE_SECONDARY_LOOT || st == BOT_STATE_TEAMMATE_SWEEP) { stText = L"[ 👥 2ND LOOT ]"; stColor = CLR_PURPLE; }
            else if (st == BOT_STATE_RETURNING_SPOT) { stText = L"[ 🔄 RETURNING ]"; stColor = CLR_AMBER; }
            else { stText = L"[ ● ACTIVE ]"; stColor = CLR_GREEN; }
        }
        SelectObject(memDC, g_hFontBold);
        SetTextColor(memDC, stColor);
        TextOutW(memDC, 340, 14, stText, (int)wcslen(stText));

        // 2. Card 1: Position & Spots Lock (Y: 55 to 215, H: 160)
        DrawRoundedCard(memDC, 14, 55, 438, 160, L"📍 SPOTS & POSITION LOCK");

        int liveX = 0, liveY = 0;
        bool hasLive = g_pBot ? g_pBot->ReadCurrentCoords(liveX, liveY) : false;
        int ancX = 742, ancY = 673;
        if (g_pBot) g_pBot->GetAnchor(ancX, ancY);
        int teamX = 684, teamY = 755;
        if (g_pBot) g_pBot->GetTeammateSpot(teamX, teamY);
        double dist = g_pBot ? g_pBot->GetDistanceToAnchor() : 0.0;

        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, CLR_MUTED);
        TextOutW(memDC, 26, 82, L"Live Coords:", 12);

        SelectObject(memDC, g_hFontBold);
        if (hasLive && liveX > 0 && liveY > 0) {
            SetTextColor(memDC, CLR_GREEN);
            std::wstring sLive = L"(" + std::to_wstring(liveX) + L", " + std::to_wstring(liveY) + L")";
            TextOutW(memDC, 105, 82, sLive.c_str(), (int)sLive.length());
        } else {
            SetTextColor(memDC, CLR_AMBER);
            TextOutW(memDC, 105, 82, L"Waiting Game...", 15);
        }

        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, CLR_MUTED);
        TextOutW(memDC, 230, 113, L"X:", 2);
        TextOutW(memDC, 310, 113, L"Y:", 2);
        TextOutW(memDC, 230, 149, L"X:", 2);
        TextOutW(memDC, 310, 149, L"Y:", 2);

        // Summary row at bottom of Card 1
        TextOutW(memDC, 26, 185, L"Locked Anchor:", 14);
        SelectObject(memDC, g_hFontBold);
        SetTextColor(memDC, CLR_CYAN);
        std::wstring sAnc = L"(" + std::to_wstring(ancX) + L", " + std::to_wstring(ancY) + L")";
        TextOutW(memDC, 115, 185, sAnc.c_str(), (int)sAnc.length());

        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, CLR_MUTED);
        TextOutW(memDC, 230, 185, L"Visual Radar:", 13);
        SelectObject(memDC, g_hFontBold);
        if (g_pBot && g_pBot->IsTeammateNear()) {
            SetTextColor(memDC, CLR_GREEN);
            TextOutW(memDC, 320, 185, L"[ 🎯 NEAR TEAMMATE ]", 20);
        } else if (g_pBot && g_pBot->IsBeaconVisible()) {
            SetTextColor(memDC, CLR_PURPLE);
            TextOutW(memDC, 320, 185, L"[ 🟣 BEACON ACTIVE ]", 20);
        } else {
            SetTextColor(memDC, CLR_CYAN);
            TextOutW(memDC, 320, 185, L"[ 👁️ AUTO-TRACKING ]", 20);
        }

        // 3. Card 2: Hotspot Presets (Y: 225 to 345, H: 120)
        DrawRoundedCard(memDC, 14, 225, 438, 120, L"🗺️ HUNTING ZONES & EXPANSION HOTSPOTS");

        // 4. Card 3: Strategy Configuration (Y: 355 to 515, H: 160)
        DrawRoundedCard(memDC, 14, 355, 438, 160, L"⚙️ HUNTING & TEAM SWEEP CONFIGURATION");

        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, CLR_MUTED);
        TextOutW(memDC, 26, 430, L"Screen Leash Radius:", 20);
        TextOutW(memDC, 215, 430, L"tiles (Max wander before snapback)", 34);

        TextOutW(memDC, 26, 484, L"Sweep Interval: Every", 21);
        TextOutW(memDC, 195, 484, L"min (Checks 2nd bags)", 22);

        // 5. Card 4: Live Farming Stats (Y: 574 to 732, H: 158)
        DrawRoundedCard(memDC, 14, 574, 438, 158, L"📊 LIVE FARMING & SWEEP STATISTICS");

        int cpbags = g_pBot ? g_pBot->GetCpbagsCount() : 0;
        int dbCount = g_pBot ? g_pBot->GetDragonBallsCount() : 0;
        int totalLoot = g_pBot ? g_pBot->GetTotalLootCount() : 0;
        std::string lastLoot = g_pBot ? g_pBot->GetLastLootName() : "None";
        if (lastLoot.empty()) lastLoot = "None";

        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, CLR_MUTED);
        TextOutW(memDC, 26, 602, L"CP Bags / DBs:", 14);
        TextOutW(memDC, 240, 602, L"Total Items Looted:", 19);
        TextOutW(memDC, 26, 630, L"Last Looted Drop:", 17);
        TextOutW(memDC, 26, 658, L"Distance to Anchor:", 19);
        TextOutW(memDC, 26, 686, L"2nd Acc Watcher:", 16);

        SelectObject(memDC, g_hFontBold);
        SetTextColor(memDC, CLR_CYAN);
        std::wstring sCp = std::to_wstring(cpbags) + L" (DB: " + std::to_wstring(dbCount) + L")";
        TextOutW(memDC, 130, 602, sCp.c_str(), (int)sCp.length());

        SetTextColor(memDC, CLR_TEXT);
        std::wstring sTot = std::to_wstring(totalLoot);
        TextOutW(memDC, 365, 602, sTot.c_str(), (int)sTot.length());

        SetTextColor(memDC, CLR_GREEN);
        std::wstring sLast(lastLoot.begin(), lastLoot.end());
        TextOutW(memDC, 150, 630, sLast.c_str(), (int)sLast.length());

        if (g_pBot && g_pBot->GetSelectedHotspot() == 3) {
            SetTextColor(memDC, CLR_CYAN);
            std::wstring sMode = L"Corridor Patrol [820, 623 -> 780, 657 -> 719, 604]";
            TextOutW(memDC, 160, 658, sMode.c_str(), (int)sMode.length());
        } else if (g_pBot && g_pBot->GetSelectedHotspot() == 2) {
            SetTextColor(memDC, CLR_CYAN);
            std::wstring sMode = L"Full Trail Patrol [577, 652 <-> 799, 627]";
            TextOutW(memDC, 160, 658, sMode.c_str(), (int)sMode.length());
        } else if (dist <= 1.5) {
            SetTextColor(memDC, CLR_GREEN);
            std::wstringstream ssDist;
            ssDist << std::fixed << std::setprecision(1) << dist << L" tiles [PERFECT ON SPOT]";
            TextOutW(memDC, 160, 658, ssDist.str().c_str(), (int)ssDist.str().length());
        } else {
            SetTextColor(memDC, CLR_AMBER);
            std::wstringstream ssDist;
            ssDist << std::fixed << std::setprecision(1) << dist << L" tiles [WITHIN SCREEN]";
            TextOutW(memDC, 160, 658, ssDist.str().c_str(), (int)ssDist.str().length());
        }

        // 2nd Account Watcher Status
        if (!g_pBot || !g_pBot->IsSecondaryWatcherEnabled()) {
            SetTextColor(memDC, CLR_MUTED);
            TextOutW(memDC, 160, 686, L"[ Disabled in configuration ]", 29);
        } else if (st == BOT_STATE_SECONDARY_LOOT) {
            SetTextColor(memDC, CLR_PURPLE);
            TextOutW(memDC, 160, 686, L"💎 AUTO-LOOTING ON 2ND ACCOUNT...", 32);
        } else {
            SetTextColor(memDC, CLR_GREEN);
            std::wstring sRem = L"Active • Background Watching (>=1 CP / >=2 Exp)";
            TextOutW(memDC, 160, 686, sRem.c_str(), (int)sRem.length());
        }

        // Copy buffer to screen
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        if (g_hBrushBg) DeleteObject(g_hBrushBg);
        if (g_hBrushPanel) DeleteObject(g_hBrushPanel);
        if (g_hFontTitle) DeleteObject(g_hFontTitle);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontMono) DeleteObject(g_hFontMono);
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

HWND VIPMenu::Create(BotEngine* pBot) {
    g_pBot = pBot;

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = VIPMenuWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ConquerVIPMenuClass";

    RegisterClassExW(&wc);

    int winW = 482;
    int winH = 810;

    // Position window smartly near the right of a 1024x576 or 1920x1080 display
    int posX = 1045;
    int posY = 80;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"ConquerVIPMenuClass",
        L"ConquerBot New 1 - VIP Hunting Menu [Spot Guard + Team Sweep]",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winW, winH,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    g_hMenuWnd = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return hwnd;
}

void VIPMenu::Run(BotEngine* pBot) {
    HWND hwnd = Create(pBot);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
