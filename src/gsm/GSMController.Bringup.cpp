/**
 * @file GSMController.Bringup.cpp
 * @brief Bring-up: фиксированный `UART_BAUD`, без синхронного autobaud в `begin()`; NV UART через `AT+IPR?` в INIT.
 */
#include "gsm/GSMController.h"

#include "gsm/GsmInitPhase.h"
#include "core/Core.h"
#include "core/ErrorManager.h"
#include "common/Logger.h"

namespace {
static void waitCooperative(uint32_t ms) {
    const uint32_t start = millis();
    while (millis() - start < ms) {
        core.feedWatchdog();
        core.cooperate();
        delay(0);
    }
}
} // namespace

void GSMController::gsmResetSessionAfterStop() {
    _verifiedModemContactSinceStop = false;
    _firstAtFallbackStartMs = 0;
    _initPhase = GsmInitPhase::None;
    _baudSearchActive = false;
    _baudSearchRound = 0;
    _baudSearchBaudIdx = 0;
    _baudSearchSub = 0;
    _baudSearchDeadlineMs = 0;
    _baudCooldownUntilMs = 0;
    _didIprNvProbeThisCycle = false;
    _resumeSapbrHadIp = false;
    _postResumeTarget = GSMState::REGISTERING;
    _initCgattBusyRetries = 0;
}

void GSMController::begin() {
    waitCooperative(GSM::BOOT_DELAY_MS);
    const uint32_t now = millis();
    const bool fromErrorRecovery = (_state == GSMState::ERROR);

    if (_bringupStartMs == 0 || _state == GSMState::IDLE) {
        _bringupStartMs = now;
    }
    ev(1);

    if (!fromErrorRecovery) {
        gsmResetSessionAfterStop();
    }

    _serial->begin(GSM::UART_BAUD);
    _baud = GSM::UART_BAUD;
    waitCooperative(GSM::UART_SETTLE_MS);
    flushInput();

    if (!fromErrorRecovery) {
        _waitFirstStep = 0;
        _waitFirstUntilMs = now + 5000UL;
    } else {
        _waitFirstUntilMs = 0;
    }

    changeState(GSMState::INIT);
    logger.log("[GSMController] begin() UART_BAUD=%lu\n", (unsigned long)GSM::UART_BAUD);
}
