// include/io/HwMap.h
#pragma once

#include <Arduino.h>
#include "common/Pins.h"
#include "common/Constants.h"

/**
 * @brief Centralized mapping between hardware channel IDs and internal indices.
 *
 * Safety-first contract:
 * - External/business layers should use IDs.
 * - Drivers may store state by index, but must not accept indices from outside.
 */
namespace HwMap {

inline int8_t relayIndexById(uint16_t relayId) {
    if (!relayId) return -1;
    for (uint8_t i = 0; i < HardwareLimits::RELAYS; i++) {
        if (Pin::RELAY_IDS[i] == relayId) return (int8_t)i;
    }
    return -1;
}

inline int8_t inputIndexById(uint16_t inputId) {
    if (!inputId) return -1;
    for (uint8_t i = 0; i < HardwareLimits::INPUTS; i++) {
        if (Pin::INPUT_IDS[i] == inputId) return (int8_t)i;
    }
    return -1;
}

} // namespace HwMap

