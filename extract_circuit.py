import cv2
import numpy as np

# Load reference frames to extract all 10 digit templates
ref_files = [
    r'C:\Users\OSOS\Documents\New folder\ConquerBot\sec_02.jpg',
    r'C:\Users\OSOS\Documents\New folder\ConquerBot\sec_10.jpg',
    r'C:\Users\OSOS\Documents\New folder\ConquerBot\sec_20.jpg',
    r'C:\Users\OSOS\Documents\New folder\ConquerBot\sec_30.jpg'
]

# We will define the exact 9x6 pixel font bitmap for digits 0-9 in Conquer Online:
# Digits are 9 rows (y=5..13) and 6 columns.
# Let's extract them from crop_1080p.png (sec_02: "[Ancient] (622,729) Ping: 0048; Fps: 0820; 10/05/2026 Monday 19:05:29;")
img = cv2.imread(ref_files[0])
mask2 = (img[5:14, 0:350, 1] > 200) & (img[5:14, 0:350, 2] > 200)

digits = {}
digits['0'] = mask2[:, 142:148].astype(np.uint8)
digits['1'] = mask2[:, 235:239].astype(np.uint8) # 4 cols
digits['2'] = mask2[:, 75:81].astype(np.uint8)
digits['4'] = mask2[:, 158:164].astype(np.uint8)
digits['6'] = mask2[:, 67:73].astype(np.uint8)
digits['7'] = mask2[:, 94:100].astype(np.uint8)
digits['8'] = mask2[:, 166:172].astype(np.uint8)
digits['9'] = mask2[:, 110:116].astype(np.uint8)

# sec_10 has 5 (652,687):
img10 = cv2.imread(ref_files[1])
mask10 = (img10[5:14, 0:350, 1] > 200) & (img10[5:14, 0:350, 2] > 200)
digits['5'] = mask10[:, 75:81].astype(np.uint8)

# sec_20 has 3 in 131: "[Ancient] (796,695)... 131"
img20 = cv2.imread(ref_files[2])
mask20 = (img20[5:14, 0:350, 1] > 200) & (img20[5:14, 0:350, 2] > 200)
# '3' is at x=110:116 in Monday 19:05:46 or date '05'
# Let's inspect where '3' appears
for x in range(60, 280):
    # Match against a known '3' pattern
    # 3 in Conquer 9x6 font:
    # ###.
    # ...#
    # ...#
    # ..##
    # .##.
    # ...#
    # ...#
    # ...#
    # ###.
    p = mask20[:, x:x+6].astype(np.uint8)
    if p[0, 1] and p[0, 2] and p[8, 1] and p[8, 2] and p[4, 2] and not p[4, 0] and not p[6, 0]:
        digits['3'] = p
        break

print(f"Loaded {len(digits)} digits: {sorted(digits.keys())}")

def match_digit(patch):
    best_d = None
    best_score = -1
    for d, templ in digits.items():
        tw = templ.shape[1]
        pw = patch.shape[1]
        w = min(tw, pw)
        score = np.sum(patch[:, :w] == templ[:, :w])
        if score > best_score:
            best_score = score
            best_d = d
    return best_d, best_score

# Open the video and read coordinates frame by frame (or 1 sec interval)
cap = cv2.VideoCapture(r'C:\Users\OSOS\Documents\Bandicam\bandicam 2026-10-05 19-05-26-394.mp4')
fps = cap.get(cv2.CAP_PROP_FPS)
total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

waypoints = []
last_coord = None

print("\nExtracting circuit waypoints from video...")

for s in range(2, int(total_frames / fps)):
    cap.set(cv2.CAP_PROP_POS_FRAMES, int(s * fps))
    ret, frame = cap.read()
    if not ret: break

    # Extract title mask
    m = (frame[5:14, 60:130, 1] > 200) & (frame[5:14, 60:130, 2] > 200)

    # Find open '(' and close ')'
    # '(' is around x=62..65 relative to frame (x=2..5 relative to crop)
    # Digits are 6px apart
    # Let's scan for digits
    # Character x positions:
    # d1 at 67, d2 at 75, d3 at 83, comma at 90, d4 at 94, d5 at 102, d6 at 110
    # Relative to x=60:
    x_positions = [67-60, 75-60, 83-60, 94-60, 102-60, 110-60]
    res_str = ""
    for xp in x_positions:
        p = m[:, xp:xp+6].astype(np.uint8)
        d, score = match_digit(p)
        res_str += str(d)
    
    x_coord = int(res_str[:3])
    y_coord = int(res_str[3:])

    if 400 < x_coord < 1200 and 400 < y_coord < 1200:
        if last_coord is None or (abs(x_coord - last_coord[0]) + abs(y_coord - last_coord[1])) >= 6:
            waypoints.append((s, x_coord, y_coord))
            last_coord = (x_coord, y_coord)
            print(f"  [{s:02d}s] Waypoint #{len(waypoints):02d}: ({x_coord}, {y_coord})")

cap.release()
print(f"\n[+] Total unique waypoints extracted: {len(waypoints)}")
