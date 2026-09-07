#include "fs/FSManager.h"
#include "common/Logger.h"
#include "common/Utils.h"

#include <ESP8266WiFi.h>

bool FSManager::gc() {
    if (!initialized) return false;

    ESP.wdtFeed();
    bool result = LittleFS.gc();
    ESP.wdtFeed();
    if (result) {
        gcCount++;
        lastGCTime = millis();
        LittleFS.info(fsInfo);
    }
    return result;
}

bool FSManager::healthCheck() {
    if (!initialized) return false;

    lastHealthCheck = millis();

    if (!exists("/config.json")) {
        errorCount++;
        return false;
    }

    if (exists("/config.bak")) {
        uint16_t crc = 0;
        uint16_t bakCRC = 0;
        {
            File f = openRead("/config.json");
            if (f) {
                crc = crc16ModbusStreamFile(f);
                f.close();
            }
        }
        {
            File f = openRead("/config.bak");
            if (f) {
                bakCRC = crc16ModbusStreamFile(f);
                f.close();
            }
        }
        if (crc != bakCRC) {
            if (copyFileAtomic_("/config.bak", "/config.json")) {
                recoveryCount++;
            }
            errorCount++;
            return false;
        }
    }

    return true;
}

size_t FSManager::getFreeSpace() {
    if (!initialized) return 0;
    LittleFS.info(fsInfo);
    return fsInfo.totalBytes - fsInfo.usedBytes;
}

size_t FSManager::getUsedSpace() {
    if (!initialized) return 0;
    LittleFS.info(fsInfo);
    return fsInfo.usedBytes;
}

void FSManager::printStats() {
#ifdef SERIAL_DEBUG
    LittleFS.info(fsInfo);
    logger.log("[FSManager] === stats ===\n");
    logger.log("[FSManager] Total: %u KB\n", fsInfo.totalBytes / 1024);
    logger.log("[FSManager] Used: %u KB\n", fsInfo.usedBytes / 1024);
    logger.log("[FSManager] Free: %u KB\n", (fsInfo.totalBytes - fsInfo.usedBytes) / 1024);
    logger.log("[FSManager] Reads: %u\n", readCount);
    logger.log("[FSManager] Writes: %u\n", writeCount);
    logger.log("[FSManager] GC runs: %u\n", gcCount);
    logger.log("[FSManager] Errors: %u\n", errorCount);
    logger.log("[FSManager] Recoveries: %u\n", recoveryCount);
    logger.log("[FSManager] === end stats ===\n");
#endif
}

