// App bootstrap: registers alpine:init listener before Alpine core runs.
(function () {
  document.addEventListener('alpine:init', () => {
    try {
      window.APP?.stores?.init?.(window.Alpine);
      window.APP?.gestures?.init?.(window.Alpine);

      // UI_SCHEMA + config are loaded via HTTP. Hardware limits arrive via /bootstrap and are updated via SSE.
      // Keep UI locked until schema+settings+programSchema are ready; SSE starts after minimal bootstrap.
      (async () => {
        const Alpine = window.Alpine;
        const log = window.APP?.utils?.log;

        let unlocked = false;
        const ui = () => {
          try { return Alpine?.store?.('uiState'); } catch (e) { return null; }
        };
        const setPhase = (p) => {
          try { ui()?.setInitPhase?.(p); } catch (e) {}
        };
        const pushReason = (code, detail) => {
          try { ui()?.pushInitReason?.(code, detail); } catch (e) {}
        };
        const unlock = () => {
          if (unlocked) return;
          unlocked = true;
          try { Alpine.store('uiState').locked = false; } catch (e) {}
        };
        const failInit = (message, phase = 'degraded') => {
          try {
            Alpine.store('uiState').initFailed = true;
            Alpine.store('uiState').initError = String(message || 'Ошибка инициализации UI.');
            setPhase(phase);
          } catch (e) {}
          unlock();
        };
        const hasValidBootstrap = () => {
          try {
            const st = Alpine.store('deviceStatus');
            return !!(st && st.hwMap && typeof st.hwMap === 'object' && st.hwCounts && st.hwCounts.locked);
          } catch (e) {
            return false;
          }
        };

        try { Alpine.store('uiState').locked = true; } catch (e) {}
        try {
          const s = Alpine.store('uiState');
          s.initFailed = false;
          s.initError = '';
          s.initReasons = [];
          s.setInitPhase?.('starting');
        } catch (e) {}

        const timeoutMs = 6500;
        const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
        const withTimeout = async (p, ms = timeoutMs) => {
          // Never throw from here: bootstrap is best-effort and must keep progressing.
          let t = null;
          try {
            return await Promise.race([
              Promise.resolve(p),
              new Promise((resolve) => { t = setTimeout(() => resolve({ __timeout: true }), ms); })
            ]);
          } catch (e) {
            return { __error: e };
          } finally {
            if (t) clearTimeout(t);
          }
        };

        // Phase: minimal bootstrap (/bootstrap)
        setPhase('bootstrap');
        try {
          const apiJson = window.APP?.api?.apiJson;
          const isLowMemoryResponse = window.APP?.api?.isLowMemoryResponse;
          const bootstrapUrl = window.APP?.api?.endpoints?.bootstrap || window.APP?.contract?.api?.endpoints?.bootstrap || '/bootstrap';
          if (!apiJson) throw new Error('apiJson unavailable');
          const bootstrapState = (window.APP.__bootstrapState = window.APP.__bootstrapState || { inFlight: null });
          const runBootstrapRequest = () => {
            if (bootstrapState.inFlight) return bootstrapState.inFlight;
            bootstrapState.inFlight = withTimeout(apiJson(bootstrapUrl, { timeoutMs: 4000 }), 6500)
              .finally(() => { bootstrapState.inFlight = null; });
            return bootstrapState.inFlight;
          };
          const retryDelayMs = (attempt, lowMem) => {
            if (lowMem) return Math.min(12000, 1200 * (1 << Math.min(attempt, 3)));
            return 180 + attempt * 240;
          };
          let ok = false;
          for (let i = 0; i < 6; i++) {
            const res = await runBootstrapRequest();
            if (res?.__timeout) pushReason('bootstrap_timeout', `attempt=${i + 1}`);
            if (res?.__error) pushReason('bootstrap_error', res.__error?.message || res.__error);
            if (res?.ok) {
              ok = true;
              Alpine.store('deviceStatus')?.applyBootstrap?.(res.data);
              try { window.APP?.sse?.configureHeartbeat?.(res.data?.uiLease); } catch (e) {}
              break;
            }
            const lowMem = !!isLowMemoryResponse?.(res);
            if (lowMem) pushReason('bootstrap_low_memory', `attempt=${i + 1}`);
            await sleep(retryDelayMs(i, lowMem));
          }
          if (!ok) pushReason('bootstrap_unavailable', 'GET /bootstrap failed');

          // Safety-first: without bootstrap hwMap/hwCounts, UI must not operate.
          if (!ok || !hasValidBootstrap()) {
            failInit('Не удалось загрузить bootstrap (/bootstrap). UI заблокирован.');
            return;
          }
        } catch (e) {
          pushReason('bootstrap_exception', e?.message || e);
          failInit('Не удалось загрузить bootstrap. UI заблокирован.');
          return;
        }

        // Start SSE only after minimal bootstrap attempt, so required stores are present.
        try { window.APP?.sse?.init?.(Alpine); } catch (e) { pushReason('sse_init_failed', e?.message || e); }

        // Phase: schemas + validators
        setPhase('schemas');
        // A few retries help on ESP8266 (radio + lwIP buffers + first-load spikes).
        try {
          let ok = false;
          for (let i = 0; i < 3; i++) {
            const r = await withTimeout(Alpine.store('settingsUiSchema')?.load?.(), 6500);
            if (r?.__timeout) pushReason('settings_schema_timeout', `attempt=${i + 1}`);
            if (r?.__error) pushReason('settings_schema_error', r.__error?.message || r.__error);
            if (Alpine.store('settingsUiSchema')?.loaded) { ok = true; break; }
            await sleep(140 + i * 220);
          }
          if (!ok) pushReason('settings_schema_missing', 'settingsUiSchema not loaded');
        } catch (e) { pushReason('settings_schema_exception', e?.message || e); }

        // Validation schema (Ajv) - best-effort for UI, but required for writes (enforced elsewhere).
        try {
          for (let i = 0; i < 2; i++) {
            const r = await withTimeout(Alpine.store('settingsValidator')?.load?.(), 6500);
            if (r?.__timeout) pushReason('settings_validator_timeout', `attempt=${i + 1}`);
            if (r?.__error) pushReason('settings_validator_error', r.__error?.message || r.__error);
            if (Alpine.store('settingsValidator')?.loaded) break;
            await sleep(80 + i * 120);
          }
        } catch (e) { pushReason('settings_validator_exception', e?.message || e); }

        // Phase: data (config + program editor schema + programs list)
        setPhase('data');
        try {
          let ok = false;
          for (let i = 0; i < 3; i++) {
            const r = await withTimeout(Alpine.store('settings')?.load?.(true), 6500);
            if (r?.__timeout) pushReason('settings_load_timeout', `attempt=${i + 1}`);
            if (r?.__error) pushReason('settings_load_error', r.__error?.message || r.__error);
            if (Alpine.store('settings')?.loaded) { ok = true; break; }
            await sleep(220 + i * 320);
          }
          if (!ok) pushReason('settings_missing', 'settings not loaded');
        } catch (e) { pushReason('settings_exception', e?.message || e); }

        // Program steps schema (editor metadata)
        try {
          let ok = false;
          for (let i = 0; i < 3; i++) {
            const r = await withTimeout(Alpine.store('programStepsUiSchema')?.load?.(), 6500);
            if (r?.__timeout) pushReason('program_steps_schema_timeout', `attempt=${i + 1}`);
            if (r?.__error) pushReason('program_steps_schema_error', r.__error?.message || r.__error);
            if (Alpine.store('programStepsUiSchema')?.loaded) { ok = true; break; }
            await sleep(160 + i * 240);
          }
          if (!ok) pushReason('program_steps_schema_missing', 'programStepsUiSchema not loaded');
        } catch (e) { pushReason('program_steps_schema_exception', e?.message || e); }

        // Programs index early-load (used across UI: program names, selectors, etc.)
        try {
          for (let i = 0; i < 3; i++) {
            const r = await withTimeout(Alpine.store('programs')?.loadList?.(true), 6500);
            if (r?.__timeout) pushReason('programs_list_timeout', `attempt=${i + 1}`);
            if (r?.__error) pushReason('programs_list_error', r.__error?.message || r.__error);
            if (Alpine.store('programs')?.loaded) break;
            await sleep(200 + i * 260);
          }
        } catch (e) { pushReason('programs_list_exception', e?.message || e); }

        // Decide ready vs degraded
        try {
          const schemaOk = !!Alpine.store('settingsUiSchema')?.loaded;
          const settingsOk = !!Alpine.store('settings')?.loaded;
          const progSchemaOk = !!Alpine.store('programStepsUiSchema')?.loaded;
          if (!schemaOk || !settingsOk || !progSchemaOk) {
            const msg = (!schemaOk && !settingsOk && !progSchemaOk)
              ? 'Не удалось загрузить схему, конфигурацию и схему программ.'
              : (!schemaOk ? 'Не удалось загрузить схему настроек.'
                    : (!settingsOk ? 'Не удалось загрузить конфигурацию.' : 'Не удалось загрузить схему редактора программ.'));
            failInit(msg);
          } else {
            setPhase('ready');
          }
        } catch (e) {
          pushReason('init_finalize_exception', e?.message || e);
          failInit('Ошибка инициализации UI.');
        }

        try {
          // Surface a compact hint to console for diagnostics (does not affect UX).
          const s = Alpine.store('uiState');
          if (s?.initReasons?.length) log?.warn?.('[init] reasons:', s.initReasons);
        } catch (e) {}

        unlock();
      })();
    } catch (e) {
      window.APP?.utils?.log?.error?.('bootstrap failed', e);
    }
  });
})();

