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

#include "jc_array.hpp"
#include "../jc_utils.hpp"
#include "ffi.h"
#include "jc_array_type.hpp"
#include "../heap.hpp"

#ifdef JCVM_FIREWALL_CHECKS
#include "../context.hpp"
#include "../heap.hpp"
#include "../jc_handlers/jc_class.hpp"
#include "../jc_handlers/jc_cp.hpp"
#endif /* JCVM_FIREWALL_CHECKS */

namespace jcvm {

namespace {
constexpr jpackage_ID_t kNoGlobalReferenceOwner = 0xff;
constexpr jpackage_ID_t kJCREGlobalReferenceOwner = 0xfe;

void checkArrayEntryIndex(const JC_Array &array, const uint16_t index) {
  if (index >= array.size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
}

uint16_t restoredArrayBackingLength(const uint8_t tag_length,
                                   const uint32_t data_length) {
  const uint32_t backing_length =
      static_cast<uint32_t>(tag_length) + sizeof(uint8_t) + data_length;
  if (backing_length > 0xffffu) {
    throw Exceptions::FullMemoryException;
  }
  return static_cast<uint16_t>(backing_length);
}
} // namespace

uint16_t JC_Array::checkedBackingLength(const uint16_t size,
                                       const jc_array_type type,
                                       const bool is_transient) {
  const uint32_t data_length =
      static_cast<uint32_t>(size) * getEntrySize(type);
  const uint32_t record_header =
      is_transient ? 0u : (type == JAVA_ARRAY_T_REFERENCE ? 5u : 3u);
  if (data_length + record_header > 0xffffu) {
    throw Exceptions::FullMemoryException;
  }
  return static_cast<uint16_t>(data_length);
}

/**
 * Constructor
 */
JC_Array::JC_Array(Heap &owner, const uint16_t size, const jc_array_type type,
                   const bool isTransientArray, const ClearEvent event)
    : JC_Object(owner, false), isTransient(isTransientArray),
      type(type), logical_length(size), reference_type(0xFFFF),
      array(checkedBackingLength(size, type, isTransientArray)), clear(event),
      global_reference_owners(),
      reference_value_owners(type == JAVA_ARRAY_T_REFERENCE ? size : 0,
                             kNoGlobalReferenceOwner) {
#ifdef JCVM_SECURE_HEAP_ACCESS

  switch (this->type) {
  case JAVA_ARRAY_T_BOOLEAN:
  case JAVA_ARRAY_T_BYTE:
  case JAVA_ARRAY_T_SHORT:

#ifdef JCVM_INT_SUPPORTED
  case JAVA_ARRAY_T_INT:
#endif /* JCVM_INT_SUPPORTED */

    // everything fine
    break;

  default:
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */
}

/**
 * Constructor
 */
JC_Array::JC_Array(Heap &owner, const uint16_t size, const jc_array_type type,
                   const jc_cp_offset_t reference_type,
                   const bool isTransientArray, const ClearEvent event)
    : JC_Object(owner, false), isTransient(isTransientArray),
      type(type), logical_length(size), reference_type(reference_type),
      array(checkedBackingLength(size, type, isTransientArray)), clear(event),
      global_reference_owners(
          owner.isGlobalHeap() && type == JAVA_ARRAY_T_REFERENCE
              ? size
              : 0,
          kNoGlobalReferenceOwner),
      reference_value_owners(type == JAVA_ARRAY_T_REFERENCE ? size : 0,
                             kNoGlobalReferenceOwner) {}

/**
 * Constructor
 */
JC_Array::JC_Array(Heap &owner, const jc_array_type type,
                   const jc_cp_offset_t reference_type, const fs::Tag &tag,
                   const bool isTransientArray, const ClearEvent event,
                   const uint16_t length)
    : JC_Object(owner, true), isTransient(isTransientArray), type(type),
      logical_length(length), reference_type(reference_type),
      array(restoredArrayBackingLength(
                tag.len, isTransientArray
                            ? static_cast<uint32_t>(length) *
                                  JC_Array::getEntrySize(type)
                            : 0),
            new uint8_t[restoredArrayBackingLength(
                tag.len, isTransientArray
                            ? static_cast<uint32_t>(length) *
                                  JC_Array::getEntrySize(type)
                            : 0)]{},
            true),
      clear(event), global_reference_owners(),
      reference_value_owners(type == JAVA_ARRAY_T_REFERENCE ? length : 0,
                             kNoGlobalReferenceOwner) {
  this->persistent_tag = tag;
  this->has_persistent_tag = true;
  this->array[0] = tag.len;

  for (uint8_t idx = 0; idx < tag.len; idx++) {
    this->array[idx + 1] = tag.value[idx];
  }
}

/*
 * Default destructor; JCVMArray owns the backing buffer.
 */
JC_Array::~JC_Array() noexcept = default;

/*
 * Compute tag value
 *
 * @return tag value
 */
fs::Tag JC_Array::computeTag() const noexcept {
  if (this->has_persistent_tag) {
    return this->persistent_tag;
  }

  fs::Tag out;

  out.len = this->array[0];

  for (uint8_t idx = 0; idx < out.len; idx++) {
    out.value[idx] = this->array[idx + 1];
  }

  return out;
}

void JC_Array::setPersistentTag(const fs::Tag &tag) noexcept {
  this->persistent_tag = tag;
  this->has_persistent_tag = true;
  this->setPersistent(true);
}

/**
 * Get the array entry size.
 *
 * @param[type] the type of each array's entry.
 * @return the array entry's size.
 */
const uint16_t JC_Array::getEntrySize(const jc_array_type type) {
  switch (type) {
  case jc_array_type::JAVA_ARRAY_T_BYTE:
    return sizeof(jbyte_t);

  case jc_array_type::JAVA_ARRAY_T_BOOLEAN:
    return sizeof(jbool_t);

  case jc_array_type::JAVA_ARRAY_T_SHORT:
    return sizeof(jshort_t);

#ifdef JCVM_INT_SUPPORTED

  case jc_array_type::JAVA_ARRAY_T_INT:
    return sizeof(jint_t);

#endif /* JCVM_INT_SUPPORTED */

  case jc_array_type::JAVA_ARRAY_T_REFERENCE:
    return sizeof(jref_t);
  }

  throw Exceptions::SecurityException;
}

/**
 * Get the array entry size.
 *
 * @return the array entry's size.
 */
uint16_t JC_Array::getEntrySize() const {
  return this->getEntrySize(this->getType());
}

/**
 * Get array type.
 *
 * @return array type.
 */
jc_array_type JC_Array::getType() const noexcept { return this->type; }

/**
 * Get array reference type.
 *
 * @return array reference type.
 */
jc_cp_offset_t JC_Array::getReferenceType() const
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
{
#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_REFERENCE) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  return this->reference_type;
}

/**
 * Get array size.
 *
 * @return array size.
 */
uint16_t JC_Array::size() const {
  if (this->isTransientArray() && this->isPersistent()) {
    return this->logical_length;
  }
  uint32_t length = 0;

  if (this->isTransientArray()) {
    length = this->array.size() / this->getEntrySize();
  } else if (this->isPersistent()) {
    fs::Tag tag = this->computeTag();
    uint8_t size_high = 0;
    uint8_t size_low = 0;
    if (fs_read_1b_at(tag.value, tag.len, 1, &size_high) ||
        fs_read_1b_at(tag.value, tag.len, 2, &size_low)) {
      throw Exceptions::IOException;
    }
    length = BYTES_TO_SHORT(size_high, size_low);
  } else {
    length = this->array.size() / this->getEntrySize();
  }

  // JCVMArray stores the backing data as bytes, while Java Card's array size
  // is the number of elements. Persistent headers already store that logical
  // count; divide transient and ordinary backing lengths by the element width.
  return static_cast<uint16_t>(length);
}

/**
 * Fetch a byte or a boolean element from an array.
 *
 * @param[index] index of the byte or boolean element to return
 * @return the indexed byte or boolean element.
 */
jbyte_t JC_Array::getByteEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
    const noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);
#ifdef JCVM_SECURE_HEAP_ACCESS

