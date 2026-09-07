// Defer Alpine.start so we can load schema/app modules after Alpine core,
// but still register stores/components before Alpine initializes the DOM.
(function () {
  if (window.__alpineDeferLoading) return;
  window.__alpineDeferLoading = true;
  window.__alpineStartFn = null;

  // Alpine CDN will call this hook if present
  window.deferLoadingAlpine = function (start) {
    window.__alpineStartFn = start;
  };
})();

