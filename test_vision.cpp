#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>

static const int YES_TMPL_W = 38;
static const int YES_TMPL_H = 22;
static const uint8_t YES_TMPL_PIXELS[22][38] = {
    { 44, 44, 44, 45, 44, 43, 46, 45, 47, 46, 44, 45, 43, 45, 44, 42, 46, 44, 44, 44, 44, 43, 44, 45, 46, 43, 45, 43, 45, 43, 46, 41, 44, 44, 49, 44, 45, 45 },
    { 44, 44, 45, 45, 45, 49, 44, 45, 45, 46, 47, 44, 44, 45, 41, 44, 43, 43, 45, 42, 43, 44, 42, 43, 43, 45, 45, 42, 43, 44, 42, 46, 44, 44, 44, 46, 46, 45 },
    { 45, 45, 45, 45, 45, 45, 46, 48, 45, 44, 44, 43, 44, 41, 44, 44, 43, 43, 42, 44, 43, 44, 44, 44, 44, 44, 42, 46, 45, 44, 44, 45, 45, 44, 45, 45, 45, 44 },
    { 45, 45, 45, 45, 46, 45, 44, 46, 47, 45, 44, 44, 44, 42, 44, 43, 42, 43, 43, 44, 43, 42, 44, 42, 46, 44, 45, 44, 44, 44, 43, 44, 46, 44, 46, 46, 44, 43 },
    { 45, 45, 45, 45, 44, 46, 45, 44, 44, 43, 44, 26, 19, 18, 21, 21, 20, 20, 19, 17, 18, 17, 17, 20, 20, 19, 20, 15, 16, 45, 45, 45, 45, 46, 45, 42, 44, 43 },
    { 45, 45, 45, 45, 45, 43, 45, 45, 43, 44, 25, 126, 110, 110, 107, 107, 107, 107, 104, 104, 100, 97, 97, 95, 95, 96, 97, 101, 85, 28, 44, 44, 46, 45, 42, 43, 42, 42 },
    { 45, 45, 45, 45, 44, 44, 45, 43, 46, 46, 156, 61, 29, 138, 122, 123, 126, 127, 100, 100, 102, 101, 104, 75, 75, 60, 39, 26, 48, 65, 12, 44, 43, 43, 42, 43, 43, 43 },
    { 45, 45, 44, 44, 45, 46, 45, 44, 46, 44, 168, 53, 150, 69, 27, 27, 24, 23, 33, 36, 35, 11, 12, 19, 18, 29, 28, 30, 17, 143, 15, 42, 42, 44, 41, 43, 44, 43 },
    { 44, 45, 44, 44, 45, 45, 43, 45, 44, 42, 168, 55, 116, 45, 35, 42, 204, 127, 158, 139, 56, 50, 16, 19, 20, 25, 30, 43, 30, 143, 13, 43, 44, 40, 43, 42, 41, 43 },
    { 44, 46, 45, 45, 45, 44, 43, 43, 44, 41, 167, 55, 116, 56, 71, 70, 64, 108, 133, 33, 46, 74, 33, 52, 76, 49, 27, 44, 42, 139, 11, 43, 41, 42, 44, 43, 44, 43 },
    { 44, 45, 43, 43, 43, 42, 43, 44, 43, 43, 169, 58, 114, 86, 83, 81, 77, 140, 128, 46, 110, 172, 134, 83, 82, 77, 77, 57, 43, 139, 13, 42, 44, 44, 43, 42, 44, 42 },
    { 45, 45, 42, 42, 44, 41, 43, 42, 44, 43, 170, 58, 109, 93, 96, 93, 73, 169, 101, 119, 199, 195, 117, 200, 92, 94, 96, 102, 63, 142, 11, 43, 44, 43, 43, 45, 43, 45 },
    { 45, 44, 42, 42, 41, 43, 44, 44, 44, 43, 170, 59, 96, 97, 101, 95, 28, 168, 97, 61, 122, 96, 124, 96, 98, 106, 105, 105, 85, 140, 14, 45, 44, 45, 45, 44, 44, 44 },
    { 43, 42, 42, 42, 42, 43, 41, 43, 44, 44, 149, 68, 60, 95, 104, 103, 74, 121, 115, 96, 136, 141, 136, 146, 119, 115, 117, 100, 100, 130, 11, 43, 44, 44, 44, 46, 45, 45 },
    { 43, 42, 43, 43, 42, 41, 44, 45, 44, 44, 39, 129, 111, 124, 126, 126, 125, 125, 127, 127, 128, 127, 127, 125, 124, 138, 141, 129, 134, 57, 30, 46, 43, 45, 44, 43, 43, 41 },
    { 42, 43, 43, 41, 43, 44, 44, 45, 43, 44, 44, 37, 76, 77, 76, 76, 73, 70, 63, 58, 59, 58, 57, 55, 55, 54, 52, 51, 44, 28, 44, 44, 44, 42, 43, 42, 44, 39 },
    { 43, 42, 43, 43, 45, 45, 44, 43, 43, 42, 42, 43, 42, 43, 42, 42, 42, 44, 43, 43, 44, 43, 43, 44, 44, 43, 43, 45, 44, 44, 44, 42, 44, 44, 42, 44, 39, 41 },
    { 43, 42, 43, 44, 44, 43, 43, 43, 43, 42, 42, 45, 43, 44, 44, 45, 44, 43, 43, 43, 44, 43, 43, 43, 44, 44, 45, 45, 43, 47, 45, 44, 44, 43, 43, 42, 38, 43 },
    { 43, 43, 43, 43, 43, 41, 44, 44, 42, 44, 43, 42, 42, 42, 42, 44, 44, 43, 43, 43, 44, 44, 43, 43, 44, 44, 44, 43, 46, 45, 45, 43, 42, 45, 45, 39, 40, 44 },
    { 44, 44, 44, 43, 45, 42, 44, 43, 41, 43, 43, 42, 44, 43, 42, 43, 43, 42, 42, 43, 43, 43, 44, 45, 44, 42, 43, 45, 43, 44, 44, 47, 44, 45, 39, 40, 41, 44 },
    { 43, 44, 44, 44, 44, 43, 43, 43, 43, 42, 42, 44, 41, 41, 42, 43, 43, 43, 42, 43, 43, 43, 44, 44, 44, 43, 43, 44, 43, 44, 43, 44, 44, 42, 42, 43, 40, 43 },
    { 44, 43, 43, 44, 42, 44, 43, 44, 44, 42, 42, 43, 43, 43, 42, 43, 42, 43, 42, 42, 44, 44, 44, 43, 44, 45, 44, 42, 42, 44, 45, 41, 46, 45, 46, 37, 40, 43 },
};

