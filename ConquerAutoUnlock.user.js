// ==UserScript==
// @name         Ancient Conquer - Drop Unlock Auto Solver
// @namespace    https://conqueronline.org/
// @version      1.0
// @description  Automated 0-Mouse Slider Captcha Solver for Ancient Conquer Drop Unlock
// @author       Antigravity
// @match        https://conqueronline.org/drop-unlock*
// @match        http://conqueronline.org/drop-unlock*
// @run-at       document-idle
// @grant        none
// ==/UserScript==

(function() {
    'use strict';

    console.log('[AutoUnlock] Script initialized. Watching for slider puzzle...');

    function trySolve() {
        const stage = document.querySelector('.slider-captcha-stage');
        const knob = document.querySelector('.slider-captcha-knob');
        const bgImg = document.querySelector('.slider-captcha-bg');
        const pieceImg = document.querySelector('.slider-captcha-piece');

        if (!stage || !knob || !bgImg) {
            return false;
        }

        // Wait until image is loaded
        if (!bgImg.complete || bgImg.naturalWidth === 0) {
            bgImg.addEventListener('load', trySolve, { once: true });
            return false;
        }

        console.log('[AutoUnlock] Captcha stage and background detected!');

        // Create canvas to inspect background pixels
        const canvas = document.createElement('canvas');
        canvas.width = 320;
        canvas.height = 160;
        const ctx = canvas.getContext('2d');
        if (!ctx) return false;

        try {
            ctx.drawImage(bgImg, 0, 0, 320, 160);
        } catch (e) {
            console.error('[AutoUnlock] Failed to draw image (CORS?):', e);
            return false;
        }

        const imgData = ctx.getImageData(0, 0, 320, 160);
        const data = imgData.data;

        // Find the slot track row by counting dark pixels (< 85) across rows 35..125
        let bestDy = 85;
        let maxDark = 0;
        for (let dy = 35; dy <= 125; dy++) {
            let darkCnt = 0;
            for (let dx = 50; dx < 310; dx++) {
                const idx = (dy * 320 + dx) * 4;
                const r = data[idx], g = data[idx + 1], b = data[idx + 2];
                const gray = 0.299 * r + 0.587 * g + 0.114 * b;
                if (gray < 85) darkCnt++;
            }
            if (darkCnt > maxDark) {
                maxDark = darkCnt;
                bestDy = dy;
            }
        }

        console.log(`[AutoUnlock] Detected slot row at dy=${bestDy} (darkCount=${maxDark})`);

        // Average brightness across 7 rows around bestDy
        const strip = new Float32Array(320);
        for (let dx = 0; dx < 320; dx++) {
            let sum = 0, count = 0;
            for (let off = -3; off <= 3; off++) {
                const cy = bestDy + off;
                if (cy >= 0 && cy < 160) {
                    const idx = (cy * 320 + dx) * 4;
                    const r = data[idx], g = data[idx + 1], b = data[idx + 2];
                    sum += 0.299 * r + 0.587 * g + 0.114 * b;
                    count++;
                }
            }
            strip[dx] = count > 0 ? (sum / count) : 255;
        }

        // Find all dark slot segments (excluding initial piece at dx < 46)
        const slots = [];
        let inSeg = false;
        let segStart = 0;
        for (let dx = 46; dx < 310; dx++) {
            const isDark = strip[dx] < 85;
            if (isDark && !inSeg) {
                inSeg = true;
                segStart = dx;
            } else if (!isDark && inSeg) {
                inSeg = false;
                const w = dx - segStart;
                if (w >= 20) {
                    const cx = (segStart + dx - 1) / 2.0;
                    slots.push({ left: segStart, right: dx - 1, cx: cx, deltaX: Math.round(cx - 24.0) });
                }
            }
        }
        if (inSeg && (310 - segStart >= 20)) {
            const cx = (segStart + 309) / 2.0;
            slots.push({ left: segStart, right: 309, cx: cx, deltaX: Math.round(cx - 24.0) });
        }

        if (slots.length === 0) {
            console.log('[AutoUnlock] No slots detected via threshold. Falling back to default.');
            slots.push({ deltaX: 43 });
            slots.push({ deltaX: 152 });
            slots.push({ deltaX: 246 });
        }

        console.log(`[AutoUnlock] Found ${slots.length} slot candidate(s):`, slots);

        // Pick Rank 1 (first slot or seam matched)
        const target = slots[0];
        console.log(`[AutoUnlock] Solving with Target Delta: +${target.deltaX}px`);

        // Smooth humanized pointer drag
        setTimeout(() => {
            performDrag(knob, target.deltaX);
        }, 300);

        return true;
    }

    function performDrag(knob, deltaX) {
        const rect = knob.getBoundingClientRect();
        const startX = rect.left + rect.width / 2;
        const startY = rect.top + rect.height / 2;

        const pDown = new PointerEvent('pointerdown', { bubbles: true, cancelable: true, clientX: startX, clientY: startY, pointerId: 1, isPrimary: true });
        const mDown = new MouseEvent('mousedown', { bubbles: true, cancelable: true, clientX: startX, clientY: startY });
        knob.dispatchEvent(pDown);
        knob.dispatchEvent(mDown);

        const steps = 28;
        let step = 0;
        const interval = setInterval(() => {
            step++;
            const t = step / steps;
            const ease = 1 - Math.pow(1 - t, 3);
            const curX = startX + deltaX * ease;

            const pMove = new PointerEvent('pointermove', { bubbles: true, cancelable: true, clientX: curX, clientY: startY, pointerId: 1, isPrimary: true });
            const mMove = new MouseEvent('mousemove', { bubbles: true, cancelable: true, clientX: curX, clientY: startY });
            window.dispatchEvent(pMove);
            window.dispatchEvent(mMove);

            if (step >= steps) {
                clearInterval(interval);
                setTimeout(() => {
                    const finalX = startX + deltaX;
                    const pUp = new PointerEvent('pointerup', { bubbles: true, cancelable: true, clientX: finalX, clientY: startY, pointerId: 1, isPrimary: true });
                    const mUp = new MouseEvent('mouseup', { bubbles: true, cancelable: true, clientX: finalX, clientY: startY });
                    window.dispatchEvent(pUp);
                    window.dispatchEvent(mUp);
                    console.log('[AutoUnlock] Slider released at target! Verification in progress...');
                }, 80);
            }
        }, 14);
    }

    // Run solver after DOM is ready
    let attempts = 0;
    const poller = setInterval(() => {
        attempts++;
        if (trySolve() || attempts > 20) {
            clearInterval(poller);
        }
    }, 400);

})();