  switch (type) {
  case jc_array_type::JAVA_ARRAY_T_BYTE:
  case jc_array_type::JAVA_ARRAY_T_BOOLEAN:
    // OK
    break;

  default:
    // NOK
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jbyte_t));

      return this->array[tag.len + sizeof(tag.len) + offset];
    } else {
      return FlashMemory_Handler::getPersistentField_Array_Byte(tag, index
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                                ,
                                                                this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACESS */
      );
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jbyte_t));

    return this->array[offset];
  }
}

/**
 * Fetch a short element from an array.
 *
 * @param[index] index of the short element to return
 * @return the indexed short element.
 */
jshort_t JC_Array::getShortEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
    const noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);
#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_SHORT) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jshort_t));

      return signedShortFromBits(BYTES_TO_SHORT(
          this->array[tag.len + sizeof(tag.len) + offset],
          this->array[tag.len + sizeof(tag.len) + offset + 1]));
    } else {
      return FlashMemory_Handler::getPersistentField_Array_Short(
          tag, index
#ifdef JCVM_SECURE_HEAP_ACCESS
          ,
          this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACESS */
      );
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jshort_t));

    return signedShortFromBits(BYTES_TO_SHORT(
        this->array[offset], this->array[(uint16_t)(offset + 1)]));
  }
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Fetch a int element from an array.
 *
 * @param[index] index of the int element to return
 * @return the indexed int element.
 */
