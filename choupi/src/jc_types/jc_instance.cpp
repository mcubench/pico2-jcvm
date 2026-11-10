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
#include "../context.hpp"
#include "ffi.h"
#include "../../../src/vm_diagnostics.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace jcvm {

namespace {

void initialize_field_slots(JCVMArray<jc_field_t> *fields) noexcept {
  if (fields == nullptr) return;
  for (uint16_t index = 0; index < fields->size(); ++index) {
    fields->at(index).type = FieldType::FIELD_TYPE_UNINITIALIZED;
    fields->at(index).reference_owner = 0xff;
    fields->at(index).value = 0;
  }
}

bool is_reference_field_type(const FieldType type) noexcept {
  switch (type) {
  case FieldType::FIELD_TYPE_OBJECT:
  case FieldType::FIELD_TYPE_GLOBAL_AID_OBJECT:
  case FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT:
  case FieldType::FIELD_TYPE_ARRAY_BYTE:
  case FieldType::FIELD_TYPE_ARRAY_BOOLEAN:
  case FieldType::FIELD_TYPE_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
  case FieldType::FIELD_TYPE_ARRAY_INT:
#endif
  case FieldType::FIELD_TYPE_ARRAY_OBJECT:
  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE:
  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN:
  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT:
#endif
  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT:
    return true;
  default:
    return false;
  }
}

void require_field_type(const FieldType actual,
                        const FieldType expected) {
  if (actual == FieldType::FIELD_TYPE_UNINITIALIZED) return;
  if (expected == FieldType::FIELD_TYPE_OBJECT) {
    if (is_reference_field_type(actual)) return;
  } else if (actual == expected ||
             (expected == FieldType::FIELD_TYPE_BYTE &&
              actual == FieldType::FIELD_TYPE_BOOLEAN)) {
    return;
  }
  throw Exceptions::SecurityException;
}

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
    if (length >= 6 && (length & 1u) == 0) return 6;
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
                         const jc_cp_offset_t instantiated_class)
    : JC_Object(owner, false) {
  ConstantPool_Handler cp(package_owner);
  std::pair<jpackage_ID_t, jclass_index_t> pair =
      cp.getClassInformation(instantiated_class);

  this->packageID = pair.first;
  this->claz = pair.second;

  Class_Handler class_handler(this->packageID);
  uint16_t fields_size = class_handler.getInstanceFieldsSize(this->claz);
  this->fields = new JCVMArray<jc_field_t>(fields_size);
  initialize_field_slots(this->fields);
}

/**
 * Default constructor.
 *
 * @param[owner] owner
 * @param[packageID] class package ID
 * @param[claz] class index
 */
JC_Instance::JC_Instance(Heap &owner, const jpackage_ID_t packageID,
                         const jclass_index_t claz)
    : JC_Object(owner, false), packageID(packageID), claz(claz) {
#ifndef PC_VERSION
  choupi_install_stage(4);
  choupi_install_origin((static_cast<uint32_t>(this->packageID) << 16) |
                        this->claz);
#endif
  Class_Handler class_handler(this->packageID);
  uint16_t fields_size = class_handler.getInstanceFieldsSize(this->claz);
#ifndef PC_VERSION
  choupi_install_stage(8);
#endif
  this->fields = new JCVMArray<jc_field_t>(fields_size);
  initialize_field_slots(this->fields);
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
                         const fs::Tag &tag,
                         const bool missing_package_proxy)
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
  std::unique_ptr<uint8_t[]> pending_tag_overflow;
  if (stored_length > sizeof(this->persistent_tag_value)) {
    pending_tag_overflow = std::make_unique<uint8_t[]>(stored_length);
    stored_tag = pending_tag_overflow.get();
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

  if (missing_package_proxy) {
    // Retained SIO proxies can outlive DELETE of their server package. Keep a
    // lightweight stale-reference object so the owning applet remains
    // restorable; routing checks fail closed before class metadata or object
    // fields are consulted.
    this->setShareableProxyPlaceholder();
    this->fields = new JCVMArray<jc_field_t>(0);
    this->persistent_fields_loaded = true;
    this->persistent_tag_overflow = pending_tag_overflow.release();
    return;
  }

  Class_Handler class_handler(this->packageID);
  const uint16_t fields_size =
      class_handler.getInstanceFieldsSize(this->claz);
  std::unique_ptr<JCVMArray<jc_field_t>> pending_fields(
      new JCVMArray<jc_field_t>(fields_size));
  this->persistent_tag_overflow = pending_tag_overflow.release();
  this->fields = pending_fields.release();
  this->persistent_fields_loaded = false;
}

