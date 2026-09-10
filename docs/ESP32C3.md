# ESP32-C3 port notes

Branch: `esp32`. PlatformIO envs: `esp32c3_release`, `esp32c3_debug` (aliases `esp32c3` / legacy `esp12e*`).

## Pins (ESP-C3-12F drop-in on ESP-12 PCB)

| Net | ESP32-C3 GPIO | Notes |
|-----|---------------|--------|
| GSM TX (MCU→SIM800) | 21 | UART0 TX pad |
| GSM RX (SIM800→MCU) | 20 | UART0 RX pad |
| 1-Wire | 9 | strap: keep HIGH at boot |
| Button / IN3 | 10 | |
| IN1 / IN2 | 5 / 4 | |
| RELAY1..5 | 3, 19, 18, 8, 2 | GPIO8 strap — prefer HIGH at boot |
| VBAT ADC | 1 | external 1:15 → ~0–1 V; `ADC_0db` |

See `include/common/Pins.h`.

## Partitions

`partitions_esp32c3.csv` — used via `board_build.partitions` in `platformio.ini`.  
FS partition named `littlefs` uses subtype `spiffs` (required by `gen_esp32part`; Arduino LittleFS still mounts it).

## UART legacy

SIM800 and debug Serial share UART0 (GPIO20/21). `ModemUart::begin` uses  
`Serial.begin(baud, SERIAL_8N1, Pin::GSM_RX, Pin::GSM_TX)`.

## SoftAP UI — 3 gzip assets

On LittleFS (after `build.py` / uploadfs):

1. `index.html.gz` — markup (links `/style.css`)
2. `style.css.gz`
3. `app.js.gz` — Alpine + app bundle (was `app.bundle.js`)

`WebAssets::REQUIRED` = `/index.html`, `/style.css`, `/app.js` (`.gz` accepted).

## HAL / RAM

- `include/common/EspHal.h` — heap, WDT feed, chip id from MAC
- ErrorManager: `RTC_DATA_ATTR` (survives soft reset, not power loss)
- ESP8266 heap-frag auto-reboot and SoftAP `LOW_MEMORY` 503 gates removed / disabled
