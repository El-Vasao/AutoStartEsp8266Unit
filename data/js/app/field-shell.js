// Field shell helpers (help popover toggling) for forms
(function () {
  const APP = (window.APP = window.APP || {});
  APP.ui = APP.ui || {};

  /**
   * Mixes helper methods into a fieldRow-like controller.
   * Compatible with both settingsForm.fieldRow and programForm.fieldRow objects.
   */
  APP.ui.withFieldShell = function withFieldShell(row) {
    const r = row && typeof row === 'object' ? row : {};
    if (r.__withFieldShell) return r;
    return Object.assign(r, {
      __withFieldShell: true,
      toggleHelp() {
        try { this.openHelp = !this.openHelp; } catch (e) {}
      },
      closeHelpSafe() {
        try { this.openHelp = false; } catch (e) {}
      },
      shouldShowHelp() {
        try { return !!(this.openHelp && ((this.field && this.field.help) || this.hasError?.())); } catch (e) { return false; }
      }
    });
  };
})();

