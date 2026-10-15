import win32gui

def cb(hwnd, _):
    if win32gui.IsWindowVisible(hwnd):
        txt = win32gui.GetWindowText(hwnd)
        if 'Osos' in txt or 'Conquer' in txt or 'play' in txt.lower():
            wr = win32gui.GetWindowRect(hwnd)
            cr = win32gui.GetClientRect(hwnd)
            pt = win32gui.ClientToScreen(hwnd, (0, 0))
            print(f"HWND {hwnd} '{txt}': WinRect={wr}, ClientRect={cr}, ClientToScreen={pt}")

win32gui.EnumWindows(cb, None)
