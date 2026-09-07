/**
 * @file DigitalInputs.Core.cpp
 * @brief Реализация подсистемы IO: цифровые входы и импульсные счётчики.
 *
 * Инварианты:
 * - `update()` не должен аллоцировать heap (никаких `String`, никаких контейнеров).
 * - Пульс-счётчики используют ISR (`attachInterrupt`) и атомарное чтение счётчиков.
 *
 * Память/устойчивость:
 * - Все структуры фиксированного размера (по `HardwareLimits::INPUTS`).
 * - Debounce и частота считаются по таймерам (`Timing::*`) без блокировок.
 *
 * Запрещено:
 * - Увеличивать количество входов без проверки ISR-обработчиков (нужно добавить case в attach).
 * - Делать тяжёлое логирование из ISR.
 */
#include "io/DigitalInputs.h"
#include "config/Config.h"
#include "common/Pins.h"
#include "common/Version.h"
#include "core/Core.h"
#include "common/Utils.h"
#include "common/Logger.h"
#include "common/Constants.h"

volatile uint32_t DigitalInputs::_pulseCounts[HardwareLimits::INPUTS] = {0};

DigitalInputs::DigitalInputs() {
    for (uint8_t i = 0; i < HardwareLimits::INPUTS; i++) {
        inputs[i].pin = Pin::INPUT_PINS[i];
        inputs[i].currentState = HIGH;
        inputs[i].lastRawState = HIGH;
        inputs[i].lastChangeTime = 0;
        inputs[i].stable = true;
        prevStates[i] = HIGH;
        _runtimeEnabled[i] = true; // по умолчанию включено, позже синхронизируем с конфигом
    }
    memset(_frequencies, 0, sizeof(_frequencies));
    memset(_lastCalcTime, 0, sizeof(_lastCalcTime));
}

int8_t DigitalInputs::findIndexById(uint16_t inputId) const {
    if (inputId == 0) return -1;
    const auto& base = config.getBase();
    for (uint8_t i = 0; i < HardwareLimits::INPUTS; i++) {
        if (base.inputs[i].id == inputId) return (int8_t)i;
    }
    return -1;
}

void DigitalInputs::begin() {
    logger.log("[DigitalInputs] begin\n");
    for (uint8_t i = 0; i < HardwareLimits::INPUTS; i++) {
        const auto& cfg = config.getBase().inputs[i];
        uint8_t pin = inputs[i].pin;
        pinMode(pin, INPUT_PULLUP);

        // Устанавливаем runtime-флаг из конфига
        _runtimeEnabled[i] = cfg.enabled;

        if (cfg.type == InputType::PULSE_COUNTER) {
            attachCounterInterrupt(i);
            _lastCalcTime[i] = millis();
        } else {
            bool raw = digitalRead(pin);
            inputs[i].currentState = raw;
            inputs[i].lastRawState = raw;
            inputs[i].stable = true;
            prevStates[i] = raw;
        }
    }

    logger.log("[DigitalInputs] OK\n");
    for (uint8_t i = 0; i < HardwareLimits::INPUTS; i++) {
        const auto& cfg = config.getBase().inputs[i];
        char fallback[TextBytes::Inputs::FALLBACK_NAME];
        snprintf(fallback, sizeof(fallback), "IN%u", (unsigned)(i + 1));
        const char* displayName = (cfg.name[0] != '\0') ? cfg.name : fallback;
        logger.log("[DigitalInputs]   Input %d: %s, type=%s, active_state=%s, runtime=%s\n", i+1, displayName,
                   cfg.type == InputType::DIGITAL ? "DIGITAL" : "PULSE_COUNTER",
                   cfg.active_state ? "HIGH" : "LOW",
                   _runtimeEnabled[i] ? "ON" : "OFF");
    }
}

void DigitalInputs::attachCounterInterrupt(uint8_t index) {
    uint8_t pin = inputs[index].pin;
    switch (index) {
        // Примечание: используем CHANGE, поэтому импульсы считаются на каждом изменении уровня.
        // Если нужен подсчёт только по фронту — заменить на RISING/FALLING и пересчитать pulses_per_rev.
        case 0: attachInterrupt(digitalPinToInterrupt(pin), handleInterrupt0, CHANGE); break;
        case 1: attachInterrupt(digitalPinToInterrupt(pin), handleInterrupt1, CHANGE); break;
        case 2: attachInterrupt(digitalPinToInterrupt(pin), handleInterrupt2, CHANGE); break;
        // при необходимости добавить case для большего количества входов
    }
}

void DigitalInputs::detachCounterInterrupt(uint8_t index) {
    uint8_t pin = inputs[index].pin;
    detachInterrupt(digitalPinToInterrupt(pin));
}

void IRAM_ATTR DigitalInputs::handleInterrupt0() {
    _pulseCounts[0]++;
}
void IRAM_ATTR DigitalInputs::handleInterrupt1() {
    _pulseCounts[1]++;
}
void IRAM_ATTR DigitalInputs::handleInterrupt2() {
    _pulseCounts[2]++;
}

