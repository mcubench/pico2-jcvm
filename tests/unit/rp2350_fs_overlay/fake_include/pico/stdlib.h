#ifndef TEST_PICO_STDLIB_H
#define TEST_PICO_STDLIB_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t absolute_time_t;

void stdio_init_all(void);
bool stdio_usb_connected(void);
void sleep_ms(uint32_t milliseconds);
void tight_loop_contents(void);
absolute_time_t make_timeout_time_ms(uint32_t milliseconds);
bool time_reached(absolute_time_t time);
absolute_time_t delayed_by_ms(absolute_time_t time, uint32_t milliseconds);

#endif
