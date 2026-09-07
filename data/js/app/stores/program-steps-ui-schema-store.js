// program UI schema store (steps/actions metadata)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerProgramStepsUiSchemaStore = function registerProgramStepsUiSchemaStore(Alpine) {
    function normalizeParams(params) {
      try {
        const out = (params && typeof params === 'object') ? params : {};
        for (const k of Object.keys(out)) {
          const p = out[k];
          if (!p || typeof p !== 'object') continue;
          if (!p.valueType) p.valueType = APP.schemaNormalize?.inferValueTypeFromParam?.(p) || 'string';
        }
        return out;
      } catch (e) {
        return (params && typeof params === 'object') ? params : {};
      }
    }

    Alpine.store('programStepsUiSchema', {
      loaded: false,
      loading: false,
      loadOk: true,
      actionCategories: [],
      params: {},

      async load() {
        if (this.loaded) return;
        if (this.loading) return;
        this.loading = true;
        this.loadOk = true;
        try {
          const json = APP.preloaded?.programStepsUiSchema;
          if (!json || typeof json !== 'object') throw new Error('bundled schema unavailable');
          const cats = json?.programSteps?.actionCategories;
          const params = json?.programSteps?.params;
          if (!Array.isArray(cats)) throw new Error('invalid schema');
          this.actionCategories = cats;
          this.params = normalizeParams(params);
          this.loaded = true;
        } catch (e) {
          this.loadOk = false;
          this.loaded = false;
        } finally {
          this.loading = false;
        }
      }
    });
  };
})();

