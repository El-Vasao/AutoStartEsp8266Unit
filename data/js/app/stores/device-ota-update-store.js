// device OTA update store
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerDeviceOtaUpdateStore = function registerDeviceOtaUpdateStore(Alpine) {
    Alpine.store('deviceOta', {
      started: false,
      uploading: false,
      progress: 0,
      status: '',
      file: null,
      setFile(file) { this.file = file; },
      async start() {
        if (this.started) return;
        if (!this.file) { Alpine.store('uiStatusBar').flash('Выберите файл обновления.', 'warning', 4000); return; }
        Alpine.store('uiStatusBar').clearHardwareError();
        this.started = true;
        this.uploading = true;
        this.progress = 0;
        this.status = 'Подготовка…';

        APP.utils.LS.set('otaPostRebootGoPanel', '1');
        APP.utils.LS.set('otaInProgress', '1');
        const formData = new FormData();
        formData.append('data', this.file);
        const xhr = new XMLHttpRequest();
        xhr.open('POST', (APP.api.endpoints?.upload || '/upload'), true);
        xhr.upload.onprogress = (e) => {
          if (e.lengthComputable) {
            this.progress = (e.loaded / e.total * 100).toFixed(1);
            this.status = 'Загрузка...';
          }
        };
        xhr.onload = () => {
          if (xhr.status === 200) {
            this.progress = 100;
            this.status = 'Файл загружен, запуск OTA…';
            APP.api.apiJson((APP.api.endpoints?.otaStart || '/ota/start'), { method: 'POST', timeoutMs: (APP.contract?.api?.timeoutsMs?.ota ?? 30000) })
              .then(({ ok, data }) => {
                if (ok && data && data.success) {
                  this.status = 'Обновление запущено, ожидайте перезагрузку…';
                } else {
                  throw new Error((data && data.message) || 'OTA start failed');
                }
              })
              .catch(() => {
                Alpine.store('uiStatusBar').setHardwareError('Не удалось запустить OTA. Перезагрузите устройство и повторите попытку.');
                this.status = 'Ошибка запуска OTA';
              });
          } else {
            Alpine.store('uiStatusBar').setHardwareError('Ошибка загрузки файла обновления. Перезагрузите устройство при необходимости.');
            this.status = 'Ошибка загрузки';
          }
        };
        xhr.onerror = () => {
          Alpine.store('uiStatusBar').setHardwareError('Ошибка сети при загрузке OTA. Проверьте соединение.');
          this.status = 'Ошибка сети';
        };
        xhr.send(formData);
      }
    });
  };
})();

