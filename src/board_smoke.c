#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pico/rand.h"
#include "pico/stdlib.h"

static bool random_stream_varies(void) {
  uint32_t first = get_rand_32();
  bool differs = false;

  for (unsigned int i = 0; i < 64; ++i) {
    differs |= get_rand_32() != first;
  }
  return differs;
}

int main(void) {
  stdio_init_all();
  sleep_ms(1500);

  printf("BOOT board=pico2 chip=rp2350 arch=%s flash=2097152\r\n",
#if defined(__riscv)
         "riscv"
#else
         "arm"
#endif
  );

  while (true) {
    if (random_stream_varies()) {
      puts("TEST:PASS random_stream_varies");
      puts("TEST:SUMMARY pass=1 fail=0");
    } else {
      puts("TEST:FAIL random_stream_varies");
      puts("TEST:SUMMARY pass=0 fail=1");
    }
    sleep_ms(1000);
  }
}
