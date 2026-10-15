import ctypes
user32 = ctypes.windll.user32

def enum_windows():
    wins = []
    def callback(hwnd, extra):
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buff = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buff, length + 1)
                wins.append((hwnd, buff.value))
        return True
    
    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    user32.EnumWindows(WNDENUMPROC(callback), 0)
    return wins

for h, t in enum_windows():
    if any(k in t.lower() for k in ['osos', 'conquer', 'bot', 'bandicam', 'c:\\']):
        print(f"HWND {h}: '{t}'")
