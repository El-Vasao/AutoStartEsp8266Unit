// SSE only (no /status polling)
(function () {
  const APP = (window.APP = window.APP || {});
  APP.sse = APP.sse || {};

  APP.sse.init = function initSse(Alpine) {
    // Restartable singleton: init() may be called multiple times after partial failures.
    const prev = APP.sse._ctx;
    if (prev && prev.running) return;

    const endpoints = APP.api?.endpoints || APP.contract?.api?.endpoints || {};
    const log = APP.utils?.log;

    const ctx = (APP.sse._ctx = {
      running: true,
      source: null,
      reconnectTimer: null,
      heartbeatTimer: null,
      heartbeatInFlight: false,
      backoffMs: 1000,
      state: 'disconnected', // connected|disconnected|backoff|stale
      lastEventAt: 0,
      heartbeatMs: 5000,
      staleTimer: null,
      uiSessionId: ((((Date.now() & 0x7fffffff) ^ ((Math.random() * 0x7fffffff) | 0)) >>> 0) || 1),
      bootstrapLiveInFlight: null,
      bootstrapLiveLastMs: 0,
      bootstrapLiveCooldownMs: 8000,
      statusQueue: [], // FIFO of { kind, payload } until stores are ready
      logQueue: [],
    });
    APP.uiSessionId = ctx.uiSessionId;

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

    function scheduleReconnect() {
      if (ctx.reconnectTimer) return;
      setState('backoff');
      ctx.reconnectTimer = setTimeout(() => {
        ctx.reconnectTimer = null;
        connectSSE();
      }, ctx.backoffMs);
      const jitter = Math.floor(Math.random() * 200);
      ctx.backoffMs = Math.min(ctx.backoffMs * 2 + jitter, 8000);
    }

    function buildSessionBody(close) {
      const body = new URLSearchParams();
      body.set('id', String(ctx.uiSessionId));
      if (close) body.set('close', '1');
      return body;
    }

    /** @returns {Promise<Response|void>} */
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
        return fetch(url, {
          method: 'POST',
          cache: 'no-store',
          keepalive: !!close,
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
            'X-Requested-With': 'ElLineUI'
          },
          body: body.toString()
        }).catch(() => {}).finally(() => {
          if (!close) ctx.heartbeatInFlight = false;
        });
      } catch (e) {
        if (!close) ctx.heartbeatInFlight = false;
        return Promise.resolve();
      }
    }

    /** Первый POST /ui/session до EventSource — снимает гонку с активной UI-сессией на устройстве. */
    function waitForUiSessionRegistered() {
      const url = endpoints.uiSession || '/ui/session';
      const body = buildSessionBody(false);
      const tries = 6;
      const delay = ms => new Promise(cb => setTimeout(cb, ms));
      async function run() {
        for (let i = 0; i < tries; i++) {
          try {
            const res = await fetch(url, {
              method: 'POST',
              cache: 'no-store',
              headers: {
                'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
                'X-Requested-With': 'ElLineUI'
              },
              body: body.toString()
            });
            if (res && res.ok) return;
          } catch (e) {}
          await delay(120 * (i + 1));
        }
      }
      return run();
    }

    function startHeartbeat() {
      if (ctx.heartbeatTimer) return;
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
        stopHeartbeat();
        startHeartbeat();
      } catch (e) {}
    };

    function canUseStores() {
      try {
        return !!(Alpine?.store?.('deviceStatus')?.patchFromSse && Alpine?.store?.('uiLogs')?.add);
      } catch (e) {
        return false;
      }
    }

    function flushQueues() {
      if (!canUseStores()) return;
      try {
        if (ctx.statusQueue.length) {
          const ds = Alpine.store('deviceStatus');
          for (const entry of ctx.statusQueue.splice(0)) {
            if (entry && typeof entry === 'object' && entry.kind) ds.patchFromSse(entry.kind, entry.payload);
            else if (typeof entry === 'object' && entry !== null && !entry.kind) ds.updateFromSSE(entry);
          }
        }
      } catch (e) {}
      try {
        if (ctx.logQueue.length) {
          const logs = Alpine.store('uiLogs');
          for (const msg of ctx.logQueue.splice(0)) logs.add(msg);
        }
      } catch (e) {}
    }

    function connectSSE() {
      if (!window.EventSource) {
        setState('disconnected');
        return;
      }

      if (ctx.source) return;

      try {
        ctx.source = new EventSource(endpoints.events || '/events');
      } catch (e) {
        scheduleReconnect();
        return;
      }

      ctx.source.onopen = () => {
        ctx.backoffMs = 1000;
        setState('connected');
      };

      function onStatusPayload(kind, raw) {
        ctx.lastEventAt = Date.now();
        const payload = JSON.parse(raw);
        if (canUseStores()) Alpine.store('deviceStatus').patchFromSse(kind, payload);
        else {
          ctx.statusQueue.push(kind === 'status' ? payload : { kind, payload });
          if (ctx.statusQueue.length > 16) ctx.statusQueue = ctx.statusQueue.slice(-16);
        }
        flushQueues();
        setState('connected');
      }

      async function refetchBootstrapLive() {
        const now = Date.now();
        if (ctx.bootstrapLiveInFlight) return ctx.bootstrapLiveInFlight;
        if ((now - ctx.bootstrapLiveLastMs) < ctx.bootstrapLiveCooldownMs) return null;
        const uiLocked = !!Alpine?.store?.('uiState')?.locked;
        if (uiLocked) return null;

        const apiJson = APP.api?.apiJson;
        const bootstrapUrl = endpoints.bootstrapLive || APP.contract?.api?.endpoints?.bootstrapLive || '/bootstrap/live';
        if (!apiJson) return;
        ctx.bootstrapLiveInFlight = (async () => {
          try {
            const res = await apiJson(bootstrapUrl, { timeoutMs: 6000 });
            if (res?.ok && res.data && typeof res.data === 'object') {
              if (canUseStores()) Alpine.store('deviceStatus').patchFromSse('snapshot', res.data);
              else ctx.statusQueue.push({ kind: 'snapshot', payload: res.data });
              ctx.bootstrapLiveLastMs = Date.now();
            }
          } catch (e) {}
          flushQueues();
        })().finally(() => {
          ctx.bootstrapLiveInFlight = null;
        });
        return ctx.bootstrapLiveInFlight;
      }

      ctx.source.addEventListener('resync', () => {
        ctx.lastEventAt = Date.now();
        const uiLocked = !!Alpine?.store?.('uiState')?.locked;
        if (!uiLocked) {
          refetchBootstrapLive();
        } else {
          // Do not trigger heavy refetch during startup lock; retry window remains open after unlock.
          ctx.bootstrapLiveLastMs = 0;
          if (ctx.bootstrapLiveInFlight) {
            // let current one finish if any, but do not start new fetches
          }
        }
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
        if (canUseStores()) Alpine.store('uiLogs').add(e.data);
        else {
          ctx.logQueue.push(String(e.data || ''));
          if (ctx.logQueue.length > 30) ctx.logQueue = ctx.logQueue.slice(-30);
        }
        flushQueues();
        setState('connected');
      });

      ctx.source.onerror = () => {
        setState('disconnected');
        try {
          // EventSource can keep failing without transitioning to CLOSED on some browsers;
          // proactively re-create connection with backoff.
          try { ctx.source?.close?.(); } catch (e) {}
          ctx.source = null;
          scheduleReconnect();
        } catch (e) { log?.debug?.('SSE onerror handler failed', e); }
      };
    }

    function stopAll() {
      stopHeartbeat();
      postUiSession(true);
      try { if (ctx.source) ctx.source.close(); } catch (e) {}
      ctx.source = null;
      if (ctx.reconnectTimer) clearTimeout(ctx.reconnectTimer);
      ctx.reconnectTimer = null;
      if (ctx.staleTimer) clearInterval(ctx.staleTimer);
      ctx.staleTimer = null;
      ctx.running = false;
    }

    function startStaleDetector() {
      if (ctx.staleTimer) return;
      // If SSE is silent for too long, reconnect (ESP can drop TCP silently).
      const maxSilentMs = 12000;
      ctx.staleTimer = setInterval(() => {
        try {
          const silentMs = Date.now() - (ctx.lastEventAt || 0);
          const net = Alpine?.store?.('net');
          if (net) net.staleMs = Math.max(0, silentMs);
          if (ctx.state === 'connected' && ctx.lastEventAt && silentMs > maxSilentMs) {
            setState('stale');
            try { ctx.source?.close?.(); } catch (e) {}
            ctx.source = null;
            scheduleReconnect();
          }
        } catch (e) {}
      }, 1000);
    }

    // If user returns to the tab, try SSE again.
    document.addEventListener('visibilitychange', () => {
      try {
        if (!document.hidden && !ctx.source) connectSSE();
      } catch (e) {}
    }, { passive: true });

    window.addEventListener('beforeunload', stopAll);
    window.addEventListener('pagehide', stopAll);

    startStaleDetector();
    waitForUiSessionRegistered()
      .then(() => {
        connectSSE();
        startHeartbeat();
      })
      .catch(() => {
        connectSSE();
        startHeartbeat();
      });
  };
})();

