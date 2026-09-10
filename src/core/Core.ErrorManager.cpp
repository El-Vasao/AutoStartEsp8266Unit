// src/core/Core.ErrorManager.cpp
#include "core/ErrorManager.h"
#include "common/Utils.h"
#include "common/Version.h"
#include "common/Logger.h"

/**
 * @file Core.ErrorManager.cpp
 * @brief Хранение/публикация последней ошибки + RTC persist (RTC_DATA_ATTR на ESP32).
 *
 * Инварианты:
 * - Быстро и детерминированно: только простые записи/чтения RTC и логирование.
 * - Без heap и без тяжёлых зависимостей.
 *
 * Запрещено:
 * - Делать сложную обработку “дерева ошибок” тут (это задача Core/подсистем).
 */

namespace {
RTC_DATA_ATTR RtcErrorRecord s_rtcErrorRecord;
}

ErrorManager::ErrorManager() : _lastError(ErrorCode::NONE), _errorTime(0) {
}

void ErrorManager::set(ErrorCode err) {
    _lastError = err;
    _errorTime = millis() / Time::MS_PER_SEC;
    logger.log("[ErrorManager] ERROR: %s [%d]\n", errorCodeToString(err), (int)err);
    saveToRtc();
}

void ErrorManager::clear() {
    _lastError = ErrorCode::NONE;
    _errorTime = 0;
    clearRtc();
    logger.log("[ErrorManager] Cleared\n");
}

void ErrorManager::loadFromRtc() {
    RtcErrorRecord record = s_rtcErrorRecord;

    // RTC может содержать мусор после прошивки/первого старта — проверяем magic+CRC.
    if (record.magic != RTC_MAGIC) return;

    uint16_t crc = calculateCRC16((const uint8_t*)&record, sizeof(record) - sizeof(record.crc));
    if (crc != record.crc) return;

    _lastError = record.code;
    _errorTime = record.uptime;

    logger.log("[ErrorManager] Loaded from RTC: %s\n", errorCodeToString(_lastError));
}

void ErrorManager::saveToRtc() {
    RtcErrorRecord record;
    record.magic = RTC_MAGIC;
    record.code = _lastError;
    record.uptime = millis() / Time::MS_PER_SEC;
    record.crc = calculateCRC16((const uint8_t*)&record, sizeof(record) - sizeof(record.crc));

    s_rtcErrorRecord = record;
}

void ErrorManager::clearRtc() {
    memset(const_cast<RtcErrorRecord*>(&s_rtcErrorRecord), 0, sizeof(s_rtcErrorRecord));
}
