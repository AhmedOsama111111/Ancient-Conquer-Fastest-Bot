import cv2
import numpy as np
import glob

print("Testing Red Alert Vignette Detector across all user screenshots:")
for f in sorted(glob.glob("sec_*.jpg") + glob.glob("demo_*.jpg") + ["latest_screen.png", "user_curr_screen.png"]):
    img = cv2.imread(f)
    if img is None: continue
    h, w, _ = img.shape
    
    # Check top 8% of screen (y from 0 to int(h*0.08))
    # and bottom 8% (y from int(h*0.92) to h)
    top_strip = img[0:int(h*0.08), int(w*0.15):int(w*0.85)]
    bottom_strip = img[int(h*0.92):h, int(w*0.15):int(w*0.85)]
    
    # Red in BGR: R > 150, G < 70, B < 70
    def count_red(strip):
        b, g, r = strip[:,:,0], strip[:,:,1], strip[:,:,2]
        return np.sum((r > 150) & (g < 70) & (b < 70))
        
    red_total = count_red(top_strip) + count_red(bottom_strip)
    print(f"  {f} ({w}x{h}): red_pixels = {red_total}")
