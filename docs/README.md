# Документация проекта

Эта папка — “источник правды” по архитектуре, ограничениям ESP8266 и правилам поддержки проекта.
Файлы **`docs/` включены в git** (не скрываются `.gitignore`), чтобы ревью и клон репозитория совпадали с документацией.
Документация написана так, чтобы её можно было читать независимо от чата/истории изменений.

## Быстрый старт
- Сборка/прошивка/FS: [`build_and_flash.md`](build_and_flash.md)
- Архитектура и правила модульности: [`ARCHITECTURE.md`](ARCHITECTURE.md)

## Обзор
- Базовое состояние (снимок перед рефактором): [`baseline_review.md`](baseline_review.md)
- Гайды по embedded-ограничениям (heap, JSON, WDT, LittleFS): [`embedded_guides.md`](embedded_guides.md)

## Модули (карта ответственности)
- Core (orchestration, режимы, периодика): [`modules/core.md`](modules/core.md)
- Config (BaseConfig, programs/index, валидация): [`modules/config.md`](modules/config.md)
- FSManager (LittleFS, atomic write, deferred): [`modules/fs.md`](modules/fs.md)
- Web (SoftAP, API, SSE): [`modules/web.md`](modules/web.md)
- GSM/Modem (SIM800, FSM/URC, transport): [`modules/gsm_modem.md`](modules/gsm_modem.md)
- MQTT (`MqttFsmClient` / `MQTTClient`, команды/статус): [`modules/mqtt.md`](modules/mqtt.md)
- OTA (update.bin формат, прошивка, ассеты): [`modules/ota.md`](modules/ota.md)
- ProgramExecutor (шаги, действия, таймеры): [`modules/program.md`](modules/program.md)
- IO (inputs/relays/sensors): [`modules/io.md`](modules/io.md)
- Common (Constants, PoolManager, Logger, Utils): [`modules/common.md`](modules/common.md)

