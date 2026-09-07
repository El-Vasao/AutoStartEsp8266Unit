# Сборка, прошивка и файловая система (PlatformIO)

Документ описывает *повторяемый* путь сборки/прошивки для ESP8266 (ESP-12E) и загрузки web-ассетов в LittleFS.

## Требования
- PlatformIO Core (через VSCode PlatformIO или через CLI).
- USB-UART (если прошивка/монитор через UART) или OTA (через UI, если доступно).

Проектные параметры см. в [`platformio.ini`](../platformio.ini).

## Базовые команды (CLI)

Из корня проекта:

```bash
# Сборка прошивки
python -m platformio run

# Очистка
python -m platformio run -t clean

# Прошивка (если настроен upload_port или автоопределение)
python -m platformio run -t upload

# Монитор порта
python -m platformio device monitor
```

Примечание: если `pio`/`platformio` в PATH — можно использовать `pio run`, но `python -m platformio ...` стабильнее,
когда PlatformIO установлен через `pip` в user-site.

## Скрипт `build.py` (альтернативный пайплайн)

В корне есть [`build.py`](../build.py) — обёртка для типичных сценариев: генерация [`include/common/Version.h`](../include/common/Version.h), сборка через PlatformIO, опционально OTA-артефакт в [`dist/`](../dist), упаковка `data/` (в т.ч. gzip для OTA/web) и `uploadfs`.

Краткая справка:

```bash
python build.py --help

# Release-сборка; с заливкой прошивки:
python build.py release -u

# Debug (SERIAL_DEBUG в Version.h — UART может совпасть с GSM, см. предупреждение в скрипте):
python build.py debug -u

# Собрать bundle в data/, загрузить LittleFS (uploadfs):
python build.py fs
```

Прямые команды `python -m platformio ...` из разделов выше остаются полностью валидны — используйте их, если не нужна автоматизация версии/`dist`/gzip.

## LittleFS: сборка и загрузка web-ассетов

В проекте включена LittleFS:
- `board_build.filesystem = littlefs`

Типовой цикл:

```bash
# Собрать образ файловой системы из папки data/
python -m platformio run -t buildfs

# Залить образ файловой системы на устройство
python -m platformio run -t uploadfs
```

### Что должно лежать в `data/`
- web-страницы и статические ассеты (UI).
- конфигурационные файлы по необходимости (но секреты/ключи лучше не хранить в репозитории).

## OTA: общий сценарий

В проекте есть “recovery OTA” страница из PROGMEM, которая используется когда web-ассеты отсутствуют в LittleFS
(см. `include/web/FallbackWebPage.h`).

Сценарий:
1) Подключиться к SoftAP устройства.
2) Открыть UI.
3) Загрузить OTA-пакет на endpoint `/upload`.
4) Запустить прошивку через `/ota/start` (или кнопку UI).

### Важно про формат OTA
`OTAHandler` читает `/update.bin` и ожидает:
- 4-байтовый little-endian заголовок размера прошивки
- бинарник прошивки
- (опционально) “хвост” с дополнительными файлами (имя/размер/данные), которые будут записаны в LittleFS

Подробности см. в [`modules/ota.md`](modules/ota.md).

## Диагностика проблем сборки

### “Missing include / symbol not declared”
В этом проекте намеренно уменьшается “транзитивность” include’ов: если тип используется в `.cpp`, он должен быть включён
в этом `.cpp`, а не случайно приходить через чужой header.

### Ограничения памяти
ESP8266 чувствителен к heap-фрагментации и к очередям lwIP.
Если прошивка стала “падать” после изменений UI/API:
- проверьте ограничения `JsonBytes::*`, `WebSseLimits::*` в `include/common/Constants.h`
- проверьте, что не появилось новых `String` конкатенаций/динамических JSON в hot-path

