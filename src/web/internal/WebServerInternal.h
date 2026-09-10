/**
 * @file WebServerInternal.h
 * @brief Внутренние утилиты WebServer (JSON ответы, заголовки кэша, парсинг параметров).
 *
 * Важно (ESP8266/память):
 * - Этот файл — часть реализации web-подсистемы. Его нельзя включать вне `src/web/` (и подпапок).
 * - JSON body: только measured streaming (`sendJsonStreaming`) — count + filler + SkippingPrint.
 * - Запрещён `beginResponseStream` для JSON: он копирует всё тело в растущий cbuf (ложный «стрим»).
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
#include "json/JsonCountingPrint.h"

namespace web_internal {

static constexpr const char* kContentTypeJson = "application/json";
static constexpr const char* kContentTypeText = "text/plain";

using JsonEmitFn = void (*)(Print&);

/**
 * Print that discards the first `skip` bytes, then copies into a fixed out buffer.
 * Used by AwsResponseFiller: re-emit full JSON each chunk, skip already-sent prefix.
 */
class SkippingPrint final : public Print {
public:
    SkippingPrint(size_t skip, uint8_t* out, size_t cap)
        : skip_(skip), out_(out), cap_(cap), produced_(0) {}

    size_t produced() const { return produced_; }

    size_t write(uint8_t b) override {
        if (skip_ > 0) {
            --skip_;
            return 1;
        }
        if (out_ && produced_ < cap_) {
            out_[produced_++] = b;
        }
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        if (!buffer || size == 0) return 0;
        size_t i = 0;
        while (i < size && skip_ > 0) {
            --skip_;
            ++i;
        }
        while (i < size && out_ && produced_ < cap_) {
            out_[produced_++] = buffer[i++];
        }
        return size;
    }

private:
    size_t skip_;
    uint8_t* out_;
    size_t cap_;
    size_t produced_;
};

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

/**
 * Measured JSON body: count with JsonCountingPrint, then beginResponse(len, fill).
 * `fill` must re-run the same emit via SkippingPrint (stable function pointer; no heap).
 * Do not use beginResponseStream for JSON.
 */
static inline bool sendJsonStreaming(AsyncWebServerRequest* request, JsonEmitFn emit, AwsResponseFiller fill) {
    if (!request || !emit || !fill) {
        if (request) request->send(500, kContentTypeText, "Bad JSON stream");
        return false;
    }
    JsonCountingPrint counter;
    emit(counter);
    const size_t len = counter.written();
    if (len == 0) {
        request->send(500, kContentTypeText, "Empty JSON");
        return false;
    }
    AsyncWebServerResponse* resp = request->beginResponse(kContentTypeJson, len, fill);
    if (!resp) {
        request->send(500, kContentTypeText, "Out of memory");
        return false;
    }
    addNoCacheHeaders(resp);
    request->send(resp);
    return true;
}

/** Same as sendJsonStreaming; kept for call sites that expect sendJsonResponse. */
static inline bool sendJsonResponse(AsyncWebServerRequest* request, JsonEmitFn emit, AwsResponseFiller fill) {
    return sendJsonStreaming(request, emit, fill);
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
    if (!resp) {
        request->send(503, kContentTypeJson, "{\"success\":false,\"error\":\"RESPONSE_ALLOC_FAILED\"}");
        return;
    }
    request->send(resp);
}

} // namespace web_internal
