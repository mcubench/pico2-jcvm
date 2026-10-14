#ifndef TEST_HARDWARE_WATCHDOG_H
#define TEST_HARDWARE_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

void watchdog_update(void);
bool watchdog_enable_caused_reboot(void);
void watchdog_enable(uint32_t delay_ms, bool pause_on_debug);

#endif