static float s_normTmpl[YES_TMPL_H][YES_TMPL_W];
static void InitTemplate() {
    const int N = YES_TMPL_W * YES_TMPL_H;
    float sum = 0.0f;
    for (int y = 0; y < YES_TMPL_H; y++) {
        for (int x = 0; x < YES_TMPL_W; x++) sum += YES_TMPL_PIXELS[y][x];
    }
    float mean = sum / N;
    float var = 0.0f;
    for (int y = 0; y < YES_TMPL_H; y++) {
        for (int x = 0; x < YES_TMPL_W; x++) {
            float d = YES_TMPL_PIXELS[y][x] - mean;
            var += d * d;
        }
    }
    float stdVal = std::sqrt(var);
    if (stdVal < 1.0f) stdVal = 1.0f;
    for (int y = 0; y < YES_TMPL_H; y++) {
        for (int x = 0; x < YES_TMPL_W; x++) {
            s_normTmpl[y][x] = (YES_TMPL_PIXELS[y][x] - mean) / stdVal;
        }
    }
}

HWND FindGameWindow() {
    HWND found = NULL;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h)) return TRUE;
        RECT rc; GetClientRect(h, &rc);
        if (rc.right >= 400 && rc.bottom >= 300) {
            char title[256] = {0};
            GetWindowTextA(h, title, 256);
            if (strstr(title, "Osos") || strchr(title, '(') || strstr(title, "Conquer")) {
                *(HWND*)lp = h;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&found);
    return found;
}

