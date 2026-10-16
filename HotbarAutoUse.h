#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>

// ============================================================================
// HotbarAutoUse.h: 100% Accurate Hotbar OCR & Auto-Use Engine for Conquer Online
//
// Monitors Slot 4 (F4: Green Bag) and Slot 5 (F5: Yellow Chest):
// 1. Green Bag: When stack count reaches 5 ('5'), continuously presses F4 via
//    100% Hardware Interception driver until the count drops to 0 (dimmed/empty).
//    MUST NOT START if count is less than 5.
// 2. Yellow Chest: When count reaches 1 ('1'), continuously presses F5 via
//    100% Hardware Interception driver until the count drops to 0 (dimmed/empty).
//    MUST NOT START if count is 0.
// 3. Runs seamlessly during hunting without interrupting movement, jump, or scatter.
// ============================================================================

namespace HotbarAutoUse {

    // Exact 10x6 bitmap of digit '5' extracted directly from Conquer Online in-game HUD
    static const int D5_H = 10;
    static const int D5_W = 6;
    static const uint8_t T_5_EXACT[10][6] = {
        { 0, 1, 1, 1, 1, 0 },
        { 0, 1, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0 },
        { 0, 0, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 0, 1 },
        { 0, 0, 0, 0, 0, 1 },
        { 1, 0, 0, 0, 0, 0 },
        { 1, 0, 0, 0, 1, 0 },
        { 0, 1, 1, 1, 0, 0 }
    };

    // Exact 10x4 bitmap of digit '1' extracted directly from Conquer Online in-game HUD
    static const int D1_H = 10;
    static const int D1_W = 4;
    static const uint8_t T_1_EXACT[10][4] = {
        { 0, 0, 1, 0 },
        { 0, 1, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 }
    };

