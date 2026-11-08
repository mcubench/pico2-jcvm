/*
** The MIT License (MIT)
**
** Copyright (c) 2020, National Cybersecurity Agency of France (ANSSI)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Author:
**   - Guillaume Bouffard <guillaume.bouffard@ssi.gouv.fr>
*/

#include "jc_static.hpp"
#include "../context.hpp"
#include "../exceptions.hpp"
#include "../jc_cap/jc_cap_static_field.hpp"
#include "../heap.hpp"

#include "flashmemory.hpp"
#include "ffi.h"

namespace jcvm {
namespace {

void validateStaticSlot(const Package &package, const uint16_t index,
                        const size_t width, const bool reference) {
  const auto *component = package.getCap().getStaticField();
  if (component == nullptr) throw Exceptions::SecurityException;

  const size_t image_size = NTOHS(component->image_size);
  const size_t reference_bytes =
      static_cast<size_t>(NTOHS(component->reference_count)) * 2;
  const size_t offset = index;
  if (reference) {
    if (width != 2 || offset % 2 != 0 || offset >= reference_bytes ||
        width > reference_bytes - offset) {
      throw Exceptions::SecurityException;
    }
  } else if (offset < reference_bytes || offset > image_size ||
             width > image_size - offset) {
    throw Exceptions::SecurityException;
  }
}

void validateStaticPrimitiveType(const fs::Tag &tag,
                                 const FieldType expected) {
  if (fs_exists(tag.value, tag.len) == 0) return;
  const FieldType actual = FlashMemory_Handler::getPersistentField_Type(tag);
  const bool compatible =
      actual == expected ||
      (expected == FieldType::FIELD_TYPE_BYTE &&
       actual == FieldType::FIELD_TYPE_BOOLEAN);
  if (!compatible) throw Exceptions::SecurityException;
}

uint16_t readDescriptorU16(const uint8_t *data) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               data[1]);
}

void validateDeclaredStaticType(const Package &package, const uint16_t index,
                                const FieldType expected) {
  if (!FlashMemory_Handler::isDynamicPackageID(package.getPackageID())) return;

  const auto *descriptor = package.getCap().getDescriptor();
  // Descriptor is optional in CAP LOAD. The method-boundary policy separately
  // rejects execution without it; retain compatibility for non-executing
  // inspection paths and make no declared-kind claim when it is absent.
  if (descriptor == nullptr) return;

  const auto *payload = reinterpret_cast<const uint8_t *>(&descriptor->class_count);
  const size_t payload_size = NTOHS(descriptor->size);
  if (payload_size == 0) throw Exceptions::SecurityException;

  size_t offset = 1;
  bool found = false;
  FieldType declared = FieldType::FIELD_TYPE_UNINITIALIZED;
  for (uint8_t class_index = 0; class_index < descriptor->class_count;
       ++class_index) {
    if (offset > payload_size || payload_size - offset < 9) {
      throw Exceptions::SecurityException;
    }
    const uint8_t *class_info = payload + offset;
    const size_t interfaces = class_info[4];
    const size_t fields = readDescriptorU16(class_info + 5);
    const size_t methods = readDescriptorU16(class_info + 7);
    const size_t field_offset = 9 + interfaces * 2;
    const size_t record_size = field_offset + fields * 7 + methods * 12;
    if (record_size > payload_size - offset) {
      throw Exceptions::SecurityException;
    }

    for (size_t field_index = 0; field_index < fields; ++field_index) {
      const uint8_t *field = class_info + field_offset + field_index * 7;
      constexpr uint8_t kStaticAccessFlag = 0x08;
      if ((field[1] & kStaticAccessFlag) == 0 ||
          (field[2] & 0x80) != 0) {
        continue;
      }

      // Internal static references use one padding byte followed by the
      // two-byte offset into the StaticField image. External references are
      // resolved in their owning package and are checked there.
      if (readDescriptorU16(field + 3) != index) continue;
      if (found) throw Exceptions::SecurityException;
      found = true;

      const uint16_t type = readDescriptorU16(field + 5);
      if ((type & 0x8000) == 0) {
        declared = FieldType::FIELD_TYPE_OBJECT;
      } else {
        switch (type & 0x7fff) {
        case 2:
          declared = FieldType::FIELD_TYPE_BOOLEAN;
          break;
        case 3:
          declared = FieldType::FIELD_TYPE_BYTE;
          break;
        case 4:
          declared = FieldType::FIELD_TYPE_SHORT;
          break;
        case 5:
          declared = FieldType::FIELD_TYPE_INT;
          break;
        default:
          throw Exceptions::SecurityException;
        }
      }
    }
    offset += record_size;
  }

  if (!found) throw Exceptions::SecurityException;
  const bool expected_byte = expected == FieldType::FIELD_TYPE_BYTE &&
      declared == FieldType::FIELD_TYPE_BOOLEAN;
  const bool expected_reference = expected == FieldType::FIELD_TYPE_OBJECT &&
      declared == FieldType::FIELD_TYPE_OBJECT;
  if (declared != expected && !expected_byte && !expected_reference) {
    throw Exceptions::SecurityException;
  }
}

