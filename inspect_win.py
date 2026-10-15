import win32gui

def cb(hwnd, extra):
    if win32gui.IsWindowVisible(hwnd):
        title = win32gui.GetWindowText(hwnd)
        if any(w in title for w in ['Conquer', 'Ancient', 'Ping', 'Game']):
            cr = win32gui.GetClientRect(hwnd)
            wr = win32gui.GetWindowRect(hwnd)
            print(f'Window: "{title}" | Client: {cr} | Window: {wr}')

win32gui.EnumWindows(cb, None)
