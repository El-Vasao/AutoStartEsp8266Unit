#pragma once

#include <stdint.h>

/**
 * Embedded-friendly dependency injection without virtuals/heap.
 *
 * Pattern:
 * - Modules store an `AppPorts` (or a subset) as plain data.
 * - `core` (composition root) wires function pointers to real methods.
 * - No vtable, no RTTI, no std::function.
 */

struct WdtPort {
    void* ctx{nullptr};
    void (*feed)(void* ctx){nullptr};
    void (*cooperate)(void* ctx){nullptr};  // yield/time-slicing

    inline void feedNow() const {
        if (feed) feed(ctx);
    }
    inline void cooperateNow() const {
        if (cooperate) cooperate(ctx);
    }
};

struct AppControlPort {
    void* ctx{nullptr};
    void (*requestReboot)(void* ctx, uint32_t delayMs){nullptr};
    void (*startProgram)(void* ctx, uint8_t programId){nullptr};
    void (*startOtaUpdate)(void* ctx){nullptr};
    void (*factoryReset)(void* ctx){nullptr};
};

struct AppPorts {
    WdtPort wdt;
    AppControlPort control;
};

