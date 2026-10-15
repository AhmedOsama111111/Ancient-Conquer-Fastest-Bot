import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32

def list_windows_for_pid(target_pid):
    WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    
    def enum_cb(hwnd, lparam):
        if user32.IsWindowVisible(hwnd):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == target_pid:
                length = user32.GetWindowTextLengthW(hwnd)
                buf = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buf, length + 1)
                
                rect = wintypes.RECT()
                user32.GetClientRect(hwnd, ctypes.byref(rect))
                print(f"HWND: {hwnd}, Title: '{buf.value}', ClientRect: ({rect.left},{rect.top},{rect.right},{rect.bottom})")
        return True

    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

list_windows_for_pid(11700)