uint16_t readStaticU16(const uint8_t *data) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               data[1]);
}

void getInitialPrimitiveBytes(const Package &package, const size_t index,
                              uint8_t *output,
                              const size_t output_size) noexcept {
  if (output == nullptr) return;
  for (size_t byte = 0; byte < output_size; ++byte) output[byte] = 0;

  const auto *component = package.getCap().getStaticField();
  if (component == nullptr) return;

  const auto *component_bytes = reinterpret_cast<const uint8_t *>(component);
  const uint16_t component_size = readStaticU16(component_bytes + 1);
  if (component_size < 10) return;

  const uint16_t image_size = readStaticU16(component_bytes + 3);
  const uint16_t reference_count = readStaticU16(component_bytes + 5);
  const uint16_t array_count = readStaticU16(component_bytes + 7);

  const uint8_t *cursor = component_bytes + 9;
  size_t remaining = component_size - 6;
  for (uint16_t array = 0; array < array_count; ++array) {
    if (remaining < 3) return;
    const uint16_t byte_count = readStaticU16(cursor + 1);
    if (static_cast<size_t>(byte_count) > remaining - 3) return;
    cursor += 3 + byte_count;
    remaining -= 3 + byte_count;
  }

  if (remaining < 4) return;
  const uint16_t default_value_count = readStaticU16(cursor);
  const uint16_t non_default_value_count = readStaticU16(cursor + 2);
  if (non_default_value_count != remaining - 4) return;

  const size_t primitive_base = static_cast<size_t>(reference_count) * 2;
  if (primitive_base + default_value_count + non_default_value_count !=
      image_size) {
    return;
  }
  for (size_t byte = 0; byte < output_size; ++byte) {
    const size_t image_offset = index + byte;
    if (image_offset >= image_size || image_offset < primitive_base) continue;
    const size_t primitive_offset = image_offset - primitive_base;
    if (primitive_offset < default_value_count) continue;
    const size_t non_default_offset = primitive_offset - default_value_count;
    if (non_default_offset < non_default_value_count) {
      output[byte] = cursor[4 + non_default_offset];
    }
  }
}

} // namespace

/**
 * Get static byte value.
 *
 * @param[index] static byte index in the current package.
 * @return the static byte value.
 */
jbyte_t Static_Handler::getPersistentByte(const uint16_t index) const {
  validateStaticSlot(this->package, index, 1, false);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_BYTE);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);
  if (fs_exists(tag.value, tag.len) == 0) {
    uint8_t value[1];
    getInitialPrimitiveBytes(this->package, index, value, sizeof(value));
    return static_cast<jbyte_t>(value[0]);
  }
  validateStaticPrimitiveType(tag, FieldType::FIELD_TYPE_BYTE);
  return FlashMemory_Handler::getPersistentField_Byte(tag);
}

