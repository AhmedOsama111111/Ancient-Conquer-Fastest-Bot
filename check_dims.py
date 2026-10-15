import cv2
import glob

for f in sorted(glob.glob("sec_*.jpg") + glob.glob("demo_*.jpg")):
    img = cv2.imread(f)
    if img is not None:
        print(f"{f}: shape = {img.shape}")
