/**
 * @file RelayController.Core.cpp
 * @brief Реализация подсистемы IO: управление реле и safety-ограничения (таймауты).
 *
 * Инварианты:
 * - Управление реле должно быть детерминированным и быстрым (без heap/`String`).
 * - Safety-check выполняется регулярно в `update()` и может форсировать OFF по таймауту.
 *
 * Память:
 * - Состояние хранится компактно: битмаска + массивы таймеров фиксированной длины.
 *
 * Запрещено:
 * - Делать блокирующие задержки при переключении реле.
 * - Включать/выключать реле из ISR (только из loop).
 */
#include "io/RelayController.h"
#include "core/Core.h"
#include "config/Config.h"
#include "core/ErrorManager.h"
#include "program/ProgramExecutor.h"
#include "common/Utils.h"
#include "common/Logger.h"
#include "common/Constants.h"
#include "io/HwMap.h"

RelayController::RelayController() : _state(0) {
    memset(_maxOnTime, 0, sizeof(_maxOnTime));
    memset(_turnOnTime, 0, sizeof(_turnOnTime));
}

void RelayController::begin() {
    logger.log("[RelayController] begin\n");
    for (int i = 0; i < HardwareLimits::RELAYS; i++) {
        uint8_t pin = pinForRelay(i);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    
    for (int i = 0; i < HardwareLimits::RELAYS; i++) {
        if (digitalRead(pinForRelay(i)) == HIGH) {
            _state |= (1 << i);
            _turnOnTime[i] = millis();
        }
    }
    
    // Устанавливаем максимальное время для реле стартера (если задано)
    const uint16_t starterRelayId = config.getBase().vehicle.starter_relay_id;
    const int8_t starterRelay = HwMap::relayIndexById(starterRelayId);
    uint16_t starterTime = config.getBase().vehicle.starter_max_time_sec;
    if (starterTime > 0 && starterRelay >= 0) {
        _maxOnTime[(uint8_t)starterRelay] = secToMs(starterTime);
        logger.log("[RelayController] Starter relay_id: %u (idx=%d), max time: %u ms\n",
                   (unsigned)starterRelayId, (int)starterRelay, (unsigned)_maxOnTime[(uint8_t)starterRelay]);
    }
    
    logger.log("[RelayController] OK\n");
}

uint8_t RelayController::pinForRelay(uint8_t relay) const {
    if (relay >= HardwareLimits::RELAYS) return 0;
    return Pin::RELAY_PINS[relay];
}

void RelayController::setRelay(uint8_t relay, bool state) {
    if (relay >= HardwareLimits::RELAYS) return;
    uint8_t pin = pinForRelay(relay);
    digitalWrite(pin, state ? HIGH : LOW);
    
    if (state) {
        _state |= (1 << relay);
        _turnOnTime[relay] = millis();
    } else {
        _state &= ~(1 << relay);
        _turnOnTime[relay] = 0;
    }
}

void RelayController::on(uint8_t relay) {
    setRelay(relay, true);
}

void RelayController::off(uint8_t relay) {
    setRelay(relay, false);
}

void RelayController::toggle(uint8_t relay) {
    if (relay >= HardwareLimits::RELAYS) return;
    setRelay(relay, !getState(relay));
}

void RelayController::allOff() {
    for (int i = 0; i < HardwareLimits::RELAYS; i++) {
        off(i);
    }
}

void RelayController::allOn() {
    for (int i = 0; i < HardwareLimits::RELAYS; i++) {
        on(i);
    }
}

void RelayController::setMask(uint8_t mask) {
    for (int i = 0; i < HardwareLimits::RELAYS; i++) {
        bool state = (mask >> i) & 1;
        if (getState(i) != state) {
            setRelay(i, state);
        }
    }
}

bool RelayController::getState(uint8_t relay) const {
    if (relay >= HardwareLimits::RELAYS) return false;
    return (_state >> relay) & 1;
}

void RelayController::checkSafety() {
    uint32_t now = millis();
    const uint16_t starterRelayId = config.getBase().vehicle.starter_relay_id;
    const int8_t starterRelay = HwMap::relayIndexById(starterRelayId);
    const bool starterActive = core.getProgramExecutor().isInStarterStep();
    for (int i = 0; i < HardwareLimits::RELAYS; i++) {
        // Таймаут стартера применяем только когда реально выполняется шаг STARTER_*.
        if (starterRelay >= 0 && (uint8_t)i == (uint8_t)starterRelay && !starterActive) continue;
        if (getState(i) && _maxOnTime[i] > 0) {
            if (now - _turnOnTime[i] > _maxOnTime[i]) {
                // Safety-net: реле “не должно” оставаться включённым дольше лимита.
                // Особенно критично для стартера/нагревателей — принудительно выключаем и фиксируем ошибку.
                logger.log("[RelayController] ⚠️ Relay %d timeout! Forcing OFF\n", i+1);
                setRelay(i, false);
                core.getErrorManager().set(ErrorCode::RELAY_TIMEOUT);
            }
        }
    }
}

void RelayController::update() {
    checkSafety();
}

