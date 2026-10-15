import cv2
import glob
import os
import numpy as np

frames = sorted(glob.glob(r'E:\Desktop\f\ffmpeg-9.0.2-essentials_build\ffmpeg-9.0.2-essentials_build\bin\bandicam 2026-10-05 19-05-26-394_frames\*.jpg'))

print(f"Total frames: {len(frames)}")

# Save cropped titles of all frames in a folder
out_dir = r'C:\Users\OSOS\Documents\New folder\ConquerBot\title_crops'
os.makedirs(out_dir, exist_ok=True)

for idx, fpath in enumerate(frames):
    img = cv2.imread(fpath)
    if img is None: continue
    # Title is in top-left: y from 0 to 20, x from 50 to 140
    crop = img[0:18, 45:130]
    out_path = os.path.join(out_dir, f"crop_{idx+1:04d}.png")
    cv2.imwrite(out_path, crop)

print(f"Saved {len(frames)} title crops to {out_dir}")
