import cv2
import numpy as np
import glob
import os

tmpl = cv2.imread("yes_button.png", cv2.IMREAD_GRAYSCALE)
th, tw = tmpl.shape
print(f"Template shape: {tw}x{th}")

for f in sorted(glob.glob("sec_*.jpg") + glob.glob("demo_*.jpg")):
    img = cv2.imread(f, cv2.IMREAD_GRAYSCALE)
    if img is None: continue
    res = cv2.matchTemplate(img, tmpl, cv2.TM_CCOEFF_NORMED)
    min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(res)
    if max_val > 0.6:
        print(f"{f}: max_score = {max_val:.4f} at ({max_loc[0]}, {max_loc[1]}), center = ({max_loc[0] + tw//2}, {max_loc[1] + th//2})")
