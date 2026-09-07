#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * RTOS portability layer used by migration runtime.
 * Keeps watchdog/time/reboot calls in one place.
 */
uint32_t platform_now_ms(void);
void platform_delay_ms(uint32_t ms);
void platform_feed_watchdog(void);
void platform_request_reboot(void);
void platform_log(const char* tag, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

