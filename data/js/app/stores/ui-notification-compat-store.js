// notification store (no-op API for compatibility)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerUiNotificationCompatStore = function registerUiNotificationCompatStore(Alpine) {
    Alpine.store('uiNotification', {
      // Compatibility wrapper: route notifications to `uiToast`.
      showMessage(text, variant = 'info', duration = 3500) {
        try { Alpine.store('uiToast')?.show?.(text, variant, duration); } catch (e) {}
      },
      success(text, duration = 3500) { this.showMessage(text, 'success', duration); },
      error(text, duration = 4500) { this.showMessage(text, 'error', duration); },
      warning(text, duration = 4000) { this.showMessage(text, 'warning', duration); },
      info(text, duration = 3500) { this.showMessage(text, 'info', duration); }
    });
  };
})();