jint_t JC_Array::getIntEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
    const noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_INT) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jint_t));

      return signedIntFromBits(BYTES_TO_INT(this->array[tag.len + sizeof(tag.len) + offset],
                          this->array[tag.len + sizeof(tag.len) + offset + 1],
                          this->array[tag.len + sizeof(tag.len) + offset + 2],
                          this->array[tag.len + sizeof(tag.len) + offset + 3]));
    } else {
      return FlashMemory_Handler::getPersistentField_Array_Int(tag, index
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                               ,
                                                               this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACESS */
      );
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jint_t));

    return signedIntFromBits(BYTES_TO_INT(this->array[offset],
                                          this->array[offset + 1],
                                          this->array[offset + 2],
                                          this->array[offset + 3]));
  }
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Fetch a reference element from an array.
 *
 * @param[index] index of the reference element to return
 * @return the indexed reference element.
 */
jref_t JC_Array::getReferenceEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_REFERENCE) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jref_t));

      return BYTES_TO_SHORT(
          this->array[tag.len + sizeof(tag.len) + offset],
          this->array[tag.len + sizeof(tag.len) + offset + 1]);
    } else {
      return FlashMemory_Handler::getPersistentField_Array_Reference(
          tag, index, this->getOwner());
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jref_t));
    return jref_t(BYTES_TO_SHORT(this->array[offset], this->array[offset + 1]));
  }
}

bool JC_Array::isReferenceEntryAccessible(const uint16_t index,
                                         const jpackage_ID_t package_id) {
  checkArrayEntryIndex(*this, index);
  if (!this->getOwner().isGlobalHeap() ||
      this->type != JAVA_ARRAY_T_REFERENCE) {
    return true;
  }
  const jpackage_ID_t owner = this->global_reference_owners.at(index);
  return owner == kNoGlobalReferenceOwner ||
         owner == kJCREGlobalReferenceOwner || owner == package_id;
}

