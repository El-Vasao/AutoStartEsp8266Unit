#pragma once

#include <Arduino.h>

#include "app/StatusSnapshot.h"
#include "config/ConfigTypes.h"

/// Canonical MQTT JSON status (`buildMqttStatusJson` parity) streamed to `Print`.
void emitMqttStatusJson(const StatusSnapshot& s, const BaseConfig& cfg, Print& p);
