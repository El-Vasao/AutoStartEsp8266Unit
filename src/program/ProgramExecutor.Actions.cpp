// src/program/ProgramExecutor.Actions.cpp
#include "program/ProgramExecutor.h"

ActionId ProgramExecutor::compileAction(const char* action) {
    if (!action || !*action) return ActionId::UNKNOWN;

    if (strcmp(action, "WAIT") == 0) return ActionId::WAIT;
    if (strcmp(action, "RELAY_PULSE_ON_OFF_ON") == 0) return ActionId::RELAY_PULSE_ON_OFF_ON;
    if (strcmp(action, "RELAY_PULSE_OFF_ON_OFF") == 0) return ActionId::RELAY_PULSE_OFF_ON_OFF;
    if (strcmp(action, "STARTER_TIMED") == 0) return ActionId::STARTER_TIMED;
    if (strcmp(action, "STARTER_WAIT_INPUT") == 0) return ActionId::STARTER_WAIT_INPUT;

    if (strcmp(action, "RELAY_ON") == 0) return ActionId::RELAY_ON;
    if (strcmp(action, "RELAY_OFF") == 0) return ActionId::RELAY_OFF;
    if (strcmp(action, "RELAY_TOGGLE") == 0) return ActionId::RELAY_TOGGLE;

    if (strcmp(action, "INPUT_ENABLE") == 0) return ActionId::INPUT_ENABLE;
    if (strcmp(action, "INPUT_DISABLE") == 0) return ActionId::INPUT_DISABLE;
    if (strcmp(action, "INPUT_TRIGGER_ENABLE") == 0) return ActionId::INPUT_TRIGGER_ENABLE;
    if (strcmp(action, "INPUT_TRIGGER_DISABLE") == 0) return ActionId::INPUT_TRIGGER_DISABLE;
    if (strcmp(action, "TEMP_TRIGGER_ENABLE") == 0) return ActionId::TEMP_TRIGGER_ENABLE;
    if (strcmp(action, "TEMP_TRIGGER_DISABLE") == 0) return ActionId::TEMP_TRIGGER_DISABLE;

    if (strcmp(action, "BATTERY_SAVER_ON") == 0) return ActionId::BATTERY_SAVER_ON;
    if (strcmp(action, "BATTERY_SAVER_OFF") == 0) return ActionId::BATTERY_SAVER_OFF;
    if (strcmp(action, "THERMOSTAT_ON") == 0) return ActionId::THERMOSTAT_ON;
    if (strcmp(action, "THERMOSTAT_OFF") == 0) return ActionId::THERMOSTAT_OFF;

    return ActionId::UNKNOWN;
}

