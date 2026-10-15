import cv2
import numpy as np

def get_char_slices(mask_row):
    col_has = mask_row.any(axis=0)
    chars = []
    in_c = False
    st = 0
    for i, h in enumerate(col_has):
        if h and not in_c:
            in_c = True
            st = i
        elif not h and in_c:
            in_c = False
            chars.append((st, i))
    if in_c: chars.append((st, len(col_has)))
    return chars

# Let's inspect sec_02, sec_10, sec_20, sec_30
cap = cv2.VideoCapture(r'C:\Users\OSOS\Documents\Bandicam\bandicam 2026-10-05 19-05-26-394.mp4')
fps = cap.get(cv2.CAP_PROP_FPS)

# Known coordinates from inspecting frames:
# sec 2: (622, 729)
# sec 10: (652, 687)
# sec 20: (796, 695)
# sec 30: (722, 654)
# sec 45: (620, 613)
# sec 50: (616, 601)

templates = {}

samples = [
    (2, "622", "729"),
    (10, "652", "687"),
    (20, "796", "695"),
    (30, "722", "654"),
    (45, "620", "613"),
    (50, "616", "601")
]

for s, sx, sy in samples:
    cap.set(cv2.CAP_PROP_POS_FRAMES, int(s * fps))
    ret, frame = cap.read()
    if not ret: continue
    mask = (frame[5:14, 0:150, 1] > 200) & (frame[5:14, 0:150, 2] > 200)
    slices = get_char_slices(mask)
    
    # Find comma (w == 1) between index 5 and 15
    comma_idx = -1
    for i, (st, en) in enumerate(slices):
        if en - st == 1 and 60 < st < 100:
            comma_idx = i
            break
            
    if comma_idx != -1 and comma_idx >= 3 and comma_idx + 3 < len(slices):
        # 3 digits before comma
        d1 = mask[:, slices[comma_idx-3][0]:slices[comma_idx-3][1]]
        d2 = mask[:, slices[comma_idx-2][0]:slices[comma_idx-2][1]]
        d3 = mask[:, slices[comma_idx-1][0]:slices[comma_idx-1][1]]
        templates[sx[0]] = d1.astype(np.uint8)
        templates[sx[1]] = d2.astype(np.uint8)
        templates[sx[2]] = d3.astype(np.uint8)
        
        # 3 digits after comma
        d4 = mask[:, slices[comma_idx+1][0]:slices[comma_idx+1][1]]
        d5 = mask[:, slices[comma_idx+2][0]:slices[comma_idx+2][1]]
        d6 = mask[:, slices[comma_idx+3][0]:slices[comma_idx+3][1]]
        templates[sy[0]] = d4.astype(np.uint8)
        templates[sy[1]] = d5.astype(np.uint8)
        templates[sy[2]] = d6.astype(np.uint8)

print("Extracted templates for digits:", sorted(templates.keys()))

def ocr_digit(patch):
    best_c = '?'
    best_diff = 999
    ph, pw = patch.shape
    for ch, t in templates.items():
        th, tw = t.shape
        w = min(pw, tw)
        diff = np.sum(patch[:, :w] != t[:, :w]) + abs(pw - tw) * 3
        if diff < best_diff:
            best_diff = diff
            best_c = ch
    return best_c

# Now decode second by second!
total_s = int(cap.get(cv2.CAP_PROP_FRAME_COUNT) / fps)
print(f"\nDecoding all {total_s} seconds from video:")

coords_history = []

for s in range(1, total_s):
    cap.set(cv2.CAP_PROP_POS_FRAMES, int(s * fps))
    ret, frame = cap.read()
    if not ret: break
    
    mask = (frame[5:14, 0:150, 1] > 200) & (frame[5:14, 0:150, 2] > 200)
    slices = get_char_slices(mask)
    
    comma_idx = -1
    for i, (st, en) in enumerate(slices):
        if en - st == 1 and 60 < st < 100:
            comma_idx = i
            break
            
    if comma_idx >= 3 and comma_idx + 3 < len(slices):
        sx = "".join([ocr_digit(mask[:, slices[k][0]:slices[k][1]]) for k in range(comma_idx-3, comma_idx)])
        sy = "".join([ocr_digit(mask[:, slices[k][0]:slices[k][1]]) for k in range(comma_idx+1, comma_idx+4)])
        try:
            ix = int(sx)
            iy = int(sy)
            if 400 < ix < 1100 and 400 < iy < 1100:
                coords_history.append((s, ix, iy))
        except:
            pass

cap.release()

print(f"\nSuccessfully decoded {len(coords_history)} seconds of movement!")

# Filter into distinct waypoints (when moved > 8 tiles)
patrol_waypoints = []
last_p = None
for s, x, y in coords_history:
    if last_p is None:
        patrol_waypoints.append((x, y, s))
        last_p = (x, y)
    else:
        dist = np.hypot(x - last_p[0], y - last_p[1])
        if dist >= 9.0:
            patrol_waypoints.append((x, y, s))
            last_p = (x, y)

print(f"\nExtracted {len(patrol_waypoints)} Core Waypoints:")
for i, (x, y, s) in enumerate(patrol_waypoints):
    print(f"  WP #{i+1:02d}: ({x}, {y}) at sec {s:02d}")
