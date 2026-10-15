import cv2

img = cv2.imread("user_curr_screen.png", cv2.IMREAD_GRAYSCALE)
x, y = 348, 273
cCenter = img[y + 10, x + 17]
cCorner = img[y, x]
print(f"At ({x}, {y}): cCenter={cCenter}, cCorner={cCorner}")
