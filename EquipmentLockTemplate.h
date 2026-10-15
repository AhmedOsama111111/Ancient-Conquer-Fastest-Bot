// EquipmentLockTemplate.h - Embedded In-Game Equipment Lock Dialog & [Okay] Button Template
#pragma once
#include <windows.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

// Embedded 45x20 grayscale template of the [Okay] button in the Equipment Lock dialog
static const int OKAY_LOCK_W = 45;
static const int OKAY_LOCK_H = 20;
static const double OKAY_LOCK_MEAN = 98.510000;
static const double OKAY_LOCK_STD  = 59.428191;

static const unsigned char OKAY_LOCK_GRAY[900] = {
    133,137,130,125,144, 13, 16, 17, 14, 12, 14, 15, 19, 19, 17, 17, 17, 16, 20, 17, 16, 15, 17, 17, 15, 15, 15, 16, 16, 18, 22, 15, 21,123,140,133,134,122,134,134,133,130,128,125,120,
    135,136,130,131, 14,201,196,200,193,199,194,186,193,191,192,192,186,188,195,187,189,188,184,183,186,184,180,184,187,180,180,177, 72, 25,126,131,133,131,131,132,130,128,130,125,117,
    126,140,134, 17,199, 67, 73, 69, 71, 74, 63, 76, 69, 70, 69, 76, 72, 69, 74, 70, 70, 77, 65, 64, 62, 63, 66, 67, 69, 73, 76, 71, 74, 71, 14,117,132,125,128,130,127,126,130,124,114,
    127,125, 21,205, 74, 70, 16, 16, 17, 10, 16, 11, 22, 14, 17, 17, 23, 21, 11, 19, 13, 13, 15, 15, 15, 14, 12, 13, 13, 11, 17, 16, 66, 71, 52, 10,119,129,125,126,124,125,126,120,112,
    124,128, 21,225, 98, 24,  9, 78,216,220,202,199,195,200,210,210,203,208,185,185,186,187,164,162,173,166,145,152,153,122, 78, 23, 28, 75, 56, 28,122,127,123,123,122,123,123,116,111,
    129,125, 26,232, 85, 32, 78,224,230,224,201, 94, 42,149,208,153, 43, 36, 46, 51, 50, 48, 50, 50, 47, 46, 44, 40, 36, 37, 77, 21, 31,160, 58, 18,121,119,122,121,122,123,120,114,111,
    125,127, 26,237, 93, 97,217,223, 80, 78,195,197, 30, 34,156, 88, 39, 37, 47, 45, 42, 43, 48, 49, 51, 49, 39, 39, 39, 41, 32, 27, 28,179, 48, 18,122,123,124,123,121,120,118,114,110,
    124,120, 25,233, 91,163,212, 13, 16, 11, 48,200, 80, 37,160,151, 27, 41, 46, 49, 45, 50, 44, 84, 47, 53, 34, 42, 36, 45, 26, 34, 28,174, 58, 20,118,123,120,119,118,117,115,112,108,
    115,117, 27,230, 94,229,132, 18, 52, 51, 75,180,182, 27,139,140, 36,194,195,206, 76,141,206,204,203, 53,100,217,217,102,144,213,143,150, 62, 18,118,118,117,116,115,114,112,109,105,
    114,119, 25,230, 93,227, 59, 11, 55, 54, 74,131,171, 30,142,143, 40,190,147, 15, 18,200, 49, 53,205,105, 46,155,164, 49, 82,212, 12, 79, 60, 26,111,112,115,114,113,112,110,106,102,
    114,119, 25,229, 92,226, 95, 58, 93,101, 79,174,179, 26,138,142,189,147, 16, 19, 76,141, 53,103,205, 95, 47,103,211, 57,150,144, 18,150, 69, 13,115,105,112,111,109,108,105,101, 97,
    113,115, 25,227, 91,226,128, 57, 94, 93, 84,187,175, 31,145,196,197,196, 22, 82, 79,197,197,156,204,101, 52, 97,215,100,215, 76,  8,154, 57, 18,106,110,106,105,104,103,100, 97, 92,
    114,110, 18,226, 85,157,182, 45, 93, 94,149,219, 86, 17,138,138, 18,201,158,103,105,201, 29, 34,185,139, 76, 79,119,222,170, 10, 60,172, 58, 12,106,102,101,101,100, 99, 98, 95, 91,
    107,112, 15,226, 84, 89,185,197,129,180,209,150, 17, 80,197,198, 72,144,210,107,160,206, 86,188,186,133, 72, 78, 74,223, 60, 63, 59,168, 58, 14,101, 99, 99, 98, 98, 98, 98, 96, 92,
    105,105, 26,226, 81, 25, 87,140,183,188,150, 25, 27,157,142,141, 76,144,153,100,107,146,183,132, 49,135,120, 69, 80,209,  9, 63, 69,164, 84, 10, 96,102, 95, 94, 94, 95, 93, 89, 86,
    107,105, 13,226, 78, 74, 43, 44, 48, 54, 21, 79, 90, 85, 70, 70, 22, 71, 60, 55, 48,104, 32, 34, 30, 84, 69, 79,129,171, 13, 66,160,166, 87,  7, 90,102, 96, 96, 94, 90, 88, 86, 82,
     93, 95, 20, 87,128,130, 69, 14, 71, 62, 59, 62, 69, 65, 91, 54, 50, 53, 68, 66, 59, 63, 60, 51, 58, 63,148,215,206, 87, 45,137,144,103, 43, 12, 95, 88, 92, 88, 86, 88, 87, 83, 80,
     94,103,103, 15, 88,130,131,178,184,175,179,183,162,183,150,143,147,148,164,170,165,171,150,156,153,162,142,147, 23, 13,135,138, 98, 40, 13, 89, 95, 86, 89, 84, 81, 82, 83, 83, 82,
     92, 91, 93, 92, 21, 89,124,131,123,123,124,119,125,118,146,147,143,148,114,117,115,117,116,113,113,105, 86, 81, 83, 86, 97, 95, 49,  6, 86, 92, 91, 98, 81, 85, 84, 78, 78, 84, 82,
     93, 89, 91, 92, 90, 12, 14, 11, 11, 12, 16, 18, 10, 13,  8,  9,  7,  8, 17, 12, 12, 14, 16, 11, 18, 14, 19, 17, 11, 20,  9,  3, 10, 82, 96, 86, 75, 84, 78, 82, 82, 77, 76, 79, 79
};