/**
 * Get static short value.
 *
 * @param[index] static short index in the current package.
 * @return the static short value.
 */
jshort_t Static_Handler::getPersistentShort(const uint16_t index) const {
  validateStaticSlot(this->package, index, sizeof(jshort_t), false);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_SHORT);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);
  if (fs_exists(tag.value, tag.len) == 0) {
    uint8_t bytes[2];
    getInitialPrimitiveBytes(this->package, index, bytes, sizeof(bytes));
    const uint16_t value = readStaticU16(bytes);
    return static_cast<jshort_t>(value);
  }
  validateStaticPrimitiveType(tag, FieldType::FIELD_TYPE_SHORT);
  return FlashMemory_Handler::getPersistentField_Short(tag);
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Get static integer value.
 *
 * @param[index] static integer index in the current package.
 * @return the static integer value.
 */
jint_t Static_Handler::getPersistentInt(const uint16_t index) const {
  validateStaticSlot(this->package, index, sizeof(jint_t), false);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_INT);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);
  if (fs_exists(tag.value, tag.len) == 0) {
    uint8_t bytes[4];
    getInitialPrimitiveBytes(this->package, index, bytes, sizeof(bytes));
    const uint32_t value = (static_cast<uint32_t>(bytes[0]) << 24) |
                           (static_cast<uint32_t>(bytes[1]) << 16) |
                           (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
    return static_cast<jint_t>(value);
  }
  validateStaticPrimitiveType(tag, FieldType::FIELD_TYPE_INT);
  return FlashMemory_Handler::getPersistentField_Int(tag);
}
#endif /* JCVM_INT_SUPPORTED */

/*
 * Read static serialized instance or array.
 *
 * @param[index] static index in the current package.
 * @return static serialized instance or array.
 */
jref_t Static_Handler::getPersistentRef(const uint16_t index,
                                        Heap &heap) const {
  return this->getPersistentRefWithOwner(
      index, heap, this->package.getPackageID()).first;
}

std::pair<jref_t, uint8_t> Static_Handler::getPersistentRefWithOwner(
    const uint16_t index, Heap &heap,
    const jpackage_ID_t current_package) const {
  validateStaticSlot(this->package, index, 2, true);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_OBJECT);
  jref_t reference;
  uint8_t reference_owner = this->package.getPackageID();
  const jpackage_ID_t package_id = this->package.getPackageID();
  if (heap.getStaticReferenceRoot(package_id, index, reference,
                                  reference_owner)) {
    if (reference.isNullPointer()) return {reference, 0xff};
    if (reference.isGlobalArray() || reference.isGlobalInstance()) {
      return {reference, 0xfe};
    }
    return {reference, reference_owner == current_package
                           ? static_cast<uint8_t>(0xff)
                           : reference_owner};
  }

  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      package_id, index);

  reference = FlashMemory_Handler::getPersistentField_Reference(tag, heap);
  reference_owner = FlashMemory_Handler::getPersistentField_ReferenceOwner(
      tag, package_id);
  heap.setStaticReferenceRoot(package_id, index, reference, reference_owner);
  if (reference.isNullPointer()) return {reference, 0xff};
  if (reference.isGlobalArray() || reference.isGlobalInstance()) {
    return {reference, 0xfe};
  }
  return {reference, reference_owner == current_package
                         ? static_cast<uint8_t>(0xff)
                         : reference_owner};
}

/**
 * Set a static reference value.
 *
 * @param[index] static reference index in the current package.
 * @param[value] reference to persist.
 * @param[heap] the current applet heap.
 */
