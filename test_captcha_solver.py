import numpy as np
from PIL import Image

def solve_captcha_image(img_path):
    im = Image.open(img_path).convert('RGB')
    arr = np.array(im)
    H, W, _ = arr.shape
    print(f"\n[*] Processing '{img_path}' ({W}x{H})...")

    r = arr[:, :, 0].astype(int)
    g = arr[:, :, 1].astype(int)
    b = arr[:, :, 2].astype(int)

    # 1. Detect Slider Knob (Tan/Beige circle)
    # Color: R: 160..235, G: 135..210, B: 90..165, R > G > B, R - B >= 35
    knob_mask = ((r >= 160) & (r <= 235) & 
                 (g >= 135) & (g <= 210) & 
                 (b >= 90) & (b <= 165) & 
                 (r > g) & (g > b) & 
                 ((r - b) >= 35))

    # Look for the knob in central area of screen (X: 20%..80%, Y: 30%..85%)
    y_min, y_max = int(H * 0.30), int(H * 0.85)
    x_min, x_max = int(W * 0.20), int(W * 0.80)
    
    sub_mask = knob_mask[y_min:y_max, x_min:x_max]
    sub_ys, sub_xs = np.where(sub_mask)
    if len(sub_xs) < 20:
        print("[!] Slider knob not found in candidate region!")
        return None

    # Group into connected blob or find dense cluster
    knob_global_xs = sub_xs + x_min
    knob_global_ys = sub_ys + y_min
    
    # Knob is the leftmost cluster if multiple exist (knob starts on left of slider)
    knob_cx = int(np.median(knob_global_xs))
    knob_cy = int(np.median(knob_global_ys))
    print(f"[+] Detected Slider Knob Center: ({knob_cx}, {knob_cy})")

    # 2. Detect Puzzle Canvas
    # Puzzle canvas is located directly above the knob (typically 40px to 250px above knob)
    p_top = max(0, knob_cy - int(H * 0.35))
    p_bot = knob_cy - int(H * 0.04)
    p_left = max(0, knob_cx - int(W * 0.10))
    p_right = min(W, knob_cx + int(W * 0.35))
    
    puzzle_area = arr[p_top:p_bot, p_left:p_right]
    pr, pg, pb = puzzle_area[:, :, 0], puzzle_area[:, :, 1], puzzle_area[:, :, 2]

    # Find the puzzle image bounds by detecting colorful pixels
    # Colors: Yellow (r>180, g>140, b<110), Purple (r>100, b>160), Green (g>140, b<120), Red (r>180, g<100)
    color_mask = (((pr > 180) & (pg > 140) & (pb < 110)) | 
                  ((pr > 90) & (pg < 130) & (pb > 150)) | 
                  ((pr < 110) & (pg > 140) & (pb < 130)) | 
                  ((pr > 180) & (pg < 110) & (pb < 110)))
    
    c_ys, c_xs = np.where(color_mask)
    if len(c_xs) < 100:
        print("[!] Puzzle colorful area not found!")
        return None

    puzzle_abs_x1 = p_left + int(np.percentile(c_xs, 1))
    puzzle_abs_x2 = p_left + int(np.percentile(c_xs, 99))
    puzzle_abs_y1 = p_top + int(np.percentile(c_ys, 1))
    puzzle_abs_y2 = p_top + int(np.percentile(c_ys, 99))
    puzzle_w = puzzle_abs_x2 - puzzle_abs_x1
    puzzle_h = puzzle_abs_y2 - puzzle_abs_y1
    print(f"[+] Detected Puzzle Area: [{puzzle_abs_x1}, {puzzle_abs_y1}] to [{puzzle_abs_x2}, {puzzle_abs_y2}] (size: {puzzle_w}x{puzzle_h})")

    # 3. Detect Slots in the Puzzle Area
    # In the middle horizontal strip of the puzzle (between 30% and 75% height)
    strip_y1 = puzzle_abs_y1 + int(puzzle_h * 0.30)
    strip_y2 = puzzle_abs_y1 + int(puzzle_h * 0.75)
    strip = arr[strip_y1:strip_y2, puzzle_abs_x1:puzzle_abs_x2]
    
    # Dark pixels inside the strip (< 85, < 85, < 90)
    sr, sg, sb = strip[:, :, 0], strip[:, :, 1], strip[:, :, 2]
    dark_mask = (sr < 85) & (sg < 85) & (sb < 90)
    
    # Histogram of dark pixels per column
    dark_hist = np.sum(dark_mask, axis=0)
    
    # Find slot peaks
    min_slot_w = int(puzzle_w * 0.05)
    max_slot_w = int(puzzle_w * 0.20)
    thresh = int((strip_y2 - strip_y1) * 0.25)
    
    slots = []
    in_slot = False
    s_start = 0
    for x in range(len(dark_hist)):
        if dark_hist[x] >= thresh:
            if not in_slot:
                in_slot = True
                s_start = x
        else:
            if in_slot:
                in_slot = False
                s_end = x
                if min_slot_w <= (s_end - s_start) <= max_slot_w:
                    slot_center_local = (s_start + s_end) // 2
                    slot_center_global = puzzle_abs_x1 + slot_center_local
                    slots.append(slot_center_global)

    print(f"[+] Detected {len(slots)} target slot(s): {slots}")

    # 4. Movable piece starting X is at the left edge of the puzzle
    piece_start_x = puzzle_abs_x1 + int(puzzle_w * 0.06)
    print(f"[+] Movable piece starting X: {piece_start_x}")

    # 5. Compute required drag deltas
    drag_targets = []
    for idx, sx in enumerate(slots, 1):
        delta = sx - piece_start_x
        release_x = knob_cx + delta
        print(f"    -> Slot #{idx}: delta = +{delta} px | Drag from {knob_cx} to {release_x}")
        drag_targets.append({
            'slot_idx': idx,
            'slot_x': sx,
            'delta_x': delta,
            'knob_x': knob_cx,
            'knob_y': knob_cy,
            'release_x': release_x
        })

    # Also detect the "NEW PUZZLE" button directly below the slider
    btn_y = knob_cy + int(H * 0.07)
    btn_x = knob_cx + int(puzzle_w * 0.45)
    print(f"[+] 'NEW PUZZLE' button estimated at: ({btn_x}, {btn_y})")

    return {
        'knob': (knob_cx, knob_cy),
        'puzzle': (puzzle_abs_x1, puzzle_abs_y1, puzzle_w, puzzle_h),
        'slots': slots,
        'drag_targets': drag_targets,
        'new_puzzle_btn': (btn_x, btn_y)
    }

if __name__ == '__main__':
    solve_captcha_image(r'C:\Users\OSOS\.gemini\antigravity-ide\brain\d903f874-f6b4-4ed1-9779-ed417a858fa6\.user_uploaded\media_1791240563213.png')
