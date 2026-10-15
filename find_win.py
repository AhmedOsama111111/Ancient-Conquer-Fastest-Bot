import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32

def enum_cb(hwnd, lparam):
    if user32.IsWindowVisible(hwnd):
        cl_rc = wintypes.RECT()
        user32.GetClientRect(hwnd, ctypes.byref(cl_rc))
        w = cl_rc.right - cl_rc.left
        h = cl_rc.bottom - cl_rc.top
        if w >= 400 and h >= 300:
            length = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buf, length + 1)
            title = buf.value
            
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            
            pt = wintypes.POINT(0, 0)
            user32.ClientToScreen(hwnd, ctypes.byref(pt))
            print(f'HWND: {hwnd}, PID: {pid.value}, Title: "{title}", Client: {w}x{h}, ScreenPos: ({pt.x}, {pt.y})')
    return True

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
