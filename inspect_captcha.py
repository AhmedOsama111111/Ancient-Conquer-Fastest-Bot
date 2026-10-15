import win32gui

def enum_cb(hwnd, results):
    if win32gui.IsWindowVisible(hwnd):
        title = win32gui.GetWindowText(hwnd)
        rect = win32gui.GetWindowRect(hwnd)
        w = rect[2] - rect[0]
        h = rect[3] - rect[1]
        if title and w > 100 and h > 100:
            print(f'{hwnd}: "{title}" {rect}')

win32gui.EnumWindows(enum_cb, None)
