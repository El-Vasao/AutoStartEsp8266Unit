// statusBar compatibility store (banners/toasts disabled)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerUiStatusBarCompatStore = function registerUiStatusBarCompatStore(Alpine) {
    Alpine.store('uiStatusBar', {
      hardwareError: '',
      flash(text, variant = 'info', duration = 3500) {
        try { Alpine.store('uiToast')?.show?.(text, variant, duration); } catch (e) {}
      },
      clearHardwareError() { this.hardwareError = ''; },
      setHardwareError(msg) { this.hardwareError = msg ? String(msg) : ''; },
      _syncVisible() {},
      goToValidationField() {}
    });
  };
})();

