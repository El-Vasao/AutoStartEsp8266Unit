# RAM stabilization playbook (ESP8266)

This document is the execution checklist for OOM mitigation on real hardware.

## 1) Memory SLO (must pass)

Target thresholds for stable AP/Web bring-up:

- `boot_done`: `freeHeap >= 15000`, `maxBlock >= 14000`
- `ap_started_no_clients`: `freeHeap >= 11000`, `maxBlock >= 10000`
- `first_client_connect`: no OOM in `AsyncServer::_accept` path, `maxBlock >= 8000`
- `bootstrap_lite_response`: no storm of `503 LOW_MEMORY` under single-client setup flow
- `bootstrap_live_response`: no repeated low-memory loop; requests are throttled/single-flight
- `ota_prepare_window`: `maxBlock >= 6000` before `Update.begin`

If any threshold is violated, do not expand SSE queue depth, AP station count, or web payload sizes.

## 2) Baseline measurements (required scenarios)

Run and capture logs for:

1. Cold boot to `setup_ap`
2. AP started, no clients for 30s
3. First UI connect (single phone/laptop, one tab)
4. `/bootstrap` (lite) + SSE handshake
5. `/bootstrap/live` after UI unlock
6. OTA upload start (pre-OTA pressure window)

For each scenario, log:

- `ESP.getFreeHeap()`
- `ESP.getMaxFreeBlockSize()`
- `ESP.getHeapFragmentation()`

## 3) Copy/buffer hotspot decisions

Current decisions (keep unless metrics regress):

- `modem -> tcp -> mqtt` staged copies: **keep** (safer framing and simpler fault isolation).
- `MqttFsmClient::publishPrintedMeasured` double pass: **keep for now** (predictable packet sizing, no heap).
- SSE incremental payload (`gSsePayload`) shared scratch: **keep** (single fixed BSS buffer).
- `/bootstrap` is split into lightweight metadata and separate `/bootstrap/live` snapshot.
- `/bootstrap/live` is protected with low-heap rejection + client throttle/single-flight.
- `/bootstrap/live` now has server-side cooldown (`BOOTSTRAP_LIVE_COOLDOWN`) and OTA-pressure rejection (`OTA_UPLOAD_PRESSURE`).

Revisit only when AP-connect OOM is gone and pressure moves to throughput.

## 4) Build profile policy

- Field firmware: `python build.py release`
  - Uses PlatformIO env `esp12e_release`
  - Version metadata without `SERIAL_DEBUG`
- Diagnostics firmware: `python build.py debug`
  - Uses PlatformIO env `esp12e_debug`
  - Enable only for controlled sessions (UART line contention with GSM is expected)

LittleFS upload must use `esp12e_release` profile to avoid accidental debug drift in field images.
Dependency trimming decisions are tracked in `docs/DEPENDENCY_RAM_AUDIT.md`.

## 4.1) RAM budget framework (build-time gate)

`build.py` now emits a deterministic footprint report after every firmware build:

- `dist/ram-footprint-<build_type>.json`
- `dist/ram-footprint-<build_type>.md`

Report contains:

- `.text/.data/.bss` from `xtensa-lx106-elf-size`
- top RAM symbols (`B/b/D/d`) from `xtensa-lx106-elf-nm --size-sort`
- secure-stack markers (`BearSSL`, `WiFiClientSecure`, `CertStore`) from linked ELF symbols

Current module budgets (baseline target for `normal_full`):

- `core`: <= 3600 B (`g_coreImpl`)
- `config`: <= 1100 B (`config`)
- `web`: <= 1500 B (`webServer` + `gSsePayload` + log scratch)
- `gsm/mqtt`: <= 1200 B static buffers (FSM/TCP transport staging)
- `fs/ota/misc`: <= 900 B (`gBytesFsScratch`, `Update`, etc.)

## 5) Structural RAM-cut roadmap

Stage A (now): protect accept-path and first connect.
Stage A.1 (now): prevent bootstrap request storms (`single-flight`, `LOW_MEMORY` backoff, `/bootstrap/live` throttle).

Stage B: move cold-path config fields out of always-resident runtime structures (FS-backed fetch on demand).

Stage C: evaluate `ESPAsyncWebServer` fork/patch only if memory SLO still fails after Stage A/B.

## 6) Async-vs-sync web server decision gate

Migrate away from async web stack only when all conditions are true:

1. AP first-client OOM still reproduces after SLO hardening and setup AP memory guards.
2. Reduced AP station limit and SSE pressure controls do not stabilize accept-path.
3. Regression budget is approved for web API/SSE behavior changes.

If condition (1) is false, keep async stack and continue incremental optimization.
