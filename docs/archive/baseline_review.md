# Baseline review (ESP8266)

Цель этого документа — зафиксировать текущее состояние архитектуры и “узкие места” по памяти/границам модулей
перед рефакторингом. Документ не является “дизайном с нуля”: он описывает то, что уже есть в кодовой базе,
и где именно мы будем ужесточать контракты.

## 1) Карта модулей (что где живёт)

### Точка входа
- `src/main.cpp`: минимальный `setup()/loop()`, делегирует в `core.begin()` и `core.update()`.

### Core orchestration
- `include/core/Core.h` + `src/core/Core.*.cpp`: единая точка управления циклом, выбор режима, периодика, health.
- `include/core/ModeManager.h` + `src/core/Core.ModeManager.cpp`: enter/exit режимов и включение подсистем.
- `src/core/internal/CorePrivate.h`: “pimpl без heap” — хранит все подсистемы как поля, фиксируя RAM-профиль.

### Domain (бизнес-логика устройства)
- `include/io/*` + `src/io/*`: входы/реле/сенсоры, горячий путь (update в каждом цикле).
- `include/program/*` + `src/program/*`: исполнитель программ, шаги, таймеры, действие/условия.
- `include/core/*Manager.h` + `src/core/Core.*Manager.cpp`: триггеры/термостат/battery-saver.

### Services
- `include/config/*` + `src/config/*`: загрузка/сохранение базового конфига, работа с файлами программ и индексом.
- `include/fs/FSManager.h` + `src/fs/*`: LittleFS (атомарные записи, deferred операции, healthcheck/GC).
- `include/gsm/*` + `src/gsm/*`: SIM800 FSM + URC разбор, транспорт для MQTT.
- `include/mqtt/MQTTClient.h` + `include/mqtt/MqttFsmClient.h` + `src/mqtt/*`: неблокирующий MQTT (FSM), публикация статуса и обработка команд.
- `include/ota/OTAHandler.h` + `src/ota/*`: режим OTA (ожидание файла, прошивка, извлечение ассетов).

### UI / Web
- `include/web/WebServer.h` + `src/web/*`: SoftAP + captive portal + HTTP API + SSE статус/лог.
- `src/web/internal/*`: “внутренние” helpers и runtime-реализация (не часть public API).

### Common
- `include/common/*` + `src/common/*`: константы/лимиты, CRC/таймеры, логгер, пул буферов.

## 2) Ключевые архитектурные решения (уже приняты)

### Предсказуемость RAM вместо динамики
- Конфиг хранит строки как фиксированные `char[]` (см. `include/config/ConfigTypes.h`), что снижает риск фрагментации heap.
- Большая часть зависимостей собрана композиционно в `CorePrivate` (а не через heap/pointers), чтобы RAM-профиль был стабильным.

### Минимизация стоимости виртуальности
- В проекте почти нет `virtual`; наследование используется точечно там, где это Arduino-контракт:
  - `Sim800ClientAdapter : Client` (TCP transport adapter)
  - `JsonFilePrint : Print` (потоковая запись JSON в файл без частых yield)

### Явные бюджеты под JSON/SSE/MQTT
- `include/common/Constants.h` содержит лимиты `JsonBytes::*`, `WebSseLimits::*`, текстовые буферы и тайминги.
- `include/common/PoolManager.h` вводит слоты для JSON/bytes, чтобы web/config/fs не конкурировали за heap.

## 3) Что считается “горячим путём”

Горячий путь — всё, что вызывается часто из `Core::update()`/`Core::handle*()`:
- `io/*::update()`
- `program::ProgramExecutor::update()`
- `TriggerManager/BatterySaverManager/ThermostatManager::update()`
- GSM/MQTT service циклы (в NORMAL режимах)
- web: `WebServer::update()` (DNS/session lease/SSE housekeeping) + SSE status broadcast по таймеру

В горячем пути считаем недопустимым:
- активное использование `String` и конкатенаций;
- частые динамические аллокации;
- тяжёлые/долгие flash операции без time-slicing.

## 4) Наблюдаемые риски и точки для усиления контрактов

### 4.1 Compile fanout / связанность заголовков
- Некоторые public заголовки тянут тяжёлые зависимости (например, web несёт `ESPAsyncWebServer.h` по всему проекту).
  Это допустимо, но требует осознанных границ: “кто имеет право включать web API”.

### 4.2 Взаимосвязь web ↔ FS ↔ Config
- В web-слое есть легитимная зависимость от FS для отдачи статических файлов и JSON из FS, но важно удержать её узкой:
  web не должен напрямую делать произвольные `LittleFS.*` вызовы, только через `FSManager` и через явно описанный контракт.

### 4.3 Места с неизбежным `String`
- `ESPAsyncWebServer` параметры (`request->getParam(...)->value()`) возвращают `String`.
- `Dir::fileName()` (LittleFS Dir API) возвращает `String`.

Политика на эти случаи:
- `String` допускается только как короткоживущая переменная, без кэширования в полях, без работы в tight loops.

### 4.4 Размер `BaseConfig` и “постоянная” RAM
`BaseConfig` содержит массивы сенсоров/входов/триггеров и всегда находится в RAM.
Это хорошо для скорости, но важно:
- держать лимиты в `Constants.h` и понимать стоимость каждого поля;
- избегать “дутых” структур (например, избыточных `char[]` в глубоко вложенных массивах).

## 5) Что будем делать дальше (коротко)
- Ввести/уточнить документ правил границ модулей (public/internal, направление зависимостей, политика `String`/JSON).
- Уменьшить транзитивные include и упростить узкие контракты между модулями без изменения поведения.
- Привести комментарии к “контрактному” стилю и убрать шум/устаревшее.