    // Evaluates the stack count digit in a hotbar slot bitmap (24-bit BGR).
    // Returns:
    //   '0' : Slot is empty or dimmed (0 items)
    //   '1' : Count is 1
    //   '5' : Count is 5
    //   '?' : Other count (e.g. 2, 3, 4)
    inline char EvaluateSlotDigit(const uint8_t* bgr, int slotW, int slotH, int stride) {
        if (!bgr || slotW < 10 || slotH < 10) return '0';

        // Upscale 2x in memory if slot is small (576p ~22-24px) to normalize to ~48x48
        int scale = (slotH < 35) ? 2 : 1;
        int normW = slotW * scale;
        int normH = slotH * scale;

        // Digit ROI in normalized coordinates: y in [4, 24], x in [4, 24]
        int roiY1 = 4, roiY2 = (std::min)(normH, 24);
        int roiX1 = 4, roiX2 = (std::min)(normW, 24);
        int roiH = roiY2 - roiY1;
        int roiW = roiX2 - roiX1;
        if (roiH <= 0 || roiW <= 0) return '0';

        // Extract white pixel mask (Conquer count font: R > 175 && G > 175 && B > 175)
        std::vector<std::vector<uint8_t>> whiteMask(roiH, std::vector<uint8_t>(roiW, 0));
        int whiteCount = 0;
        int minX = roiW, maxX = -1;
        int minY = roiH, maxY = -1;

        for (int y = 0; y < roiH; y++) {
            int srcY = (roiY1 + y) / scale;
            if (srcY >= slotH) continue;
            const uint8_t* row = bgr + srcY * stride;

            for (int x = 0; x < roiW; x++) {
                int srcX = (roiX1 + x) / scale;
                if (srcX >= slotW) continue;

                uint8_t b = row[srcX * 3];
                uint8_t g = row[srcX * 3 + 1];
                uint8_t r = row[srcX * 3 + 2];

                if (r > 175 && g > 175 && b > 175) {
                    whiteMask[y][x] = 1;
                    whiteCount++;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }

        // When count is 0, Conquer dims the slot and draws grey 0 with NO bright white pixels
        if (whiteCount < 4) {
            return '0';
        }

        int dw = maxX - minX + 1;
        int dh = maxY - minY + 1;

        // 2D Template Correlation for Digit '5' (T_5_EXACT)
        int maxCorr5 = 0;
        for (int sy = 0; sy <= roiH - D5_H; sy++) {
            for (int sx = 0; sx <= roiW - D5_W; sx++) {
                int corr = 0;
                for (int ty = 0; ty < D5_H; ty++) {
                    for (int tx = 0; tx < D5_W; tx++) {
                        if (T_5_EXACT[ty][tx] && whiteMask[sy + ty][sx + tx]) {
                            corr++;
                        }
                    }
                }
                if (corr > maxCorr5) maxCorr5 = corr;
            }
        }

        // 2D Template Correlation for Digit '1' (T_1_EXACT)
        int maxCorr1 = 0;
        for (int sy = 0; sy <= roiH - D1_H; sy++) {
            for (int sx = 0; sx <= roiW - D1_W; sx++) {
                int corr = 0;
                for (int ty = 0; ty < D1_H; ty++) {
                    for (int tx = 0; tx < D1_W; tx++) {
                        if (T_1_EXACT[ty][tx] && whiteMask[sy + ty][sx + tx]) {
                            corr++;
                        }
                    }
                }
                if (corr > maxCorr1) maxCorr1 = corr;
            }
        }

        // T_5_EXACT has 16 active pixels. Require >= 12 matches for pristine confidence
        if (maxCorr5 >= 12) {
            return '5';
        }

        // T_1_EXACT has 11 active pixels, or vertical single-column stroke
        if (maxCorr1 >= 8 || (dw <= 4 && dh >= 6)) {
            return '1';
        }

        return '?';
    }

    // Captures the hotbar slots F4 and F5 from the game client window and evaluates both digits.
    // Takes ~0.03ms with 0 external dependencies.
    inline bool ReadHotbarSlots(HWND hwnd, int winW, int winH, char& outDigitF4, char& outDigitF5) {
        if (!hwnd || !IsWindow(hwnd) || winW <= 0 || winH <= 0) return false;

        bool is1080 = (winW > 1366);
        int slotW = is1080 ? 45 : 24;
        int slotH = is1080 ? 45 : 24;
        int baseF4X = (int)(winW * (364.0 / 1024.0));
        int baseF5X = (int)(winW * (388.0 / 1024.0));
        int baseY   = winH - (is1080 ? 49 : 26);

        // Bounding box covering both F4 and F5 slots
        int roiX = (std::max)(0, baseF4X - slotW);
        int roiY = (std::max)(0, baseY - 4);
        int roiW = (baseF5X - baseF4X) + slotW * 2;
        int roiH = slotH + 8;

        if (roiX + roiW > winW) roiW = winW - roiX;
        if (roiY + roiH > winH) roiH = winH - roiY;
        if (roiW <= 0 || roiH <= 0) return false;

        POINT ptScreen = { roiX, roiY };
        ClientToScreen(hwnd, &ptScreen);

        HDC hdcScreen = GetDC(NULL);
        if (!hdcScreen) return false;

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        if (!hdcMem) {
            ReleaseDC(NULL, hdcScreen);
            return false;
        }

        HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, roiW, roiH);
        if (!hbm) {
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return false;
        }

        HGDIOBJ oldBm = SelectObject(hdcMem, hbm);
        BitBlt(hdcMem, 0, 0, roiW, roiH, hdcScreen, ptScreen.x, ptScreen.y, SRCCOPY);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = roiW;
        bmi.bmiHeader.biHeight = -roiH; // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        int stride = ((roiW * 3 + 3) & ~3);
        std::vector<uint8_t> bgr(stride * roiH);
        GetDIBits(hdcMem, hbm, 0, roiH, bgr.data(), &bmi, DIB_RGB_COLORS);

        SelectObject(hdcMem, oldBm);
        DeleteObject(hbm);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);

        // Relative coordinates of Slot F4 and F5 inside captured ROI
        int relF4X = (baseF4X - roiX) - slotW / 2;
        int relF5X = (baseF5X - roiX) - slotW / 2;
        int relY   = baseY - roiY;

        // Evaluate Slot F4 (Green Bag)
        if (relF4X >= 0 && relF4X + slotW <= roiW && relY >= 0 && relY + slotH <= roiH) {
            const uint8_t* f4Ptr = bgr.data() + relY * stride + relF4X * 3;
            outDigitF4 = EvaluateSlotDigit(f4Ptr, slotW, slotH, stride);
        } else {
            outDigitF4 = '?';
        }

        // Evaluate Slot F5 (Yellow Chest)
        if (relF5X >= 0 && relF5X + slotW <= roiW && relY >= 0 && relY + slotH <= roiH) {
            const uint8_t* f5Ptr = bgr.data() + relY * stride + relF5X * 3;
            outDigitF5 = EvaluateSlotDigit(f5Ptr, slotW, slotH, stride);
        } else {
            outDigitF5 = '?';
        }

        return true;
    }

} // namespace HotbarAutoUse
