// toast store (minimal overlay toast)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerUiToastStore = function registerUiToastStore(Alpine) {
    Alpine.store('uiToast', {
      open: false,
      text: '',
      variant: 'info',
      _timer: null,
      show(text, variant = 'info', duration = 3500) {
        const msg = String(text || '');
        if (!msg) return;
        this.text = msg;
        this.variant = String(variant || 'info');
        this.open = true;
        clearTimeout(this._timer);
        const d = Number(duration) || 0;
        if (d > 0) {
          this._timer = setTimeout(() => { this.open = false; }, d);
        }
      },
      hide() {
        this.open = false;
        clearTimeout(this._timer);
        this._timer = null;
      }
    });
  };
})();

