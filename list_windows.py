import ctypes

user32 = ctypes.windll.user32

titles = []
def enum_windows_proc(hwnd, lParam):
    if user32.IsWindowVisible(hwnd):
        length = user32.GetWindowTextLengthW(hwnd)
        if length > 0:
            buff = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buff, length + 1)
            t = buff.value
            pid = ctypes.c_ulong()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            cls = ctypes.create_unicode_buffer(256)
            user32.GetClassNameW(hwnd, cls, 256)
            titles.append((hwnd, pid.value, cls.value, t))
    return True

WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
user32.EnumWindows(WNDENUMPROC(enum_windows_proc), 0)

with open('all_windows.txt', 'w', encoding='utf-8') as f:
    for h, p, c, t in titles:
        f.write(f"HWND: 0x{h:X} | PID: {p} | Class: {c} | Title: {t}\n")
print(f"Total windows: {len(titles)}")