inline bool FindEquipmentLockOkayButton(const std::vector<uint32_t>& pixels, int winW, int winH, POINT ptClient, POINT& outOkayScreen) {
    if (winW < 400 || winH < 300) return false;

    // 1. Scan for the distinctive tan/gold parchment body of the Equipment Lock window
    // Alternating parchment stripes in Conquer: R >= 150, G >= 110, B <= 130, R > G, G > B
    int x1 = (int)(winW * 0.20);
    int x2 = (int)(winW * 0.80);
    int y1 = (int)(winH * 0.15);
    int y2 = (int)(winH * 0.85);

    int tanCount = 0;
    int minTanX = winW, maxTanX = 0;
    int minTanY = winH, maxTanY = 0;

    for (int y = y1; y < y2; y++) {
        int rowIdx = y * winW;
        for (int x = x1; x < x2; x++) {
            uint32_t c = pixels[rowIdx + x];
            int b = (int)(c & 0xFF);
            int g = (int)((c >> 8) & 0xFF);
            int r = (int)((c >> 16) & 0xFF);
            if (r >= 150 && g >= 110 && b <= 130 && r > g && g > b) {
                tanCount++;
                if (x < minTanX) minTanX = x;
                if (x > maxTanX) maxTanX = x;
                if (y < minTanY) minTanY = y;
                if (y > maxTanY) maxTanY = y;
            }
        }
    }

    // Must have at least 10,000 tan parchment pixels (the dialog contains ~75,000 pixels)
    if (tanCount < 10000 || maxTanX <= minTanX || maxTanY <= minTanY) {
        return false;
    }

    // 2. Focused Normalized Cross-Correlation (NCC) search for the [Okay] button
    // In Conquer Online, [Okay] is positioned at the bottom-right of the parchment scroll
    int bx1 = (std::max)(0, minTanX + (int)((maxTanX - minTanX) * 0.40));
    int bx2 = (std::min)(winW - OKAY_LOCK_W, maxTanX + 40);
    int by1 = (std::max)(0, maxTanY - 70);
    int by2 = (std::min)(winH - OKAY_LOCK_H, maxTanY + 50);

    if (bx2 <= bx1 || by2 <= by1) return false;

    // Convert pixels in search ROI to grayscale
    const int N = OKAY_LOCK_W * OKAY_LOCK_H;
    float bestScore = -1.0f;
    int bestX = 0, bestY = 0;

    for (int y = by1; y <= by2; y++) {
        for (int x = bx1; x <= bx2; x++) {
            // Compute mean of image patch
            float pSum = 0.0f;
            for (int ty = 0; ty < OKAY_LOCK_H; ty++) {
                int patchRow = (y + ty) * winW;
                for (int tx = 0; tx < OKAY_LOCK_W; tx++) {
                    uint32_t c = pixels[patchRow + x + tx];
                    int b = (int)(c & 0xFF);
                    int g = (int)((c >> 8) & 0xFF);
                    int r = (int)((c >> 16) & 0xFF);
                    pSum += (0.299f * r + 0.587f * g + 0.114f * b);
                }
            }
            float pMean = pSum / N;

            // Compute variance of image patch
            float pVar = 0.0f;
            float dot = 0.0f;
            for (int ty = 0; ty < OKAY_LOCK_H; ty++) {
                int patchRow = (y + ty) * winW;
                int tplRow = ty * OKAY_LOCK_W;
                for (int tx = 0; tx < OKAY_LOCK_W; tx++) {
                    uint32_t c = pixels[patchRow + x + tx];
                    int b = (int)(c & 0xFF);
                    int g = (int)((c >> 8) & 0xFF);
                    int r = (int)((c >> 16) & 0xFF);
                    float grayVal = (0.299f * r + 0.587f * g + 0.114f * b);
                    float diff = grayVal - pMean;
                    pVar += diff * diff;
                    dot += (OKAY_LOCK_GRAY[tplRow + tx] - (float)OKAY_LOCK_MEAN) * diff;
                }
            }

            if (pVar < 1.0f) continue;
            float pStd = std::sqrt(pVar);
            float score = dot / ((float)OKAY_LOCK_STD * std::sqrt((float)N) * pStd);
            if (score > bestScore) {
                bestScore = score;
                bestX = x;
                bestY = y;
            }
        }
    }

    // High confidence threshold (exact button gives ~0.999, non-dialog gives < 0.35)
    if (bestScore >= 0.70f) {
        outOkayScreen.x = ptClient.x + bestX + OKAY_LOCK_W / 2;
        outOkayScreen.y = ptClient.y + bestY + OKAY_LOCK_H / 2;
        return true;
    }

    return false;
}
