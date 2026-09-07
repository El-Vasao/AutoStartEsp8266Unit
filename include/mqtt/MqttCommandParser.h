#pragma once

#include "mqtt/MqttCommand.h"

/// Parse command JSON into a small POD command.
/// Returns true when a supported command was parsed.
bool parseMqttCommandJson(const char* json, MqttCommand& out);

