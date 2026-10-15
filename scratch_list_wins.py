import win32gui

def enum_windows_callback(hwnd, extra):
    if win32gui.IsWindowVisible(hwnd):
        title = win32gui.GetWindowText(hwnd)
        if title:
            print(f"{hwnd}: {title}")

win32gui.EnumWindows(enum_windows_callback, None)
