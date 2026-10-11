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

#include "jc_instance.hpp"
#include "../jc_handlers/flashmemory.hpp"
#include "../jc_handlers/jc_class.hpp"
#include "../jc_handlers/jc_cp.hpp"
#include "../jc_utils.hpp"
#include "jc_array.hpp"
#include "ffi.h"

#include <cstdio>
#include <cstring>

#ifndef PC_VERSION
extern "C" void choupi_install_stage(uint8_t stage);
#endif

namespace jcvm {

namespace {

uint8_t persistent_tag_root_length(uint8_t first, uint8_t length) {
  if (length == 0) {
    return 0;
  }
  switch (first) {
  case 0x00:
    return 1;
  case 0x01:
    return length >= 2 ? 2 : 0;
  case 0x02:
    return length >= 3 ? 3 : 0;
  case 0x03:
    return length >= 5 ? 5 : 0;
  default:
    return 0;
  }
}

} // namespace

/**
 * Default constructor.
 *
 * @param[owner] owner
 * @param[package_owner] package owner
 * @param[instantiated_class] Constant Pool token to the instantiated class.
 */
JC_Instance::JC_Instance(Heap &owner, const Package &package_owner,
                         const jc_cp_offset_t instantiated_class) noexcept
    : JC_Object(owner, false) {
  ConstantPool_Handler cp(package_owner);
  std::pair<jpackage_ID_t, jclass_index_t> pair =
      cp.getClassInformation(instantiated_class);

  this->packageID = pair.first;
  this->claz = pair.second;

  Class_Handler class_handler(this->packageID);
  uint16_t fields_size = class_handler.getInstanceFieldsSize(this->claz);
  this->fields = new JCVMArray<jc_field_t>(fields_size);
}

/**
 * Default constructor.
 *
 * @param[owner] owner
 * @param[packageID] class package ID
 * @param[claz] class index
 */
JC_Instance::JC_Instance(Heap &owner, const jpackage_ID_t packageID,
                         const jclass_index_t claz) noexcept
    : JC_Object(owner, false), packageID(packageID), claz(claz) {
#ifndef PC_VERSION
  choupi_install_stage(4);
#endif
  Class_Handler class_handler(this->packageID);
  uint16_t fields_size = class_handler.getInstanceFieldsSize(this->claz);
#ifndef PC_VERSION
  choupi_install_stage(8);
#endif
  this->fields = new JCVMArray<jc_field_t>(fields_size);
#ifndef PC_VERSION
  choupi_install_stage(9);
#endif
};

/**
 * Default constructor.
 *
 * @param[owner] owner
 * @param[packageID] class package ID
 * @param[claz] class index
 * @param[tag] persistant associated tag value
 * @param[isStatic] is a static instance?
 */
JC_Instance::JC_Instance(Heap &owner, const jpackage_ID_t packageID,
                         const jclass_index_t claz_index,
                         const fs::Tag &tag) noexcept
    : JC_Object(owner, true), packageID(packageID), claz(claz_index) {
  this->has_persistent_tag = true;
  this->persistent_tag_length = tag.len;
  const uint8_t root_length =
      persistent_tag_root_length(tag.value[0], tag.len);
  this->persistent_tag_compressed = root_length != 0 &&
                                    ((tag.len - root_length) & 1) == 0 &&
                                    tag.len > root_length;
  for (uint8_t idx = root_length; this->persistent_tag_compressed &&
                                  idx < tag.len; idx += 2) {
    if (tag.value[idx] != tag.value[idx + 1]) {
      this->persistent_tag_compressed = false;
    }
  }
  const uint8_t stored_length =
      this->persistent_tag_compressed
          ? static_cast<uint8_t>(root_length + (tag.len - root_length) / 2)
          : tag.len;
  uint8_t *stored_tag = this->persistent_tag_value;
  if (stored_length > sizeof(this->persistent_tag_value)) {
    this->persistent_tag_overflow = new uint8_t[stored_length];
    stored_tag = this->persistent_tag_overflow;
  }
  if (this->persistent_tag_compressed) {
    std::memcpy(stored_tag, tag.value, root_length);
    uint8_t output = root_length;
    for (uint8_t idx = root_length; idx < tag.len; idx += 2) {
      stored_tag[output++] = tag.value[idx];
    }
  } else {
    std::memcpy(stored_tag, tag.value, tag.len);
  }

  Class_Handler class_handler(this->packageID);
  const uint16_t fields_size =
      class_handler.getInstanceFieldsSize(this->claz);
  this->fields = new JCVMArray<jc_field_t>(fields_size);
  this->persistent_fields_loaded = false;
}

JC_Instance::JC_Instance(JC_Instance &&other) noexcept
    : JC_Object(other.getOwner(), other.isPersistent()),
      packageID(other.packageID), claz(other.claz),
      persistent_tag_length(other.persistent_tag_length),
      persistent_tag_compressed(other.persistent_tag_compressed),
      persistent_tag_overflow(other.persistent_tag_overflow),
      has_persistent_tag(other.has_persistent_tag),
      persistent_fields_loaded(other.persistent_fields_loaded),
      fields(other.fields) {
  std::memcpy(this->persistent_tag_value, other.persistent_tag_value,
              sizeof(this->persistent_tag_value));
  other.persistent_tag_overflow = nullptr;
  other.fields = nullptr;
  other.persistent_tag_length = 0;
  other.has_persistent_tag = false;
  other.persistent_fields_loaded = true;
}

/*
 * Default destructor
 */
JC_Instance::~JC_Instance() noexcept {
  delete[] this->persistent_tag_overflow;
  if (this->fields != nullptr) {
    delete this->fields;
  }
}

/**
 * Recompute instance Tag
 *
 * @return original instance tag
 */
fs::Tag JC_Instance::recomputeOriginalTag() const noexcept {
  fs::Tag tag;

  tag.len = this->persistent_tag_length;
  const uint8_t *stored_tag = this->persistent_tag_overflow == nullptr
                                  ? this->persistent_tag_value
                                  : this->persistent_tag_overflow;
  if (this->persistent_tag_compressed) {
    const uint8_t root_length =
        persistent_tag_root_length(stored_tag[0], tag.len);
    std::memcpy(tag.value, stored_tag, root_length);
    uint8_t input = root_length;
    for (uint8_t idx = root_length; idx < tag.len; idx += 2) {
      tag.value[idx] = stored_tag[input];
      tag.value[idx + 1] = stored_tag[input++];
    }
  } else {
    std::memcpy(tag.value, stored_tag, tag.len);
  }

  return tag;
}

bool JC_Instance::hasPersistentTag() const noexcept {
  return this->has_persistent_tag;
}

fs::Tag JC_Instance::getPersistentTag() const noexcept {
  return this->recomputeOriginalTag();
}

void JC_Instance::setPersistentTag(const fs::Tag &tag) {
  if (this->has_persistent_tag) {
    return;
  }
  this->persistent_tag_length = tag.len;
  const uint8_t root_length = persistent_tag_root_length(tag.value[0], tag.len);
  this->persistent_tag_compressed = root_length != 0 &&
                                    ((tag.len - root_length) & 1) == 0 &&
                                    tag.len > root_length;
  for (uint8_t idx = root_length; this->persistent_tag_compressed &&
                                  idx < tag.len; idx += 2) {
    if (tag.value[idx] != tag.value[idx + 1]) {
      this->persistent_tag_compressed = false;
    }
  }
  const uint8_t stored_length =
      this->persistent_tag_compressed
          ? static_cast<uint8_t>(root_length + (tag.len - root_length) / 2)
          : tag.len;
  uint8_t *stored_tag = this->persistent_tag_value;
  if (stored_length > sizeof(this->persistent_tag_value)) {
    this->persistent_tag_overflow = new uint8_t[stored_length];
    stored_tag = this->persistent_tag_overflow;
  }
  if (this->persistent_tag_compressed) {
    std::memcpy(stored_tag, tag.value, root_length);
    uint8_t output = root_length;
    for (uint8_t idx = root_length; idx < tag.len; idx += 2) {
      stored_tag[output++] = tag.value[idx];
    }
  } else {
    std::memcpy(stored_tag, tag.value, tag.len);
  }
  this->has_persistent_tag = true;
  this->setPersistent(true);
}

/*
 * Get package ID
 *
 * @return Class package ID
 */
jpackage_ID_t JC_Instance::getPackageID() const noexcept {
  return this->packageID;
}

/*
 * Get class index
 *
 * @return Class index
 */
jclass_index_t JC_Instance::getClassIndex() const noexcept {
  return this->claz;
}

/*
 * Set package ID
 *
 * @param[packageID] new package ID
 */
void JC_Instance::setPackageID(const jpackage_ID_t packageID) noexcept {
  this->packageID = packageID;
}

/*
 * Set class index
 *
 * @param[class_index] new class index
 */
void JC_Instance::setClassIndex(const jclass_index_t class_index) noexcept {
  this->claz = class_index;
}

/**
 * Fetch byte or boolean from object.
 *
 * @param[index] index of the instance field to fetch.
 * @return The fetched instance field value.
 */
const jbyte_t JC_Instance::getField_Byte(const uint16_t index) {
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    return FlashMemory_Handler::getPersistentField_Byte(tag);
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be verified.
#endif /* JCVM_TYPED_HEAP */

    return (jbyte_t)(this->fields->at(index).value);
  }
}

