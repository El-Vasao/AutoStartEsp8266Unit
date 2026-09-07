// Centralized DOM ids/selectors used by runtime + gestures.
(function () {
  const APP = (window.APP = window.APP || {});
  APP.domIds = APP.domIds || {};

  APP.domIds.panelTab = 'tab-panel';

  APP.domIds.settings = {
    thermostatCard: 'settings-thermostat',
    batteryCard: 'settings-battery',
    sensorItem: (index) => `settings-sensor-${index}`,
    inputItem: (index) => `settings-input-${index}`,
    inputTriggerItem: (index) => `settings-in-trigger-${index}`,
    tempTriggerItem: (index) => `settings-temp-trigger-${index}`,
  };
})();

