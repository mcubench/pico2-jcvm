#include "../../src/vm_cap_initialization.h"

#include "heap.hpp"
#include "jc_cap/jc_cap_static_field.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_static.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"
#include "ffi.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace jcvm {
namespace {
uint16_t read_u16_be(const uint8_t *data) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                data[1]);
}
} // namespace

bool initializeCapStaticArrays(uint8_t package_id) {
  const jcvm::JC_Cap cap = jcvm::Package(package_id).getCap();
  const auto *component = cap.getStaticField();
  if (component == nullptr) return true;

  const auto *component_bytes = reinterpret_cast<const uint8_t *>(component);
  const size_t component_size = read_u16_be(component_bytes + 1);
  // The StaticField payload starts with image_size, reference_count, and
  // array_init_count. Each array initializer then has a 3-byte header.
  if (component_size < 6) return false;
  const uint16_t array_count = read_u16_be(component_bytes + 7);
  size_t remaining = component_size - 6;
  const uint8_t *cursor = component_bytes + 9;
  const uint16_t image_size = read_u16_be(component_bytes + 3);
  jcvm::Static_Handler statics{jcvm::Package(package_id)};

  for (uint16_t index = 0; index < array_count; ++index) {
    if (remaining < 3) return false;
    const uint8_t cap_type = cursor[0];
    const uint16_t byte_count = read_u16_be(cursor + 1);
    cursor += 3;
    remaining -= 3;
    if (byte_count > remaining) return false;

    jcvm::jc_array_type array_type;
    uint16_t entry_size = 0;
    switch (cap_type) {
    case 2:
      array_type = jcvm::JAVA_ARRAY_T_BOOLEAN;
      entry_size = 1;
      break;
    case 3:
      array_type = jcvm::JAVA_ARRAY_T_BYTE;
      entry_size = 1;
      break;
    case 4:
      array_type = jcvm::JAVA_ARRAY_T_SHORT;
      entry_size = 2;
      break;
#ifdef JCVM_INT_SUPPORTED
    case 5:
      array_type = jcvm::JAVA_ARRAY_T_INT;
      entry_size = 4;
      break;
#endif
    default:
      return false;
    }
    if (byte_count % entry_size != 0) return false;

    // CAP StaticField array initializers correspond to the 16-bit static
    // image slot at the same initializer index. The persistent tag stores
    // the complete 16-bit byte offset, preserving the legacy 8-bit form when
    // the offset is representable there.
    const size_t static_offset = static_cast<size_t>(index) * 2;
    if (static_offset + sizeof(jcvm::jref_t) > image_size ||
        static_offset > UINT16_MAX) {
      return false;
    }
    const auto tag = jcvm::FlashMemory_Handler::getStaticFieldTag(
        package_id, static_cast<uint16_t>(static_offset));
    if (fs_exists(tag.value, tag.len) != 0 &&
        jcvm::FlashMemory_Handler::getPersistentField_Type(tag) !=
            jcvm::FieldType::FIELD_TYPE_UNINITIALIZED) {
      cursor += byte_count;
      remaining -= byte_count;
      continue;
    }

    const uint16_t entry_count = byte_count / entry_size;
    jcvm::Heap heap(0);
    const jcvm::jref_t reference = heap.addArray(entry_count, array_type);
    const auto array = heap.getArray(reference);
    if (array == nullptr) return false;
    for (uint16_t entry = 0; entry < entry_count; ++entry) {
      const uint8_t *value = cursor + entry * entry_size;
      if (entry_size == 1) {
        array->setByteEntry(entry, static_cast<jcvm::jbyte_t>(value[0]));
      } else if (entry_size == 2) {
        array->setShortEntry(entry,
                             static_cast<jcvm::jshort_t>(read_u16_be(value)));
#ifdef JCVM_INT_SUPPORTED
      } else {
        const uint32_t int_value =
            (static_cast<uint32_t>(value[0]) << 24) |
            (static_cast<uint32_t>(value[1]) << 16) |
            (static_cast<uint32_t>(value[2]) << 8) | value[3];
        array->setIntEntry(entry, static_cast<jcvm::jint_t>(int_value));
#endif
      }
    }
    statics.setPersistentRef(static_cast<uint16_t>(static_offset), reference,
                             heap);
    cursor += byte_count;
    remaining -= byte_count;
  }

  // The two trailing counts are followed only by the non-default bytes.
  if (remaining < 4) return false;
  const uint16_t default_value_count = read_u16_be(cursor);
  const uint16_t non_default_value_count = read_u16_be(cursor + 2);
  (void)default_value_count;
  return non_default_value_count == remaining - 4;
}

} // namespace jcvm
