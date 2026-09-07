#include "core/CoreHardRestart.h"

#include <ESP8266WiFi.h>

[[noreturn]] __attribute__((noinline)) void core_hard_restart_now() {
    ESP.wdtFeed();
#ifdef SERIAL_DEBUG
    Serial.flush();
#endif
    // После тяжёлого FS/JSON стека даём SDK один проход перед system_restart().
    yield();
    ESP.wdtFeed();
    ESP.restart();
    while (true) {
        ESP.wdtFeed();
        delayMicroseconds(5000);
    }
}