/**
 * Fetch short from object.
 *
 * @param[index] index of the instance field to fetch.
 * @return The fetched instance field value.
 */
const jshort_t JC_Instance::getField_Short(const uint16_t index) {
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    return FlashMemory_Handler::getPersistentField_Short(tag);
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be verified.
#endif /* JCVM_TYPED_HEAP */

    return (jshort_t)(this->fields->at(index).value);
  }
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Fetch int from object.
 *
 * @param[index] index of the instance field to fetch.
 * @return The fetched instance field value.
 */
const jint_t JC_Instance::getField_Int(const uint16_t index) {
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    return FlashMemory_Handler::getPersistentField_Int(tag);
  } else {

    const jword_t field_high_part = this->fields->at(index).value;
    const jword_t field_low_part =
        this->fields->at((uint16_t)(index + 1)).value;

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be verified.
#endif /* JCVM_TYPED_HEAP */

    return (jint_t)(SHORTS_TO_INT(field_high_part, field_low_part));
  }
}

#endif /* JCVM_INT_SUPPORTED */
/**
 * Fetch reference from object.
 *
 * @param[index] index of the instance field to fetch.
 * @return The fetched instance field value.
 */
const jref_t JC_Instance::getField_Reference(const uint16_t index) {
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    return FlashMemory_Handler::getPersistentField_Reference(tag,
                                                              this->getOwner());
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be verified.
#endif /* JCVM_TYPED_HEAP */

    return jref_t(this->fields->at(index).value);
  }
}

