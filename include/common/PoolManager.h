// include/common/PoolManager.h
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/Constants.h"

/**
 * Статические byte-слоты (без malloc). Ранее JSON-пул убран в пользу потокового JSON.
 */
namespace PoolManager {

enum class BufferSlot : uint8_t {
    FsScratch = 0,
    ProgramExecSteps,
    ProgramExecActions,
    SlotCount
};

bool acquireBytes(BufferSlot slot, size_t size);
void releaseBytes(BufferSlot slot);
bool bytesHeld(BufferSlot slot);
uint8_t* bytes(BufferSlot slot);
size_t bytesSize(BufferSlot slot);

class ScopedBytes {
public:
    explicit ScopedBytes(BufferSlot slot, size_t size) : slot_(slot), ok_(acquireBytes(slot, size)) {}
    ~ScopedBytes() {
        if (ok_) releaseBytes(slot_);
    }
    explicit operator bool() const { return ok_; }
    uint8_t* data() { return bytes(slot_); }
    size_t size() const { return bytesSize(slot_); }
    BufferSlot slot() const { return slot_; }

private:
    BufferSlot slot_;
    bool ok_;
};

} // namespace PoolManager
