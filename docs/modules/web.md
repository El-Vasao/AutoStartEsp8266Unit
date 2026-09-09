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
- **JSON body:** запрещён `AsyncResponseStream` (копит всё тело в cbuf). Использовать
  `sendJsonStreaming` — `JsonCountingPrint` + `beginResponse(len, filler)` + `SkippingPrint`
  (пик RAM маленький; эмиттер может прогоняться несколько раз).

### `/bootstrap`

Вызывается при старте UI. Ответ — measured streaming JSON (без ArduinoJson и без растущего
body-буфера): metadata (`version`, `uiLease`, `hwCounts`, `hwMap`, ROMs). Live snapshot —
отдельный `GET /bootstrap/live`. Heap-gate (`MIN_HEAP_FOR_BOOTSTRAP_*` = 5500) смотрит
free **после** accept TCP/request; запас под send chunk, не под полный SoftAP+body.

### SoftAP UI init ↔ cellular

Тяжёлые маршруты (`/`, static, `/bootstrap*`, `/config/get`, programs GET) и STA associate
вызывают `noteHeavyUiTraffic()` (сброс ready + немедленный GSM suspend). SSE connect **не**
heavy — иначе после `/ui/ready` reconnect сбрасывал ready.

Порядок FE (SSE last, UI locked until panel status):

1. checklist HTTP (`/bootstrap` → schemas → `/config/get` → `/programs`) через `deviceFetch` (UI locked);
2. `sseStartDelayMs` drain;
3. `POST /ui/session` → **один** EventSource `/events`;
4. на SSE connect firmware — **paced baseline** (`clocks` → `mode` → `gsm` → `hardware`);
5. unlock после `mode`+`hardware`; FE **не** вызывает `/bootstrap/live`;
6. heartbeat только после panel-ready → quiet → `POST /ui/ready` если SSE OPEN.

**Anti-flap:** при `onerror` и `readyState===CONNECTING` FE не делает `close()` и не открывает второй
EventSource (`MAX_SSE_CLIENTS=1`). Reconnect только при `CLOSED`. Force status на устройстве =
`requestBaselineResync()`, без `captureDedup` (иначе mode/hardware не уходят).

Heartbeat `/ui/session` не heavy.

## UI visibility contract

Чтобы не возвращалась «плавающая пустая вкладка»:

1. **Settings subtabs (`.settings-subtab-pane`):** видимость только через CSS
   `display:none` + класс `.active`. **Запрещён** Alpine `x-show` на тех же элементах —
   иначе Alpine запоминает CSS `none` и ставит inline `display:none`, которое бьёт `.active`.
2. **Main tabs (`.tab-pane`):** только Alpine `x-show` по `activeTab`, без CSS `display:none`
   на `.tab-pane`.
3. **System tab:** шелл (info / reboot / OTA / logs) всегда в DOM после unlock; не гейтить
   `deviceStatus.loaded`. Поля могут быть `—` до clocks.
4. Смена главного таба — через `uiState.setActiveTab()` (enter-hook: flush `uiLogs` на System).
5. **Вкладки не persist’ятся:** F5 / reload всегда открывают «Панель» и sub-tab wifi
   (legacy `localStorage ui.activeTab` / `ui.activeSubTab` сбрасываются).
6. **F5 SoftAP:** reload = init с нуля, но **тот же `uiSessionId`** (reuse; без beacon `close`).
   Lock до SSE panel-ready (`sseInitDeadlineMs`=12s) или degraded; hard safety ~20s.
   `pagehide` рвёт только EventSource, сессию на FW не закрывает.
7. **Boot UI:** `#boot-splash` / `html.ui-booting` до `unlock`/`failInit` (splash сам перекрывает клики).
   Запрещён `pointer-events: none` на `.container` через `ui-booting` — даёт «тихий» мёртвый UI.

## UI init lifecycle

Контракт boot / F5 (чтобы не было «белой страницы» и «ложно живой» панели):

