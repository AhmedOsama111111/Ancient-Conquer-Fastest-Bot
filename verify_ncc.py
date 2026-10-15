import cv2
import numpy as np

img = cv2.imread("user_curr_screen.png", cv2.IMREAD_GRAYSCALE)
h, w = img.shape

# YES_TMPL_PIXELS from BotEngine.cpp
tmpl = cv2.imread("yes_button.png", cv2.IMREAD_GRAYSCALE)
th, tw = tmpl.shape
print(f"Template size: {tw}x{th}")

# Normalize template
t_mean = np.mean(tmpl)
t_std = np.std(tmpl)
norm_tmpl = (tmpl - t_mean) / t_std

best_score = -1
best_loc = (0, 0)

for y in range(int(h * 0.2), int(h * 0.7)):
    for x in range(int(w * 0.2), int(w * 0.7)):
        patch = img[y:y+th, x:x+tw]
        p_std = np.std(patch)
        if p_std < 1.0: continue
        p_norm = (patch - np.mean(patch)) / p_std
        score = np.sum(norm_tmpl * p_norm) / (tw * th)
        if score > best_score:
            best_score = score
            best_loc = (x, y)

print(f"NCC Best Score: {best_score:.4f} at {best_loc}, center = ({best_loc[0] + tw//2}, {best_loc[1] + th//2})")
