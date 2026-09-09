// SoftAP-safe SSE: one EventSource, no auto /bootstrap/live, heartbeat after panel-ready.
(function () {
  const APP = (window.APP = window.APP || {});
  APP.sse = APP.sse || {};

  APP.sse.init = function initSse(Alpine) {
    const prev = APP.sse._ctx;
    if (prev && prev.running) return;

    const endpoints = APP.api?.endpoints || APP.contract?.api?.endpoints || {};
    const timeouts = APP.api?.timeoutsMs || APP.contract?.api?.timeoutsMs || {};
    const log = APP.utils?.log;

    // Re-init after spurious teardown (SoftAP F5 / bfcache) while document still alive.
    if (prev && !prev.running) {
      try {
        if (prev.reconnectTimer) clearTimeout(prev.reconnectTimer);
        if (prev.heartbeatTimer) clearInterval(prev.heartbeatTimer);
        if (prev.staleTimer) clearInterval(prev.staleTimer);
        try { prev.source?.close?.(); } catch (e) {}
      } catch (e) {}
    }

    const ctx = (APP.sse._ctx = {
      running: true,
      stopped: false,
      eventsStarted: false,
      source: null,
      reconnectTimer: null,
      heartbeatTimer: null,
      heartbeatInFlight: false,
      backoffMs: 4000,
      state: 'idle', // idle|connecting|connected|backoff|stale
      lastEventAt: 0,
      connectingSince: 0,
      heartbeatMs: 5000,
      staleTimer: null,
      // SoftAP F5: reuse prior session id when present (avoid close/open race).
      uiSessionId: (function () {
        try {
          const raw = sessionStorage.getItem('uiSessionId');
          const n = raw ? (parseInt(raw, 10) >>> 0) : 0;
          if (n) return n;
        } catch (e) {}
        return ((((Date.now() & 0x7fffffff) ^ ((Math.random() * 0x7fffffff) | 0)) >>> 0) || 1);
      })(),
      panelReadyWaiters: [],
      gotMode: false,
      gotHardware: false,
    });
    APP.uiSessionId = ctx.uiSessionId;
    try { sessionStorage.setItem('uiSessionId', String(ctx.uiSessionId)); } catch (e) {}

    Alpine.store('net', Alpine.store('net') || {
      state: 'disconnected',
      backoffMs: 0,
      lastEventAt: 0,
      staleMs: 0
    });

    function setState(next) {
      ctx.state = next;
      try {
        const net = Alpine.store('net');
        net.state = ctx.state;
        net.backoffMs = ctx.backoffMs;
        net.lastEventAt = ctx.lastEventAt;
        net.staleMs = Math.max(0, Date.now() - (ctx.lastEventAt || 0));
      } catch (e) {}
    }

    function isPanelReady() {
      try {
        const ds = Alpine?.store?.('deviceStatus');
        if (!ds) return false;
        const modeOk = !!(ds.mode && ds.mode !== '—');
        const nRelays = Number(ds.hwCounts?.relays) || 0;
        const nInputs = Number(ds.hwCounts?.inputs) || 0;
        const relaysOk = Array.isArray(ds.relays) && (nRelays === 0 || ds.relays.length === nRelays);
        const inputsOk = Array.isArray(ds.inputs) && (nInputs === 0 || ds.inputs.length === nInputs);
        return modeOk && relaysOk && inputsOk && ctx.gotMode && ctx.gotHardware;
      } catch (e) {
        return false;
      }
    }

    function notifyPanelReady() {
      if (!isPanelReady()) return;
      if (!ctx.heartbeatTimer) startHeartbeat();
      const waiters = ctx.panelReadyWaiters.splice(0);
      for (const w of waiters) {
        try { w(true); } catch (e) {}
      }
    }

    function resetPanelFlags() {
      ctx.gotMode = false;
      ctx.gotHardware = false;
    }

    function scheduleReconnect() {
      if (ctx.reconnectTimer) return;
      if (!ctx.eventsStarted || !ctx.running) return;
      if (ctx.source) return;
      setState('backoff');
      // Until panel-ready: fast SoftAP reconnect (F5 zombie). After ready: gentler ≥4s.
      const floor = isPanelReady() ? 4000 : 500;
      const waitMs = Math.max(ctx.backoffMs, floor);
      ctx.reconnectTimer = setTimeout(() => {
        ctx.reconnectTimer = null;
        if (ctx.source) return;
        connectSSE();
      }, waitMs);
      const jitter = Math.floor(Math.random() * 400);
      ctx.backoffMs = Math.min(Math.max(ctx.backoffMs, floor) * 2 + jitter, 12000);
    }

    function buildSessionBody(close) {
      const body = new URLSearchParams();
      body.set('id', String(ctx.uiSessionId));
      if (close) body.set('close', '1');
      return body;
    }

    function postUiSession(close) {
      const url = endpoints.uiSession || '/ui/session';
      const body = buildSessionBody(close);

      if (close && navigator.sendBeacon) {
        try {
          const blob = new Blob([body.toString()], { type: 'application/x-www-form-urlencoded;charset=UTF-8' });
          if (navigator.sendBeacon(url, blob)) return Promise.resolve();
        } catch (e) {}
      }

      if (!close && ctx.heartbeatInFlight) return Promise.resolve();
      if (!close) ctx.heartbeatInFlight = true;
      try {
        const deviceFetch = APP.api?.deviceFetch;
        const doFetch = deviceFetch
          ? deviceFetch(url, {
              method: 'POST',
              keepalive: !!close,
              headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
              body: body.toString()
            })
          : fetch(url, {
              method: 'POST',
              cache: 'no-store',
              keepalive: !!close,
              headers: {
                'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
                'X-Requested-With': 'ElLineUI'
              },
              body: body.toString()
            });
        return doFetch.catch(() => {}).finally(() => {
          if (!close) ctx.heartbeatInFlight = false;
        });
      } catch (e) {
        if (!close) ctx.heartbeatInFlight = false;
        return Promise.resolve();
      }
    }

    function waitForUiSessionRegistered() {
      const url = endpoints.uiSession || '/ui/session';
      const body = buildSessionBody(false);
      const delay = ms => new Promise(cb => setTimeout(cb, ms));
      const attemptMs = 3000;
      return (async () => {
        for (let i = 0; i < 6; i++) {
          const controller = (typeof AbortController !== 'undefined') ? new AbortController() : null;
          const t = controller ? setTimeout(() => {
            try { controller.abort(); } catch (e) {}
          }, attemptMs) : null;
          try {
            const deviceFetch = APP.api?.deviceFetch;
            const opts = {
              method: 'POST',
              headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
              body: body.toString()
            };
            if (controller) opts.signal = controller.signal;
            const res = deviceFetch
              ? await deviceFetch(url, opts)
              : await fetch(url, {
                  ...opts,
                  cache: 'no-store',
                  headers: {
                    ...opts.headers,
                    'X-Requested-With': 'ElLineUI'
                  }
                });
            if (res && res.ok) return;
          } catch (e) {
            // timeout / network — retry
          } finally {
            if (t) clearTimeout(t);
          }
          await delay(200 * (i + 1));
        }
      })();
    }

    function startHeartbeat() {
      if (ctx.heartbeatTimer) return;
      if (!isPanelReady()) return;
      postUiSession(false);
      ctx.heartbeatTimer = setInterval(() => postUiSession(false), ctx.heartbeatMs);
    }

    function stopHeartbeat() {
      if (ctx.heartbeatTimer) clearInterval(ctx.heartbeatTimer);
      ctx.heartbeatTimer = null;
    }

    APP.sse.configureHeartbeat = function configureHeartbeat(lease) {
      try {
        const next = lease && Number.isFinite(lease.heartbeatMs) ? lease.heartbeatMs : null;
        if (!next || next < 500 || next > 60000) return;
        if (next === ctx.heartbeatMs) return;
        ctx.heartbeatMs = next;
        if (ctx.heartbeatTimer) {
          stopHeartbeat();
          startHeartbeat();
        }
      } catch (e) {}
    };

    function markStatusKind(kind, payload) {
      if (kind === 'mode' || ((kind === 'snapshot' || kind === 'status') && payload?.mode)) {
        ctx.gotMode = true;
      }
      if (kind === 'hardware' || kind === 'snapshot' || kind === 'status') {
        if (kind === 'hardware' || payload?.relaysById || payload?.inputsById || Array.isArray(payload?.relays)) {
          ctx.gotHardware = true;
        }
      }
    }

    function connectSSE() {
      if (!window.EventSource) {
        setState('idle');
        return;
      }
      if (ctx.source) return;

      try {
        ctx.connectingSince = Date.now();
        setState('connecting');
        ctx.source = new EventSource(endpoints.events || '/events');
      } catch (e) {
        ctx.source = null;
        scheduleReconnect();
        return;
      }

      ctx.source.onopen = () => {
        ctx.backoffMs = 4000;
        ctx.connectingSince = 0;
        setState('connected');
      };

      function onStatusPayload(kind, raw) {
        ctx.lastEventAt = Date.now();
        const payload = JSON.parse(raw);
        markStatusKind(kind, payload);
        try {
          Alpine.store('deviceStatus').patchFromSse(kind, payload);
        } catch (e) {}
        setState('connected');
        notifyPanelReady();
      }

      ctx.source.addEventListener('resync', () => {
        // Firmware paced baseline follows; do not HTTP /bootstrap/live.
        ctx.lastEventAt = Date.now();
        resetPanelFlags();
        setState('connected');
      });

      const sseKinds = ['clocks', 'hardware', 'runtime', 'program', 'flash', 'mode', 'gsm', 'error', 'status'];
      for (const k of sseKinds) {
        ctx.source.addEventListener(k, e => {
          try { onStatusPayload(k, e.data); } catch (err) { log?.warn?.('SSE payload parse failed', k, err); }
        });
      }
      ctx.source.addEventListener('log', e => {
        ctx.lastEventAt = Date.now();
        try { Alpine.store('uiLogs').add(e.data); } catch (err) {}
        setState('connected');
      });

      ctx.source.onerror = () => {
        setState('disconnected');
        const es = ctx.source;
        if (!es) {
          scheduleReconnect();
          return;
        }
        // Browser is auto-reconnecting the same EventSource — do NOT close or open a second one
        // (MAX_SSE_CLIENTS=1 → reject → permanent flap).
        if (es.readyState === EventSource.CONNECTING) {
          if (!ctx.connectingSince) ctx.connectingSince = Date.now();
          if ((Date.now() - ctx.connectingSince) > 20000) {
            try { es.close(); } catch (e) {}
            ctx.source = null;
            ctx.connectingSince = 0;
            resetPanelFlags();
            scheduleReconnect();
          }
          return;
        }
        if (es.readyState === EventSource.CLOSED) {
          ctx.source = null;
          ctx.connectingSince = 0;
          resetPanelFlags();
          scheduleReconnect();
          return;
        }
        // OPEN + spurious error: ignore (do not close).
      };
    }

    function waitForPanelReady(timeoutMs) {
      if (isPanelReady()) return Promise.resolve(true);
      return new Promise(resolve => {
        let done = false;
        const finish = (ok) => {
          if (done) return;
          done = true;
          resolve(!!ok);
        };
        const timer = setTimeout(() => finish(false), Math.max(1000, timeoutMs || 12000));
        ctx.panelReadyWaiters.push((ok) => {
          clearTimeout(timer);
          finish(ok);
        });
      });
    }

    function stopAll(ev) {
      // pagehide on F5: tear down EventSource/timers only. Keep uiSessionId so reload reuses
      // the same SoftAP UI session (close+new races MAX_SSE_CLIENTS=1).
      if (ctx.stopped) return;
      ctx.stopped = true;
      stopHeartbeat();
      // Do NOT postUiSession(true) / clear sessionStorage — session expires on FW timeout.
      try { if (ctx.source) ctx.source.close(); } catch (e) {}
      ctx.source = null;
      if (ctx.reconnectTimer) clearTimeout(ctx.reconnectTimer);
      ctx.reconnectTimer = null;
      if (ctx.staleTimer) clearInterval(ctx.staleTimer);
      ctx.staleTimer = null;
      ctx.running = false;
      ctx.eventsStarted = false;
      const waiters = ctx.panelReadyWaiters.splice(0);
      for (const w of waiters) {
        try { w(false); } catch (e) {}
      }
      void ev;
    }

    function startStaleDetector() {
      if (ctx.staleTimer) return;
      const maxSilentMs = 20000;
      ctx.staleTimer = setInterval(() => {
        try {
          if (!ctx.eventsStarted) return;
          const silentMs = Date.now() - (ctx.lastEventAt || 0);
          const net = Alpine?.store?.('net');
          if (net) net.staleMs = Math.max(0, silentMs);
          // Only force-close when we had traffic then went silent while claiming connected.
          if (ctx.state === 'connected' && ctx.lastEventAt && silentMs > maxSilentMs) {
            setState('stale');
            try { ctx.source?.close?.(); } catch (e) {}
            ctx.source = null;
            resetPanelFlags();
            scheduleReconnect();
          }
        } catch (e) {}
      }, 1000);
    }

    document.addEventListener('visibilitychange', () => {
      try {
        if (!document.hidden && ctx.eventsStarted && !ctx.source && !ctx.reconnectTimer) {
          scheduleReconnect();
        }
      } catch (e) {}
    }, { passive: true });

    // Only pagehide — beforeunload+pagehide duplicated session close on every F5.
    window.addEventListener('pagehide', stopAll);

    startStaleDetector();

    APP.sse.isPanelReady = function isPanelReadyExport() {
      return isPanelReady();
    };

    APP.sse.isConnected = function isConnected() {
      return !!ctx.source && ctx.source.readyState === EventSource.OPEN;
    };

    /**
     * Session → one EventSource → wait mode+hardware → heartbeat (only when ready).
     * @param {{ timeoutMs?: number }} [opts]
     * @returns {Promise<boolean>}
     */
    APP.sse.startEvents = async function startEvents(opts) {
      // SoftAP F5: pagehide may tear EventSource while document reloads/stays.
      // Revive and keep the same uiSessionId (touch on server); do not rotate id.
      if (!ctx.running) {
        if (document.visibilityState === 'hidden') return false;
        ctx.stopped = false;
        ctx.running = true;
        ctx.eventsStarted = false;
        ctx.source = null;
        stopHeartbeat();
        if (ctx.reconnectTimer) {
          clearTimeout(ctx.reconnectTimer);
          ctx.reconnectTimer = null;
        }
        try {
          const raw = sessionStorage.getItem('uiSessionId');
          const n = raw ? (parseInt(raw, 10) >>> 0) : 0;
          if (n) ctx.uiSessionId = n;
        } catch (e) {}
        APP.uiSessionId = ctx.uiSessionId;
        try { sessionStorage.setItem('uiSessionId', String(ctx.uiSessionId)); } catch (e) {}
        resetPanelFlags();
        log?.warn?.('[sse] revived after teardown (reuse session)', ctx.uiSessionId);
      }
      if (ctx.eventsStarted && isPanelReady()) {
        startHeartbeat();
        return true;
      }
      ctx.eventsStarted = true;
      try {
        await waitForUiSessionRegistered();
      } catch (e) {}
      connectSSE();
      const panelTimeout = Number(opts?.timeoutMs)
        || Number(timeouts.sseInitDeadlineMs)
        || Number(timeouts.ssePanelReadyTimeoutMs)
        || 12000;
      const ready = await waitForPanelReady(panelTimeout);
      if (ready) startHeartbeat();
      // On timeout: leave EventSource running (browser/our reconnect); no heartbeat until panel-ready.
      return ready;
    };
  };
})();
