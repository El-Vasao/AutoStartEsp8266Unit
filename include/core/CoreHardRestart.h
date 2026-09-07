#pragma once

#include <Arduino.h>

/// Минимальный CPU reset: только WDT + `ESP.restart()` (без yield/SSE/глубокого стека).
[[noreturn]] void core_hard_restart_now();
