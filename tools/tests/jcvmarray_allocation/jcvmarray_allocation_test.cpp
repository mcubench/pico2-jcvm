#include "../../../choupi/src/jc_types/jc_array.hpp"
#include "../../../choupi/src/jcvm_types/jcvmarray.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdint.h>
#include <stdexcept>
#include <type_traits>

static_assert(
    !std::is_nothrow_constructible_v<
        jcvm::JC_Array, jcvm::Heap &, jcvm::jc_array_type,
        jcvm::jc_cp_offset_t, const jcvm::fs::Tag &, bool, jcvm::ClearEvent,
        uint16_t>,
    "persistent array allocation must be allowed to propagate failure");

namespace {
bool fail_next_array_allocation = false;
std::size_t array_deallocations = 0;

struct ThrowOnCopy {
  static bool fail_assignment;
  uint8_t value = 0;

  ThrowOnCopy &operator=(const ThrowOnCopy &other) {
    if (fail_assignment) {
      throw std::runtime_error("injected element copy failure");
    }
    value = other.value;
    return *this;
  }
};

bool ThrowOnCopy::fail_assignment = false;
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

void operator delete[](void *memory) noexcept {
  ++array_deallocations;
  std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
  ++array_deallocations;
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

  jcvm::JCVMArray<uint8_t> source(3);
  source[0] = 0x11;
  source[1] = 0x22;
  source[2] = 0x33;
  fail_next_array_allocation = true;
  bool copy_construction_failed = false;
  try {
    jcvm::JCVMArray<uint8_t> copy(source);
    if (copy.size() != source.size() || copy[0] != source[0]) {
      std::puts("TEST:FAIL copy_construction_contents");
      return EXIT_FAILURE;
    }
  } catch (const std::bad_alloc &) {
    copy_construction_failed = true;
  }
  if (!copy_construction_failed) {
    std::puts("TEST:FAIL copy_construction_failure_propagates");
    return EXIT_FAILURE;
  }
  std::puts("TEST:PASS copy_construction_failure_propagates");

  jcvm::JCVMArray<ThrowOnCopy> throwing_source(2);
  throwing_source[0].value = 0x66;
  throwing_source[1].value = 0x77;
  const std::size_t deallocations_before_copy = array_deallocations;
  ThrowOnCopy::fail_assignment = true;
  bool element_copy_failed = false;
  try {
    jcvm::JCVMArray<ThrowOnCopy> copy(throwing_source);
    if (copy.size() != throwing_source.size()) {
      std::puts("TEST:FAIL throwing_copy_contents");
      return EXIT_FAILURE;
    }
  } catch (const std::runtime_error &) {
    element_copy_failed = true;
  }
  ThrowOnCopy::fail_assignment = false;
  if (!element_copy_failed ||
      array_deallocations != deallocations_before_copy + 1) {
    std::puts("TEST:FAIL copy_constructor_releases_partial_buffer");
    return EXIT_FAILURE;
  }
  std::puts("TEST:PASS copy_constructor_releases_partial_buffer");

  jcvm::JCVMArray<ThrowOnCopy> throwing_destination(1);
  throwing_destination[0].value = 0x88;
  const std::size_t deallocations_before_assignment = array_deallocations;
  ThrowOnCopy::fail_assignment = true;
  bool element_assignment_failed = false;
  try {
    throwing_destination = throwing_source;
  } catch (const std::runtime_error &) {
    element_assignment_failed = true;
  }
  ThrowOnCopy::fail_assignment = false;
  if (!element_assignment_failed || throwing_destination.size() != 1 ||
      throwing_destination[0].value != 0x88 ||
      array_deallocations != deallocations_before_assignment + 1) {
    std::puts("TEST:FAIL copy_assignment_releases_temporary_and_preserves_destination");
    return EXIT_FAILURE;
  }
  std::puts("TEST:PASS copy_assignment_releases_temporary_and_preserves_destination");

  jcvm::JCVMArray<uint8_t> destination(2);
  destination[0] = 0x44;
  destination[1] = 0x55;
  fail_next_array_allocation = true;
  bool assignment_failed = false;
  try {
    destination = source;
  } catch (const std::bad_alloc &) {
    assignment_failed = true;
  }
  if (!assignment_failed || destination.size() != 2 ||
      destination[0] != 0x44 || destination[1] != 0x55) {
    std::puts("TEST:FAIL copy_assignment_preserves_destination");
    return EXIT_FAILURE;
  }
  std::puts("TEST:PASS copy_assignment_preserves_destination");

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
