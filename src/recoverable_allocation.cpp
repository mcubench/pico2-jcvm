// Firmware replacement for the global C++ operator new.
//
// The Pico SDK wraps malloc (pico_malloc) and, with PICO_MALLOC_PANIC (the
// default, kept), turns every failed allocation into panic("Out of memory").
// That stays the policy for all firmware code: a panic resets the card like a
// tear, and the persistent store is tear-safe.
//
// Code that is written to survive std::bad_alloc can opt in with
// jcvm::RecoverableAllocationScope (choupi/src/recoverable_allocation.hpp).
// Inside such a scope, operator new allocates from the unwrapped allocator and
// throws std::bad_alloc on failure instead of panicking. The object-deletion
// pass uses it: its scans fail closed (an allocation failure marks the trace
// incomplete or keeps the candidate), so running out of RAM while looking for
// garbage defers the request instead of resetting the card.
//
// C allocations (Mbed TLS, newlib) keep the wrapped, panicking malloc.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "pico/malloc.h"

#if PICO_USE_MALLOC_MUTEX
#error "recoverable allocation bypasses the pico_malloc mutex"
#endif

extern "C" void *__real_malloc(std::size_t size);
extern "C" char __StackLimit;

namespace {
uint32_t recoverable_allocation_depth = 0;
uint32_t recoverable_allocation_failures = 0;
} // namespace

extern "C" void choupi_recoverable_allocation_enter(void) {
  ++recoverable_allocation_depth;
}

extern "C" void choupi_recoverable_allocation_exit(void) {
  if (recoverable_allocation_depth != 0) --recoverable_allocation_depth;
}

extern "C" uint32_t choupi_recoverable_allocation_failures(void) {
  return recoverable_allocation_failures;
}

void *operator new(std::size_t size) {
  if (size == 0) size = 1;
  if (recoverable_allocation_depth == 0) {
    // Wrapped malloc: panics on failure, as for the rest of the firmware.
    return std::malloc(size);
  }
  void *memory = __real_malloc(size);
  if (memory != nullptr &&
      static_cast<char *>(memory) + size > &__StackLimit) {
    std::free(memory);
    memory = nullptr;
  }
  if (memory == nullptr) {
    ++recoverable_allocation_failures;
    throw std::bad_alloc();
  }
  return memory;
}

void *operator new[](std::size_t size) { return ::operator new(size); }
