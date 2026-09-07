// busy store (blocking modal)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerUiBusyModalStore = function registerUiBusyModalStore(Alpine) {
    Alpine.store('uiBusy', {
      open: false,
      title: 'Сохранение',
      message: 'Идёт запись…',
      okMode: false,
      okLabel: 'OK',
      show({ title = 'Сохранение', message = 'Идёт запись…' } = {}) {
        this.title = String(title || 'Сохранение');
        this.message = String(message || 'Идёт запись…');
        this.okMode = false;
        this.open = true;
      },
      showOk({ title = 'Готово', message = '', okLabel = 'OK' } = {}) {
        this.title = String(title || 'Готово');
        this.message = String(message || '');
        this.okLabel = String(okLabel || 'OK');
        this.okMode = true;
        this.open = true;
      },
      ok() {
        this.hide();
      },
      hide() {
        this.open = false;
        this.okMode = false;
      }
    });
  };
})();

