#include "../../../choupi/src/jcvm_types/jcvmarray.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdint.h>

namespace {
bool fail_next_array_allocation = false;
}

void *operator new[](const std::size_t size) {
  if (fail_next_array_allocation) {
    fail_next_array_allocation = false;
    throw std::bad_alloc();
  }
  if (void *memory = std::malloc(size)) {
    return memory;
  }
  throw std::bad_alloc();
}

void operator delete[](void *memory) noexcept { std::free(memory); }

void operator delete[](void *memory, std::size_t) noexcept {
  std::free(memory);
}

int main() {
  fail_next_array_allocation = true;
  try {
    jcvm::JCVMArray<uint8_t> array(16);
    (void)array;
    std::puts("TEST:FAIL allocation_failure_was_swallowed");
    return EXIT_FAILURE;
  } catch (const std::bad_alloc &) {
    std::puts("TEST:PASS allocation_failure_propagates");
    return EXIT_SUCCESS;
  } catch (...) {
    std::puts("TEST:FAIL allocation_failure_wrong_exception");
    return EXIT_FAILURE;
  }
}
