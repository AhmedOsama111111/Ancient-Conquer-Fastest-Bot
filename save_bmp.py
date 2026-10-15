import cv2

img = cv2.imread("latest_screen.png")
# Save as standard 24/32-bit uncompressed BMP
cv2.imwrite("test_screen.bmp", img)
print("Saved test_screen.bmp, shape:", img.shape)
