#include "common/PoolManager.h"
#include "program/CompiledStep.h"
#include <Arduino.h>

namespace {

constexpr unsigned kPoolSlotCount = static_cast<unsigned>(PoolManager::BufferSlot::SlotCount);

static uint8_t gBytesFsScratch[PoolLimits::FS_SCRATCH_BYTES]{};
static CompiledStep gProgExecSteps[Limits::MAX_STEPS_PER_PROGRAM]{};
static ActionId gProgExecActions[Limits::MAX_STEPS_PER_PROGRAM]{};

static uint8_t* gBytesSlot[kPoolSlotCount]{};
static size_t gBytesSize[kPoolSlotCount]{};
static uint8_t gBytesRef[kPoolSlotCount]{};

static int slotIndex(PoolManager::BufferSlot s) {
    return static_cast<int>(static_cast<unsigned>(s));
}

} // namespace

bool PoolManager::acquireBytes(BufferSlot slot, size_t size) {
    if (size == 0) return false;
    const int i = slotIndex(slot);

    if (slot == BufferSlot::FsScratch) {
        if (size != PoolLimits::FS_SCRATCH_BYTES) return false;
        if (gBytesRef[i] > 0) {
            gBytesRef[i]++;
            return true;
        }
        gBytesSlot[i] = gBytesFsScratch;
        gBytesSize[i] = PoolLimits::FS_SCRATCH_BYTES;
        gBytesRef[i] = 1;
        return true;
    }

    if (slot == BufferSlot::ProgramExecSteps) {
        if (size != sizeof(gProgExecSteps)) return false;
        if (gBytesRef[i] > 0) {
            gBytesRef[i]++;
            return true;
        }
        gBytesSlot[i] = reinterpret_cast<uint8_t*>(gProgExecSteps);
        gBytesSize[i] = sizeof(gProgExecSteps);
        gBytesRef[i] = 1;
        return true;
    }

    if (slot == BufferSlot::ProgramExecActions) {
        if (size != sizeof(gProgExecActions)) return false;
        if (gBytesRef[i] > 0) {
            gBytesRef[i]++;
            return true;
        }
        gBytesSlot[i] = reinterpret_cast<uint8_t*>(gProgExecActions);
        gBytesSize[i] = sizeof(gProgExecActions);
        gBytesRef[i] = 1;
        return true;
    }

    return false;
}

void PoolManager::releaseBytes(BufferSlot slot) {
    const int i = slotIndex(slot);
    if (gBytesRef[i] == 0) return;
    gBytesRef[i]--;
}

bool PoolManager::bytesHeld(BufferSlot slot) {
    return gBytesRef[slotIndex(slot)] > 0;
}

uint8_t* PoolManager::bytes(BufferSlot slot) {
    return gBytesSlot[slotIndex(slot)];
}

size_t PoolManager::bytesSize(BufferSlot slot) {
    return gBytesSize[slotIndex(slot)];
}