/**
 * Set byte or boolean field from object.
 *
 * @param[index] index of the instance field to set.
 * @param[value] new value.
 */
void JC_Instance::setField_Byte(uint16_t index, jbyte_t value) {
  if (this->isPersistent()) {
    (void)this->getFields();
  }
  this->fields->at(index).type = FieldType::FIELD_TYPE_BYTE;
  this->fields->at(index).value = static_cast<jword_t>(BYTE_TO_WORD(value));
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    FlashMemory_Handler::setPersistentField_Byte(tag, value);
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be saved.
#endif /* JCVM_TYPED_HEAP */

  }
}

/**
 * Set short field from object.
 *
 * @param[index] index of the instance field to set.
 * @param[value] new value.
 */
void JC_Instance::setField_Short(const uint16_t index, const jshort_t value) {
  if (this->isPersistent()) {
    (void)this->getFields();
  }
  this->fields->at(index).type = FieldType::FIELD_TYPE_SHORT;
  this->fields->at(index).value = static_cast<jword_t>(value);
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    FlashMemory_Handler::setPersistentField_Short(tag, value);
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be saved.
#endif /* JCVM_TYPED_HEAP */

  }
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Set int field from object.
 *
 * @param[index] index of the instance field to set.
 * @param[value] new value.
 */
void JC_Instance::setField_Int(const uint16_t index, const jint_t value) {
  if (this->isPersistent()) {
    (void)this->getFields();
  }
  this->fields->at(index).type = FieldType::FIELD_TYPE_INT;
  this->fields->at(index).value = static_cast<jword_t>(INT_2_MSSHORTS(value));
  this->fields->at(static_cast<uint16_t>(index + 1)).type =
      FieldType::FIELD_TYPE_UNINITIALIZED;
  this->fields->at(static_cast<uint16_t>(index + 1)).value =
      static_cast<jword_t>(INT_2_LSSHORTS(value));
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    FlashMemory_Handler::setPersistentField_Int(tag, value);
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be saved.
#endif /* JCVM_TYPED_HEAP */

  }
}

#endif /* JCVM_INT_SUPPORTED */

/**
 * Set reference field from object.
 *
 * @param[index] index of the instance field to set.
 * @param[value] new value.
 */
