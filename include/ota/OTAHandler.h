// include/ota/OTAHandler.h
#pragma once

#include <Arduino.h>
#include <FS.h>

/**
 * @brief Обработчик OTA-обновлений.
 * Отвечает за ожидание файла update.bin, проверку заголовка,
 * запись прошивки, извлечение дополнительных файлов и перезагрузку.
 *
 * Формат `/update.bin` должен оставаться согласованным между:
 * - HTTP upload обработчиком (web-подсистема, запись файла в LittleFS)
 * - обработчиком OTA (этот класс: чтение заголовка/прошивка/извлечение ассетов)
 *
 * Ограничения ESP8266:
 * - запись прошивки должна периодически “yield()/feedWatchdog()”;
 * - файл прошивки должен быть достаточно маленьким для свободного места во flash/FS.
 */
class OTAHandler {
public:
    OTAHandler();

    // Запуск процесса OTA (вызывается из ModeManager)
    void begin();

    // Выполнение шагов обновления (вызывается в цикле)
    void update();

    /// Сброс сессии при выходе из `CoreMode::OTA_UPDATE` (таймаут, смена режима).
    void onModeExit();

    /// Перед `POST /upload`: ждём multipart `final`, иначе таймаут `Timing::OTA_HTTP_UPLOAD_IDLE_MS`.
    void prepareHttpUploadSession();

    /// Перед `POST /ota/start`, когда `/update.bin` уже на диске — сразу разрешаем `processUpdateFile()`.
    void prepareImmediateFirmwareProcess();

    /// Завершение HTTP upload (`final` multipart): `ok==false` — выход в NORMAL без перезагрузки.
    void notifyHttpUploadComplete(bool ok);

    // Проверка, идёт ли процесс обновления
    bool isOngoing() const { return _ongoing; }

private:
    bool _ongoing;               ///< флаг, что процесс запущен
    uint32_t _lastLogTime;       ///< время последнего вывода "Waiting for update file..."

    bool _awaitHttpUploadSession{false};
    bool _httpUploadDone{false};
    bool _httpUploadOk{false};
    uint32_t _httpSessionStartMs{0};

    // Обработка файла update.bin (прошивка + доп. файлы)
    bool processUpdateFile();

    // Извлечение дополнительных файлов после прошивки
    bool extractFiles(File& f);
};

