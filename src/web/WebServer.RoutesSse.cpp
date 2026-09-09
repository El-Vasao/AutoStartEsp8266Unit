#include "web/WebServer.h"
#include "web/internal/WebServerInternal.h"
#include "web/internal/WebServerRuntime.h"
#include "common/Logger.h"
#include "core/Core.h"
#include "gsm/GSMController.h"
#include <ESP8266WiFi.h>

using namespace web_internal;

void WebServer::setupSseEndpointRoutes_() {
    events.onConnect([](AsyncEventSourceClient* client) {
        // Do NOT noteHeavyUiTraffic here: that clears uiBrowserReady and suspends GSM on every
        // EventSource (re)connect — after FE POST /ui/ready the modem would stay IDLE forever.
        webServer.refreshSseClientCount();
        // SoftAP / F5: newest browser wins. Closing only the new client left a zombie SSE
        // that blocked reconnect forever (UI stayed locked waiting for panel-ready).
        if (webServer.sseClientCount > WebSseLimits::MAX_SSE_CLIENTS) {
            logger.logSerialOnly("[WebServer] SSE over cap: reset all clients (newest wins, have=%u lim=%u)\n",
                                 (unsigned)webServer.sseClientCount,
                                 (unsigned)WebSseLimits::MAX_SSE_CLIENTS);
            webServer.events.close();
            webServer.sseClientCount = 0;
            webServer.lastObservedSseClients_ = 0;
            return;
        }
        // Force next incremental tick to re-emit baseline (mode/hardware/…); do not rely on /bootstrap/live.
        WebServerRuntime::requestSseIncrementalBaseline(webServer);
        logger.logSerialOnly("[WebServer] SSE connected (clients=%u, uiSessions=%u)\n",
                             (unsigned)webServer.sseClientCount,
                             (unsigned)webServer.activeUiSessionCount());
    });
    events.onDisconnect([](AsyncEventSourceClient* client) {
        (void)client;
        webServer.refreshSseClientCount();
        logger.logSerialOnly("[WebServer] SSE disconnected (clients=%u, uiSessions=%u, sta=%u, heap=%u, max=%u, frag=%u%%, gsm=%s)\n",
                             (unsigned)webServer.sseClientCount,
                             (unsigned)webServer.activeUiSessionCount(),
                             (unsigned)WiFi.softAPgetStationNum(),
                             (unsigned)ESP.getFreeHeap(),
                             (unsigned)ESP.getMaxFreeBlockSize(),
                             (unsigned)ESP.getHeapFragmentation(),
                             core.getGSM().getStateString());
    });
    server.addHandler(&events);
}
