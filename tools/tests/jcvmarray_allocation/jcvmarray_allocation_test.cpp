#include "../../../choupi/src/jc_types/jc_array.hpp"
#include "../../../choupi/src/jcvm_types/jcvmarray.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdint.h>
#include <type_traits>

static_assert(
    !std::is_nothrow_constructible_v<
        jcvm::JC_Array, jcvm::Heap &, jcvm::jc_array_type,
        jcvm::jc_cp_offset_t, const jcvm::fs::Tag &, bool, jcvm::ClearEvent,
        uint16_t>,
    "persistent array allocation must be allowed to propagate failure");

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
  } catch (...) {
    std::puts("TEST:FAIL allocation_failure_wrong_exception");
    return EXIT_FAILURE;
  }

  jcvm::Heap owner(1);
  jcvm::fs::Tag tag{};
  tag.len = 3;
  tag.value[0] = 0xa0;
  tag.value[1] = 0x01;
  tag.value[2] = 0x01;
  fail_next_array_allocation = true;
  try {
    jcvm::JC_Array array(owner, jcvm::JAVA_ARRAY_T_BYTE, 0, tag, false,
                         jcvm::ClearEvent::None, 0);
    (void)array;
    std::puts("TEST:FAIL persistent_array_failure_was_swallowed");
    return EXIT_FAILURE;
  } catch (const std::bad_alloc &) {
    std::puts("TEST:PASS persistent_array_failure_propagates");
  } catch (...) {
    std::puts("TEST:FAIL persistent_array_failure_wrong_exception");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
