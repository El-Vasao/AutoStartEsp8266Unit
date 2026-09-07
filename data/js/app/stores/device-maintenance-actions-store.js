// device maintenance actions store (misc UI actions)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerDeviceMaintenanceActionsStore = function registerDeviceMaintenanceActionsStore(Alpine) {
    Alpine.store('deviceMaintenance', {
      performReboot() {
        Alpine.store('uiDialog').show({
          title: 'Перезагрузка устройства',
          message: 'Вы уверены, что хотите перезагрузить устройство?',
          onConfirm: async () => {
            try {
              // Soft UI guard: reboot/reset/OTA are assumed safe only in trusted network (device AP).
              // If user enabled debug mode, we still show confirmation above.
              await APP.api.apiJson(APP.api.endpoints?.reboot || '/reboot', { method: 'POST' });
              Alpine.store('uiNotification').info('Перезагрузка...');
            } catch (e) {
              Alpine.store('uiNotification').error('Ошибка сети');
            }
          }
        });
      },
      performModemReboot() {
        Alpine.store('uiDialog').show({
          title: 'Перезагрузка модема',
          message: 'Перезагрузить SIM800 (AT+CFUN=1,1)?',
          onConfirm: async () => {
            try {
              await APP.api.apiJson('/modem/reboot', { method: 'POST' });
              Alpine.store('uiNotification').info('Перезагрузка модема запрошена...');
            } catch (e) {
              Alpine.store('uiNotification').error('Ошибка сети');
            }
          }
        });
      },
      resetBaseConfig() {
        Alpine.store('uiDialog').show({
          title: 'Сброс конфигурации',
          message: 'Вы уверены, что хотите сбросить базовую конфигурацию к заводским настройкам? Это действие необратимо.',
          onConfirm: async () => {
            try {
              const res = await APP.api.runFlashWrite({
                Alpine,
                busyTitle: 'Сохранение',
                busyMessage: 'Сброс конфигурации…',
                expectedLastOp: 'reset_config',
                timeoutMsCommit: 8000,
                request: async () => await APP.api.apiJson(APP.api.endpoints?.configReset || '/config/reset', { method: 'POST' }),
                onCommitOk: async () => {
                  Alpine.store('uiNotification').success('Конфигурация сброшена. Требуется перезагрузка.');
                  Alpine.store('uiBusy').showOk({
                    title: 'Сброс выполнен',
                    message: 'Конфигурация сброшена. Требуется перезагрузка устройства.',
                    okLabel: 'OK'
                  });
                }
              });
              if (res.busy409) return;
              if (!res.ok && !res.timeout) Alpine.store('uiNotification').error('Ошибка сброса');
            } catch (e) {}
          }
        });
      },
      resetAllPrograms() {
        Alpine.store('programs').resetAll();
      },
      otaFileSelected(e) {
        Alpine.store('deviceOta').setFile(e.target.files[0]);
      },
      startOTA() {
        Alpine.store('deviceOta').start();
      }
    });
  };
})();

