#pragma once
#include <windows.h>
#include <cmath>
#include <algorithm>

class ConquerProjection {
public:
    // Transforms 2D world tile coordinates into 2.5D isometric screen pixel coordinates
    static bool WorldToScreen(HWND gameHwnd, int playerX, int playerY, int targetX, int targetY, POINT& outPt) {
        if (!gameHwnd || !IsWindow(gameHwnd)) return false;

        RECT rc;
        GetClientRect(gameHwnd, &rc);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;
        if (winW <= 0 || winH <= 0) return false;

        int centerX = winW / 2;
        int centerY = winH / 2;

        int dMapX = targetX - playerX;
        int dMapY = targetY - playerY;

        // Conquer Online 2.5D Isometric Tile Projection:
        // 1 tile step horizontally = (dx - dy) * 26.0
        // 1 tile step vertically   = (dx + dy) * 13.0
        double screenDx = (dMapX - dMapY) * 26.0;
        double screenDy = (dMapX + dMapY) * 13.0;

        int localX = centerX + (int)std::round(screenDx);
        int localY = centerY + (int)std::round(screenDy);

        // Clamping to visible window area
        int marginX = 20;
        int marginY = 30;
        if (localX < marginX || localX > winW - marginX || localY < marginY || localY > winH - marginY) {
            // Target is outside immediate screen view
            return false;
        }

        POINT pt = { localX, localY };
        ClientToScreen(gameHwnd, &pt);
        outPt = pt;
        return true;
    }

    // Calculates tile distance between two points
    static double GetDistance(int x1, int y1, int x2, int y2) {
        return std::hypot(x2 - x1, y2 - y1);
    }
};
