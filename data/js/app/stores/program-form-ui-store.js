// program form UI helpers (like settingsForm.fieldRow, but for programEditor)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.stores = APP.stores || {};

  APP.stores.registerProgramFormStore = function registerProgramFormStore(Alpine) {
    function isIndex(s) { return /^\d+$/.test(String(s)); }

    function getByPath(obj, path) {
      try {
        if (!obj) return undefined;
        const parts = String(path || '').split('.').filter(Boolean);
        let cur = obj;
        for (const p of parts) {
          if (cur == null) return undefined;
          cur = isIndex(p) ? cur[Number(p)] : cur[p];
        }
        return cur;
      } catch (e) { return undefined; }
    }

    function setByPath(obj, path, value) {
      try {
        if (!obj) return false;
        const parts = String(path || '').split('.').filter(Boolean);
        if (!parts.length) return false;
        let cur = obj;
        for (let i = 0; i < parts.length - 1; i++) {
          const p = parts[i];
          const next = parts[i + 1];
          const idx = isIndex(p) ? Number(p) : null;
          if (idx !== null) {
            if (!Array.isArray(cur)) return false;
            if (cur[idx] == null || typeof cur[idx] !== 'object') cur[idx] = (isIndex(next) ? [] : {});
            cur = cur[idx];
          } else {
            if (cur[p] == null || typeof cur[p] !== 'object') cur[p] = (isIndex(next) ? [] : {});
            cur = cur[p];
          }
        }
        const last = parts[parts.length - 1];
        if (isIndex(last)) {
          const idx = Number(last);
          if (!Array.isArray(cur)) return false;
          cur[idx] = value;
        } else {
          cur[last] = value;
        }
        return true;
      } catch (e) { return false; }
    }

    Alpine.store('programForm', {
      getByPath,
      setByPath,
      optionsForField(field, opts = {}) {
        try {
          if (!field || typeof field !== 'object') return [];
          if (Array.isArray(field.options)) return field.options;
          const from = String(field.optionsFrom || '');
          if (!from) return [];
          const AlpineRef = opts?.Alpine || Alpine;
          const shared = APP.options?.forSource?.(from, AlpineRef);
          if (!Array.isArray(shared)) return [];

          return shared;
        } catch (e) {
          return [];
        }
      },
      coerceValue(field, raw) {
        try {
          const t = String(field?.type || '');
          const vt = String(field?.valueType || '');
          const coerce = (v) => (APP.coerce?.byValueType ? APP.coerce.byValueType(vt, v, field?.default) : v);
          if (t === 'number') {
            // Use plain string to avoid `1e+…` in UI.
            const s = APP.utils?.numberToPlainString ? APP.utils.numberToPlainString(raw) : String(raw ?? '');
            if (s === '') return '';
            const n = Number(s);
            return coerce(Number.isFinite(n) ? n : raw);
          }
          return coerce(raw);
        } catch (e) {
          return raw;
        }
      },

      /**
       * Create a row-controller for a program field (same ergonomics as settingsForm.fieldRow).
       *
       * @param {object} target - object to read/write (usually programEditor.form or a step object)
       * @param {object} field - {type,label,help,valueType,...}
       * @param {string} path - dotted path inside target (e.g. "name" or "ms")
       * @param {object} opts - { errors, errorKey, onChange }
       */
      fieldRow(target, field, path, opts = {}) {
        const ui = this;
        const resolvedPath = String(path ?? '');
        const errorKey = String(opts?.errorKey || resolvedPath || '');
        const errors = opts?.errors || null;
        const onChange = (typeof opts?.onChange === 'function') ? opts.onChange : null;
        const onSelect = (typeof opts?.onSelect === 'function') ? opts.onSelect : null;

        return {
          field,
          path: resolvedPath,
          openHelp: false,
          _touched: false,
          _errTimer: null,
          _wasInvalid: false,

          get value() { return ui.getByPath(target, this.path); },
          setValue(v) {
            ui.setByPath(target, this.path, v);
            try { onChange?.(); } catch (e) {}
          },

          hasError() {
            try { return !!(errors && errorKey && errors[errorKey]); } catch (e) { return false; }
          },
          errorText() {
            try { return String((errors && errorKey) ? (errors[errorKey] || '') : ''); } catch (e) { return ''; }
          },

          helpBtnClass() {
            const base = (String(field?.type || '') === 'toggle') ? 'help-btn-toggle' : 'help-btn-control';
            return this.hasError() ? `${base} has-error` : base;
          },
          closeHelp() { this.openHelp = false; },
          onHelpKeydown(e) { if (e && e.key === 'Escape') this.openHelp = false; },

          inputClass() {
            // Same shape as settingsForm.fieldInputClass(): class-map object.
            return { 'field-invalid': this.hasError() };
          },

          displayValue() {
            const vt = String(field?.valueType || '');
            if (vt === 'int' || vt === 'float') return APP.utils?.numberToPlainString ? APP.utils.numberToPlainString(this.value) : String(this.value ?? '');
            return (this.value ?? '');
          },

          options() { return ui.optionsForField(field, opts); },

          _closeHelpIfNotError() {
            try {
              if (!this.openHelp) return;
              if (this.hasError()) return;
              this.closeHelp();
            } catch (e) {}
          },
          _syncErrorPopoverDebounced() {
            try {
              if (this._errTimer) clearTimeout(this._errTimer);
              this._errTimer = setTimeout(() => {
                try {
                  const nowInvalid = this.hasError();
                  const becameInvalid = !this._wasInvalid && nowInvalid;
                  this._wasInvalid = nowInvalid;
                  if (!this._touched) return;
                  if (becameInvalid) this.openHelp = true;
                  // If it became valid again, keep popover state as user chose (same as settings).
                } catch (e) {}
              }, 160);
            } catch (e) {}
          },

          onTextInput(e) {
            this._touched = true;
            this._closeHelpIfNotError();
            this.setValue(e?.target?.value ?? '');
            this._syncErrorPopoverDebounced();
          },
          onNumberInput(e) {
            this._touched = true;
            this._closeHelpIfNotError();
            const v = ui.coerceValue(field, e?.target?.value);
            this.setValue(v);
            this._syncErrorPopoverDebounced();
          },
          onSelectChange(e) {
            this._touched = true;
            this._closeHelpIfNotError();
            const v = ui.coerceValue(field, e?.target?.value);
            this.setValue(v);
            try { onSelect?.(v, e); } catch (e2) {}
            this._syncErrorPopoverDebounced();
          }
        };
      }
    });
  };
})();

