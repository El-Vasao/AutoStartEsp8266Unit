# `ota`: обновление прошивки и ассетов

## Роль
`OTAHandler` выполняет обновление из файла `/update.bin`, который предварительно загружает web-подсистема.

Основные файлы:
- `include/ota/OTAHandler.h`
- `src/ota/OTAHandler.Core.cpp`, `src/ota/OTAHandler.Firmware.cpp`, `src/ota/OTAHandler.Files.cpp`

## Формат `/update.bin`
Последовательность:
1) **Header (4 байта, little-endian)**: размер прошивки `fwSize`
2) **Firmware bytes**: `fwSize` байт, пишутся через `Updater`
3) **Tail (опционально)**: набор файлов для LittleFS:
   - `nameLen` (2 байта, little-endian). `0` означает конец.
   - `name` (`nameLen` байт, без `\0`)
   - `fileSize` (4 байта, little-endian)
   - `fileData` (`fileSize` байт)

## Инварианты ESP8266
- Во время прошивки и извлечения файлов обязательно обслуживать watchdog и делать `yield()`/`cooperate()`.
- Запись ассетов в LittleFS делается через `FSManager` потоковую запись (temp + commit).
- Перед `Update.begin` и на ключевых этапах OTA фиксируются heap-метрики (`free`, `max block`, `fragmentation`) для baseline/регрессий.

## Memory lifecycle в OTA пути

1. `POST /upload` открывает поток `/update.bin` и активирует пред-OTA pressure режим.
2. Пока идёт upload, NORMAL-подсистемы (GSM/MQTT) приглушаются, SSE/log работают в более жёстких лимитах очереди.
3. При переходе в `OTA_UPDATE`:
   - pressure режим выключается;
   - SSE закрывается (`closeSseForOta`);
   - `OTAHandler` выполняет `Update.begin/write/end` и tail extraction.
4. На ошибке upload/OTA выполняется очистка `/update.bin` и возврат/ребут по текущему контракту.

## Failure-моды
- Если update не удался — `/update.bin` удаляется и выполняется reboot (чтобы не зависнуть в OTA режиме).

