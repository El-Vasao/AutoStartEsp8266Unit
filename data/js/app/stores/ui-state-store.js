// ui state store (tabs/subtabs + lock)
(function () {
  const APP = (window.APP = (window.APP || {}));
  APP.stores = APP.stores || {};

  APP.stores.registerUiStateStore = function registerUiStateStore(Alpine) {
    const persist = (val, key) => Alpine.$persist(val).as(key);

    Alpine.store('uiState', {
      locked: false,
      initFailed: false,
      initError: '',
      // Bootstrap lifecycle (for reliability + diagnostics)
      // starting -> bootstrap -> schemas -> data -> ready | degraded
      initPhase: 'starting',
      initReasons: [],
      lastBootstrapAt: 0,
      lastReadyAt: 0,
      unsavedHint: false,
      activeTab: (() => {
        try {
          const v = persist('panel', 'ui.activeTab');
          return (typeof v === 'string' && v) ? v : 'panel';
        } catch (e) {
          return 'panel';
        }
      })(),
      activeSubTab: (() => {
        try {
          const v = persist('wifi', 'ui.activeSubTab');
          return (typeof v === 'string' && v) ? v : 'wifi';
        } catch (e) {
          return 'wifi';
        }
      })(),
      goToSettings(target) {
        if (target === 'programs') {
          this.activeTab = 'programs';
        } else {
          this.activeTab = 'settings';
          this.activeSubTab = target;
        }
      },

      setInitPhase(phase) {
        const p = String(phase || '');
        if (!p) return;
        this.initPhase = p;
        if (p === 'bootstrap') this.lastBootstrapAt = Date.now();
        if (p === 'ready') this.lastReadyAt = Date.now();
      },

      pushInitReason(code, detail) {
        const c = String(code || '');
        if (!c) return;
        const d = (detail == null) ? '' : String(detail);
        const msg = d ? `${c}: ${d}` : c;
        if (!Array.isArray(this.initReasons)) this.initReasons = [];
        // Keep it short to avoid LS/memory bloat
        if (!this.initReasons.includes(msg)) this.initReasons.push(msg);
        if (this.initReasons.length > 12) this.initReasons = this.initReasons.slice(-12);
      }
    });
  };
})();