jpackage_ID_t JC_Array::getReferenceEntryOwner(const uint16_t index) {
  checkArrayEntryIndex(*this, index);
  if (this->type != JAVA_ARRAY_T_REFERENCE) {
    return kNoGlobalReferenceOwner;
  }
  if (this->getOwner().isGlobalHeap()) {
    return this->global_reference_owners.at(index);
  }
  return this->reference_value_owners.at(index);
}

void JC_Array::visitGlobalReferences(
    const jpackage_ID_t package_id, void (*visitor)(jref_t, void *),
    void *visitor_context) {
  if (!this->getOwner().isGlobalHeap() ||
      this->type != JAVA_ARRAY_T_REFERENCE || visitor == nullptr) {
    return;
  }
  for (uint16_t index = 0; index < this->global_reference_owners.size();
       ++index) {
    if (this->global_reference_owners[index] == package_id) {
      const jref_t reference = this->getReferenceEntry(index);
      if (!reference.isNullPointer()) visitor(reference, visitor_context);
    }
  }
}

void JC_Array::clearGlobalReferences(const jpackage_ID_t package_id) noexcept {
  if (!this->getOwner().isGlobalHeap() ||
      this->type != JAVA_ARRAY_T_REFERENCE || this->isPersistent()) {
    return;
  }
  for (uint16_t index = 0; index < this->global_reference_owners.size();
       ++index) {
    if (this->global_reference_owners[index] != package_id) continue;
    const uint16_t offset =
        static_cast<uint16_t>(index * sizeof(jref_t));
    this->array[offset] = 0;
    this->array[static_cast<uint16_t>(offset + 1)] = 0;
    this->global_reference_owners[index] = kNoGlobalReferenceOwner;
  }
}

bool JC_Array::hasForeignReferenceEntries() const noexcept {
  for (const jpackage_ID_t owner : this->reference_value_owners) {
    if (owner != kNoGlobalReferenceOwner) return true;
  }
  return false;
}

void JC_Array::visitForeignReferencesForPackage(
    const jpackage_ID_t package_id,
    void (*visitor)(jref_t, void *), void *visitor_context) {
  if (this->getOwner().isGlobalHeap() ||
      this->type != JAVA_ARRAY_T_REFERENCE || visitor == nullptr ||
      this->isPersistent()) {
    return;
  }
  for (uint16_t index = 0; index < this->reference_value_owners.size();
       ++index) {
    if (this->reference_value_owners[index] != package_id) continue;
    const jref_t reference = this->getReferenceEntry(index);
    if (!reference.isNullPointer()) visitor(reference, visitor_context);
  }
}

void JC_Array::clearForeignReferencesForPackage(
    const jpackage_ID_t package_id) noexcept {
  if (this->getOwner().isGlobalHeap() ||
      this->type != JAVA_ARRAY_T_REFERENCE || this->isPersistent()) {
    return;
  }
  for (uint16_t index = 0; index < this->reference_value_owners.size();
       ++index) {
    if (this->reference_value_owners[index] != package_id) continue;
    const uint16_t offset = static_cast<uint16_t>(index * sizeof(jref_t));
    this->array[offset] = 0;
    this->array[static_cast<uint16_t>(offset + 1)] = 0;
    this->reference_value_owners[index] = kNoGlobalReferenceOwner;
  }
}

/**
 * Write a byte or a boolean element from an array.
 *
 * @param[index] index of the element to set.
 * @param[value] the new value
 */
void JC_Array::setByteEntry(const uint16_t index, const jbyte_t value)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);
#ifdef JCVM_SECURE_HEAP_ACCESS

  switch (type) {
  case jc_array_type::JAVA_ARRAY_T_BOOLEAN:
  case jc_array_type::JAVA_ARRAY_T_BYTE:
    // OK
    break;

  default:
    // NOK
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jbyte_t));

      this->array[tag.len + sizeof(tag.len) + offset] = value;
    } else {
      FlashMemory_Handler::setPersistentField_Array_Byte(tag, index, value
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                         ,
                                                         this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACESS */
      );
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jbyte_t));

    this->array[offset] = value;
  }
}

