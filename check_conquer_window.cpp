#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h)) return TRUE;
        RECT rc;
        GetClientRect(h, &rc);
        if (rc.right < 300 || rc.bottom < 200) return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);

        wchar_t cls[256] = {0};
        GetClassNameW(h, cls, 256);

        wchar_t title[256] = {0};
        GetWindowTextW(h, title, 256);

        std::wcout << L"HWND=" << h << L" | PID=" << pid 
                   << L" | Size=" << rc.right << L"x" << rc.bottom
                   << L" | Class=\"" << cls << L"\" | Title=\"" << title << L"\"\n";
        return TRUE;
    }, 0);
    return 0;
}
