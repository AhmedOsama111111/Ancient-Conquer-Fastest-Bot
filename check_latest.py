import glob
import cv2
import os

user_files = glob.glob(r"C:\Users\OSOS\.gemini\antigravity-ide\brain\d903f874-f6b4-4ed1-9779-ed417a858fa6\.user_uploaded\*.*")
latest = sorted(user_files, key=os.path.getmtime, reverse=True)[0]
print(f"Latest file: {os.path.basename(latest)}")

img = cv2.imread(latest)
print("Latest image shape:", img.shape)
cv2.imwrite("latest_screen.png", img)
print("Saved latest_screen.png")
