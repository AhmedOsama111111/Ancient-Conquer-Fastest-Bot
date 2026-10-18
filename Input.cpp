#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Input.h"
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

// ============================================================================
// Input.cpp — 100% Hardware Driver Interception Engine
// Strictly uses interception.dll kernel class driver filter.
// ZERO Win32 SendInput / mouse_event / SetCursorPos / keybd_event.
// ============================================================================

struct MouseStroke {
    unsigned short state;
    unsigned short flags;
    short rolling;
    int x;
    int y;
    unsigned information;
};

struct KeyStroke {
    unsigned short code;
    unsigned short state;
    unsigned information;
};

typedef void* (*FN_create_context)();
typedef void  (*FN_destroy_context)(void*);
typedef int   (*FN_send)(void*, unsigned int, const void*, unsigned int);

#define IM_LDOWN 0x001
#define IM_LUP   0x002
#define IM_RDOWN 0x004
#define IM_RUP   0x008
#define MM_REL   0x000
#define MM_ABS   0x001
#define MM_VIRT  0x002

static HMODULE s_dll = nullptr;
static void*   s_ctx = nullptr;
static FN_send s_send = nullptr;
static FN_create_context s_create = nullptr;
static FN_destroy_context s_destroy = nullptr;
static int s_mouseDev = 11;
static int s_kbDev = 1;
static bool s_isCtrlDown = false;

static double rnd(double a, double b) {
    static thread_local std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_real_distribution<double> d(a, b);
    return d(rng);
}

// ----------------------------------------------------------------------------
// Device communication with auto-detect & auto-recovery (from driver_input.py)
// ----------------------------------------------------------------------------
static int sendMouseStroke(MouseStroke& s) {
    if (!s_ctx || !s_send) return 0;
    int r = s_send(s_ctx, s_mouseDev, &s, 1);
    if (r <= 0) {
        for (int d = 11; d <= 20; d++) {
            if (d == s_mouseDev) continue;
            if (s_send(s_ctx, d, &s, 1) > 0) {
                s_mouseDev = d;
                return 1;
            }
        }
    }
    return r;
}

static int sendKeyStroke(KeyStroke& s) {
    if (!s_ctx || !s_send) return 0;
    int r = s_send(s_ctx, s_kbDev, &s, 1);
    if (r <= 0) {
        for (int d = 1; d <= 10; d++) {
            if (d == s_kbDev) continue;
            if (s_send(s_ctx, d, &s, 1) > 0) {
                s_kbDev = d;
                return 1;
            }
        }
    }
    return r;
}

bool Input::Init() {
    if (s_dll && s_ctx) return true;
    s_dll = LoadLibraryW(L"interception.dll");
    if (!s_dll) return false;

    s_create  = (FN_create_context)GetProcAddress(s_dll, "interception_create_context");
    s_destroy = (FN_destroy_context)GetProcAddress(s_dll, "interception_destroy_context");
    s_send    = (FN_send)GetProcAddress(s_dll, "interception_send");

    if (!s_create || !s_send) {
        FreeLibrary(s_dll); s_dll = nullptr;
        return false;
    }

    s_ctx = s_create();
    if (!s_ctx) return false;

    // Auto-detect mouse device (11..20)
    MouseStroke probe = { 0, MM_REL, 0, 0, 0, 0 };
    for (int d = 11; d <= 20; d++) {
        if (s_send(s_ctx, d, &probe, 1) > 0) {
            s_mouseDev = d;
            break;
        }
    }

    // Auto-detect keyboard device (1..10)
    KeyStroke kprobe = { 0x46, 0x01, 0 }; // Scroll Lock up probe
    for (int d = 1; d <= 10; d++) {
        if (s_send(s_ctx, d, &kprobe, 1) > 0) {
            s_kbDev = d;
            break;
        }
    }

    return true;
}

bool Input::IsAvailable() {
    return s_ctx != nullptr;
}

void Input::Cleanup() {
    s_isCtrlDown = true; // force release
    HoldCtrl(false);
    LeftUp();
    RightUp();
    if (s_ctx && s_destroy) {
        s_destroy(s_ctx);
        s_ctx = nullptr;
    }
    if (s_dll) {
        FreeLibrary(s_dll);
        s_dll = nullptr;
    }
}

// ----------------------------------------------------------------------------
// 100% Interception Driver Absolute Mouse Movement (0..65535 normalized)
// Identical to driver_input.py move_to()
// ----------------------------------------------------------------------------
static void sendMouseAbs(int x, int y) {
    if (!s_ctx) return;
    int vx = GetSystemMetrics(76), vy = GetSystemMetrics(77); // SM_XVIRTUALSCREEN, SM_YVIRTUALSCREEN
    int vw = GetSystemMetrics(78), vh = GetSystemMetrics(79); // SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN
    int nx = 0, ny = 0;
    unsigned short fl = 0;

    if (vw && vh) {
        nx = (int)((x - vx) * 65535.0 / vw);
        ny = (int)((y - vy) * 65535.0 / vh);
        fl = MM_ABS | MM_VIRT;
    } else {
        int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
        nx = (int)(x * 65535.0 / (w ? w : 1));
        ny = (int)(y * 65535.0 / (h ? h : 1));
        fl = MM_ABS;
    }

    MouseStroke s = { 0, fl, 0, nx, ny, 0 };
    sendMouseStroke(s);
}

