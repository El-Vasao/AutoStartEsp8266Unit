// include/core/ErrorManager.h
#pragma once

#include <Arduino.h>
#include "common/Constants.h"
#include "common/ErrorCodes.h"

/**
 * @brief Структура для хранения ошибки в RTC-памяти.
 */
struct RtcErrorRecord {
    uint32_t magic;        ///< магическое число для проверки валидности (0xDEADBEEF)
    ErrorCode code;        ///< код ошибки
    uint32_t uptime;       ///< время возникновения ошибки (секунды с момента запуска)
    uint16_t crc;          ///< контрольная сумма для проверки целостности
} __attribute__((aligned(4)));

/**
 * @brief Менеджер ошибок с сохранением в RTC.
 * Хранит текущую ошибку и время её возникновения, дублирует в RTC_DATA_ATTR
 * (переживает deep sleep / soft reset; не переживает полное отключение питания).
 *
 * Зачем RTC:
 * - если устройство перезагрузилось (WDT/OOM), последняя “важная” ошибка остаётся доступной после старта,
 *   и её можно показать в UI/логах.
 *
 * Ограничение:
 * - RTC retention маленькая и не предназначена для частых/больших записей; здесь хранится только один рекорд.
 */
class ErrorManager {
public:
    ErrorManager();

    // Установить текущую ошибку (автоматически сохраняется в RTC)
    void set(ErrorCode err);

    // Очистить текущую ошибку и стереть запись в RTC
    void clear();

    // Получить текущий код ошибки
    ErrorCode get() const { return _lastError; }

    // Получить строковое описание текущей ошибки
    const char* getMessage() const { return errorCodeToString(_lastError); }

    // Время возникновения текущей ошибки (секунды аптайма)
    uint32_t getTime() const { return _errorTime; }

    // Загрузить ошибку из RTC-памяти (вызывается в конструкторе)
    void loadFromRtc();

    // Сохранить текущую ошибку в RTC
    void saveToRtc();

    // Очистить запись в RTC
    void clearRtc();

private:
    ErrorCode _lastError;           ///< текущий код ошибки
    uint32_t _errorTime;            ///< время возникновения (секунды)

    static constexpr uint32_t RTC_MAGIC = 0xDEADBEEF;
};
