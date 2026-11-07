#include "jc_handlers/flashmemory.hpp"

#include <cstdint>

extern "C" int fs_length(const uint8_t *, uint8_t, uint32_t *) { return 0; }

extern "C" uint8_t fs_read_1b_at(const uint8_t *, uint8_t, uint32_t,
                                 uint8_t *) {
  return 1;
}

extern "C" uint8_t fs_exists(const uint8_t *, uint8_t) { return 0; }

namespace jcvm {
const jbyte_t FlashMemory_Handler::getPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, const jbyte_t, Heap &) {}
} // namespace jcvm
