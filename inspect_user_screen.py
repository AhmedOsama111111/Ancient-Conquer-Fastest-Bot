import cv2
import numpy as np

img = cv2.imread(r"C:\Users\OSOS\.gemini\antigravity-ide\brain\d903f874-f6b4-4ed1-9779-ed417a858fa6\.user_uploaded\media_1791235139721.jpg")
print("User screenshot shape:", img.shape)

# Let's search for "Solve Security Check" or dialog box in this screenshot
# Crop around where the dialog appears:
# In 1024x576, dialog is in the center
cv2.imwrite("user_curr_screen.png", img)
print("Saved user_curr_screen.png")

# Let's test template matching with yes_button.png and dialog_template.png
for tmpl_name in ["yes_button.png", "dialog_template.png"]:
    tmpl = cv2.imread(tmpl_name)
    if tmpl is None: continue
    res = cv2.matchTemplate(img, tmpl, cv2.TM_CCOEFF_NORMED)
    min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(res)
    print(f"{tmpl_name}: max_score = {max_val:.4f} at {max_loc}, center = ({max_loc[0] + tmpl.shape[1]//2}, {max_loc[1] + tmpl.shape[0]//2})")
