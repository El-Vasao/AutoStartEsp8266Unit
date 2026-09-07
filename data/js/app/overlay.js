// Overlay manager for dialogs/popovers (Esc + basic stacking)
(function () {
  if (window.__elLineOverlayStarted) return;
  window.__elLineOverlayStarted = true;

  document.addEventListener('alpine:init', () => {
    const log = window.APP?.utils?.log;
    const stack = [];

    Alpine.store('overlay', {
      openCount() { return stack.length; },
      open({ id, close, type = 'overlay' } = {}) {
        const item = { id: String(id || ''), type: String(type || 'overlay'), close: (typeof close === 'function') ? close : null };
        stack.push(item);
        return item;
      },
      remove(id) {
        const sid = String(id || '');
        for (let i = stack.length - 1; i >= 0; i--) {
          if (!sid || stack[i].id === sid) {
            stack.splice(i, 1);
            return true;
          }
        }
        return false;
      },
      close(id) {
        const sid = String(id || '');
        for (let i = stack.length - 1; i >= 0; i--) {
          if (!sid || stack[i].id === sid) {
            const it = stack.splice(i, 1)[0];
            try { it?.close?.(); } catch (e) { log?.warn?.('overlay close() handler failed', e); }
            return true;
          }
        }
        return false;
      },
      closeTop() {
        if (!stack.length) return false;
        const it = stack.pop();
        try { it?.close?.(); } catch (e) { log?.warn?.('overlay closeTop() handler failed', e); }
        return true;
      }
    });

    document.addEventListener('keydown', (e) => {
      if (e.key !== 'Escape') return;
      try {
        const ok = Alpine.store('overlay')?.closeTop?.();
        if (ok) e.preventDefault();
      } catch (err) { log?.warn?.('overlay Escape handler failed', err); }
    }, { capture: true });

    // click-outside: if top overlay is a dialog and click lands on backdrop (not inside card),
    // close it. This centralizes behavior for modals.
    document.addEventListener('click', (e) => {
      try {
        if (!stack.length) return;
        const top = stack[stack.length - 1];
        if (top?.type !== 'dialog') return;
        const backdrop = e.target?.closest?.('.modal-backdrop');
        const card = e.target?.closest?.('.modal-card');
        if (backdrop && !card) {
          Alpine.store('overlay')?.closeTop?.();
        }
      } catch (err) { log?.warn?.('overlay click handler failed', err); }
    }, { capture: true, passive: true });
  });
})();

