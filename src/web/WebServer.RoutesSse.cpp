#include "web/WebServer.h"
#include "web/internal/WebServerInternal.h"
#include "common/Logger.h"
#include "core/Core.h"
#include "gsm/GSMController.h"
#include <ESP8266WiFi.h>

using namespace web_internal;

void WebServer::setupSseEndpointRoutes_() {
    events.onConnect([](AsyncEventSourceClient* client) {
        webServer.refreshSseClientCount();
        if (webServer.sseClientCount == 0) {
            webServer.sseClientCount = 1;
        }
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

