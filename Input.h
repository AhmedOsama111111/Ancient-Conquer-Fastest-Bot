#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

// ============================================================================
// Input.h — 100% Hardware Interception Driver Input (Direct Kernel Filter)
// Pure driver simulation modeled after ancient co / pubg (driver_input.py & clicker_engine.py)
// ZERO Win32 SendInput / mouse_event / SetCursorPos / keybd_event.
// ============================================================================

namespace Input {
    bool Init();
    bool IsAvailable();
    void Cleanup();

    // Natural smooth human mouse movement using Bezier curves & Interception driver
    void MoveToSmooth(int targetX, int targetY, double speedMultiplier = 1.0);
    void MoveToInstant(int targetX, int targetY);

    // Continuous Ctrl hold (DIK_LCONTROL 0x1D) for uninterrupted jumping across circuit
    void HoldCtrl(bool hold);

    // Ultra-Fast Hardware Jump Click (re-asserts Ctrl, 14ms crisp Left Click via Interception driver)
    void JumpClick(int targetX, int targetY, int holdMs = 14);

    // Hardware Walk Click (releases Ctrl, 14ms crisp Left Click via Interception driver)
    void WalkClick(int targetX, int targetY, int holdMs = 14);

    // Ultra-Fast Hardware Scatter Attack (10ms crisp Right Click via Interception driver)
    void AttackRightClick(int targetX, int targetY, int holdMs = 10);

    // Mouse buttons (100% Interception Driver only)
    void LeftClick(double holdSec = 0.045);
    void RightClick(double holdSec = 0.045);
    void LeftDown();
    void LeftUp();
    void RightDown();
    void RightUp();

    // Keyboard keys (100% Interception Driver only)
    void KeyPress(WORD scanCode, double holdSec = 0.040);
    void KeyDown(WORD scanCode);
    void KeyUp(WORD scanCode);

    // 100% Hardware Driver Keyboard Shortcuts (ZERO keybd_event)
    void SendAltTab();
    void SendCtrlTab();
    void SendCtrl9();
    void SendCtrlW();

    // Humanized Log-Normal Timing Engine (from clicker_engine.py)
    double GetNaturalDelay(double targetCps = 6.0);
    void PreciseSleep(double seconds);
}