1. **`#boot-splash`** в `index.html` **вне** `x-cloak` — виден с первого paint до
   `uiState.unlock()` / `failInit()`. Без Alpine экран не белый.
2. **`initLock` (default true)** vs **`flashLock`**: `.ui-locked` = `initLock || flashLock`.
   Flash write (`runFlashWrite`) трогает только `flashLock` + busy-modal.
3. Единый API: `beginInit()` → `setInitProgress` / phases → `unlock()` |
   `unlock({ degraded: true })` | `failInit(message)`. Splash снимается только отсюда.
4. Head-loader `/app.bundle.js`: успех = событие `alpine:initialized`, не голый
   `script.onload`. Watchdog ~12s без Alpine → cache-bust reload (лимит 4).
5. Captive `onNotFound`: отсутствующие `.js`/`.css` → **404**, не redirect `/` (иначе
   браузер парсит HTML как JS и cloak навсегда).
6. SSE panel-ready держит lock до `sseInitDeadlineMs` (12s), иначе degraded unlock. Hard fail checklist →
   emergency backdrop; табы Panel/Programs/Settings `:disabled` только при `initFailed`.
   Safety unlock ~20s от beginInit, если путь завис. F5 reuse `uiSessionId` (без close/open).
7. Boot splash до unlock; без `pointer-events: none` на `.container` через `ui-booting`.

## SoftAP UI delivery (index + bundle)

Раньше весь UI (HTML+CSS+~500 KB JS) уходил одним `index.html.gz` (~155 KB). Вкладки
programs/settings/system жили в последних ~12% документа — после огромного inline-script в
`<head>`. Обрыв SoftAP/TCP после панели оставлял Alpine живым, а поздние `#tab-*` без DOM.

Сейчас:

1. `GET /` → `index.html.gz` (разметка всех вкладок + CSS) — сравнительно короткий ответ.
2. Затем `GET /app.bundle.js` (с retry в head-loader; на FS — `app.bundle.js.gz`).
3. Sentinel `#ui-doc-complete` + проверка `tab-panel|programs|settings|system`: при неполной
   доставке HTML — cache-bust reload (лимит 4).
4. Белая страница до Alpine закрыта `#boot-splash`; hang после битого bundle — watchdog.

Обязательные ассеты: `WebAssets::REQUIRED` = `/index.html` + `/app.bundle.js` (допускается `.gz`).

## SSE

Два канала событий:

- `log` — текст через `logger.log()` (только при активной UI-сессии + подписчике `/events`);
- incremental status — kinds (`clocks`/`hardware`/…); connect/force → paced baseline.

Лимиты (ESP8266 RAM):

- `WebSseLimits::MAX_SSE_CLIENTS = 1` — второй EventSource закрывается на connect;
- soft queue `SSE_SOFT_QUEUE_MAX = 4`, hard `SSE_MAX_QUEUED_MESSAGES = 8` (`platformio.ini`);
- период incremental tick: `Timing::SSE_STATUS_INTERVAL_MS = 1000`.

UI: HTTP-чеклист → SSE panel-ready (lock, deadline `sseInitDeadlineMs`=12s) → unlock;
boot-splash до unlock; hard safety ≤20s; без silent `ui-booting` pointer-events на container.
без live HTTP рядом с EventSource.

Прикладной код не шлёт лог в SSE напрямую — только через `logger`.

## SSE logs и нагрузка

При «шторме» логов (особенно на фоне churn GSM/TCP) транспорт может перегрузить цикл. Политика: дроп по soft queue / low heap/maxBlock предпочтительнее WDT; без активной UI-сессии log-SSE не ставится в очередь.

## Flash busy / deferred

- `WebServer::isFlashBusy()` блокирует опасные write/delete до безопасной фазы.
- POST JSON для конфига обычно пишется во временный файл; применение — deferred совместно с `FSManager`/`Config`, чтобы не пересекаться с выполнением программ.
