// include/io/RelayController.h
#pragma once

#include <Arduino.h>
#include "common/Pins.h"
#include "common/Constants.h"

// Пины реле больше не объявляем отдельно — они в Pins.h

/**
 * @brief Контроллер управления реле.
 * Обеспечивает включение/выключение реле, контроль максимального времени работы.
 *
 * Инварианты:
 * - индексация во всём проекте: публичные методы принимают индекс 0..MAX_RELAYS-1.
 *   В конфиге и UI чаще используется 1..N — при конвертации не перепутайте.
 *
 * Безопасность:
 * - при аварийном завершении программ `ProgramExecutor` может выключать реле;
 * - `checkSafety()` принудительно выключает реле, если превышен таймаут (например, для стартера).
 */
class RelayController {
public:
    RelayController();

    // Инициализация пинов, установка максимального времени из конфига
    void begin();

    // Проверка таймаутов (вызывается в loop)
    void update();

    // Включить реле (индекс с 0)
    void on(uint8_t relay);

    // Выключить реле
    void off(uint8_t relay);

    // Переключить состояние реле
    void toggle(uint8_t relay);

    // Выключить все реле
    void allOff();

    // Включить все реле
    void allOn();

    // Установить состояние по битовой маске
    void setMask(uint8_t mask);

    // Получить состояние конкретного реле
    bool getState(uint8_t relay) const;

    // Получить битовую маску состояний
    uint8_t getMask() const { return _state; }

private:
    uint8_t _state;                             ///< битовая маска текущего состояния
    uint32_t _maxOnTime[HardwareLimits::RELAYS];    ///< максимальное время работы (0 = без ограничения)
    uint32_t _turnOnTime[HardwareLimits::RELAYS];   ///< момент включения (0 = выключено)

    // Получить номер пина для реле (использует Pin::RELAY_PINS)
    uint8_t pinForRelay(uint8_t relay) const;

    // Установить состояние реле (низкоуровнево)
    void setRelay(uint8_t relay, bool state);

    // Проверить превышение времени работы
    void checkSafety();
};

