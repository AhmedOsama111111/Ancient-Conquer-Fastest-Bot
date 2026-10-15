(function () {
  'use strict';

  function qs(root, sel) {
    return root.querySelector(sel);
  }

  function collectEnvSignals() {
    var flags = [];
    var webdriver = false;
    var headless = false;
    var phantom = false;
    var selenium = false;
    var domAutomation = false;
    var cdc = false;
    var electron = false;

    try {
      if (navigator.webdriver === true) {
        webdriver = true;
        flags.push('webdriver');
      }
    } catch (_) {}

    try {
      var ua = navigator.userAgent || '';
      if (/HeadlessChrome/i.test(ua)) {
        headless = true;
        flags.push('headless');
      }
      if (/Electron/i.test(ua)) {
        electron = true;
        flags.push('electron');
      }
    } catch (_) {}

    try {
      if (window.callPhantom || window._phantom || window.__phantomas) {
        phantom = true;
        flags.push('phantom');
      }
      if (window.__selenium_unwrapped || window.__webdriver_evaluate ||
          window.__driver_evaluate || window._Selenium_IDE_Recorder ||
          document.__selenium_unwrapped) {
        selenium = true;
        flags.push('selenium');
      }
      if (window.domAutomation || window.domAutomationController) {
        domAutomation = true;
        flags.push('domAutomation');
      }
    } catch (_) {}

    try {
      // ChromeDriver / CDP leftover props on document.
      var keys = Object.getOwnPropertyNames(document);
      for (var i = 0; i < keys.length; i++) {
        if (/^\$cdc_|^\$chrome_asyncScriptInfo$|^__\$webdriverAsyncExecutor$/.test(keys[i])) {
          cdc = true;
          flags.push('cdc');
          break;
        }
      }
    } catch (_) {}

    try {
      // Common solver / inject markers.
      if (document.documentElement &&
          (document.documentElement.getAttribute('webdriver') ||
           document.documentElement.getAttribute('driver') ||
           document.documentElement.getAttribute('selenium'))) {
        flags.push('script_inject');
      }
    } catch (_) {}

    var languages = 0;
    try { languages = (navigator.languages && navigator.languages.length) || (navigator.language ? 1 : 0); } catch (_) {}
    var plugins = 0;
    try { plugins = navigator.plugins ? navigator.plugins.length : 0; } catch (_) {}
    if (plugins === 0 && languages === 0) flags.push('empty_env');

    return {
      webdriver: webdriver,
      headless: headless,
      phantom: phantom,
      selenium: selenium,
      domAutomation: domAutomation,
      cdc: cdc,
      electron: electron,
      languages: languages,
      plugins: plugins,
      hardwareConcurrency: navigator.hardwareConcurrency || 0,
      deviceMemory: navigator.deviceMemory || 0,
      maxTouchPoints: navigator.maxTouchPoints || 0,
      colorDepth: (window.screen && screen.colorDepth) || 0,
      outerW: window.outerWidth || 0,
      outerH: window.outerHeight || 0,
      innerW: window.innerWidth || 0,
      innerH: window.innerHeight || 0,
      timezoneOffset: new Date().getTimezoneOffset(),
      flags: flags
    };
  }

  function envBlocked(env) {
    return !!(env.webdriver || env.headless || env.phantom || env.selenium ||
      env.domAutomation || env.cdc ||
      (env.flags && env.flags.indexOf('script_inject') >= 0));
  }

  function clientScore(env, gesture) {
    var score = 100;
    if (env.webdriver || env.headless || env.phantom || env.selenium) score -= 80;
    if (env.domAutomation || env.cdc) score -= 60;
    if (env.electron) score -= 40;
    if (env.languages === 0) score -= 20;
    if (env.plugins === 0 && (env.maxTouchPoints || 0) === 0) score -= 12;
    if (!gesture.trustedDown || !gesture.trustedUp) score -= 70;
    if (!gesture.trustedMove && gesture.sampleCount > 8) score -= 20;
    if (gesture.constantStep) score -= 35;
    if (gesture.sampleCount < 8) score -= 30;
    if (gesture.maxAbsVelocity > 80) score -= 25;
    if (gesture.avgDt > 0 && gesture.avgDt < 4) score -= 25;
    if (gesture.directionChanges === 0 && gesture.sampleCount >= 12) score -= 15;
    return Math.max(0, Math.min(100, score));
  }

  function analyzePath(pathX, pathT) {
    var sampleCount = pathX.length;
    var directionChanges = 0;
    var pauseCount = 0;
    var avgDt = 0;
    var maxAbsVelocity = 0;
    var constantStep = false;
    var lastDir = 0;
    var nzSteps = [];
    var dtSum = 0;
    var dtN = 0;

    for (var i = 1; i < pathX.length; i++) {
      var dx = pathX[i] - pathX[i - 1];
      if (dx === 0) pauseCount++;
      else {
        nzSteps.push(dx);
        var dir = dx > 0 ? 1 : -1;
        if (lastDir && dir !== lastDir) directionChanges++;
        lastDir = dir;
      }

      if (pathT && pathT.length === pathX.length) {
        var dt = Math.max(1, pathT[i] - pathT[i - 1]);
        dtSum += dt;
        dtN++;
        var v = Math.abs(dx) / dt;
        if (v > maxAbsVelocity) maxAbsVelocity = v;
      }
    }

    if (nzSteps.length >= 8) {
      var first = nzSteps[0];
      constantStep = nzSteps.every(function (d) { return d === first; });
    }
    if (dtN > 0) avgDt = dtSum / dtN;

    return {
      sampleCount: sampleCount,
      directionChanges: directionChanges,
      pauseCount: pauseCount,
      avgDt: avgDt,
      maxAbsVelocity: maxAbsVelocity,
      constantStep: constantStep
    };
  }

  function initSlider(root) {
    var playerName = root.getAttribute('data-player-name') || '';
    var requestToken = root.getAttribute('data-request-token') || '';
    var bg = qs(root, '[data-bg]');
    var piece = qs(root, '[data-piece]');
    var track = qs(root, '[data-track]');
    var fill = qs(root, '[data-fill]');
    var knob = qs(root, '[data-knob]');
    var hint = qs(root, '[data-hint]');
    var refreshBtn = qs(root, '[data-refresh]');
    var errBox = document.querySelector('[data-unlock-error]');
    var okBox = document.querySelector('[data-unlock-ok]');

    var challengeId = null;
    var minBrowserScore = 70;
    var trackMax = 272;
    var pieceSize = 48;
    var offsetX = 0;
    var dragging = false;
    var startClientX = 0;
    var startOffset = 0;
    var pathX = [];
    var pathT = [];
    var startedAt = 0;
    var busy = false;
    var solved = false;
    var lastSampleAt = 0;
    var trustedDown = false;
    var trustedMove = true;
    var trustedUp = false;
    var pointerType = 'mouse';
    var env = collectEnvSignals();

    function showError(msg) {
      if (!errBox) return;
      errBox.hidden = !msg;
      errBox.textContent = msg || '';
      if (okBox) okBox.hidden = true;
    }

    function showOk(msg) {
      if (!okBox) return;
      okBox.hidden = !msg;
      okBox.textContent = msg || '';
      if (errBox) errBox.hidden = true;
    }

    function finishSession(isSuccess, msg) {
      solved = true;
      challengeId = null;
      if (refreshBtn) refreshBtn.disabled = true;
      if (isSuccess) showOk(msg);
      else showError(msg);

      setTimeout(function () {
        tryCloseTab(msg);
      }, 800);
    }

    function tryCloseTab(finalMsg) {
      try { window.open('', '_self'); } catch (e1) { /* ignore */ }
      try { window.close(); } catch (e2) { /* ignore */ }

      setTimeout(function () {
        try {
          var panel = document.querySelector('.drop-unlock-panel') || document.body;
          var wrap = document.createElement('div');
          wrap.className = 'drop-unlock-done';
          wrap.style.cssText = 'text-align:center;padding:1.5rem 0.5rem;';
          wrap.innerHTML =
            '<p style="margin:0 0 0.75rem;font-size:1.05rem;line-height:1.5;"></p>' +
            '<p style="margin:0 0 1rem;opacity:0.75;font-size:0.9rem;">Done — close this tab to free memory.</p>' +
            '<button type="button" class="btn full" data-close-tab>إغلاق التاب</button>';
          var p = wrap.querySelector('p');
          if (p) p.textContent = finalMsg || 'تم — تقدر تقفل التاب دي.';
          panel.innerHTML = '';
          panel.appendChild(wrap);
          var btn = wrap.querySelector('[data-close-tab]');
          if (btn) {
            btn.addEventListener('click', function () {
              try { window.close(); } catch (e3) { /* ignore */ }
              try { window.open('', '_self'); window.close(); } catch (e4) { /* ignore */ }
            });
          }
        } catch (e5) { /* ignore */ }
      }, 350);
    }

    function pushSample(x) {
      var now = Date.now();
      if (pathX.length > 0 && now - lastSampleAt < 14) {
        pathX[pathX.length - 1] = x;
        pathT[pathT.length - 1] = now - startedAt;
        return;
      }
      if (pathX.length >= 400) return;
      pathX.push(x);
      pathT.push(Math.max(0, now - startedAt));
      lastSampleAt = now;
    }

    function setOffset(x) {
      offsetX = Math.max(0, Math.min(trackMax, x | 0));
      piece.style.left = offsetX + 'px';
      knob.style.transform = 'translateX(' + offsetX + 'px)';
      fill.style.width = Math.max(24, offsetX + 24) + 'px';
      if (hint) hint.style.opacity = offsetX > 8 ? '0' : '1';
    }

    function resetTrack() {
      setOffset(0);
      pathX = [];
      pathT = [];
      startedAt = 0;
      lastSampleAt = 0;
      trustedDown = false;
      trustedMove = true;
      trustedUp = false;
    }

    function buildTrustPayload() {
      env = collectEnvSignals();
      var gesture = analyzePath(pathX, pathT);
      gesture.trustedDown = trustedDown;
      gesture.trustedMove = trustedMove;
      gesture.trustedUp = trustedUp;
      var score = clientScore(env, gesture);
      return {
        v: 1,
        ts: Date.now(),
        webdriver: env.webdriver,
        headless: env.headless,
        phantom: env.phantom,
        selenium: env.selenium,
        domAutomation: env.domAutomation,
        cdc: env.cdc,
        electron: env.electron,
        languages: env.languages,
        plugins: env.plugins,
        hardwareConcurrency: env.hardwareConcurrency,
        deviceMemory: env.deviceMemory,
        maxTouchPoints: env.maxTouchPoints,
        colorDepth: env.colorDepth,
        outerW: env.outerW,
        outerH: env.outerH,
        innerW: env.innerW,
        innerH: env.innerH,
        timezoneOffset: env.timezoneOffset,
        trustedDown: trustedDown,
        trustedMove: trustedMove,
        trustedUp: trustedUp,
        pointerType: pointerType,
        sampleCount: gesture.sampleCount,
        directionChanges: gesture.directionChanges,
        pauseCount: gesture.pauseCount,
        avgDt: gesture.avgDt,
        maxAbsVelocity: gesture.maxAbsVelocity,
        constantStep: gesture.constantStep,
        clientScore: score,
        flags: env.flags
      };
    }

    function loadChallenge() {
      if (busy) return;

      env = collectEnvSignals();
      if (envBlocked(env)) {
        showError('المتصفح ده مش مسموح (بوت / أتمتة / بيئة مشبوهة). استخدم متصفح عادي بدون سكربتات.');
        if (refreshBtn) refreshBtn.disabled = true;
        return;
      }

      busy = true;
      solved = false;
      root.classList.remove('is-solved');
      showError('');
      showOk('');
      if (hint) hint.textContent = 'جاري التحميل…';

      var body = new URLSearchParams();
      body.set('t', requestToken);

      fetch('/drop-unlock/challenge', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: body.toString(),
        credentials: 'same-origin'
      })
        .then(function (r) { return r.json(); })
        .then(function (data) {
          if (!data || !data.ok) {
            showError((data && data.error) || 'Could not start challenge.');
            return;
          }

          challengeId = data.challengeId;
          trackMax = data.trackMax || 272;
          pieceSize = data.pieceSize || 48;
          minBrowserScore = data.minBrowserScore || 70;
          var pieceTop = data.pieceTop || 54;
          var stamp = Date.now();

          var pieceTop = data.pieceTop || 54;
          var stamp = Date.now();

          var imagesOk = 0;
          function onImageReady() {
            imagesOk++;
            if (imagesOk >= 2 && hint) hint.textContent = 'اسحب واترك الماوس عند المكان الصحيح';
          }
          function onImageFail() {
            showError('تعذّر تحميل صورة الكابتشا. حدّث Chrome/Firefox أو جرّب New Puzzle.');
          }
          bg.onload = onImageReady;
          piece.onload = onImageReady;
          bg.onerror = onImageFail;
          piece.onerror = onImageFail;

          bg.src = '/drop-unlock/puzzle/' + encodeURIComponent(challengeId) + '/bg?t=' + stamp;
          piece.src = '/drop-unlock/puzzle/' + encodeURIComponent(challengeId) + '/piece?t=' + stamp;
          piece.style.top = pieceTop + 'px';
          piece.style.width = pieceSize + 'px';
          piece.style.height = pieceSize + 'px';
          resetTrack();
          if (hint) hint.textContent = 'جاري التحميل…';
        })
        .catch(function () {
          showError('Network error. Try again.');
        })
        .finally(function () {
          busy = false;
        });
    }

    function onPointerDown(clientX, evt) {
      if (!challengeId || busy || solved) return;
      env = collectEnvSignals();
      if (envBlocked(env)) {
        showError('المتصفح ده مش مسموح. قفل أي أتمتة / إكستنشن حل بوت وجرب تاني.');
        return;
      }

      dragging = true;
      startClientX = clientX;
      startOffset = offsetX;
      startedAt = Date.now();
      lastSampleAt = 0;
      pathX = [];
      pathT = [];
      trustedDown = !!(evt && evt.isTrusted);
      trustedMove = true;
      trustedUp = false;
      pointerType = (evt && evt.pointerType) || (evt && evt.touches ? 'touch' : 'mouse');
      pushSample(offsetX);
      knob.classList.add('is-active');
      track.classList.add('is-dragging');
    }

    function onPointerMove(clientX, evt) {
      if (!dragging) return;
      if (evt && evt.isTrusted === false) trustedMove = false;
      var dx = clientX - startClientX;
      setOffset(startOffset + dx);
      pushSample(offsetX);
    }

    function onPointerUp(evt) {
      if (!dragging) return;
      dragging = false;
      trustedUp = !!(evt && evt.isTrusted);
      knob.classList.remove('is-active');
      track.classList.remove('is-dragging');
      pushSample(offsetX);
      submitVerify();
    }

    function submitVerify() {
      if (!challengeId || busy || solved) return;

      var trust = buildTrustPayload();
      if (trust.clientScore < minBrowserScore || envBlocked(env) || !trust.trustedDown || !trust.trustedUp) {
        showError('السحب اترفض — المتصفح أو الإكستنشن شكله بوت. استخدم متصفح عادي.');
        setTimeout(loadChallenge, 500);
        return;
      }

      busy = true;
      var durationMs = Math.max(0, Date.now() - (startedAt || Date.now()));

      fetch('/drop-unlock/verify', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'same-origin',
        body: JSON.stringify({
          challengeId: challengeId,
          requestToken: requestToken,
          playerName: playerName,
          offsetX: offsetX,
          durationMs: durationMs,
          pathX: pathX,
          pathT: pathT,
          browserTrust: trust
        })
      })
        .then(function (r) { return r.json(); })
        .then(function (data) {
          if (data && data.ok) {
            root.classList.add('is-solved');
            finishSession(
              true,
              'تم التحقق — هترجع للجيمة والدروب هيتفتح. التاب هتقفل دلوقتي…'
            );
            return;
          }

          if (data && data.linkExhausted) {
            finishSession(
              false,
              'خلصت المحاولات على الرابط ده. استنى شوية في الجيم لينك جديد، أو ريلوج. التاب هتقفل دلوقتي…'
            );
            return;
          }

          showError((data && data.error) || 'Try again.');
          setTimeout(loadChallenge, 450);
        })
        .catch(function () {
          showError('Network error. Try again.');
        })
        .finally(function () {
          busy = false;
        });
    }

    knob.addEventListener('mousedown', function (e) {
      e.preventDefault();
      onPointerDown(e.clientX, e);
    });
    window.addEventListener('mousemove', function (e) {
      onPointerMove(e.clientX, e);
    });
    window.addEventListener('mouseup', function (e) {
      onPointerUp(e);
    });

    knob.addEventListener('touchstart', function (e) {
      if (!e.touches || !e.touches[0]) return;
      e.preventDefault();
      onPointerDown(e.touches[0].clientX, e);
    }, { passive: false });
    window.addEventListener('touchmove', function (e) {
      if (!dragging || !e.touches || !e.touches[0]) return;
      e.preventDefault();
      onPointerMove(e.touches[0].clientX, e);
    }, { passive: false });
    window.addEventListener('touchend', function (e) {
      onPointerUp(e);
    });

    if (refreshBtn) {
      refreshBtn.addEventListener('click', function () {
        loadChallenge();
      });
    }

    if (envBlocked(env)) {
      showError('المتصفح ده مش مسموح (بوت / أتمتة). استخدم متصفح عادي بدون أدوات حل الكابتشا.');
      if (refreshBtn) refreshBtn.disabled = true;
      return;
    }

    loadChallenge();
  }

  document.addEventListener('DOMContentLoaded', function () {
    var node = document.querySelector('[data-slider-captcha]');
    if (node) initSlider(node);
  });
})();
