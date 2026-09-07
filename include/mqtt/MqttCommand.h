#pragma once

#include <stdint.h>

enum class MqttCommandKind : uint8_t {
    None = 0,
    RunProgram,
    ListPrograms
};

struct MqttCommand {
    MqttCommandKind kind{MqttCommandKind::None};
    uint8_t programId{0}; // for RunProgram
};

