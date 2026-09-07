// src/ota/OTAHandler.Files.cpp
#include "ota/OTAHandler.h"

#include "core/Core.h"
#include "common/Logger.h"
#include "common/Constants.h"
#include "fs/FSManager.h"

bool OTAHandler::extractFiles(File& f) {
    uint8_t buf[OTA::BUFFER_SIZE] __attribute__((aligned(4)));
    while (f.available() > 0) {
        uint8_t lenBuf[2];
        if (f.read(lenBuf, 2) != 2) {
            logger.log("[OTAHandler] Error reading filename length\n");
            return false;
        }
        uint16_t nameLen = lenBuf[0] | (lenBuf[1] << 8);
        if (nameLen == 0) break;

        char nameBuf[OTA::BUFFER_SIZE] __attribute__((aligned(4)));
        if (nameLen >= sizeof(nameBuf)) {
            logger.log("[OTAHandler] Filename too long, skipping\n");
            f.seek(nameLen, SeekCur);
            continue;
        }
        if (f.read((uint8_t*)nameBuf, nameLen) != nameLen) {
            logger.log("[OTAHandler] Error reading filename\n");
            return false;
        }
        nameBuf[nameLen] = '\0';

        uint8_t sizeBuf[4];
        if (f.read(sizeBuf, 4) != 4) {
            logger.log("[OTAHandler] Error reading file size\n");
            return false;
        }
        uint32_t fileSize = sizeBuf[0] | (sizeBuf[1] << 8) | (sizeBuf[2] << 16) | (sizeBuf[3] << 24);

        logger.log("[OTAHandler] Extracting %s (%u bytes)\n", nameBuf, fileSize);

        File outFile = fileSystem.openWriteStream(nameBuf, fileSize);
        if (!outFile) {
            logger.log("[OTAHandler] Failed to open stream for %s, skipping\n", nameBuf);
            f.seek(fileSize, SeekCur);
            continue;
        }

        uint32_t fileRemaining = fileSize;
        bool writeError = false;
        while (fileRemaining > 0) {
            size_t toRead = min(fileRemaining, (uint32_t)sizeof(buf));
            size_t read = f.read(buf, toRead);
            if (read != toRead) {
                logger.log("[OTAHandler] Error reading file data\n");
                writeError = true;
                break;
            }
            if (outFile.write(buf, read) != read) {
                logger.log("[OTAHandler] Error writing file data\n");
                writeError = true;
                break;
            }
            fileRemaining -= read;
            core.feedWatchdog();
            yield();
        }

        if (writeError) {
            fileSystem.closeWriteStream(outFile, nameBuf, false);
            return false;
        } else {
            if (!fileSystem.closeWriteStream(outFile, nameBuf, true)) {
                logger.log("[OTAHandler] Failed to finalize %s\n", nameBuf);
                return false;
            }
        }
    }
    return true;
}

