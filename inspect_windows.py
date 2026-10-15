import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

with open('all_windows.txt', 'w', encoding='utf-8') as f:
    def enum_windows_proc(hwnd, lparam):
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buff = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buff, length + 1)
                rect = wintypes.RECT()
                user32.GetWindowRect(hwnd, ctypes.byref(rect))
                w = rect.right - rect.left
                h = rect.bottom - rect.top
                if w > 100 and h > 100:
                    line = f"{hwnd}: '{buff.value}' ({rect.left}, {rect.top}, {w}x{h})\n"
                    f.write(line)
                    sys.stdout.write(line)
                    sys.stdout.flush()
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_windows_proc), 0)
