#include <windows.h>
#include <iostream>

int main() {
    std::cout << "[*] Listing all visible top-level windows:\n";
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h)) return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        RECT rc; GetClientRect(h, &rc);
        if (rc.right >= 300 && rc.bottom >= 200) {
            wchar_t title[256] = {0};
            GetWindowTextW(h, title, 256);
            std::wcout << L"  HWND=" << h << L" PID=" << pid << L" Client=" << rc.right << L"x" << rc.bottom << L" Title='" << title << L"'\n";
        }
        return TRUE;
    }, 0);
    return 0;
}
