#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>

DWORD FindConquerPID() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, "Conquer.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main() {
    DWORD pid = FindConquerPID();
    std::cout << "[*] Conquer PID: " << pid << "\n";

    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        DWORD p = 0;
        GetWindowThreadProcessId(h, &p);
        DWORD targetPid = (DWORD)lp;
        if (p == targetPid) {
            char title[256] = {0};
            GetWindowTextA(h, title, 256);
            RECT rc; GetClientRect(h, &rc);
            std::cout << "  HWND=" << h << " Visible=" << IsWindowVisible(h) << " Title='" << title 
                      << "' Client=(" << rc.right - rc.left << "x" << rc.bottom - rc.top << ")\n";
        }
        return TRUE;
    }, (LPARAM)pid);

    return 0;
}
