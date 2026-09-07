/**
 * @file GSMController.Fsm.Registration.cpp
 */
#include "gsm/GSMController.h"

#include "core/Core.h"
#include "core/ErrorManager.h"
#include "common/Logger.h"

void GSMController::handleRegistering() {
    const uint32_t now = millis();

    // Если URC уже принёс регистрацию — не тормозим.
    if (_cregStat == 1 || _cregStat == 5) {
        clearResponse();
        _retryCount = 0;
        if (core.getErrorManager().get() == ErrorCode::GSM_REG_FAIL) {
            core.getErrorManager().clear();
        }
        changeState(GSMState::GPRS_SETUP);
        return;
    }

    if (_lastCommandTime == 0) {
        sendAt("AT+CREG?", nullptr, AwaitKind::CREG, GSM::REG_TIMEOUT_MS);
        return;
    }

    if (_awaitError) {
        _retryCount++;
        resetAwait();
        clearResponse();
        if (_retryCount >= GSM::MAX_RETRIES) {
            changeState(GSMState::ERROR);
            core.getErrorManager().set(ErrorCode::GSM_REG_FAIL);
            logRxSnippet("CREG error");
            logger.log("[GSMController] CREG returned ERROR\n");
            return;
        }
        sendAt("AT+CREG?", nullptr, AwaitKind::CREG, GSM::REG_TIMEOUT_MS);
        return;
    }

    // Для AT+CREG? считаем успехом: увидели +CREG: и итоговый OK, и stat = 1/5.
    if (_awaitOk && _awaitGotCreg && (_cregStat == 1 || _cregStat == 5)) {
        logRxSnippet("CREG");
        clearResponse();
        _retryCount = 0;
        if (core.getErrorManager().get() == ErrorCode::GSM_REG_FAIL) {
            core.getErrorManager().clear();
        }
        changeState(GSMState::GPRS_SETUP);
        return;
    }

    if (awaitTimedOut(now)) {
        _retryCount++;
        resetAwait();
        if (_retryCount >= GSM::MAX_RETRIES) {
            changeState(GSMState::ERROR);
            core.getErrorManager().set(ErrorCode::GSM_REG_FAIL);
            logRxSnippet("CREG timeout");
            logger.log("[GSMController] Registration failed\n");
            return;
        }
        sendAt("AT+CREG?", nullptr, AwaitKind::CREG, GSM::REG_TIMEOUT_MS);
    }
}