void Static_Handler::setPersistentRef(const uint16_t index,
                                      const jref_t value,
                                      const uint8_t reference_owner,
                                      Heap &heap) {
  validateStaticSlot(this->package, index, 2, true);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_OBJECT);
  if (value.isGlobalArray()) {
    throw Exceptions::SecurityException;
  }
  const jpackage_ID_t package_id = this->package.getPackageID();
  heap.reserveStaticReferenceRoot(package_id, index);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      package_id, index);
  if (value.isNullPointer()) {
    FlashMemory_Handler::setPersistentField_Reference(tag, value, heap);
    heap.setStaticReferenceRoot(package_id, index, value, package_id);
    return;
  }
  if (value.isGlobalInstance()) {
    FlashMemory_Handler::setPersistentField_Reference(tag, value, heap);
    heap.setStaticReferenceRoot(package_id, index, value, 0xfe);
    return;
  }
  if (reference_owner == 0xff || reference_owner == package_id) {
    FlashMemory_Handler::setPersistentField_Reference(tag, value, heap);
    heap.setStaticReferenceRoot(package_id, index, value, package_id);
    return;
  }
  if (reference_owner == 0xfe || value.isArray()) {
    throw Exceptions::SecurityException;
  }
  Context *owner_context =
      Context::findRetainedPackageContext(reference_owner);
  if (owner_context == nullptr) throw Exceptions::SecurityException;
  const auto instance = owner_context->getHeap().getInstance(value);
  Heap *expected_heap = Context::findRetainedAppletHeap(
      reference_owner, instance->getOwner().getOwner());
  if (expected_heap != &instance->getOwner()) {
    throw Exceptions::SecurityException;
  }
  FlashMemory_Handler::setPersistentForeignReference(
      tag, value, reference_owner, instance->getOwner());
  heap.setStaticReferenceRoot(package_id, index, value, reference_owner);
}

void Static_Handler::setPersistentRef(const uint16_t index,
                                      const jref_t value, Heap &heap) {
  const uint8_t owner =
      value.isGlobalArray() || value.isGlobalInstance()
          ? 0xfe
          : this->package.getPackageID();
  this->setPersistentRef(index, value, owner, heap);
}

/**
 * Set static byte value.
 *
 * @param[index] static byte index in the current package.
 * @param[value] new the static byte value.
 */
void Static_Handler::setPersistentByte(const uint16_t index,
                                       const jbyte_t value) {
  validateStaticSlot(this->package, index, 1, false);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_BYTE);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);

  validateStaticPrimitiveType(tag, FieldType::FIELD_TYPE_BYTE);
  FlashMemory_Handler::setPersistentField_Byte(tag, value);
}

/**
 * Set static short value.
 *
 * @param[index] static short index in the current package.
 * @param[value] new the static short value.
 */
void Static_Handler::setPersistentShort(const uint16_t index,
                                        const jshort_t value) {
  validateStaticSlot(this->package, index, sizeof(jshort_t), false);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_SHORT);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);

  validateStaticPrimitiveType(tag, FieldType::FIELD_TYPE_SHORT);
  FlashMemory_Handler::setPersistentField_Short(tag, value);
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Set static integer value.
 *
 * @param[index] static integer index in the current package.
 * @param[value] new the static integer value.
 */
void Static_Handler::setPersistentInt(const uint16_t index,
                                      const jint_t value) {
  validateStaticSlot(this->package, index, sizeof(jint_t), false);
  validateDeclaredStaticType(this->package, index, FieldType::FIELD_TYPE_INT);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);

  JCVMArray<jbyte_t> data(sizeof(jint_t));
  data[0] = HIGH_BYTE_SHORT(INT_2_MSSHORTS(value));
  data[1] = LOW_BYTE_SHORT(INT_2_MSSHORTS(value));
  data[2] = HIGH_BYTE_SHORT(INT_2_LSSHORTS(value));
  data[3] = LOW_BYTE_SHORT(INT_2_LSSHORTS(value));

  validateStaticPrimitiveType(tag, FieldType::FIELD_TYPE_INT);
  FlashMemory_Handler::setPersistentField_Int(tag, value);
}
#endif /* JCVM_INT_SUPPORTED */

} // namespace jcvm