JC_Instance::JC_Instance(JC_Instance &&other) noexcept
    : JC_Object(other.getOwner(), other.isPersistent(),
                other.isShareableProxyPlaceholder()),
      packageID(other.packageID), claz(other.claz),
      persistent_tag_length(other.persistent_tag_length),
      persistent_tag_compressed(other.persistent_tag_compressed),
      persistent_tag_overflow(other.persistent_tag_overflow),
      has_persistent_tag(other.has_persistent_tag),
      persistent_fields_loaded(other.persistent_fields_loaded),
      persistent_rollback_generation(other.persistent_rollback_generation),
      fields(other.fields),
      native_session_fields(std::move(other.native_session_fields)) {
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

jbyte_t JC_Instance::getNativeSessionField_Byte(
    const uint16_t index) const noexcept {
  if (!this->native_session_fields ||
      index >= this->native_session_fields->size()) {
    return 0;
  }
  return static_cast<jbyte_t>((*this->native_session_fields)[index].value);
}

jshort_t JC_Instance::getNativeSessionField_Short(
    const uint16_t index) const noexcept {
  if (!this->native_session_fields ||
      index >= this->native_session_fields->size()) {
    return 0;
  }
  return signedShortFromBits((*this->native_session_fields)[index].value);
}

void JC_Instance::setNativeSessionField_Byte(const uint16_t index,
                                              const jbyte_t value) {
  if (index >= this->fields->size()) throw Exceptions::ArrayIndexOutOfBoundsException;
  if (!this->native_session_fields) {
    this->native_session_fields =
        std::make_unique<std::vector<jc_field_t>>(this->fields->size());
  }
  jc_field_t &field = this->native_session_fields->at(index);
  field.type = FIELD_TYPE_BYTE;
  field.value = static_cast<jword_t>(BYTE_TO_WORD(value));
}

void JC_Instance::setNativeSessionField_Short(const uint16_t index,
                                               const jshort_t value) {
  if (index >= this->fields->size()) throw Exceptions::ArrayIndexOutOfBoundsException;
  if (!this->native_session_fields) {
    this->native_session_fields =
        std::make_unique<std::vector<jc_field_t>>(this->fields->size());
  }
  jc_field_t &field = this->native_session_fields->at(index);
  field.type = FIELD_TYPE_SHORT;
  field.value = static_cast<jword_t>(value);
}

void JC_Instance::clearNativeSessionFields() noexcept {
  this->native_session_fields.reset();
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
  this->storePersistentTag(tag);
}

void JC_Instance::retargetPersistentTag(const fs::Tag &tag) {
  this->storePersistentTag(tag);
}

void JC_Instance::clearPersistentTag() noexcept {
  delete[] this->persistent_tag_overflow;
  this->persistent_tag_overflow = nullptr;
  this->persistent_tag_length = 0;
  this->persistent_tag_compressed = false;
  this->has_persistent_tag = false;
  this->setPersistent(false);
}

void JC_Instance::invalidatePersistentFields() noexcept {
  if (this->isPersistent()) {
    this->persistent_fields_loaded = false;
  }
}

void JC_Instance::storePersistentTag(const fs::Tag &tag) {
  if (tag.len == 0 || tag.len > sizeof(tag.value)) {
    throw Exceptions::SecurityException;
  }
  const uint8_t root_length = persistent_tag_root_length(tag.value[0], tag.len);
  bool compressed = root_length != 0 &&
                    ((tag.len - root_length) & 1) == 0 &&
                    tag.len > root_length;
  for (uint8_t idx = root_length; compressed && idx < tag.len; idx += 2) {
    if (tag.value[idx] != tag.value[idx + 1]) {
      compressed = false;
    }
  }
  const uint8_t stored_length =
      compressed
          ? static_cast<uint8_t>(root_length + (tag.len - root_length) / 2)
          : tag.len;
  uint8_t inline_tag[sizeof(this->persistent_tag_value)]{};
  std::unique_ptr<uint8_t[]> overflow_tag;
  uint8_t *stored_tag = inline_tag;
  if (stored_length > sizeof(this->persistent_tag_value)) {
    overflow_tag = std::make_unique<uint8_t[]>(stored_length);
    stored_tag = overflow_tag.get();
  }
  if (compressed) {
    std::memcpy(stored_tag, tag.value, root_length);
    uint8_t output = root_length;
    for (uint8_t idx = root_length; idx < tag.len; idx += 2) {
      stored_tag[output++] = tag.value[idx];
    }
  } else {
    std::memcpy(stored_tag, tag.value, tag.len);
  }
  delete[] this->persistent_tag_overflow;
  this->persistent_tag_overflow = overflow_tag.release();
  std::memcpy(this->persistent_tag_value, inline_tag,
              sizeof(this->persistent_tag_value));
  if (this->persistent_tag_overflow != nullptr) {
    std::memcpy(this->persistent_tag_overflow, stored_tag, stored_length);
  }
  this->persistent_tag_length = tag.len;
  this->persistent_tag_compressed = compressed;
  this->has_persistent_tag = true;
  if (this->persistent_fields_loaded) {
    this->persistent_rollback_generation =
        FlashMemory_Handler::persistentWriteRollbackGeneration();
  }
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
    const auto fields = this->getFields();
    const jc_field_t &field = fields->at(index);
    require_field_type(field.type, FieldType::FIELD_TYPE_BYTE);
    return static_cast<jbyte_t>(field.value);
  } else {
    const jc_field_t &field = this->fields->at(index);
    require_field_type(field.type, FieldType::FIELD_TYPE_BYTE);
    return static_cast<jbyte_t>(field.value);
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
    const auto fields = this->getFields();
    const jc_field_t &field = fields->at(index);
    require_field_type(field.type, FieldType::FIELD_TYPE_SHORT);
    return signedShortFromBits(field.value);
  } else {
    const jc_field_t &field = this->fields->at(index);
    require_field_type(field.type, FieldType::FIELD_TYPE_SHORT);
    return signedShortFromBits(field.value);
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
    const auto fields = this->getFields();
    if (static_cast<std::size_t>(index) + 1 >= fields->size()) {
      throw Exceptions::SecurityException;
    }
    const jc_field_t &high = fields->at(index);
    const jc_field_t &low = fields->at(static_cast<uint16_t>(index + 1));
    if (high.type == FieldType::FIELD_TYPE_UNINITIALIZED &&
        low.type == FieldType::FIELD_TYPE_UNINITIALIZED) {
      return 0;
    }
    if (high.type != FieldType::FIELD_TYPE_INT ||
        low.type != FieldType::FIELD_TYPE_INT_LOW) {
      throw Exceptions::SecurityException;
    }
    const jword_t field_high_part = high.value;
    const jword_t field_low_part = low.value;
    return signedIntFromBits(SHORTS_TO_INT(field_high_part, field_low_part));
  } else {
    if (static_cast<std::size_t>(index) + 1 >= this->fields->size()) {
      throw Exceptions::SecurityException;
    }
    const jc_field_t &high = this->fields->at(index);
    const jc_field_t &low = this->fields->at(static_cast<uint16_t>(index + 1));
    if (high.type == FieldType::FIELD_TYPE_UNINITIALIZED &&
        low.type == FieldType::FIELD_TYPE_UNINITIALIZED) {
      return 0;
    }
    if (high.type != FieldType::FIELD_TYPE_INT ||
        low.type != FieldType::FIELD_TYPE_INT_LOW) {
      throw Exceptions::SecurityException;
    }
    const jword_t field_high_part = high.value;
    const jword_t field_low_part = low.value;

    return signedIntFromBits(SHORTS_TO_INT(field_high_part, field_low_part));
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
  const auto value = this->getField_ReferenceWithOwner(index);
  if (value.second == 0xff || value.second == this->packageID ||
      value.second == 0xfe) {
    return value.first;
  }
  // Native API implementations use this legacy accessor and have no owner
  // side channel. Keep them fail-closed if an applet field holds an opaque
  // reference from another package.
  throw Exceptions::SecurityException;
}

std::pair<jref_t, uint8_t> JC_Instance::getField_ReferenceWithOwner(
    const uint16_t index) {
  if (this->isPersistent()) {
    // Persistent references are restored into the instance's field cache on
    // first access. Re-reading them from flash on every GETFIELD_A allocates a
    // fresh heap wrapper each time; transient arrays stored by crypto objects
    // then leak one backing allocation per operation and lose object identity.
    const auto fields = this->getFields();
    const jc_field_t &field = fields->at(index);
    require_field_type(field.type, FieldType::FIELD_TYPE_OBJECT);
    return {jref_t(field.value), field.reference_owner};
  } else {
    const jc_field_t &field = this->fields->at(index);
    require_field_type(field.type, FieldType::FIELD_TYPE_OBJECT);
    return {jref_t(field.value), field.reference_owner};
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
  jc_field_t &field = this->fields->at(index);
  require_field_type(field.type, FieldType::FIELD_TYPE_BYTE);
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    FlashMemory_Handler::setPersistentField_Byte(tag, value);
  }
  field.type = FieldType::FIELD_TYPE_BYTE;
  field.value = static_cast<jword_t>(BYTE_TO_WORD(value));
  if (!this->isPersistent()) {

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
  jc_field_t &field = this->fields->at(index);
  require_field_type(field.type, FieldType::FIELD_TYPE_SHORT);
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    FlashMemory_Handler::setPersistentField_Short(tag, value);
  }
  field.type = FieldType::FIELD_TYPE_SHORT;
  field.value = static_cast<jword_t>(value);
  if (!this->isPersistent()) {

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
  jc_field_t &field = this->fields->at(index);
  require_field_type(field.type, FieldType::FIELD_TYPE_INT);
  if (static_cast<std::size_t>(index) + 1 >= this->fields->size()) {
    throw Exceptions::SecurityException;
  }
  jc_field_t &low = this->fields->at(static_cast<uint16_t>(index + 1));
  if (low.type != FieldType::FIELD_TYPE_UNINITIALIZED &&
      low.type != FieldType::FIELD_TYPE_INT_LOW) {
    throw Exceptions::SecurityException;
  }
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);

    FlashMemory_Handler::setPersistentField_Int(tag, value);
  }
  field.type = FieldType::FIELD_TYPE_INT;
  field.value = static_cast<jword_t>(INT_2_MSSHORTS(value));
  low.type = FieldType::FIELD_TYPE_INT_LOW;
  low.value = static_cast<jword_t>(INT_2_LSSHORTS(value));
  if (!this->isPersistent()) {

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
void JC_Instance::setField_Reference(const uint16_t index, const jref_t ref,
                                     uint8_t reference_owner) {
  if (ref.isGlobalArray() && !this->getOwner().isGlobalHeap()) {
    throw Exceptions::SecurityException;
  }
  if (ref.isNullPointer()) {
    reference_owner = 0xff;
  } else if (ref.isGlobalArray() || ref.isGlobalInstance()) {
    if (reference_owner != 0xff && reference_owner != 0xfe) {
      throw Exceptions::SecurityException;
    }
    reference_owner = 0xfe;
  } else if (reference_owner == 0xfe) {
    throw Exceptions::SecurityException;
  }
  const jpackage_ID_t effective_owner =
      reference_owner == 0xff ? this->packageID : reference_owner;
  const bool foreign_reference =
      !ref.isNullPointer() && !ref.isGlobalArray() &&
      !ref.isGlobalInstance() && effective_owner != this->packageID;
  if (foreign_reference && this->isPersistent()) {
    // The durable field encoding has no package provenance. Preserve the
    // existing fail-closed behavior until cross-package serialization and
    // restore have a stable identity format.
    throw Exceptions::SecurityException;
  }
  const auto record_field_write = [&](uint8_t stage, uint8_t exception_id) {
#ifndef PC_VERSION
    choupi_vm_record_field_write(
        stage, this->packageID, this->claz, index, ref.compact(),
        static_cast<uint16_t>(this->fields->size()),
        static_cast<uint8_t>((this->isPersistent() ? 1u : 0u) |
                             (ref.isArray() ? 2u : 0u)),
        exception_id);
#else
    (void)stage;
    (void)exception_id;
#endif
  };
  record_field_write(1, 0);
  if (this->isPersistent()) {
    try {
      (void)this->getFields();
    } catch (Exceptions exception) {
      record_field_write(10, static_cast<uint8_t>(exception));
      throw;
    }
  }
  record_field_write(2, 0);
#ifdef JCVM_ARRAY_SIZE_CHECK
  if (index >= this->fields->size()) {
#ifndef PC_VERSION
    choupi_vm_record_field_bounds(
        index, static_cast<uint16_t>(this->fields->size()), this->packageID,
        this->claz, ref.compact());
#endif
  }
#endif
  jc_field_t &field = this->fields->at(index);
  require_field_type(field.type, FieldType::FIELD_TYPE_OBJECT);
  record_field_write(3, 0);
  FieldType new_type = FieldType::FIELD_TYPE_OBJECT;
  if (ref.isNullPointer()) {
    new_type = FieldType::FIELD_TYPE_UNINITIALIZED;
  } else if (ref.isArray()) {
    std::shared_ptr<JC_Array> array;
    try {
      if (foreign_reference) {
        Context *owner_context =
            Context::findRetainedPackageContext(effective_owner);
        if (owner_context == nullptr) throw Exceptions::SecurityException;
        array = owner_context->getHeap().getArray(ref);
      } else if (ref.isGlobalArray()) {
        array = Heap::getGlobalArray(ref);
      } else {
        array = this->getOwner().getArray(ref);
      }
    } catch (Exceptions exception) {
      record_field_write(11, static_cast<uint8_t>(exception));
      throw;
    }
    switch (array->getType()) {
    case JAVA_ARRAY_T_BYTE:
      new_type = array->isTransientArray()
                     ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE
                     : FieldType::FIELD_TYPE_ARRAY_BYTE;
      break;
    case JAVA_ARRAY_T_BOOLEAN:
      new_type = array->isTransientArray()
                     ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN
                     : FieldType::FIELD_TYPE_ARRAY_BOOLEAN;
      break;
    case JAVA_ARRAY_T_SHORT:
      new_type = array->isTransientArray()
                     ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT
                     : FieldType::FIELD_TYPE_ARRAY_SHORT;
      break;
#ifdef JCVM_INT_SUPPORTED
    case JAVA_ARRAY_T_INT:
      new_type = array->isTransientArray()
                     ? FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT
                     : FieldType::FIELD_TYPE_ARRAY_INT;
      break;
#endif /* JCVM_INT_SUPPORTED */
    case JAVA_ARRAY_T_REFERENCE:
      new_type = array->isTransientArray()
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
  } else if (!ref.isGlobalInstance()) {
    try {
      if (foreign_reference) {
        Context *owner_context =
            Context::findRetainedPackageContext(effective_owner);
        if (owner_context == nullptr) throw Exceptions::SecurityException;
        (void)owner_context->getHeap().getInstance(ref);
      } else {
        (void)this->getOwner().getInstance(ref);
      }
    } catch (Exceptions exception) {
      record_field_write(11, static_cast<uint8_t>(exception));
      throw;
    }
  } else {
    (void)Heap::getGlobalInstance(ref);
  }
  if (foreign_reference &&
      !this->getOwner().registerForeignReferenceInstance()) {
    throw Exceptions::SecurityException;
  }
  if (this->isPersistent()) {
    fs::Tag tag =
        FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);
    try {
      FlashMemory_Handler::setPersistentField_Reference(tag, ref,
                                                        this->getOwner());
      record_field_write(4, 0);
    } catch (Exceptions exception) {
      record_field_write(12, static_cast<uint8_t>(exception));
      throw;
    }
  }

  field.value = static_cast<jword_t>(ref.compact());
  field.type = new_type;
  field.reference_owner = foreign_reference
                              ? static_cast<uint8_t>(effective_owner)
                              : (reference_owner == 0xfe ? 0xfe : 0xff);
  if (!this->isPersistent()) {

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

bool JC_Instance::hasLoadedFields() const noexcept {
  if (!this->isPersistent()) {
    return true;
  }
  return this->persistent_fields_loaded &&
         this->persistent_rollback_generation ==
             FlashMemory_Handler::persistentWriteRollbackGeneration();
}

bool JC_Instance::hasForeignReferenceFields() const noexcept {
  if (this->isPersistent() || this->fields == nullptr) return false;
  for (uint16_t index = 0; index < this->fields->size(); ++index) {
    const jc_field_t &field = this->fields->at(index);
    if (field.reference_owner != 0xff && field.reference_owner != 0xfe &&
        !jref_t(field.value).isNullPointer()) {
      return true;
    }
  }
  return false;
}

void JC_Instance::visitForeignReferencesForPackage(
    const jpackage_ID_t package_id,
    void (*visitor)(jref_t, void *), void *visitor_context) {
  if (visitor == nullptr || this->isPersistent() || this->fields == nullptr) {
    return;
  }
  for (uint16_t index = 0; index < this->fields->size(); ++index) {
    const jc_field_t &field = this->fields->at(index);
    if (field.reference_owner != package_id) continue;
    const jref_t reference(field.value);
    if (!reference.isNullPointer()) visitor(reference, visitor_context);
  }
}

void JC_Instance::clearForeignReferencesForPackage(
    const jpackage_ID_t package_id) noexcept {
  if (this->isPersistent() || this->fields == nullptr) return;
  for (uint16_t index = 0; index < this->fields->size(); ++index) {
    jc_field_t &field = this->fields->at(index);
    if (field.reference_owner != package_id) continue;
    field.type = FieldType::FIELD_TYPE_UNINITIALIZED;
    field.value = 0;
    field.reference_owner = 0xff;
  }
}

///  Get fields arrays
/**
 * Get the number of instance field.
 *
 * @return the number of instance field.
 */
auto JC_Instance::getFields() const -> decltype(fields) {
  const uint32_t rollback_generation =
      FlashMemory_Handler::persistentWriteRollbackGeneration();
  if (this->persistent_fields_loaded &&
      this->persistent_rollback_generation != rollback_generation) {
    this->persistent_fields_loaded = false;
  }
  if (!this->isPersistent() || this->persistent_fields_loaded) {
    return this->fields;
  }

  for (uint16_t index = 0; index < this->fields->size(); index++) {
    const fs::Tag field_tag = FlashMemory_Handler::computeTag(
        this->recomputeOriginalTag(), index);
    jc_field_t &field = this->fields->at(index);
    // Persistent fields with no record have never been assigned. Match the
    // reference getter's missing-tag behavior instead of asking the type
    // reader to turn the expected absence into an IOException.
    const bool field_exists = fs_exists(field_tag.value, field_tag.len) != 0;
    if (!field_exists) {
      field.type = FieldType::FIELD_TYPE_UNINITIALIZED;
      field.value = 0;
      continue;
    }
    try {
      field.type = FlashMemory_Handler::getPersistentField_Type(field_tag);
    } catch (Exceptions exception) {
#ifndef PC_VERSION
      choupi_vm_record_persistent_restore_failure(
          this->packageID, this->claz, index,
          static_cast<uint8_t>(field.type), 1,
          static_cast<uint8_t>(exception));
      std::printf("CHOUPI:RESTORE_FIELD_TAG package=%u class=%04X field=%u "
                  "exists=%u tag=",
                  static_cast<unsigned>(this->packageID),
                  static_cast<unsigned>(this->claz),
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(fs_exists(field_tag.value,
                                                  field_tag.len)));
      for (uint8_t tag_index = 0; tag_index < field_tag.len; ++tag_index) {
        std::printf("%02X", field_tag.value[tag_index]);
      }
      std::printf(" exception=%u\r\n", static_cast<unsigned>(exception));
#else
      std::fprintf(stderr, "CHOUPI:RESTORE_FIELD_TAG package=%u class=%04X "
                           "field=%u exists=%u tag=",
                   static_cast<unsigned>(this->packageID),
                   static_cast<unsigned>(this->claz),
                   static_cast<unsigned>(index),
                   static_cast<unsigned>(fs_exists(field_tag.value,
                                                   field_tag.len)));
      for (uint8_t tag_index = 0; tag_index < field_tag.len; ++tag_index) {
        std::fprintf(stderr, "%02X", field_tag.value[tag_index]);
      }
      std::fprintf(stderr, " exception=%u\n",
                   static_cast<unsigned>(exception));
#endif
      throw;
    }

    try {
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
      this->fields->at(++index).type = FieldType::FIELD_TYPE_INT_LOW;
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
      try {
        field.value = static_cast<jword_t>(
            FlashMemory_Handler::getPersistentField_Reference(
                field_tag, const_cast<JC_Instance *>(this)->getOwner())
                .compact());
      } catch (Exceptions exception) {
#ifndef PC_VERSION
        choupi_vm_record_persistent_restore_failure(
            this->packageID, this->claz, index,
            static_cast<uint8_t>(field.type), 2,
            static_cast<uint8_t>(exception));
#endif
        throw;
      }
      break;
      default:
        std::printf("CHOUPI:PERSIST_INVALID loaded-field-type=%u index=%u package=%u class=%u\r\n",
                    static_cast<unsigned>(field.type),
                    static_cast<unsigned>(index),
                    static_cast<unsigned>(this->packageID),
                    static_cast<unsigned>(this->claz));
        throw Exceptions::SecurityException;
      }
    } catch (Exceptions exception) {
#ifdef PC_VERSION
      std::fprintf(stderr,
                   "CHOUPI:RESTORE_FIELD_VALUE package=%u class=%04X "
                   "field=%u type=%u exception=%u tag=",
                   static_cast<unsigned>(this->packageID),
                   static_cast<unsigned>(this->claz),
                   static_cast<unsigned>(index),
                   static_cast<unsigned>(field.type),
                   static_cast<unsigned>(exception));
      for (uint8_t tag_index = 0; tag_index < field_tag.len; ++tag_index) {
        std::fprintf(stderr, "%02X", field_tag.value[tag_index]);
      }
      std::fprintf(stderr, "\n");
#endif
      throw;
    }
  }
  this->persistent_rollback_generation =
      FlashMemory_Handler::persistentWriteRollbackGeneration();
  this->persistent_fields_loaded = true;
  return this->fields;
}

} // namespace jcvm
