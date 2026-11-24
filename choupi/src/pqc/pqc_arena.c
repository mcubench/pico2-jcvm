#include "pqc_arena.h"

#include <string.h>

static uint8_t *arena_base;
static size_t arena_length;
static size_t arena_used;
static size_t arena_peak;

void pqc_arena_begin(void *buffer, size_t length)
{
  arena_base = (uint8_t *)buffer;
  arena_length = length;
  arena_used = 0;
  arena_peak = 0;
}

size_t pqc_arena_end(void)
{
  size_t peak = arena_peak;
  arena_base = 0;
  arena_length = arena_used = 0;
  return peak;
}

void *choupi_pqc_alloc(size_t length)
{
  /* Lengths are already 32-byte aligned by the library macros. */
  if (arena_base == 0 || length > arena_length - arena_used) return 0;
  void *p = arena_base + arena_used;
  arena_used += length;
  if (arena_used > arena_peak) arena_peak = arena_used;
  return p;
}

void choupi_pqc_free(void *pointer, size_t length)
{
  if (pointer == 0) return;
  /* The libraries free in reverse allocation order (scoped buffers). */
  if ((uint8_t *)pointer + length == arena_base + arena_used)
    arena_used -= length;
}

#define PQC_STACK_PAINT 0xA5u

#if defined(__arm__) && !defined(PC_VERSION)
/* r0 = new stack top, r1 = fn, r2 = arg. Saves the caller's sp in r4. */
__attribute__((naked, noinline)) static int pqc_switch_stack(void *top,
                                                              pqc_stack_fn fn,
                                                              void *arg)
{
  __asm volatile(
      "push {r4, lr}\n"
      "mov r4, sp\n"
      "mov sp, r0\n"
      "mov r0, r2\n"
      "blx r1\n"
      "mov sp, r4\n"
      "pop {r4, pc}\n");
}
#endif

int pqc_run_on_stack(void *stack, size_t len, pqc_stack_fn fn, void *arg)
{
  memset(stack, PQC_STACK_PAINT, len);
#if defined(__arm__) && !defined(PC_VERSION)
  /* AAPCS requires an 8-byte aligned stack at public interfaces. */
  uintptr_t top = ((uintptr_t)stack + len) & ~(uintptr_t)7u;
  return pqc_switch_stack((void *)top, fn, arg);
#else
  /* Host builds have a large native stack; keep the call direct so ASan sees
   * ordinary frames. */
  return fn(arg);
#endif
}

size_t pqc_stack_used(const void *stack, size_t len)
{
  const uint8_t *p = (const uint8_t *)stack;
  size_t untouched = 0;
  while (untouched < len && p[untouched] == PQC_STACK_PAINT) ++untouched;
  return len - untouched;
}