void DigitalInputs::update() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < HardwareLimits::INPUTS; i++) {
        if (!_runtimeEnabled[i]) {
            // Если вход отключён, пропускаем обработку
            continue;
        }

        const auto& cfg = config.getBase().inputs[i];
        if (cfg.type == InputType::PULSE_COUNTER) {
            if (now - _lastCalcTime[i] >= Timing::PULSE_COUNTER_INTERVAL_MS) {
                uint32_t interval = now - _lastCalcTime[i];
                uint32_t cnt;
                ATOMIC_START();
                cnt = _pulseCounts[i];
                _pulseCounts[i] = 0;
                ATOMIC_END();
                float freqHz = (cnt * Time::MS_PER_SEC_F) / interval;
                _frequencies[i] = freqHz * Time::SEC_PER_MIN_F / cfg.pulse.pulses_per_rev;
                _lastCalcTime[i] = now;
            }
        } else {
            prevStates[i] = inputs[i].currentState;
            bool raw = digitalRead(inputs[i].pin);
            if (raw != inputs[i].lastRawState) {
                inputs[i].lastRawState = raw;
                inputs[i].lastChangeTime = now;
                inputs[i].stable = false;
            }
            if (!inputs[i].stable && (now - inputs[i].lastChangeTime >= Timing::DEBOUNCE_DELAY_MS)) {
                inputs[i].currentState = raw;
                inputs[i].stable = true;
            }
        }
    }
}

bool DigitalInputs::getState(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return false;
    if (!_runtimeEnabled[index]) return false; // если отключён, считаем неактивным
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type == InputType::PULSE_COUNTER) {
        // Для счётчика состояние = превышение порога (если threshold_rpm > 0)
        if (cfg.pulse.threshold_rpm > 0) {
            return _frequencies[index] >= cfg.pulse.threshold_rpm;
        }
        return false; // без порога состояние не определено
    }
    bool raw = inputs[index].currentState;
    return (raw == cfg.active_state);
}

bool DigitalInputs::getRawState(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return HIGH;
    return inputs[index].currentState;
}

bool DigitalInputs::isCounter(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return false;
    return config.getBase().inputs[index].type == InputType::PULSE_COUNTER;
}

float DigitalInputs::getFrequency(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return 0;
    return _frequencies[index];
}

uint32_t DigitalInputs::getRawPulseCount(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return 0;
    return _pulseCounts[index];
}

bool DigitalInputs::isStable(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return false;
    return inputs[index].stable;
}

bool DigitalInputs::wasPressed(uint8_t index) {
    if (index >= HardwareLimits::INPUTS) return false;
    if (!_runtimeEnabled[index]) return false;
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type != InputType::DIGITAL) return false;
    // Переход из неактивного в активное
    bool wasActive = (prevStates[index] == cfg.active_state);
    bool nowActive = (inputs[index].currentState == cfg.active_state);
    return !wasActive && nowActive && inputs[index].stable;
}

bool DigitalInputs::wasReleased(uint8_t index) {
    if (index >= HardwareLimits::INPUTS) return false;
    if (!_runtimeEnabled[index]) return false;
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type != InputType::DIGITAL) return false;
    bool wasActive = (prevStates[index] == cfg.active_state);
    bool nowActive = (inputs[index].currentState == cfg.active_state);
    return wasActive && !nowActive && inputs[index].stable;
}

bool DigitalInputs::wasTriggered(uint8_t index, bool level) {
    if (index >= HardwareLimits::INPUTS) return false;
    if (!_runtimeEnabled[index]) return false;
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type != InputType::DIGITAL) return false;
    return prevStates[index] != level && inputs[index].currentState == level && inputs[index].stable;
}

bool DigitalInputs::isHeld(uint8_t index, uint32_t ms) {
    if (index >= HardwareLimits::INPUTS) return false;
    if (!_runtimeEnabled[index]) return false;
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type != InputType::DIGITAL) return false;
    if (!inputs[index].stable) return false;
    if (inputs[index].currentState != cfg.active_state) return false; // удерживается только активное состояние
    return (millis() - inputs[index].lastChangeTime) >= ms;
}

uint32_t DigitalInputs::getHoldTime(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return 0;
    if (!_runtimeEnabled[index]) return 0;
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type != InputType::DIGITAL) return 0;
    if (inputs[index].currentState != cfg.active_state) return 0;
    return millis() - inputs[index].lastChangeTime;
}

bool DigitalInputs::wasButtonPressed() {
    return wasPressed(2);
}
bool DigitalInputs::wasButtonReleased() {
    return wasReleased(2);
}
bool DigitalInputs::isButtonHeld(uint32_t ms) {
    return isHeld(2, ms);
}

bool DigitalInputs::isRuntimeEnabled(uint8_t index) const {
    if (index >= HardwareLimits::INPUTS) return false;
    return _runtimeEnabled[index];
}

void DigitalInputs::setRuntimeEnabled(uint8_t index, bool en) {
    if (index >= HardwareLimits::INPUTS) return;
    if (_runtimeEnabled[index] == en) return;
    _runtimeEnabled[index] = en;
    const auto& cfg = config.getBase().inputs[index];
    if (cfg.type == InputType::PULSE_COUNTER) {
        if (en) {
            attachCounterInterrupt(index);
            _lastCalcTime[index] = millis(); // сброс времени для корректного расчёта частоты
        } else {
            detachCounterInterrupt(index);
            _frequencies[index] = 0; // сброс частоты
            // счётчик импульсов не сбрасываем – можно оставить для истории
        }
    } else {
        // Для цифрового входа можно ничего не делать, просто перестаём обновлять состояние
        // (уже учтено в update)
    }
    logger.log("[DigitalInputs] Input %d runtime %s\n", index+1, en ? "enabled" : "disabled");
}