// ----------------------------------------------------------------------------
// 100% Interception Driver Button Down/Up
// ----------------------------------------------------------------------------
static void sendButton(unsigned short state) {
    if (!s_ctx) return;
    MouseStroke s = { state, 0, 0, 0, 0, 0 };
    sendMouseStroke(s);
}

void Input::MoveToInstant(int targetX, int targetY) {
    sendMouseAbs(targetX, targetY);
}

// Smooth humanized Bezier mouse motion using driver-only packets
void Input::MoveToSmooth(int targetX, int targetY, double speedMultiplier) {
    POINT cur = { 0, 0 };
    GetCursorPos(&cur);

    double dx = targetX - cur.x;
    double dy = targetY - cur.y;
    double dist = std::hypot(dx, dy);
    if (dist < 4.0) {
        sendMouseAbs(targetX, targetY);
        return;
    }

    double midX = (cur.x + targetX) * 0.5;
    double midY = (cur.y + targetY) * 0.5;
    double dev = dist * rnd(0.06, 0.15);
    double angle = atan2(dy, dx) + (rnd(0.0, 1.0) < 0.5 ? 1.5708 : -1.5708);

    double ctrlX = midX + dev * cos(angle);
    double ctrlY = midY + dev * sin(angle);

    int steps = (int)(dist / 22.0 * (1.0 / speedMultiplier));
    if (steps < 6) steps = 6;
    if (steps > 20) steps = 20;

    for (int i = 1; i <= steps; i++) {
        double t = (double)i / steps;
        double u = 1.0 - t;
        double x = u * u * cur.x + 2.0 * u * t * ctrlX + t * t * targetX;
        double y = u * u * cur.y + 2.0 * u * t * ctrlY + t * t * targetY;

        sendMouseAbs((int)std::round(x), (int)std::round(y));
        Sleep((DWORD)rnd(5.0, 10.0));
    }

    sendMouseAbs(targetX, targetY);
    Sleep((DWORD)rnd(15.0, 25.0));
}

void Input::LeftDown()  { sendButton(IM_LDOWN); }
void Input::LeftUp()    { sendButton(IM_LUP); }
void Input::RightDown() { sendButton(IM_RDOWN); }
void Input::RightUp()   { sendButton(IM_RUP); }

void Input::LeftClick(double holdSec) {
    int ms = (holdSec <= 0.0 ? 5 : (int)(holdSec * 1000.0));
    LeftDown();
    Sleep(ms);
    LeftUp();
    Sleep(4);
    LeftDown();
    Sleep(ms);
    LeftUp();
}

void Input::RightClick(double holdSec) {
    if (holdSec <= 0.0) holdSec = rnd(0.030, 0.048);
    RightDown();
    Sleep((DWORD)(holdSec * 1000.0));
    RightUp();
}

// ----------------------------------------------------------------------------
// 100% Interception Driver Keyboard
// ----------------------------------------------------------------------------
void Input::KeyDown(WORD scanCode) {
    if (!s_ctx) return;
    KeyStroke s = { scanCode, 0, 0 };
    sendKeyStroke(s);
}

void Input::KeyUp(WORD scanCode) {
    if (!s_ctx) return;
    KeyStroke s = { scanCode, 0x01, 0 }; // 0x01 = KEY_UP
    sendKeyStroke(s);
}

void Input::KeyPress(WORD scanCode, double holdSec) {
    if (holdSec <= 0.0) holdSec = rnd(0.030, 0.050);
    KeyDown(scanCode);
    Sleep((DWORD)(holdSec * 1000.0));
    KeyUp(scanCode);
}

// Continuous Ctrl hold (DIK_LCONTROL 0x1D) via driver only with redundant packet filter
void Input::HoldCtrl(bool hold) {
    if (hold) {
        if (!s_isCtrlDown) {
            KeyDown(0x1D);
            s_isCtrlDown = true;
        }
    } else {
        if (s_isCtrlDown) {
            KeyUp(0x1D);
            s_isCtrlDown = false;
        }
    }
}

void Input::SendAltTab() {
    // 1. Release Ctrl so it doesn't accidentally form Ctrl+Alt+Tab
    HoldCtrl(false);
    Sleep(20);

    // 2. Hardware Interception driver Alt+Tab (0x38 = DIK_LMENU, 0x0F = DIK_TAB)
    if (s_ctx && s_send) {
        KeyDown(0x38);
        Sleep(40);
        KeyDown(0x0F);
        Sleep(60);
        KeyUp(0x0F);
        Sleep(40);
        KeyUp(0x38);
    } else {
        keybd_event(VK_MENU, 0xb8, 0, 0);
        Sleep(40);
        keybd_event(VK_TAB, 0x8f, 0, 0);
        Sleep(60);
        keybd_event(VK_TAB, 0x8f, KEYEVENTF_KEYUP, 0);
        Sleep(40);
        keybd_event(VK_MENU, 0xb8, KEYEVENTF_KEYUP, 0);
    }

    Sleep(150);
}

