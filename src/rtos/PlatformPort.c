#include "rtos/PlatformPort.h"

#if defined(AUTOSTART_RTOS)

#include <stdarg.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t platform_now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

void platform_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void platform_feed_watchdog(void) { (void)esp_task_wdt_reset(); }

void platform_request_reboot(void) { esp_restart(); }

void platform_log(const char* tag, const char* fmt, ...) {
    if (!tag || !fmt) return;
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ESP_LOGI(tag, "%s", buf);
}

#else

uint32_t platform_now_ms(void) { return 0; }
void platform_delay_ms(uint32_t ms) { (void)ms; }
void platform_feed_watchdog(void) {}
void platform_request_reboot(void) {}
void platform_log(const char* tag, const char* fmt, ...) {
    (void)tag;
    (void)fmt;
}

#endif