void JC_Instance::setField_Reference(const uint16_t index, const jref_t ref) {
  if (this->isPersistent()) {
    std::printf("CHOUPI:PUTFIELD_A phase=load index=%u package=%u class=%u\r\n",
                static_cast<unsigned>(index),
                static_cast<unsigned>(this->packageID),
                static_cast<unsigned>(this->claz));
    try {
      (void)this->getFields();
    } catch (Exceptions exception) {
      std::printf("CHOUPI:PUTFIELD_A phase=load-fail id=%u index=%u\r\n",
                  static_cast<unsigned>(exception),
                  static_cast<unsigned>(index));
      throw;
    }
  }
  if (this->packageID == 20 && this->claz == 121) {
    std::printf("CHOUPI:PUTFIELD_A phase=loaded index=%u ref=0x%04X array=%u null=%u\r\n",
                static_cast<unsigned>(index),
                static_cast<unsigned>(ref.compact()), ref.isArray() ? 1u : 0u,
                ref.isNullPointer() ? 1u : 0u);
    std::fflush(stdout);
  }
  jc_field_t &field = this->fields->at(index);
  field.value = static_cast<jword_t>(ref.compact());
  if (ref.isNullPointer()) {
    field.type = FieldType::FIELD_TYPE_UNINITIALIZED;
  } else if (ref.isArray()) {
    if (this->packageID == 20 && this->claz == 121) {
      std::printf("CHOUPI:PUTFIELD_A phase=array-lookup index=%u\r\n",
                  static_cast<unsigned>(index));
      std::fflush(stdout);
    }
    const auto array = this->getOwner().getArray(ref);
    if (this->packageID == 20 && this->claz == 121) {
      std::printf("CHOUPI:PUTFIELD_A phase=array-found index=%u type=%u\r\n",
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(array->getType()));
      std::fflush(stdout);
    }
    switch (array->getType()) {
    case JAVA_ARRAY_T_BYTE:
      field.type = array->isTransientArray()
                       ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE
                       : FieldType::FIELD_TYPE_ARRAY_BYTE;
      break;
    case JAVA_ARRAY_T_BOOLEAN:
      field.type = array->isTransientArray()
                       ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN
                       : FieldType::FIELD_TYPE_ARRAY_BOOLEAN;
      break;
    case JAVA_ARRAY_T_SHORT:
      field.type = array->isTransientArray()
                       ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT
                       : FieldType::FIELD_TYPE_ARRAY_SHORT;
      break;
#ifdef JCVM_INT_SUPPORTED
    case JAVA_ARRAY_T_INT:
      field.type = array->isTransientArray()
                       ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT
                       : FieldType::FIELD_TYPE_ARRAY_INT;
      break;
#endif /* JCVM_INT_SUPPORTED */
    case JAVA_ARRAY_T_REFERENCE:
      field.type = array->isTransientArray()
                       ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT
                       : FieldType::FIELD_TYPE_ARRAY_OBJECT;
      break;
    default:
      std::printf("CHOUPI:PERSIST_INVALID loaded-field-type=%u index=%u package=%u class=%u\r\n",
                  static_cast<unsigned>(field.type),
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz));
      throw Exceptions::SecurityException;
    }
  } else {
    field.type = FieldType::FIELD_TYPE_OBJECT;
  }
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);
    std::printf("CHOUPI:PUTFIELD_A phase=store tag_len=%u index=%u\r\n",
                static_cast<unsigned>(tag.len),
                static_cast<unsigned>(index));
    try {
      FlashMemory_Handler::setPersistentField_Reference(tag, ref,
                                                        this->getOwner());
      if (this->packageID == 20 && this->claz == 121) {
        std::printf("CHOUPI:PUTFIELD_A phase=stored index=%u\r\n",
                    static_cast<unsigned>(index));
        std::fflush(stdout);
      }
    } catch (Exceptions exception) {
      std::printf("CHOUPI:PUTFIELD_A phase=store-fail id=%u tag_len=%u index=%u\r\n",
                  static_cast<unsigned>(exception),
                  static_cast<unsigned>(tag.len),
                  static_cast<unsigned>(index));
      throw;
    }
  } else {

#ifdef JCVM_TYPED_HEAP
    // TODO: the instance data value type should be saved.
#endif /* JCVM_TYPED_HEAP */

  }
}

/**
 * Get the number of instance field.
 *
 * @return the number of instance field.
 */
