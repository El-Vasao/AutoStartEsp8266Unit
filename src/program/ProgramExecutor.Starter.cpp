// src/program/ProgramExecutor.Starter.cpp
#include "program/ProgramExecutor.h"

bool ProgramExecutor::isInStarterStep() const {
    if (!_running) return false;
    if (!_localActions) return false;
    if (_currentStep >= _localStepCount) return false;
    const ActionId id = _localActions[_currentStep];
    return (id == ActionId::STARTER_TIMED || id == ActionId::STARTER_WAIT_INPUT);
}