void JC_Array::setByteEntryNonAtomic(const uint16_t index,
                                     const jbyte_t value) {
  checkArrayEntryIndex(*this, index);
#ifdef JCVM_SECURE_HEAP_ACCESS
  if (this->type != jc_array_type::JAVA_ARRAY_T_BYTE) {
    throw Exceptions::SecurityException;
  }
#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    const fs::Tag tag = this->computeTag();
    if (this->isTransientArray()) {
      const uint16_t offset =
          static_cast<uint16_t>(tag.len + sizeof(tag.len) + index);
      this->array[offset] = value;
    } else {
      FlashMemory_Handler::setPersistentField_Array_ByteNonAtomic(
          tag, index, value
#ifdef JCVM_SECURE_HEAP_ACCESS
          , this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACCESS */
      );
    }
  } else {
    this->array[index] = value;
  }
}

/**
 * Write a byte or a boolean element from an array.
 *
 * @param[index] index of the element to set.
 * @param[value] the new value
 */
void JC_Array::setShortEntry(const uint16_t index, const jshort_t value)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);
#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_SHORT) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jshort_t));

      this->array[tag.len + sizeof(tag.len) + offset] = HIGH_BYTE_SHORT(value);
      this->array[tag.len + sizeof(tag.len) + offset + 1] =
          LOW_BYTE_SHORT(value);
    } else {
      FlashMemory_Handler::setPersistentField_Array_Short(tag, index, value
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                          ,
                                                          this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACESS */
      );
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jshort_t));

    this->array[offset] = HIGH_BYTE_SHORT(value);
    this->array[offset + 1] = LOW_BYTE_SHORT(value);
  }
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Write a byte or a boolean element from an array.
 *
 * @param[index] index of the element to set.
 * @param[value] the new value
 */
void JC_Array::setIntEntry(const uint16_t index, const jint_t value)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
{
  checkArrayEntryIndex(*this, index);
#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_INT) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jint_t));

      this->array[tag.len + sizeof(tag.len) + offset] =
          HIGH_BYTE_SHORT(INT_2_MSSHORTS(value));
      this->array[tag.len + sizeof(tag.len) + offset + 1] =
          LOW_BYTE_SHORT(INT_2_MSSHORTS(value));
      this->array[tag.len + sizeof(tag.len) + offset + 2] =
          HIGH_BYTE_SHORT(INT_2_LSSHORTS(value));
      this->array[tag.len + sizeof(tag.len) + offset + 3] =
          LOW_BYTE_SHORT(INT_2_LSSHORTS(value));
    } else {
      FlashMemory_Handler::setPersistentField_Array_Int(tag, index, value
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                        ,
                                                        this->getOwner()
#endif /* JCVM_SECURE_HEAP_ACESS */
      );
    }
  } else {
    const uint16_t offset = (uint16_t)(index * sizeof(jint_t));

    this->array[offset] = HIGH_BYTE_SHORT(INT_2_MSSHORTS(value));
    this->array[offset + 1] = LOW_BYTE_SHORT(INT_2_MSSHORTS(value));
    this->array[offset + 2] = HIGH_BYTE_SHORT(INT_2_LSSHORTS(value));
    this->array[offset + 3] = LOW_BYTE_SHORT(INT_2_LSSHORTS(value));
  }
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Write a byte or a boolean element from an array.
 *
 * @param[index] index of the element to set.
 * @param[value] the new value
 */
