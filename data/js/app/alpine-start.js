// Start Alpine after all stores/components are registered.
(function () {
  try {
    const start = window.__alpineStartFn;
    if (typeof start === 'function') start();
  } catch (e) {}
})();

