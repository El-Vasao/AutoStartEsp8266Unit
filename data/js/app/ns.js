// Minimal namespace to allow splitting app code into multiple files
// without introducing a bundler. Loaded before Alpine core.
(function () {
  if (!window.APP) window.APP = {};
  window.APP.utils = window.APP.utils || {};
  window.APP.api = window.APP.api || {};
  window.APP.preloaded = window.APP.preloaded || {};
  window.APP.stores = window.APP.stores || {};
  window.APP.gestures = window.APP.gestures || {};
  window.APP.sse = window.APP.sse || {};
})();

