# Auto Hunt v1

🚀 **Auto Hunt v1** is a high-performance, automated farming and security-unlocking bot designed for Conquer Online (Ancient Conquer). Built in C++20 with direct hardware-driver input emulation and optimized memory/visual radar capabilities.

---

## ✨ Features

- **Automated Drop Security Captcha Solver**:
  - Automatically clicks `[Yes]` in-game upon detection of security checks.
  - Instantly solves web-based slider puzzle captchas (Drop Unlock) via NCC template matching and edge-continuity alignment.
  - Closes the browser tab and returns focus to the game window immediately within ~2-3 seconds without delays or UI thread deadlocks.
- **Hardware-Level Input Emulation**:
  - Full Interception driver integration for undetectable keystrokes and mouse movements.
  - Humanized bezier curve mouse paths and micro-overshoots.
- **VIP Spot & Circuit Farming**:
  - Precision waypoint circuit navigation and anchor locking.
  - Auto-jump continuous movement (`Ctrl` loop).
- **Intelligent Loot Filter**:
  - Memory & visual scanning for high-priority drops (CP Bags, Sycees, Essences).
- **Interactive VIP GUI**:
  - Live coordinates display, hotkey management (`[F7]` Start / Resume, `[F8]` Pause, `[F9]` Stop).
  - Spot selectors and sweep timers.

---

## 🛠️ Build Instructions

### Prerequisites
- Windows 10/11 (x64)
- Visual Studio 2022+ with C++ Desktop Development workload (MSBuild, MSVC C++20)
- Interception Driver installed on the system

### Compiling
Run `build.bat` in the root project directory:
```cmd
build.bat
```
The compiled executable `ConquerBotNew.exe` will be generated in the root directory.

---

## 🎮 How to Run
1. Start Conquer Online and log into your character.
2. Run `ConquerBotNew.exe` as Administrator.
3. Configure your hunting spot in the VIP Menu.
4. Press **[F7]** to start auto-hunting.
5. Press **[F8]** to pause, or **[F9]** to exit.
