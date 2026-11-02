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
#include "../exceptions.hpp"
#include "../jc_cap/jc_cap_static_field.hpp"
#include "../heap.hpp"
#include "flashmemory.hpp"
#include "ffi.h"

namespace jcvm {
namespace {

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
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);
  if (fs_exists(tag.value, tag.len) == 0) {
    uint8_t value[1];
    getInitialPrimitiveBytes(this->package, index, value, sizeof(value));
    return static_cast<jbyte_t>(value[0]);
  }
  return FlashMemory_Handler::getPersistentField_Byte(tag);
}

/**
 * Get static short value.
 *
 * @param[index] static short index in the current package.
 * @return the static short value.
 */
jshort_t Static_Handler::getPersistentShort(const uint16_t index) const {
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);
  if (fs_exists(tag.value, tag.len) == 0) {
    uint8_t bytes[2];
    getInitialPrimitiveBytes(this->package, index, bytes, sizeof(bytes));
    const uint16_t value = readStaticU16(bytes);
    return static_cast<jshort_t>(value);
  }
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
  jref_t reference;
  const jpackage_ID_t package_id = this->package.getPackageID();
  if (heap.getStaticReferenceRoot(package_id, index, reference)) {
    return reference;
  }

  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      package_id, index);

  reference = FlashMemory_Handler::getPersistentField_Reference(tag, heap);
  heap.setStaticReferenceRoot(package_id, index, reference);
  return reference;
}

/**
 * Set a static reference value.
 *
 * @param[index] static reference index in the current package.
 * @param[value] reference to persist.
 * @param[heap] the current applet heap.
 */
void Static_Handler::setPersistentRef(const uint16_t index,
                                      const jref_t value, Heap &heap) {
  if (value.isGlobalArray()) {
    throw Exceptions::SecurityException;
  }
  const jpackage_ID_t package_id = this->package.getPackageID();
  heap.reserveStaticReferenceRoot(package_id, index);
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      package_id, index);
  FlashMemory_Handler::setPersistentField_Reference(tag, value, heap);
  heap.setStaticReferenceRoot(package_id, index, value);
}

/**
 * Set static byte value.
 *
 * @param[index] static byte index in the current package.
 * @param[value] new the static byte value.
 */
void Static_Handler::setPersistentByte(const uint16_t index,
                                       const jbyte_t value) {
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);

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
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);

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
  fs::Tag tag = FlashMemory_Handler::getStaticFieldTag(
      this->package.getPackageID(), index);

  JCVMArray<jbyte_t> data(sizeof(jint_t));
  data[0] = HIGH_BYTE_SHORT(INT_2_MSSHORTS(value));
  data[1] = LOW_BYTE_SHORT(INT_2_MSSHORTS(value));
  data[2] = HIGH_BYTE_SHORT(INT_2_LSSHORTS(value));
  data[3] = LOW_BYTE_SHORT(INT_2_LSSHORTS(value));

  FlashMemory_Handler::setPersistentField_Int(tag, value);
}
#endif /* JCVM_INT_SUPPORTED */

} // namespace jcvm
