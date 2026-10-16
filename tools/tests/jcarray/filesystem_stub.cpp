#include "jc_handlers/flashmemory.hpp"

#include <cstdint>

extern "C" int fs_length(const uint8_t *, uint8_t, uint32_t *) { return 0; }

extern "C" uint8_t fs_read_1b_at(const uint8_t *, uint8_t, uint32_t,
                                 uint8_t *) {
  return 1;
}

namespace jcvm {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
const jbyte_t FlashMemory_Handler::getPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, const jbyte_t, Heap &) {}

const jshort_t FlashMemory_Handler::getPersistentField_Array_Short(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Short(
    const fs::Tag &, const uint16_t, const jshort_t, Heap &) {}

const jint_t FlashMemory_Handler::getPersistentField_Array_Int(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Int(
    const fs::Tag &, const uint16_t, const jint_t, Heap &) {}

const jref_t FlashMemory_Handler::getPersistentField_Array_Reference(
    const fs::Tag &, const uint16_t, Heap &) {
  return jref_t{};
}
#pragma GCC diagnostic pop
} // namespace jcvm
