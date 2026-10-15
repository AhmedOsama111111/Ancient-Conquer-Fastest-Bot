import ctypes

user32 = ctypes.windll.user32
try:
    shcore = ctypes.windll.shcore
    shcore.SetProcessDpiAwareness(2) # Per-monitor DPI aware
except:
    pass

w = user32.GetSystemMetrics(0)
h = user32.GetSystemMetrics(1)
vw = user32.GetSystemMetrics(78)
vh = user32.GetSystemMetrics(79)
vx = user32.GetSystemMetrics(76)
vy = user32.GetSystemMetrics(77)

print(f"Primary: {w}x{h}, Virtual: {vw}x{vh} at ({vx}, {vy})")
try:
    dpi = user32.GetDpiForSystem()
    print(f"System DPI: {dpi} (Scale: {dpi / 96.0 * 100:.1f}%)")
except:
    pass
