import glob
import cv2
import os

user_files = glob.glob(r"C:\Users\OSOS\.gemini\antigravity-ide\brain\d903f874-f6b4-4ed1-9779-ed417a858fa6\.user_uploaded\*.*")
print("User uploaded files count:", len(user_files))
for f in sorted(user_files, key=os.path.getmtime, reverse=True)[:5]:
    img = cv2.imread(f)
    shape = img.shape if img is not None else "None"
    print(f"{os.path.basename(f)}: size={os.path.getsize(f)}, shape={shape}")
