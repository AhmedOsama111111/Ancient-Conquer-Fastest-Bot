import cv2

img = cv2.imread("latest_screen.png")
for tmpl_name in ["yes_button.png", "dialog_template.png"]:
    tmpl = cv2.imread(tmpl_name)
    res = cv2.matchTemplate(img, tmpl, cv2.TM_CCOEFF_NORMED)
    min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(res)
    print(f"{tmpl_name}: max_score = {max_val:.4f} at {max_loc}, center = ({max_loc[0] + tmpl.shape[1]//2}, {max_loc[1] + tmpl.shape[0]//2})")
