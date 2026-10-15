import win32gui

def callback(hwnd, extra):
    if win32gui.IsWindowVisible(hwnd):
        title = win32gui.GetWindowText(hwnd)
        if len(title) > 0:
            print(f"HWND: {hwnd}, Title: '{title}'")
    return True

win32gui.EnumWindows(callback, None)