auto JC_Instance::getNumberOfFields() const noexcept
    -> decltype(fields->size()) {
  if (this->isPersistent()) {
    Class_Handler class_handler(this->packageID);
    return class_handler.getInstanceFieldsSize(this->claz);
  } else {
    return this->fields->size();
  }
}

///  Get fields arrays
/**
 * Get the number of instance field.
 *
 * @return the number of instance field.
 */
auto JC_Instance::getFields() const -> decltype(fields) {
  if (!this->isPersistent() || this->persistent_fields_loaded) {
    return this->fields;
  }

  static uint16_t field_load_trace_count = 0;
  for (uint16_t index = 0; index < this->fields->size(); index++) {
    const fs::Tag field_tag = FlashMemory_Handler::computeTag(
        this->recomputeOriginalTag(), index);
    jc_field_t &field = this->fields->at(index);
    const bool trace = field_load_trace_count < 64;
    if (trace) {
      std::printf("CHOUPI:FIELD_LOAD exists:start package=%u class=%u index=%u count=%u tag_len=%u\r\n",
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz),
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(this->fields->size()),
                  static_cast<unsigned>(field_tag.len));
      std::fflush(stdout);
    }
    // Persistent fields with no record have never been assigned. Match the
    // reference getter's missing-tag behavior instead of asking the type
    // reader to turn the expected absence into an IOException.
    const bool field_exists = fs_exists(field_tag.value, field_tag.len) != 0;
    if (trace) {
      std::printf("CHOUPI:FIELD_LOAD exists:done package=%u class=%u index=%u exists=%u\r\n",
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz),
                  static_cast<unsigned>(index),
                  field_exists ? 1u : 0u);
      std::fflush(stdout);
    }
    if (!field_exists) {
      field.type = FieldType::FIELD_TYPE_UNINITIALIZED;
      field.value = 0;
      ++field_load_trace_count;
      continue;
    }
    field.type = FlashMemory_Handler::getPersistentField_Type(field_tag);
    if (trace) {
      std::printf("CHOUPI:FIELD_LOAD type package=%u class=%u index=%u type=%u\r\n",
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz),
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(field.type));
      std::fflush(stdout);
    }

    switch (field.type) {
    case FieldType::FIELD_TYPE_BYTE:
    case FieldType::FIELD_TYPE_BOOLEAN:
      field.value = static_cast<jword_t>(
          BYTE_TO_WORD(FlashMemory_Handler::getPersistentField_Byte(field_tag)));
      break;
    case FieldType::FIELD_TYPE_SHORT:
      field.value = static_cast<jword_t>(
          FlashMemory_Handler::getPersistentField_Short(field_tag));
      break;
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_INT: {
      const jint_t value = FlashMemory_Handler::getPersistentField_Int(field_tag);
      field.value = static_cast<jword_t>(INT_2_MSSHORTS(value));
      if (index + 1 >= this->fields->size()) {
        throw Exceptions::SecurityException;
      }
      this->fields->at(++index).type = FieldType::FIELD_TYPE_UNINITIALIZED;
      this->fields->at(index).value =
          static_cast<jword_t>(INT_2_LSSHORTS(value));
      break;
    }
#endif /* JCVM_INT_SUPPORTED */
    case FieldType::FIELD_TYPE_OBJECT:
    case FieldType::FIELD_TYPE_ARRAY_BYTE:
    case FieldType::FIELD_TYPE_ARRAY_BOOLEAN:
    case FieldType::FIELD_TYPE_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_ARRAY_INT:
#endif /* JCVM_INT_SUPPORTED */
    case FieldType::FIELD_TYPE_ARRAY_OBJECT:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT:
#endif /* JCVM_INT_SUPPORTED */
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT:
    case FieldType::FIELD_TYPE_UNINITIALIZED:
      field.value = static_cast<jword_t>(
          FlashMemory_Handler::getPersistentField_Reference(
              field_tag, const_cast<JC_Instance *>(this)->getOwner())
              .compact());
      break;
    default:
      std::printf("CHOUPI:PERSIST_INVALID loaded-field-type=%u index=%u package=%u class=%u\r\n",
                  static_cast<unsigned>(field.type),
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz));
      throw Exceptions::SecurityException;
    }
    if (trace) {
      std::printf("CHOUPI:FIELD_LOAD value:done package=%u class=%u index=%u\r\n",
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz),
                  static_cast<unsigned>(index));
      std::fflush(stdout);
    }
    ++field_load_trace_count;
  }
  this->persistent_fields_loaded = true;
  return this->fields;
}

} // namespace jcvm
