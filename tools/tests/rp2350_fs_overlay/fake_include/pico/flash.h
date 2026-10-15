#ifndef RP2350_FS_TEST_PICO_FLASH_H
#define RP2350_FS_TEST_PICO_FLASH_H

#include <stdint.h>

#define PICO_OK 0
int flash_safe_execute(void (*func)(void *), void *param,
                       uint32_t timeout_ms);

#endif
