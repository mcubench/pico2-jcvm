#ifndef TEST_HARDWARE_STRUCTS_WATCHDOG_H
#define TEST_HARDWARE_STRUCTS_WATCHDOG_H

#include <stdint.h>

typedef struct {
  uint32_t scratch[8];
} test_watchdog_hw_t;

extern test_watchdog_hw_t *watchdog_hw;

#endif