void JC_Array::setReferenceEntry(const uint16_t index, const jref_t value,
                                 Context &context,
                                 const jpackage_ID_t value_owner)
{
  checkArrayEntryIndex(*this, index);
  if (value.isGlobalArray()) {
    // A global array cannot be nested in another reference array.
    throw Exceptions::SecurityException;
  }
#ifdef JCVM_SECURE_HEAP_ACCESS

  if (this->type != jc_array_type::JAVA_ARRAY_T_REFERENCE) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  const bool foreign_reference =
      value_owner != 0xff && value_owner != 0xfe &&
      value_owner != context.getCurrentPackageID();
  bool object_component_array = false;
#ifdef JCVM_FIREWALL_CHECKS
  if (foreign_reference && !this->getOwner().isGlobalHeap() &&
      !this->isPersistent() &&
      this->type == jc_array_type::JAVA_ARRAY_T_REFERENCE &&
      this->reference_type != 0xFFFF) {
    auto current_package = context.getCurrentPackage();
    ConstantPool_Handler cp_handler(current_package);
    const auto component = cp_handler.classref2class(
        cp_handler.getClassRef(this->reference_type));
    object_component_array = component.second != nullptr &&
                             component.second->isObjectClass();
  }
#endif
  if (foreign_reference && this->isPersistent()) {
    // Persistent foreign references need a stable cross-package object
    // identity in the flash format. Keep failing closed until that format is
    // implemented.
    throw Exceptions::SecurityException;
  }
  if (foreign_reference &&
      !this->getOwner().isGlobalHeap() &&
      (this->isPersistent() ||
       this->type != jc_array_type::JAVA_ARRAY_T_REFERENCE ||
       (this->reference_type != 0xFFFF && !object_component_array) ||
       this->getOwner().getOwner() != context.getAppletID())) {
    // Only volatile Object[] arrays owned by the active applet may retain
    // foreign entries. Context routes their roots into the referenced package
    // heap before it is collected.
    throw Exceptions::SecurityException;
  }
  if (foreign_reference && this->getOwner().isGlobalHeap() &&
      this->reference_type != 0xFFFF) {
    throw Exceptions::SecurityException;
  }

#ifdef JCVM_FIREWALL_CHECKS
  // Check if types are compatible.
  Heap &heap = context.getHeap();
  auto current_package = context.getCurrentPackage();
  ConstantPool_Handler cp_handler(current_package);
  Class_Handler class_handler(current_package);

  // Native-created Object[] arrays use 0xFFFF as a wildcard component type;
  // unlike CAP-backed arrays, they have no component class in a CAP pool.
  if (this->reference_type == 0xFFFF || value.isNullPointer() ||
      (foreign_reference && object_component_array)) {
    // Wildcard arrays accept any reference; null is valid for all arrays.
    ;
  } else if (value.isInstance()) {
    auto instanceref_to_add = heap.getInstance(value);

    auto classref_in = std::make_pair<uint32_t, const uint8_t *>(
        instanceref_to_add->getPackageID(),
        reinterpret_cast<const uint8_t *>(
            ConstantPool_Handler(instanceref_to_add->getPackageID())
                .getClassFromClassIndex(instanceref_to_add->getClassIndex())));
    auto classref_out = cp_handler.getClassRef(this->reference_type);

    if (class_handler.docheckcast(
            classref_in, cp_handler.resolveClassref(classref_out)) == FALSE) {
      throw Exceptions::ArrayStoreException;
    }
  } else {
    auto arrayref_to_add = heap.getArray(value);

    /// It is an array reference.
    switch (arrayref_to_add->getType()) {
    case jc_array_type::JAVA_ARRAY_T_BOOLEAN:
    case jc_array_type::JAVA_ARRAY_T_BYTE:
    case jc_array_type::JAVA_ARRAY_T_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case jc_array_type::JAVA_ARRAY_T_INT:
#endif /* JCVM_INT_SUPPORTED */
    {
      auto array_reference_classref =
          cp_handler.getClassRef(this->reference_type);
      auto array_reference_class =
          cp_handler.classref2class(array_reference_classref);

      if (!(array_reference_class.second->isObjectClass())) {
        throw Exceptions::ArrayStoreException;
      }

      break;
    }

    case jc_array_type::JAVA_ARRAY_T_REFERENCE: {

      auto classref_in =
          cp_handler.getClassRef(arrayref_to_add->getReferenceType());
      auto classref_out = cp_handler.getClassRef(this->reference_type);

      if (class_handler.docheckcast(cp_handler.resolveClassref(classref_in),
                                    cp_handler.resolveClassref(classref_out)) ==
          FALSE) {
        throw Exceptions::ArrayStoreException;
      }
    }

    break;

    default:
      throw Exceptions::SecurityException;
    }
  }

#endif /* JCVM_FIREWALL_CHECKS */

  jpackage_ID_t global_reference_owner = kNoGlobalReferenceOwner;
  if (this->getOwner().isGlobalHeap() && !value.isNullPointer()) {
    if (value.isGlobalInstance()) {
      global_reference_owner = kJCREGlobalReferenceOwner;
    } else if (value_owner != kNoGlobalReferenceOwner &&
               value_owner != kJCREGlobalReferenceOwner) {
      global_reference_owner = value_owner;
    } else {
      // Validate the local handle before retaining it and associate it with
      // the CAP package heap so equal handles in another package cannot alias.
      Heap &heap = context.getHeap();
      if (value.isInstance()) {
        (void)heap.getInstance(value);
      } else {
        (void)heap.getArray(value);
      }
      global_reference_owner = context.getCurrentPackageID();
    }
  }

  if (foreign_reference && !this->getOwner().isGlobalHeap() &&
      !this->getOwner().registerForeignReferenceArray(this)) {
    throw Exceptions::SecurityException;
  }

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      const uint16_t offset = (uint16_t)(index * sizeof(jref_t));

      this->array[tag.len + sizeof(tag.len) + offset] =
          HIGH_BYTE_SHORT(value.compact());
      this->array[tag.len + sizeof(tag.len) + offset + 1] =
          LOW_BYTE_SHORT(value.compact());
    } else {
      FlashMemory_Handler::setPersistentField_Array_Reference(tag, index, value,
                                                              this->getOwner());
    }
  } else {

    const uint16_t offset = (uint16_t)(index * sizeof(jref_t));

    this->array[offset] = HIGH_BYTE_SHORT(value.compact());
    this->array[(uint16_t)(offset + 1)] = LOW_BYTE_SHORT(value.compact());
  }
  if (!this->global_reference_owners.empty()) {
    this->global_reference_owners[index] = global_reference_owner;
  }
  if (!this->reference_value_owners.empty()) {
    this->reference_value_owners[index] =
        value.isNullPointer() || !foreign_reference
            ? kNoGlobalReferenceOwner
            : value_owner;
    if (!this->getOwner().isGlobalHeap() &&
        !this->hasForeignReferenceEntries()) {
      this->getOwner().unregisterForeignReferenceArray(this);
    }
  }
}

/*
 * Get a const pointer to array data
 *
 * @return Const pointer to array data
 */
const uint8_t *JC_Array::getData() const {

  if (this->isPersistent()) {
    auto tag = this->computeTag();

    if (this->isTransientArray()) {
      return &(this->array.data()[tag.len + sizeof(tag.len)]);
    } else {
      throw Exceptions::RuntimeException;
    }
  }

  return static_cast<const uint8_t *>(this->array.data());
}

/*
 * Is a transient array?
 *
 */
const bool JC_Array::isTransientArray() const noexcept {
  return this->isTransient;
}

/*
 * Get clear event
 *
 */
const ClearEvent JC_Array::getClearEvent() const noexcept {
  return this->clear;
}

/**
 * Clear transient array contents to the Java Card default value.
 */
void JC_Array::clearTransientData() {
  if (!this->isTransientArray()) {
    throw Exceptions::SecurityException;
  }

  this->array.fill(0);
  std::fill(this->global_reference_owners.begin(),
            this->global_reference_owners.end(), kNoGlobalReferenceOwner);
  std::fill(this->reference_value_owners.begin(),
            this->reference_value_owners.end(), kNoGlobalReferenceOwner);
}

} // namespace jcvm
