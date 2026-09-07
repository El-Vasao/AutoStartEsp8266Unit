# `web`: SoftAP, HTTP API, SSE (UI)

## Роль

Web-подсистема — локальный UI поверх SoftAP/captive portal:

- статические файлы из LittleFS, с fallback из PROGMEM;
- HTTP API для настроек и программ;
- SSE для статуса и логов.

Основные файлы:

- `include/web/WebServer.h`
- `src/web/WebServer.*.cpp`
- internal: `src/web/internal/*`
- fallback UI: `include/web/FallbackWebPage.h`

## UI sessions

- Клиент периодически обновляет lease (`POST /ui/session`).
- Фиксированный пул сессий без heap; таймаут по `WebUi::SESSION_TIMEOUT_MS`, heartbeat — `WebUi::HEARTBEAT_INTERVAL_MS`.
- Если активных UI-сессий нет, SSE может быть закрыт при housekeeping.

## Контракт по памяти (ESP8266)

- Лимиты: `JsonBytes::Web`, `WebSseLimits`, `APConfig` в `include/common/Constants.h`.
- SSE status — через слоты `PoolManager`, без динамики в hot-path.
- `String` только кратковременно (например параметры запросов AsyncWebServer).

### `/bootstrap`

Вызывается при старте UI. Ответ — **потоковый** JSON (без ArduinoJson на стороне прошивки), чтобы не держать большой статический буфер ответа.

## SSE

Два канала событий:

- `log` — текст через `logger.log()`;
- `status` — компактный JSON, дедупликация по CRC, keepalive.

Прикладной код не шлёт лог в SSE напрямую — только через `logger`.

## SSE logs и нагрузка

При «шторме» логов (особенно на фоне churn GSM/TCP) транспорт может перегрузить цикл. Политика: **token bucket / throttling** в `Logger` — лишние сообщения отбрасываются предпочтительнее WDT.

## Flash busy / deferred

- `WebServer::isFlashBusy()` блокирует опасные write/delete до безопасной фазы.
- POST JSON для конфига обычно пишется во временный файл; применение — deferred совместно с `FSManager`/`Config`, чтобы не пересекаться с выполнением программ.
