window.AncientConquer = window.AncientConquer || {};

(function () {
  const toggle = document.querySelector('[data-nav-toggle]');
  const nav = document.querySelector('[data-site-nav]');
  if (toggle && nav) {
    toggle.addEventListener('click', () => {
      const open = nav.classList.toggle('open');
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
      document.body.classList.toggle('nav-open', open);
    });
    nav.querySelectorAll('a').forEach((link) => {
      link.addEventListener('click', () => {
        nav.classList.remove('open');
        toggle.setAttribute('aria-expanded', 'false');
        document.body.classList.remove('nav-open');
      });
    });
  }

  function initHeroSlider() {
    const slider = document.querySelector('[data-hero-slider]');
    if (!slider) return;

    const slides = slider.querySelectorAll('.hero-slide');
    const dots = document.querySelectorAll('[data-hero-dot]');
    if (slides.length <= 1) return;

    let index = 0;
    let timer;

    function show(i) {
      index = (i + slides.length) % slides.length;
      slides.forEach((s, j) => s.classList.toggle('active', j === index));
      dots.forEach((d, j) => d.classList.toggle('active', j === index));
    }

    function next() {
      show(index + 1);
    }

    function resetTimer() {
      clearInterval(timer);
      timer = setInterval(next, 6000);
    }

    dots.forEach((dot) => {
      dot.addEventListener('click', () => {
        show(parseInt(dot.getAttribute('data-hero-dot'), 10));
        resetTimer();
      });
    });

    resetTimer();
  }

  window.AncientConquer.initHeroSlider = initHeroSlider;

  function formatRemaining(ms) {
    if (ms < 0) ms = 0;
    const totalSec = Math.floor(ms / 1000);
    const days = Math.floor(totalSec / 86400);
    const hours = Math.floor((totalSec % 86400) / 3600);
    const mins = Math.floor((totalSec % 3600) / 60);
    const secs = totalSec % 60;
    if (days >= 1) return days + 'd ' + hours + 'h ' + mins + 'm';
    if (hours >= 1) return hours + 'h ' + mins + 'm ' + String(secs).padStart(2, '0') + 's';
    if (mins >= 1) return mins + 'm ' + String(secs).padStart(2, '0') + 's';
    return secs + 's';
  }

  function initEventCountdowns() {
    const nodes = document.querySelectorAll('[data-event-countdown]');
    if (!nodes.length) return;

    const SPAWN_ACTIVE_MS = 60 * 1000;
    const board = document.querySelector('[data-event-board]');
    const serverNowAttr = board && board.getAttribute('data-server-now-ms');
    const serverNowMs = serverNowAttr ? parseInt(serverNowAttr, 10) : NaN;
    // Align browser clock with server so skew does not force endless reloads.
    const clockOffsetMs = Number.isFinite(serverNowMs) ? (serverNowMs - Date.now()) : 0;
    let reloadArmed = false;

    function nowMs() {
      return Date.now() + clockOffsetMs;
    }

    function requestBoardRefresh() {
      if (reloadArmed) return;
      // At most one soft refresh per tab session — never loop-reload the homepage.
      try {
        if (sessionStorage.getItem('ac-event-board-reloaded') === '1') return;
        sessionStorage.setItem('ac-event-board-reloaded', '1');
      } catch (_) { /* ignore */ }

      reloadArmed = true;
      setTimeout(function () { location.reload(); }, 800);
    }

    function setCountdownText(el, label, text) {
      const strong = el.querySelector('strong');
      if (strong) {
        if (el.childNodes[0] && el.childNodes[0].nodeType === 3) {
          el.childNodes[0].textContent = label + ' ';
        } else {
          el.insertBefore(document.createTextNode(label + ' '), strong);
        }
        strong.textContent = text;
      } else {
        el.textContent = label + ' ' + text;
      }
    }

    function tick() {
      const now = nowMs();
      let needsRefresh = false;

      nodes.forEach((el) => {
        const target = parseInt(el.getAttribute('data-target-ms'), 10);
        if (!target) return;

        const live = el.getAttribute('data-live') === '1';
        const mode = el.getAttribute('data-mode') || 'event';
        const remaining = target - now;
        const sawPositive = el.getAttribute('data-saw-positive') === '1';

        if (remaining > 0) {
          el.setAttribute('data-saw-positive', '1');
        }

        if (mode === 'spawn' && !live && remaining <= 0) {
          const spawnedUntil = target + SPAWN_ACTIVE_MS;
          if (now < spawnedUntil) {
            el.setAttribute('data-live', '1');
            el.classList.add('is-due');
            const hero = el.closest('.next-event-hero, li');
            if (hero) hero.classList.add('is-live');
            setCountdownText(el, 'Spawned', formatRemaining(spawnedUntil - now));
            return;
          }
          setCountdownText(el, 'Spawned', '0s');
          // Only refresh after a real countdown transition on this page.
          if (sawPositive) needsRefresh = true;
          return;
        }

        if (remaining <= 0) {
          el.classList.add('is-due');
          if (mode === 'spawn' && live) {
            setCountdownText(el, 'Spawned', '0s');
          } else if (live) {
            setCountdownText(el, 'Ended', '0s');
          } else {
            setCountdownText(el, 'Starting', '0s');
          }
          // Already expired on first paint (clock skew / stale board): do not loop-reload.
          if (sawPositive) needsRefresh = true;
          return;
        }

        const label = mode === 'spawn'
          ? (live ? 'Spawned' : 'Spawns in')
          : (live ? 'Ends in' : 'Starts in');
        setCountdownText(el, label, formatRemaining(remaining));
      });

      if (needsRefresh) requestBoardRefresh();
    }

    tick();
    setInterval(tick, 1000);
  }

  window.AncientConquer.initEventCountdowns = initEventCountdowns;

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initHeroSlider);
  } else {
    initHeroSlider();
  }
})();
