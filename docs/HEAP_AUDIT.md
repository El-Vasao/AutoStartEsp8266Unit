# Heap / static RAM audit (ESP8266)

Latest field symptom (AP bring-up, no web payload yet):
- `boot_done`: ~15672 B free heap
- after AP start (no clients): ~11328 B free heap, max block ~11040 B
- OOM on first browser connect in `AsyncServer::_accept` (`AsyncClient::AsyncClient`, failed alloc 312 bytes)

This confirms accept-path headroom is the first stabilization target.

Next observed phase after accept-path hardening:
- AP starts, first UI access triggers repeated `/bootstrap` attempts.
- Under retries, free heap can dip to ~2400-2700 bytes and max block to ~1080-2550.
- Mitigation direction: split bootstrap into lightweight metadata + separate `/bootstrap/live`,
  then enforce single-flight/backoff to prevent request storms during low-memory windows.

Post-change `firmware.elf` (release `esp12e_release`): **RAM used 63276 / 81920 (77.2%)**, `data=2068`, `bss=34480`
(`python build.py release --no-ota`, footprint from `dist/ram-footprint-release.json`).

Largest application symbols (BSS/DATA, top from `ram-footprint-release.json`):

| Approx. size | Symbol / area |
|-------------:|----------------|
| 3620 | `g_coreImpl` |
| 1108 | `config` (`BaseConfig`) |
| 832 | `gSsePayload` (incremental SSE JSON scratch) |
| 468 | `webServer` |
| 0x100 | `gLastErr`, FS scratch, `broadcastLog` dedupe buffer |
| 0x100 | `gBytesFsScratch` |

SDK WiFi / lwIP (`g_ic`, `event_TaskQueue`, `dns_table`, …) still dominate outside application code.

## Recent deep refactor deltas

- `CorePrivate` no longer stores persistent `StatusSnapshot`; MQTT status now builds snapshot on-demand.
- `BaseConfig` moved `thermostat.sensor_name` and `temperature_triggers[*].sensor_name` out of resident RAM model;
  JSON output restores compatibility field by resolving `sensor_id -> sensors[].name`.
- GSM/MQTT fixed buffers were right-sized (`Sim800TcpTransport` RX/TX and `MqttFsmClient` RX/TX/topic staging).

## Acceptance checkpoint (`normal_full`)

- Build gate: `python build.py release --no-ota` passes and produces `dist/ram-footprint-release.{json,md}`.
- Static RAM margin after link: `81920 - 63276 = 18644 B` (this is upper bound before runtime allocations).
- Target `freeHeap >= 20KB` is **not yet proven** from linker numbers alone; final decision still requires on-device
  log capture in `normal_full` after all subsystems start (no UI client), with free/maxBlock/frag metrics.

## Uncontrolled heap (`src` / `include`)

- **`new` / `malloc` / `std::vector`**: no matches in `src` application `.cpp` files (relies on fixed buffers / pools).
- **`String`**: no use in GSM/modem hot paths or JSON SAX listeners. Remaining `String` is mostly **ESPAsyncWebServer** callbacks (`request->getParam()->value()`, upload `filename`) and **LittleFS** `dir.fileName()` — third-party / unavoidable surfaces; avoid extra concatenation there.
- **MQTT commands**: `parseMqttCommandJson` uses a tiny flat-object scanner (no `JsonStreamingParser`, no heap).
- **Config / programs JSON**: `JsonStreamingParser` fork in `lib/JsonStreamingParser` — `JsonListener::key`/`value` are `const char*` (no per-token Arduino `String`).

## Config offload to FS-only

Not implemented: `BaseConfig` stays resident for fast access; long fields are bounded by `TextBytes::*` / `Limits::CONFIG_JSON_SIZE`.

## Network stack

See `platformio.ini` comments: ESP8266 keeps **ESPAsyncWebServer** + **lwIP2 low-mem** build flag; replacing the stack is a separate migration.

## Logging

- **`logger.log`**: SSE (+ serial when `SERIAL_DEBUG`).
- **`logger.logSerialOnly`**: serial only in debug builds (e.g. noisy MQTT RX traces); release builds discard the call cheaply after `va_start`/`va_end`.

## SSE log dedupe

`WebServerRuntime::broadcastLog` skips sending when the message is **identical to the previous** line (reduces duplicate SSE `log` events and lwIP queue pressure).

## SSE queue depth (`SSE_MAX_QUEUED_MESSAGES`)

Build flag in `platformio.ini`: **16** slots per AsyncEventSource client (ESPAsyncWebServer). Below ~8, `runSseIncrementalTick` could enqueue more events than the library hard cap (`E async_ws … overflow`). Connect/disconnect use `logSerialOnly` so handshake does not push an extra `log` event into the same queue. A short `yield()` before the burst lets lwIP drain the queue between loop iterations.
