#pragma once

// Internal helpers for Config storage/program ops.
// Must not be included outside config subsystem.

namespace config_storage_internal {
void prepareFlashWriteLogGcAndWdt();
bool ensureProgramsDir();
} // namespace config_storage_internal

