// include/common/EspHal.h
#pragma once

#include <Arduino.h>
#include <esp_system.h>

/**
 * Thin ESP32-C3 HAL shims replacing ESP8266-only APIs
 * (ESP.wdtFeed / getChipId / getMaxFreeBlockSize / heap fragmentation).
 */
inline uint32_t espHalFreeHeap() {
    return ESP.getFreeHeap();
}

inline uint32_t espHalMaxBlock() {
    return ESP.getMaxAllocHeap();
}

inline void espHalFeedWdt() {
    yield();
}

inline void espHalWdtEnable() {
    // ESP32 task WDT is configured by the framework; no WDTO_8S equivalent.
}

/** Lower 24 bits of eFuse MAC — stable MQTT client-id suffix (replaces ESP.getChipId()). */
inline uint32_t espHalChipId() {
    const uint64_t mac = ESP.getEfuseMac();
    return static_cast<uint32_t>(mac & 0xFFFFFFull);
}

inline bool espHalChipIdValid() {
    return espHalChipId() != 0;
}
