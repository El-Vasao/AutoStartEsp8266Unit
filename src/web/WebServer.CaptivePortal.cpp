#include "web/WebServer.h"
#include "web/internal/WebServerInternal.h"

using namespace web_internal;

void WebServer::setupCaptivePortalRoutes_() {
    server.onNotFound([](AsyncWebServerRequest* request) {
        char urlBuf[BufferBytes::Web::URL];
        const String& u = request->url();
        strlcpy(urlBuf, u.c_str(), sizeof(urlBuf));

        if (request->method() == HTTP_GET && isCaptiveProbeUrl(urlBuf)) {
            request->redirect("/");
            return;
        }

        size_t urlLen = strlen(urlBuf);
        if (urlLen > 1 && urlBuf[urlLen - 1] != '/') {
            const char* etag = core.getVersionString();
            const char* contentType = contentTypeByPath(urlBuf);

            if (urlEndsWith(urlBuf, ".json")) {
                webServer.noteHeavyUiTraffic();
                sendJsonFromFs(request, urlBuf, "no-cache, no-store, must-revalidate");
                return;
            }

            if (fileSystem.exists(urlBuf)) {
                webServer.noteHeavyUiTraffic();
                auto* resp = request->beginResponse(fileSystem.webFs(), urlBuf, contentType);
                addSessionRevalidateHeaders(resp, etag);
                request->send(resp);
                return;
            }

            if (urlEndsWith(urlBuf, ".js") || urlEndsWith(urlBuf, ".css") || urlEndsWith(urlBuf, ".html")) {
                char gzPath[BufferBytes::Fs::GZIP_PATH];
                snprintf(gzPath, sizeof(gzPath), "%s.gz", urlBuf);
                if (fileSystem.exists(gzPath)) {
                    webServer.noteHeavyUiTraffic();
                    auto* resp = request->beginResponse(fileSystem.webFs(), gzPath, contentType);
                    resp->addHeader("Content-Encoding", "gzip");
                    resp->addHeader("Vary", "Accept-Encoding");
                    addSessionRevalidateHeaders(resp, etag);
                    request->send(resp);
                    return;
                }
                // Missing static assets must 404 (not captive-redirect to HTML as JS body).
                if (urlEndsWith(urlBuf, ".js") || urlEndsWith(urlBuf, ".css")) {
                    request->send(404, "text/plain", "Not Found");
                    return;
                }
            }
        }

        // Path-only .js/.css outside the block above (trailing slash / short paths)
        if (request->method() == HTTP_GET &&
            (urlEndsWith(urlBuf, ".js") || urlEndsWith(urlBuf, ".css"))) {
            request->send(404, "text/plain", "Not Found");
            return;
        }

        request->redirect("/");
    });
}

