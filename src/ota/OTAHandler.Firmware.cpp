// src/ota/OTAHandler.Firmware.cpp
#include "ota/OTAHandler.h"

#include "core/Core.h"
#include "common/Logger.h"
#include "common/Constants.h"
#include "common/EspHal.h"
#include "fs/FSManager.h"

#include <Update.h>

bool OTAHandler::processUpdateFile() {
    File f = fileSystem.openRead("/update.bin");
    if (!f) {
        logger.log("[OTAHandler] Cannot open update.bin\n");
        return false;
    }

    union {
        uint8_t b[4];
        uint32_t val;
    } header;

    if (f.read(header.b, OTA::HEADER_SIZE) != OTA::HEADER_SIZE) {
        logger.log("[OTAHandler] Failed to read header\n");
        f.close();
        return false;
    }
    uint32_t fwSize = header.val;
    logger.log("[OTAHandler] Firmware size: %u bytes\n", fwSize);

    yield();
    logger.log("[OTAHandler] heap before Update.begin: free=%u maxBlk=%u frag=%u%%\n",
               (unsigned)espHalFreeHeap(), (unsigned)espHalMaxBlock(), (unsigned)0 /* heap frag N/A on ESP32 */);

    if (!Update.begin(fwSize)) {
        logger.log("[OTAHandler] Update.begin failed\n");
        core.logHeapSnapshot("ota_update_begin_failed");
        f.close();
        return false;
    }
    core.logHeapSnapshot("ota_update_begin_ok");

    uint8_t buf[OTA::BUFFER_SIZE] __attribute__((aligned(4)));
    uint32_t remaining = fwSize;
    while (remaining > 0) {
        size_t toRead = min(remaining, (uint32_t)sizeof(buf));
        size_t read = f.read(buf, toRead);
        if (read != toRead) {
            logger.log("[OTAHandler] Failed to read firmware data\n");
            core.logHeapSnapshot("ota_fw_read_failed");
            f.close();
            return false;
        }
        if (Update.write(buf, read) != read) {
            logger.log("[OTAHandler] Update.write failed\n");
            core.logHeapSnapshot("ota_update_write_failed");
            f.close();
            return false;
        }
        remaining -= read;
        core.feedWatchdog();
        yield();
    }

    if (!Update.end()) {
        logger.log("[OTAHandler] Update.end failed\n");
        core.logHeapSnapshot("ota_update_end_failed");
        f.close();
        return false;
    }
    core.logHeapSnapshot("ota_update_end_ok");

    if (!extractFiles(f)) {
        core.logHeapSnapshot("ota_extract_failed");
        f.close();
        return false;
    }
    core.logHeapSnapshot("ota_extract_ok");

    f.close();
    fileSystem.deleteFile("/update.bin");
    return true;
}