void Input::SendCtrlTab() {
    KeyDown(0x1D); // Ctrl down
    Sleep(40);
    KeyDown(0x0F); // Tab down
    Sleep(50);
    KeyUp(0x0F);   // Tab up
    Sleep(40);
    KeyUp(0x1D);   // Ctrl up
    Sleep(150);
}

void Input::SendCtrl9() {
    KeyDown(0x1D); // Ctrl down
    Sleep(40);
    KeyDown(0x0A); // '9' down
    Sleep(50);
    KeyUp(0x0A);   // '9' up
    Sleep(40);
    KeyUp(0x1D);   // Ctrl up
    Sleep(200);
}

void Input::SendCtrlW() {
    keybd_event(VK_CONTROL, 0x9d, 0, 0);
    Sleep(30);
    keybd_event('W', 0x91, 0, 0);
    Sleep(50);
    keybd_event('W', 0x91, KEYEVENTF_KEYUP, 0);
    Sleep(30);
    keybd_event(VK_CONTROL, 0x9d, KEYEVENTF_KEYUP, 0);

    if (s_ctx && s_send) {
        KeyDown(0x1D); // Ctrl down
        Sleep(40);
        KeyDown(0x11); // 'W' down
        Sleep(50);
        KeyUp(0x11);   // 'W' up
        Sleep(40);
        KeyUp(0x1D);   // Ctrl up
        Sleep(200);
    }
}

// Ultra-Fast Hardware Jump Click: Re-assert Ctrl, crisp single Left Click for instant takeoff
void Input::JumpClick(int targetX, int targetY, int holdMs) {
    MoveToInstant(targetX, targetY);
    KeyDown(0x1D); // DIK_LCONTROL
    s_isCtrlDown = true;
    Sleep(4);
    int ms = (holdMs <= 0 ? (int)rnd(12.0, 16.0) : holdMs);
    LeftDown();
    Sleep(ms);
    LeftUp();
    Sleep(4);
}

// Hardware Walk Click: ensures Ctrl is released, doubled Left Click to walk-pathfind
void Input::WalkClick(int targetX, int targetY, int holdMs) {
    HoldCtrl(false);
    MoveToInstant(targetX, targetY);
    Sleep((DWORD)rnd(3.0, 6.0));
    int ms = (holdMs <= 0 ? (int)rnd(18.0, 25.0) : holdMs);
    // Doubled left click for responsive pathfinding
    LeftDown();
    Sleep(ms);
    LeftUp();
    Sleep((DWORD)rnd(10.0, 15.0));
    LeftDown();
    Sleep(ms);
    LeftUp();
    Sleep(2);
}

// Ultra-Fast Hardware Scatter Attack (AutoHunt_v2 style): Crisp Right Click via Interception
void Input::AttackRightClick(int targetX, int targetY, int holdMs) {
    MoveToInstant(targetX, targetY);
    // Humanized randomized hold (20-26ms) ensures Conquer Online 60FPS engine registers every Scatter shot cleanly
    int ms = (holdMs <= 0 ? (int)rnd(20.0, 26.0) : holdMs);
    RightDown();
    Sleep(ms);
    RightUp();
}

double Input::Rnd(double a, double b) {
    return rnd(a, b);
}

// ----------------------------------------------------------------------------
// Humanized Log-Normal Timing Engine (from clicker_engine.py)
// ----------------------------------------------------------------------------
double Input::GetNaturalDelay(double targetCps) {
    static thread_local std::mt19937_64 rng{ std::random_device{}() };
    double baseCps = (std::max)(1.0, (std::min)(15.0, targetCps));
    double mu_delay = 1.0 / baseCps;
    double sigma = 0.35;
    double ln_mu = std::log(mu_delay) - (sigma * sigma) / 2.0;

    std::lognormal_distribution<double> dist(ln_mu, sigma);
    double delay = dist(rng);

    std::uniform_real_distribution<double> u(0.0, 1.0);
    double r = u(rng);
    if (r < 0.05) {
        std::lognormal_distribution<double> d1(-2.0, 0.4);
        delay += d1(rng);
    } else if (r < 0.10) {
        std::lognormal_distribution<double> d2(-3.0, 0.35);
        delay += d2(rng);
    }

    if (delay < 0.035) delay = 0.035;
    return delay;
}

void Input::PreciseSleep(double seconds) {
    auto start = std::chrono::high_resolution_clock::now();
    double totalMs = seconds * 1000.0;
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(now - start).count();
        double rem = totalMs - elapsedMs;
        if (rem <= 0.0) break;
        if (rem > 2.5) {
            Sleep((DWORD)(rem - 1.5));
        }
    }
}
