#include "jc_handlers/flashmemory.hpp"

#include <cstdint>

extern "C" int fs_length(const uint8_t *, uint8_t, uint32_t *) { return 0; }

extern "C" uint8_t fs_read_1b_at(const uint8_t *, uint8_t, uint32_t,
                                 uint8_t *) {
  return 1;
}

extern "C" uint8_t fs_exists(const uint8_t *, uint8_t) { return 0; }

namespace jcvm {
uint32_t FlashMemory_Handler::persistentWriteRollbackGeneration() noexcept {
  return 0;
}

uint16_t FlashMemory_Handler::readPersistentArrayLength(
    const fs::Tag &, FieldType, uint16_t, uint16_t) {
  throw Exceptions::IOException;
}

void FlashMemory_Handler::setPersistentField_Array(
    const fs::Tag &, JC_Array &, Heap &) {
  throw Exceptions::IOException;
}

const uint8_t *FlashMemory_Handler::borrowPersistentArrayData(
    const fs::Tag &, FieldType, uint16_t, uint8_t) {
  throw Exceptions::IOException;
}

void FlashMemory_Handler::setPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, const jbyte_t, Heap &) {}
} // namespace jcvm
