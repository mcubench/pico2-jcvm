/* Per-operation scratch arena for the PQC libraries (LIFO bump allocator). */
#ifndef CHOUPI_PQC_ARENA_H
#define CHOUPI_PQC_ARENA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void pqc_arena_begin(void *buffer, size_t length);
/* Returns the high-water mark in bytes and detaches the arena. */
size_t pqc_arena_end(void);
void *choupi_pqc_alloc(size_t length);
void choupi_pqc_free(void *pointer, size_t length);
/* Runs fn(arg) with the stack pointer moved to the top of [stack, stack+len).
 * The RP2350 main stack is 4 KiB while the libraries still keep up to ~5 KiB
 * of locals on the stack, so each PQC call gets a private heap-backed stack.
 * fn must not throw or longjmp. The region is painted first so
 * pqc_stack_used() can report its high-water mark afterwards. */
typedef int (*pqc_stack_fn)(void *arg);
int pqc_run_on_stack(void *stack, size_t len, pqc_stack_fn fn, void *arg);
size_t pqc_stack_used(const void *stack, size_t len);
/* Implemented by the platform: services watchdog and transport; 0 = abort. */
int choupi_pqc_progress(void);
#ifdef __cplusplus
}
#endif
#endif
