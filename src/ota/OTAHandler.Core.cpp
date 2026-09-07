// src/ota/OTAHandler.Core.cpp
#include "ota/OTAHandler.h"

#include "core/Core.h"
#include "common/Logger.h"
#include "common/Constants.h"
#include "fs/FSManager.h"

OTAHandler::OTAHandler() : _ongoing(false), _lastLogTime(0) {}

void OTAHandler::prepareHttpUploadSession() {
    core.logHeapSnapshot("ota_wait_upload");
    _awaitHttpUploadSession = true;
    _httpUploadDone = false;
    _httpUploadOk = false;
    _httpSessionStartMs = millis();
}

void OTAHandler::prepareImmediateFirmwareProcess() {
    _awaitHttpUploadSession = false;
    _httpUploadDone = true;
    _httpUploadOk = true;
}

void OTAHandler::notifyHttpUploadComplete(bool ok) {
    _httpUploadDone = true;
    _httpUploadOk = ok;
    core.logHeapSnapshot(ok ? "ota_upload_complete_ok" : "ota_upload_complete_fail");
}

void OTAHandler::onModeExit() {
    _ongoing = false;
    _awaitHttpUploadSession = false;
    _httpUploadDone = false;
    _httpUploadOk = false;
    _lastLogTime = 0;
}

void OTAHandler::begin() {
    logger.log("[OTAHandler] OTA update started\n");
    core.logHeapSnapshot("ota_mode_begin");
    _ongoing = true;
    _lastLogTime = 0;
}

void OTAHandler::update() {
    if (!_ongoing) return;

    const uint32_t now = millis();

    if (_awaitHttpUploadSession && !_httpUploadDone) {
        if ((uint32_t)(now - _httpSessionStartMs) >= Timing::OTA_HTTP_UPLOAD_IDLE_MS) {
            logger.log("[OTAHandler] HTTP upload idle timeout (no final within %u ms), leaving OTA\n",
                       (unsigned)Timing::OTA_HTTP_UPLOAD_IDLE_MS);
            core.onOtaHttpUploadAwaitTimedOut();
            return;
        }
        if (now - _lastLogTime > Timing::OTA_WAIT_LOG_INTERVAL_MS) {
            logger.log("[OTAHandler] Receiving firmware upload...\n");
            _lastLogTime = now;
        }
        return;
    }

    if (_awaitHttpUploadSession && _httpUploadDone && !_httpUploadOk) {
        logger.log("[OTAHandler] HTTP upload failed, leaving OTA mode\n");
        core.logHeapSnapshot("ota_mode_upload_failed");
        if (fileSystem.exists("/update.bin")) {
            fileSystem.deleteFile("/update.bin");
        }
        core.exitOtaToNormalMode();
        return;
    }

    if (!fileSystem.exists("/update.bin")) {
        if (now - _lastLogTime > Timing::OTA_WAIT_LOG_INTERVAL_MS) {
            logger.log("[OTAHandler] Waiting for update.bin...\n");
            _lastLogTime = now;
        }
        return;
    }

    if (processUpdateFile()) {
        logger.log("[OTAHandler] Update successful, rebooting...\n");
        core.logHeapSnapshot("ota_update_success");
        delay(Delays::REBOOT_HTTP_REPLY_MS);
        core.reboot();
    } else {
        logger.log("[OTAHandler] Update failed, rebooting...\n");
        core.logHeapSnapshot("ota_update_failed");
        fileSystem.deleteFile("/update.bin");
        delay(Delays::REBOOT_HTTP_REPLY_MS);
        core.reboot();
    }
    _ongoing = false;
}