int main() {
    InitTemplate();

    HWND hwnd = FindGameWindow();
    if (!hwnd) {
        std::cout << "[!] Conquer game window not found!\n";
        return 1;
    }

    char title[256] = {0};
    GetWindowTextA(hwnd, title, 256);
    RECT rc; GetClientRect(hwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    POINT ptClient = { 0, 0 };
    ClientToScreen(hwnd, &ptClient);

    std::cout << "[+] Found Window: HWND=" << hwnd << " Title='" << title << "' Client=" << winW << "x" << winH 
              << " ScreenPos=(" << ptClient.x << ", " << ptClient.y << ")\n";

    // Capture using GetDC(NULL) [Desktop DWM]
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, winW, winH);
    SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, winW, winH, hdcScreen, ptClient.x, ptClient.y, SRCCOPY);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(winW * winH);
    GetDIBits(hdcMem, hBmp, 0, winH, pixels.data(), &bmi, DIB_RGB_COLORS);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    std::vector<uint8_t> gray(winW * winH);
    for (int i = 0; i < winW * winH; i++) {
        uint32_t c = pixels[i];
        uint8_t b = (uint8_t)(c & 0xFF);
        uint8_t g = (uint8_t)((c >> 8) & 0xFF);
        uint8_t r = (uint8_t)((c >> 16) & 0xFF);
        gray[i] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
    }

    int yStart = (int)(winH * 0.20);
    int yEnd   = (int)(winH * 0.70);
    int xStart = (int)(winW * 0.15);
    int xEnd   = (int)(winW * 0.80);

    float bestScore = -1.0f;
    int bestX = 0, bestY = 0;

    for (int y = yStart; y <= yEnd - YES_TMPL_H; y++) {
        for (int x = xStart; x <= xEnd - YES_TMPL_W; x++) {
            const int N = YES_TMPL_W * YES_TMPL_H;
            float pSum = 0.0f;
            for (int ty = 0; ty < YES_TMPL_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < YES_TMPL_W; tx++) pSum += row[tx];
            }
            float pMean = pSum / N;
            float pVar = 0.0f;
            for (int ty = 0; ty < YES_TMPL_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < YES_TMPL_W; tx++) {
                    float d = row[tx] - pMean;
                    pVar += d * d;
                }
            }
            float pStd = std::sqrt(pVar);
            if (pStd < 1.0f) continue;

            float dot = 0.0f;
            for (int ty = 0; ty < YES_TMPL_H; ty++) {
                const uint8_t* row = &gray[(y + ty) * winW + x];
                for (int tx = 0; tx < YES_TMPL_W; tx++) {
                    dot += s_normTmpl[ty][tx] * (row[tx] - pMean);
                }
            }
            float score = dot / pStd;
            if (score > bestScore) {
                bestScore = score;
                bestX = x;
                bestY = y;
            }
        }
    }

    std::cout << "[*] Vision Scan Complete:\n";
    std::cout << "    Best Score: " << bestScore << "\n";
    std::cout << "    Best Location: (" << bestX << ", " << bestY << ")\n";
    std::cout << "    Center of [Yes] in Client: (" << bestX + YES_TMPL_W / 2 << ", " << bestY + YES_TMPL_H / 2 << ")\n";
    std::cout << "    Center of [Yes] on Screen: (" << ptClient.x + bestX + YES_TMPL_W / 2 << ", " << ptClient.y + bestY + YES_TMPL_H / 2 << ")\n";

    if (bestScore >= 0.80f) {
        std::cout << "[+] SUCCESS! Security dialog [Yes] button DETECTED with confidence " << (bestScore * 100.0f) << "%!\n";
    } else {
        std::cout << "[-] Dialog not currently on screen (score < 0.80).\n";
    }

    return 0;
}
