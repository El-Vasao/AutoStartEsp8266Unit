// Touch gestures (double-tap + long-press) binding
(function () {
  const APP = (window.APP = window.APP || {});
  APP.gestures = APP.gestures || {};

  APP.gestures.init = function initGestures(Alpine) {
    if (window.__elLineGesturesStarted) return;
    window.__elLineGesturesStarted = true;

    function bindGestures(root = document) {
      const Hammer = window.Hammer;
      if (!Hammer) return;

      const nodes = root.querySelectorAll?.('[data-gesture]') || [];
      for (const el of nodes) {
        if (!(el instanceof Element)) continue;
        if (el.dataset.hammerBound === '1') continue;
        el.dataset.hammerBound = '1';

        const mc = new Hammer.Manager(el, {
          touchAction: 'manipulation',
          inputClass: Hammer.SUPPORT_POINTER_EVENTS ? Hammer.PointerEventInput : Hammer.TouchInput
        });

        const doubleTap = new Hammer.Tap({ event: 'doubletap', taps: 2, interval: 300, time: 250, threshold: 10, posThreshold: 30 });
        const singleTap = new Hammer.Tap({ event: 'singletap' });
        const press = new Hammer.Press({ event: 'press', time: 450, threshold: 9 });

        mc.add([doubleTap, singleTap, press]);
        singleTap.requireFailure(doubleTap);

        const type = String(el.getAttribute('data-gesture') || '');
        const idxAttr = el.getAttribute('data-index');
        const index = (idxAttr === null || idxAttr === undefined || idxAttr === '') ? null : parseInt(String(idxAttr), 10);

        mc.on('press', () => {
          try {
            if (Alpine.store('deviceOta')?.started) return;
            if (Alpine.store('uiState')?.activeTab !== 'panel') return;
            Alpine.store('deviceRuntime')?.handleLongPress?.(type, Number.isFinite(index) ? index : undefined);
          } catch (e) {}
        });

        mc.on('doubletap', () => {
          try {
            if (Alpine.store('deviceOta')?.started) return;
            Alpine.store('deviceRuntime')?.handleDoubleTap?.(type, Number.isFinite(index) ? index : undefined);
          } catch (e) {}
        });
      }
    }

    function start() {
      try {
        bindGestures(document);

        const panelId = APP.domIds?.panelTab || 'tab-panel';
        const panel = document.getElementById(panelId);
        if (!panel || !window.MutationObserver) return;

        let t = null;
        const obs = new MutationObserver(() => {
          clearTimeout(t);
          t = setTimeout(() => bindGestures(panel), 60);
        });
        obs.observe(panel, { childList: true, subtree: true });
      } catch (e) {}
    }

    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => setTimeout(start, 0), { once: true });
    } else {
      setTimeout(start, 0);
    }
  };
})();

