#include "config/Config.h"
#include "config/DefaultConfig.h"
#include "fs/FSManager.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include "config/internal/BaseConfigJsonIo.h"

#include <ESP8266WiFi.h>

namespace {

size_t encodeResetConfig(Print& p, void* ctx) {
    const auto* cfg = reinterpret_cast<const BaseConfig*>(ctx);
    return config_internal::serializeBaseConfigToPrint(*cfg, p);
}

} // namespace

bool Config::reset() {
    logger.log("[Config] Resetting to factory defaults\n");

    char uniqueSSID[TextBytes::Wifi::SSID];
    generateUniqueSSID(uniqueSSID, sizeof(uniqueSSID));

    BaseConfig cfg{};
    if (!config_internal::parseBaseConfigStreamingFromProgmem(cfg, DEFAULT_CONFIG_JSON)) {
        logger.log("[Config] Failed to parse default config JSON\n");
        return false;
    }
    strlcpy(cfg.wifi.ssid, uniqueSSID, sizeof(cfg.wifi.ssid));

    if (!fileSystem.writeJsonAtomicStream("/config.json", encodeResetConfig, &cfg, Limits::CONFIG_JSON_SIZE)) {
        logger.log("[Config] Failed to write config file\n");
        return false;
    }
    ESP.wdtFeed();

    baseCache = cfg;
    size_t serLen = 0;
    configCRC = config_internal::crc16SerializedBaseConfig(baseCache, &serLen);
    loaded = true;

    logger.log("[Config] Write successful, CRC=%04X\n", configCRC);
    return true;
}
