import cv2
import numpy as np

img = cv2.imread("latest_screen.png")
h, w, _ = img.shape
print(f"Image dimensions: {w}x{h}")

# Check the red border pixels!
# Conquer's captcha alert covers the screen border with bright red!
# Let's check top border, left border, right border:
top_strip = img[10:40, 200:800] # Top region
# In BGR: Red has high R, low G, low B
b, g, r = top_strip[:,:,0], top_strip[:,:,1], top_strip[:,:,2]
red_mask = (r > 150) & (g < 60) & (b < 60)
red_count = np.sum(red_mask)
print("Top strip red pixels count:", red_count)

# Let's check normal gameplay screen without captcha (e.g. sec_00.jpg or demo_30.jpg)
for normal_f in ["demo_30.jpg", "sec_00.jpg", "user_curr_screen.png"]:
    n_img = cv2.imread(normal_f)
    if n_img is None: continue
    nh, nw, _ = n_img.shape
    scale_y = nh / 576.0
    scale_x = nw / 1024.0
    n_strip = n_img[int(10*scale_y):int(40*scale_y), int(200*scale_x):int(800*scale_x)]
    nb, ng, nr = n_strip[:,:,0], n_strip[:,:,1], n_strip[:,:,2]
    n_red = np.sum((nr > 150) & (ng < 60) & (nb < 60))
    print(f"{normal_f} red pixels: {n_red}")
