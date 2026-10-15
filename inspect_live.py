import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

def enum_cb(hwnd, lparam):
    if user32.IsWindowVisible(hwnd):
        buf = ctypes.create_unicode_buffer(512)
        user32.GetWindowTextW(hwnd, buf, 512)
        title = buf.value
        if title:
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            
            rc = wintypes.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(rc))
            rw = wintypes.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rw))
            pt = wintypes.POINT()
            user32.ClientToScreen(hwnd, ctypes.byref(pt))
            
            cw = rc.right - rc.left
            ch = rc.bottom - rc.top
            if cw > 200 and ch > 150:
                print(f"HWND: {hex(hwnd)} | PID: {pid.value} | Title: '{title}' | Client: {cw}x{ch} | Window: ({rw.left},{rw.top})-({rw.right},{rw.bottom}) | ClientToScreen: ({pt.x},{pt.y})")
    return True

cb = WNDENUMPROC(enum_cb)
user32.EnumWindows(cb, 0)
