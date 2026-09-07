/**
 * @file WebServerInternal.h
 * @brief Внутренние утилиты WebServer (JSON ответы, заголовки кэша, парсинг параметров).
 *
 * Важно (ESP8266/память):
 * - Этот файл — часть реализации web-подсистемы. Его нельзя включать вне `src/web/` (и подпапок).
 * - JSON ответы по возможности пишутся потоково в `AsyncResponseStream` без большого промежуточного DOM.
 *
 * Запрещено:
 * - Добавлять сюда “общепроектные” зависимости: только то, что нужно WebServer.
 * - Возвращать `String` из hot-path helpers (делаем работу на `char[]` и `snprintf`).
 */
#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

#include "web/WebServer.h"
#include "fs/FSManager.h"
#include "core/Core.h"
#include "common/Logger.h"
#include "common/Constants.h"

namespace web_internal {

static constexpr const char* kContentTypeJson = "application/json";
static constexpr const char* kContentTypeText = "text/plain";

static inline void addNoCacheHeaders(AsyncWebServerResponse* resp) {
    if (!resp) return;
    resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    resp->addHeader("Pragma", "no-cache");
    resp->addHeader("Expires", "0");
}

static inline void addSessionRevalidateHeaders(AsyncWebServerResponse* resp, const char* etag) {
    if (!resp) return;
    resp->addHeader("Cache-Control", WebCache::SESSION_REVALIDATE);
    if (etag && etag[0]) {
        resp->addHeader("ETag", etag);
    }
}

// Не добавляем Connection: close — HTTP/1.1 может переиспользовать TCP для цепочки запросов UI.

static inline bool urlEndsWith(const char* url, const char* suffix) {
    if (!url || !suffix) return false;
    size_t lu = strlen(url);
    size_t ls = strlen(suffix);
    return lu >= ls && strcmp(url + (lu - ls), suffix) == 0;
}

static inline const char* contentTypeByPath(const char* urlPath) {
    if (!urlPath) return "application/octet-stream";
    if (urlEndsWith(urlPath, ".js")) return "application/javascript";
    if (urlEndsWith(urlPath, ".css")) return "text/css";
    if (urlEndsWith(urlPath, ".html")) return "text/html; charset=utf-8";
    if (urlEndsWith(urlPath, ".ico")) return "image/x-icon";
    if (urlEndsWith(urlPath, ".svg")) return "image/svg+xml";
    return "application/octet-stream";
}

static inline bool isCaptiveProbeUrl(const char* urlPath) {
    if (!urlPath) return false;
    return strcmp(urlPath, "/generate_204") == 0 ||
           strcmp(urlPath, "/fwlink") == 0 ||
           strcmp(urlPath, "/connecttest.txt") == 0 ||
           strcmp(urlPath, "/hotspot-detect.html") == 0 ||
           strcmp(urlPath, "/library/test/success.html") == 0 ||
           strcmp(urlPath, "/kindle-wifi/wifistub.html") == 0;
}

static inline bool parseUint32Param(AsyncWebServerRequest* request, const char* name, uint32_t* out) {
    if (!request || !name || !out || !request->hasParam(name, true)) return false;
    const String& value = request->getParam(name, true)->value();
    if (value.length() == 0) return false;
    char* end = nullptr;
    const unsigned long parsed = strtoul(value.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    *out = static_cast<uint32_t>(parsed);
    return true;
}

// JSON by contract is always served without gzip.
static inline bool sendJsonFromFs(AsyncWebServerRequest* request, const char* path,
                                  const char* cacheControl) {
    if (!path || path[0] != '/') {
        request->send(500, kContentTypeText, "Bad path");
        return false;
    }
    if (!fileSystem.exists(path)) {
        request->send(404, kContentTypeJson, "{\"success\":false,\"error\":\"FILE_NOT_FOUND\"}");
        return false;
    }
    AsyncWebServerResponse* resp = request->beginResponse(fileSystem.webFs(), path, kContentTypeJson);
    resp->addHeader("Cache-Control", cacheControl ? cacheControl : "no-cache");
    resp->addHeader("Pragma", "no-cache");
    resp->addHeader("Expires", "0");
    request->send(resp);
    return true;
}

/** `filler(Print&) -> bool`; пишите JSON символами/фрагментами в поток ответа. */
template <typename F>
static inline bool sendJsonResponse(AsyncWebServerRequest* request, F&& filler) {
    AsyncResponseStream* resp = request->beginResponseStream(kContentTypeJson);
    if (!filler(*resp)) {
        request->send(500, kContentTypeText, "Failed to generate response");
        return false;
    }
    addNoCacheHeaders(resp);
    request->send(resp);
    return true;
}

static inline void sendBusy(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* resp = request->beginResponse(
        409, kContentTypeJson,
        "{\"success\":false,\"error\":\"BUSY\",\"message\":\"Flash operation in progress\"}"
    );
    request->send(resp);
}

static inline bool rejectIfFlashBusy(AsyncWebServerRequest* request) {
    if (!webServer.isFlashBusy()) return false;
    sendBusy(request);
    return true;
}

static inline void sendJsonSuccess(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* resp = request->beginResponse(200, kContentTypeJson, "{\"success\":true}");
    request->send(resp);
}

} // namespace web_internal
