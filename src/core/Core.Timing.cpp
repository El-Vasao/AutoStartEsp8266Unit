// src/core/Core.Timing.cpp
#include "core/Core.h"
#include "core/internal/CorePrivate.h"

void Core::updateUptime() {
    CorePrivate& impl = *_impl;
    uint32_t now = millis() / Time::MS_PER_SEC;
    if (now != impl.lastUptimeSecond) impl.lastUptimeSecond = now;
}

uint32_t Core::getUptime() const {
    const CorePrivate& impl = *_impl;
    return impl.lastUptimeSecond;
}

void Core::feedWatchdog() {
    CorePrivate& impl = *_impl;
    uint32_t now = millis();
    if (now - impl.lastWdtFeed >= Timing::WATCHDOG_FEED_INTERVAL_MS) {
        // Контракт:
        // - функция безопасна для частого вызова; фактически кормит WDT не чаще заданного интервала,
        //   чтобы не раздувать стоимость “длинных” операций (FS/OTA) лишними вызовами.
        // - используем `ESP.wdtFeed()` вместо `yield()` — yield может быть слишком частым в tight loops.
        ESP.wdtFeed();
        impl.lastWdtFeed = now;
    }
}

void Core::cooperate() {
    CorePrivate& impl = *_impl;
    const uint32_t now = millis();
    if (now - impl.lastCooperateMs < Timing::COOPERATE_INTERVAL_MS) return;
    impl.lastCooperateMs = now;
    // Контракт:
    // - отдаёт управление SDK (WiFi/lwIP/таймеры) через `yield()`, но с троттлингом.
    // - использовать в потенциально длинных циклах, где нет естественных `delay()/yield()`.
    yield();
}

