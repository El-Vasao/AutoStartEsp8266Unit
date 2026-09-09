// SoftAP UI init: HTTP checklist, then wait SSE panel-ready; hard safety unlock if hung.
(function () {
  document.addEventListener('alpine:init', () => {
    try {
      window.APP?.stores?.init?.(window.Alpine);
      window.APP?.gestures?.init?.(window.Alpine);

      (async () => {
        const Alpine = window.Alpine;
        const log = window.APP?.utils?.log;
        const timeouts = window.APP?.contract?.api?.timeoutsMs || {};
        const phaseGapMs = Number(timeouts.initPhaseGapMs) || 400;
        const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
        // Longer than checklist + sseInitDeadlineMs so normal SSE-wait UX is not cut short.
        const SAFETY_UNLOCK_MS = 20000;

        let finished = false;
        let safetyTimer = null;
        const ui = () => {
          try { return Alpine?.store?.('uiState'); } catch (e) { return null; }
        };
        const setPhase = (p) => {
          try { ui()?.setInitPhase?.(p); } catch (e) {}
        };
        const setProgress = (pct, label) => {
          try { ui()?.setInitProgress?.(pct, label); } catch (e) {}
        };
        const pushReason = (code, detail) => {
          try { ui()?.pushInitReason?.(code, detail); } catch (e) {}
        };
        const clearSafety = () => {
          if (safetyTimer) {
            clearTimeout(safetyTimer);
            safetyTimer = null;
          }
        };
        const unlock = (opts) => {
          if (finished) return;
          finished = true;
          clearSafety();
          try { ui()?.unlock?.(opts); } catch (e) {}
        };
        const failInit = (message, phase = 'degraded') => {
          if (finished) return;
          finished = true;
          clearSafety();
          try { ui()?.failInit?.(message, phase); } catch (e) {}
        };
        const hasValidBootstrap = () => {
          try {
            const st = Alpine.store('deviceStatus');
            return !!(st && st.hwMap && typeof st.hwMap === 'object' && st.hwCounts && st.hwCounts.locked);
          } catch (e) {
            return false;
          }
        };

        const withTimeout = async (p, ms = 6500) => {
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
        const phaseGap = async () => { await sleep(phaseGapMs); };

        async function postUiReady(sessionId) {
          const readyUrl = window.APP?.api?.endpoints?.uiReady
            || window.APP?.contract?.api?.endpoints?.uiReady
            || '/ui/ready';
          const body = new URLSearchParams();
          body.set('id', String(sessionId));
          const deviceFetch = window.APP?.api?.deviceFetch;
          const res = deviceFetch
            ? await deviceFetch(readyUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
                body: body.toString()
              })
            : await fetch(readyUrl, {
                method: 'POST',
                cache: 'no-store',
                headers: {
                  'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
                  'X-Requested-With': 'ElLineUI'
                },
                body: body.toString()
              });
          if (!res || !res.ok) pushReason('ui_ready_failed', `status=${res && res.status}`);
          return !!(res && res.ok);
        }

        function startSseBackgroundOnly(bootstrapUiLease) {
          // Used when UI already unlocked/failed — still need EventSource + /ui/ready.
          try { window.APP?.sse?.init?.(Alpine); } catch (e) { pushReason('sse_init_failed', e?.message || e); }
          try { if (bootstrapUiLease) window.APP?.sse?.configureHeartbeat?.(bootstrapUiLease); } catch (e) {}
          const quietMs = Number(timeouts.cellularAfterUiQuietMs) || 15000;
          const sseDelayMs = Number(timeouts.sseStartDelayMs) || 1500;
          const deadlineMs = Number(timeouts.sseInitDeadlineMs) || 12000;
          const startedAt = Date.now();
          (async () => {
            try {
              await sleep(sseDelayMs);
              if (typeof window.APP?.sse?.startEvents === 'function') {
                const ok = await window.APP.sse.startEvents({ timeoutMs: deadlineMs });
                if (!ok) pushReason('sse_panel_timeout', 'mode/hardware not received (background)');
              }
            } catch (e) {
              pushReason('sse_start_failed', e?.message || e);
            }
            try {
              const elapsed = Date.now() - startedAt;
              await sleep(Math.max(0, quietMs - elapsed));
              if (!window.APP?.sse?.isConnected?.()) {
                pushReason('ui_ready_skipped', 'SSE not connected after quiet');
              } else if (!hasValidBootstrap()) {
                pushReason('ui_ready_skipped', 'bootstrap invalid');
              } else {
                const sessionId = window.APP?.uiSessionId || window.APP?.sse?._ctx?.uiSessionId;
                if (!sessionId) pushReason('ui_ready_no_session', 'missing uiSessionId');
                else await postUiReady(sessionId);
              }
            } catch (e) {
              pushReason('ui_ready_exception', e?.message || e);
            }
          })().catch((e) => pushReason('sse_bg_exception', e?.message || e));
        }

        try { ui()?.beginInit?.(); } catch (e) {}

        // Hard safety if HTTP/SSE path hangs (F5 SoftAP). Longer than normal checklist+SSE wait.
        safetyTimer = setTimeout(() => {
          if (finished) return;
          pushReason('init_safety_unlock', `forced after ${SAFETY_UNLOCK_MS}ms`);
          unlock({ degraded: true });
        }, SAFETY_UNLOCK_MS);

        let bootstrapUiLease = null;

        // --- bootstrap ---
        setPhase('bootstrap');
        setProgress(8, 'Bootstrap…');
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
            if (lowMem) return Math.min(12000, 1500 * (1 << Math.min(attempt, 3)));
            return 400 + attempt * 350;
          };
          let ok = false;
          for (let i = 0; i < 6; i++) {
            if (finished) break;
            setProgress(10 + i * 2, `Bootstrap (попытка ${i + 1})…`);
            const res = await runBootstrapRequest();
            if (res?.__timeout) pushReason('bootstrap_timeout', `attempt=${i + 1}`);
            if (res?.__error) pushReason('bootstrap_error', res.__error?.message || res.__error);
            if (res?.ok) {
              ok = true;
              Alpine.store('deviceStatus')?.applyBootstrap?.(res.data);
              bootstrapUiLease = res.data?.uiLease || null;
              break;
            }
            const lowMem = !!isLowMemoryResponse?.(res);
            if (lowMem) pushReason('bootstrap_low_memory', `attempt=${i + 1}`);
            await sleep(retryDelayMs(i, lowMem));
          }
          if (finished) {
            startSseBackgroundOnly(bootstrapUiLease);
            return;
          }
          if (!ok || !hasValidBootstrap()) {
            pushReason('bootstrap_unavailable', 'GET /bootstrap failed');
            failInit('Не удалось загрузить bootstrap (/bootstrap). UI заблокирован.');
          } else {
            setProgress(28, 'Bootstrap готов');
          }
        } catch (e) {
          pushReason('bootstrap_exception', e?.message || e);
          failInit('Не удалось загрузить bootstrap. UI заблокирован.');
        }

        if (Alpine.store('uiState')?.initFailed || finished) {
          startSseBackgroundOnly(bootstrapUiLease);
          return;
        }

        await phaseGap();
        if (finished) { startSseBackgroundOnly(bootstrapUiLease); return; }
        setPhase('schemas');
        setProgress(35, 'Схема настроек…');
        try {
          for (let i = 0; i < 3; i++) {
            if (finished) break;
            await withTimeout(Alpine.store('settingsUiSchema')?.load?.(), 6500);
            if (Alpine.store('settingsUiSchema')?.loaded) break;
            await sleep(200 + i * 250);
          }
          if (!Alpine.store('settingsUiSchema')?.loaded) pushReason('settings_schema_missing', 'settingsUiSchema not loaded');
        } catch (e) { pushReason('settings_schema_exception', e?.message || e); }

        await phaseGap();
        if (finished) { startSseBackgroundOnly(bootstrapUiLease); return; }
        setProgress(42, 'Валидатор настроек…');
        try {
          for (let i = 0; i < 2; i++) {
            if (finished) break;
            await withTimeout(Alpine.store('settingsValidator')?.load?.(), 6500);
            if (Alpine.store('settingsValidator')?.loaded) break;
            await sleep(150 + i * 200);
          }
        } catch (e) { pushReason('settings_validator_exception', e?.message || e); }

        await phaseGap();
        if (finished) { startSseBackgroundOnly(bootstrapUiLease); return; }
        setPhase('data');
        setProgress(52, 'Конфигурация…');
        try {
          for (let i = 0; i < 4; i++) {
            if (finished) break;
            await withTimeout(Alpine.store('settings')?.load?.(true), 8000);
            if (Alpine.store('settings')?.loaded) break;
            await sleep(350 + i * 400);
          }
          if (!Alpine.store('settings')?.loaded) pushReason('settings_missing', 'settings not loaded');
        } catch (e) { pushReason('settings_exception', e?.message || e); }

        await phaseGap();
        if (finished) { startSseBackgroundOnly(bootstrapUiLease); return; }
        setProgress(62, 'Схема программ…');
        try {
          for (let i = 0; i < 3; i++) {
            if (finished) break;
            await withTimeout(Alpine.store('programStepsUiSchema')?.load?.(), 6500);
            if (Alpine.store('programStepsUiSchema')?.loaded) break;
            await sleep(200 + i * 250);
          }
          if (!Alpine.store('programStepsUiSchema')?.loaded) pushReason('program_steps_schema_missing', 'programStepsUiSchema not loaded');
        } catch (e) { pushReason('program_steps_schema_exception', e?.message || e); }

        await phaseGap();
        if (finished) { startSseBackgroundOnly(bootstrapUiLease); return; }
        setProgress(70, 'Список программ…');
        try {
          for (let i = 0; i < 3; i++) {
            if (finished) break;
            await withTimeout(Alpine.store('programs')?.loadList?.(true), 8000);
            if (Alpine.store('programs')?.loaded) break;
            await sleep(300 + i * 350);
          }
        } catch (e) { pushReason('programs_list_exception', e?.message || e); }

        if (finished) {
          startSseBackgroundOnly(bootstrapUiLease);
          return;
        }

        let initOk = false;
        {
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
            setPhase('checklist');
            setProgress(78, 'Чеклист готов');
            initOk = true;
          }
        }

        try {
          const s = Alpine.store('uiState');
          if (s?.initReasons?.length) log?.warn?.('[init] reasons:', s.initReasons);
        } catch (e) {}

        if (!initOk || Alpine.store('uiState')?.initFailed || finished) {
          startSseBackgroundOnly(bootstrapUiLease);
          return;
        }

        // --- SSE: keep lock until panel-ready (or deadline / safety) ---
        await phaseGap();
        if (finished) { startSseBackgroundOnly(bootstrapUiLease); return; }
        setPhase('sse');
        setProgress(82, 'Подключение SSE…');
        try { window.APP?.sse?.init?.(Alpine); } catch (e) { pushReason('sse_init_failed', e?.message || e); }
        try { if (bootstrapUiLease) window.APP?.sse?.configureHeartbeat?.(bootstrapUiLease); } catch (e) {}

        const quietMs = Number(timeouts.cellularAfterUiQuietMs) || 15000;
        const sseDelayMs = Number(timeouts.sseStartDelayMs) || 1500;
        const deadlineMs = Number(timeouts.sseInitDeadlineMs) || 12000;
        const startedAt = Date.now();

        await sleep(sseDelayMs);
        if (finished) return;

        setProgress(86, 'Ждём статус устройства…');
        let panelReady = false;
        try {
          if (typeof window.APP?.sse?.startEvents !== 'function') {
            pushReason('sse_start_failed', 'startEvents missing');
          } else {
            panelReady = !!(await window.APP.sse.startEvents({ timeoutMs: deadlineMs }));
            if (!panelReady) pushReason('sse_panel_timeout', 'mode/hardware not received within deadline');
          }
        } catch (e) {
          pushReason('sse_start_failed', e?.message || e);
        }

        if (finished) return;

        if (!panelReady) {
          pushReason('sse_panel_timeout_unlock', 'unlocking without panel-ready');
          unlock({ degraded: true });
        } else {
          setProgress(100, 'Готово');
          unlock();
        }

        try {
          const elapsed = Date.now() - startedAt;
          await sleep(Math.max(0, quietMs - elapsed));
          if (!window.APP?.sse?.isConnected?.()) {
            pushReason('ui_ready_skipped', 'SSE not connected after quiet');
          } else if (!hasValidBootstrap()) {
            pushReason('ui_ready_skipped', 'bootstrap invalid');
          } else {
            const sessionId = window.APP?.uiSessionId || window.APP?.sse?._ctx?.uiSessionId;
            if (!sessionId) pushReason('ui_ready_no_session', 'missing uiSessionId');
            else await postUiReady(sessionId);
          }
        } catch (e) {
          pushReason('ui_ready_exception', e?.message || e);
        }
      })();
    } catch (e) {
      window.APP?.utils?.log?.error?.('bootstrap failed', e);
    }
  });
})();
