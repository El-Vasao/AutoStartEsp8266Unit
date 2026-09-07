#include "core/FlashCommitCoordinator.h"

#include "core/Core.h"
#include "program/ProgramExecutor.h"
#include "config/Config.h"
#include "fs/FSManager.h"
#include "web/WebServer.h"
#include "common/Constants.h"
#include "common/Logger.h"

FlashCommitCoordinator flashCommit;

const char* FlashCommitCoordinator::getCurrentFlashOpString() const {
    if (deferredPostedConfigApply_) return "save_config";
    if (deferredPostedProgramApply_) return "save_program";
    switch (pendingFsOp_) {
        case PendingFsOp::DELETE_PROGRAM: return "delete_program";
        case PendingFsOp::RESET_CONFIG: return "reset_config";
        case PendingFsOp::RESET_PROGRAMS: return "reset_programs";
        default: return "none";
    }
}

void FlashCommitCoordinator::recordFlashCommitSnapshot(FlashCommitOp op, bool ok, bool changed, uint16_t crc) {
    lastFlashOp_ = op;
    lastFlashOk_ = ok;
    lastFlashMillis_ = millis();
    lastFlashChanged_ = changed;
    lastFlashCrc_ = crc;
}

void FlashCommitCoordinator::tick(Core& core) {
    core.feedWatchdog();
    ESP.wdtFeed();

    if (core.getProgramExecutor().isRunning()) {
        if (isPending()) {
            static uint32_t lastProgramBusyLog = 0;
            const uint32_t now = millis();
            if (now - lastProgramBusyLog >= 2000UL) {
                lastProgramBusyLog = now;
                logger.log("[FlashCommit] Deferred flash commit paused: program is running\n");
            }
        }
        return;
    }

    if (deferredPostedConfigApply_) {
        core.feedWatchdog();
        ESP.wdtFeed();
        logger.log("[FlashCommit] Config: deferred apply start %s (heap=%u)\n",
                   HttpPostJson::TMP_CONFIG, ESP.getFreeHeap());
        const uint16_t beforeCrc = config.getCRC();
        const bool cfgOk = config.applyPostedConfigJsonFile(HttpPostJson::TMP_CONFIG);
        core.feedWatchdog();
        ESP.wdtFeed();

        if (!cfgOk) {
            recordFlashCommitSnapshot(FlashCommitOp::SAVE_CONFIG, false, false, config.getCRC());
            logger.log("[FlashCommit] Config: apply failed\n");
            webServer.broadcastStatusForce();
        } else {
            const uint16_t afterCrc = config.getCRC();
            const bool changed = (beforeCrc != afterCrc);
            recordFlashCommitSnapshot(FlashCommitOp::SAVE_CONFIG, true, changed, afterCrc);
            core.setRebootRequired(true);
            logger.log("[FlashCommit] Config: /config.json written (changed=%d crc:%u->%u heap=%u)\n",
                       (int)changed,
                       (unsigned)beforeCrc,
                       (unsigned)afterCrc,
                       ESP.getFreeHeap());
            webServer.broadcastStatusForce();
        }
        deferredPostedConfigApply_ = false;
    }

    if (deferredPostedProgramApply_) {
        core.feedWatchdog();
        ESP.wdtFeed();

        if (!deferredPostedProgramIndexPhase_) {
            logger.log("[FlashCommit] program: deferred apply start %s (heap=%u)\n",
                       HttpPostJson::TMP_PROGRAM, ESP.getFreeHeap());
            uint8_t postedId = 0;
            const bool parsedOk = config.commitPostedProgramFile(HttpPostJson::TMP_PROGRAM, &postedId);
            if (!parsedOk) {
                logger.log("[FlashCommit] program: parse JSON from tmp failed\n");
                recordFlashCommitSnapshot(FlashCommitOp::SAVE_PROGRAM, false, false, config.getCRC());
                webServer.broadcastStatusForce();
                deferredPostedProgramApply_ = false;
            } else {
                logger.log("[FlashCommit] program: JSON OK id=%u, file written, scheduling rebuildProgramIndex\n",
                           (unsigned)postedId);
                deferredPostedProgramIndexPhase_ = true;
                webServer.broadcastStatusForce();
                core.feedWatchdog();
                ESP.wdtFeed();
                return; // next tick will rebuild index
            }
        } else {
            logger.log("[FlashCommit] program: deferred rebuildProgramIndex (heap=%u)\n", ESP.getFreeHeap());
            const bool ok = config.rebuildProgramIndex();
            recordFlashCommitSnapshot(FlashCommitOp::SAVE_PROGRAM, ok, ok, config.getCRC());
            logger.log("[FlashCommit] program: flash save %s (heap=%u)\n", ok ? "OK" : "FAILED", ESP.getFreeHeap());
            webServer.broadcastStatusForce();
            deferredPostedProgramApply_ = false;
            deferredPostedProgramIndexPhase_ = false;
        }
    }

    if (pendingFsOp_ == PendingFsOp::NONE) return;

    const PendingFsOp op = pendingFsOp_;
    pendingFsOp_ = PendingFsOp::NONE;

    logger.log("[FlashCommit] deferred FS op: %u start (heap=%u)\n", (unsigned)op, ESP.getFreeHeap());
    core.feedWatchdog();
    ESP.wdtFeed();
    bool ok = false;
    FlashCommitOp flashOp = FlashCommitOp::NONE;
    if (op == PendingFsOp::DELETE_PROGRAM) {
        logger.log("[FlashCommit] deferred delete_program: id=%u\n", (unsigned)pendingProgramId_);
        ok = config.deleteProgram(pendingProgramId_);
        flashOp = FlashCommitOp::DELETE_PROGRAM;
    } else if (op == PendingFsOp::RESET_CONFIG) {
        logger.log("[FlashCommit] deferred reset_config\n");
        ok = config.reset();
        flashOp = FlashCommitOp::RESET_CONFIG;
        if (ok) core.setRebootRequired(true);
    } else if (op == PendingFsOp::RESET_PROGRAMS) {
        logger.log("[FlashCommit] deferred reset_programs\n");
        ok = config.resetPrograms();
        flashOp = FlashCommitOp::RESET_PROGRAMS;
    }
    recordFlashCommitSnapshot(flashOp, ok, ok, config.getCRC());
    logger.log("[FlashCommit] deferred FS op: %u done ok=%d crc=%u heap=%u\n",
               (unsigned)op, (int)ok, (unsigned)lastFlashCrc_, ESP.getFreeHeap());
}

