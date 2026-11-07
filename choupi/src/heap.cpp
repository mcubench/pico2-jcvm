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

#include "heap.hpp"
#include "context.hpp"
#include "ffi.h"
#include "jc_handlers/jc_cp.hpp"
#include "jc_types/jc_array.hpp"
#include "../../src/vm_diagnostics.h"

#include <cstring>
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
#include <cstdio>
#endif

#include <algorithm>
#include <utility>

namespace jcvm {

namespace {
constexpr uint16_t kMaximumLocalArrayHandles = 0x3fff;

struct VectorRootContext {
  const std::vector<jref_t> *roots;
};

Heap &globalArrayHeap() {
  static Heap heap(0, true);
  return heap;
}

void appendRootToVector(const jref_t reference, void *opaque) {
  static_cast<std::vector<jref_t> *>(opaque)->push_back(reference);
}

bool enumerateVectorRoots(Heap::ReferenceVisitor visitor,
                          void *visitor_context, void *root_context) {
  const auto &roots = *static_cast<VectorRootContext *>(root_context)->roots;
  for (const jref_t &root : roots) {
    visitor(root, visitor_context);
  }
  return true;
}

} // namespace

/**
 * Default constructor.
 *
 * @param[owner] Applet ID owner
 */
Heap::Heap(const japplet_ID_t owner, const bool global_heap)
    : owner(owner), global_heap(global_heap),
      shared_storage(std::make_shared<SharedStorage>()) {}

Heap::Heap(const japplet_ID_t owner, const Heap &shared_heap)
    : owner(owner), global_heap(false),
      shared_storage(shared_heap.shared_storage) {}

std::vector<std::weak_ptr<Heap::SharedStorage>> &
Heap::foreignReferenceStorageRegistry() noexcept {
  static std::vector<std::weak_ptr<SharedStorage>> registry;
  return registry;
}

/*
 * Getting the applet ID owner
 *
 * @return heap owner.
 */
japplet_ID_t Heap::getOwner() const { return this->owner; }

bool Heap::isGlobalHeap() const noexcept { return this->global_heap; }

bool Heap::hasLiveObjects() const noexcept {
  for (const auto &array : this->shared_storage->arrays) {
    if (array && &array->getOwner() == this) return true;
  }
  for (const auto &instance : this->shared_storage->instances) {
    if (instance && &instance->getOwner() == this) return true;
  }
  return false;
}

jref_t Heap::addGlobalArray(const uint16_t length, const jc_array_type type) {
  Heap &heap = globalArrayHeap();
  constexpr ClearEvent clear_on_reset = ClearEvent::CLEAR_ON_RESET;
  std::shared_ptr<JC_Array> array;
  if (type == JAVA_ARRAY_T_REFERENCE) {
    // A global Object[] has no applet-specific component class. Keep its
    // component type wildcarded, while giving it the required reset lifetime.
    array = std::make_shared<JC_Array>(
        heap, length, type, static_cast<jc_cp_offset_t>(0xffff), true,
        clear_on_reset);
  } else {
    array = std::make_shared<JC_Array>(heap, length, type, true,
                                       clear_on_reset);
  }

  uint16_t slot = 0;
  for (auto &existing : heap.shared_storage->arrays) {
    if (!existing) {
      existing = std::move(array);
      break;
    }
    ++slot;
  }
  if (slot == heap.shared_storage->arrays.size()) {
    if (heap.shared_storage->arrays.size() >= kMaximumLocalArrayHandles) {
      throw Exceptions::RuntimeException;
    }
    heap.shared_storage->arrays.push_back(std::move(array));
  }

  jref_t reference;
  reference.setAsArray(true);
  reference.setOffset(static_cast<uint16_t>(slot + 1));
  reference.setAsGlobalArray();
  return reference;
}

jref_t Heap::addGlobalRetainedArray(const uint16_t length,
                                    const jc_array_type type) {
  Heap &heap = globalArrayHeap();
  std::shared_ptr<JC_Array> array;
  if (type == JAVA_ARRAY_T_REFERENCE) {
    array = std::make_shared<JC_Array>(
        heap, length, type, static_cast<jc_cp_offset_t>(0xffff), false,
        ClearEvent::None);
  } else {
    array = std::make_shared<JC_Array>(heap, length, type, false,
                                       ClearEvent::None);
  }
  uint16_t slot = 0;
  for (auto &existing : heap.shared_storage->arrays) {
    if (!existing) {
      existing = std::move(array);
      break;
    }
    ++slot;
  }
  if (slot == heap.shared_storage->arrays.size()) {
    if (heap.shared_storage->arrays.size() >= kMaximumLocalArrayHandles) {
      throw Exceptions::RuntimeException;
    }
    heap.shared_storage->arrays.push_back(std::move(array));
  }
  jref_t reference;
  reference.setOffset(static_cast<uint16_t>((slot + 1) | 0x4000u));
  reference.setAsGlobalArray();
  return reference;
}

std::shared_ptr<JC_Array> Heap::getGlobalArray(const jref_t reference) {
  if (!reference.isGlobalArray()) {
    throw Exceptions::SecurityException;
  }
  const uint16_t index = reference.getGlobalArrayIndex();
  if (index == 0 || index > globalArrayHeap().shared_storage->arrays.size() ||
      !globalArrayHeap().shared_storage->arrays.at(static_cast<uint16_t>(index - 1))) {
    throw Exceptions::SecurityException;
  }
  return globalArrayHeap().shared_storage->arrays.at(index - 1);
}

void Heap::releaseGlobalArray(const jref_t reference) noexcept {
  if (!reference.isGlobalArray()) {
    return;
  }
  const uint16_t index = reference.getGlobalArrayIndex();
  Heap &heap = globalArrayHeap();
  if (index == 0 || index > heap.shared_storage->arrays.size()) {
    return;
  }
  heap.shared_storage->arrays.at(static_cast<uint16_t>(index - 1)).reset();
}

void Heap::clearGlobalArrays(const ClearEvent event) {
  if (event != ClearEvent::CLEAR_ON_RESET) {
    return;
  }
  for (const auto &array : globalArrayHeap().shared_storage->arrays) {
    if (array && array->isTransientArray() &&
        array->getClearEvent() == event) {
      array->clearTransientData();
    }
  }
}

uint32_t Heap::reclaimTransientGlobalArrays() noexcept {
  uint32_t reclaimed = 0;
  for (auto &array : globalArrayHeap().shared_storage->arrays) {
    if (array && array->isTransientArray()) {
      array.reset();
      ++reclaimed;
    }
  }
  return reclaimed;
}

void Heap::visitGlobalArrayReferencesForPackage(
    const jpackage_ID_t package_id, const ReferenceVisitor visitor,
    void *visitor_context) {
  if (visitor == nullptr) return;
  for (const auto &array : globalArrayHeap().shared_storage->arrays) {
    if (array) {
      array->visitGlobalReferences(package_id, visitor, visitor_context);
    }
  }
}

void Heap::clearGlobalArrayReferencesForPackage(
    const jpackage_ID_t package_id) noexcept {
  for (const auto &array : globalArrayHeap().shared_storage->arrays) {
    if (array) array->clearGlobalReferences(package_id);
  }
}

bool Heap::registerForeignReferenceArray(JC_Array *array) noexcept {
  if (array == nullptr || this->global_heap) return false;
  auto &arrays = this->shared_storage->foreign_reference_arrays;
  if (std::find(arrays.begin(), arrays.end(), array) != arrays.end()) {
    return true;
  }

  auto &registry = foreignReferenceStorageRegistry();
  bool storage_registered = false;
  for (auto it = registry.begin(); it != registry.end();) {
    const auto storage = it->lock();
    if (!storage) {
      it = registry.erase(it);
      continue;
    }
    storage_registered = storage_registered ||
                         storage.get() == this->shared_storage.get();
    ++it;
  }

  bool array_added = false;
  try {
    arrays.push_back(array);
    array_added = true;
    if (!storage_registered) registry.push_back(this->shared_storage);
  } catch (...) {
    if (array_added) arrays.pop_back();
    return false;
  }
  return true;
}

void Heap::unregisterForeignReferenceArray(JC_Array *array) noexcept {
  if (array == nullptr || this->global_heap) return;
  auto &arrays = this->shared_storage->foreign_reference_arrays;
  arrays.erase(std::remove(arrays.begin(), arrays.end(), array), arrays.end());
}

void Heap::visitForeignArrayReferencesForPackage(
    const jpackage_ID_t package_id, const ReferenceVisitor visitor,
    void *visitor_context) {
  if (visitor == nullptr) return;
  auto &registry = foreignReferenceStorageRegistry();
  for (auto it = registry.begin(); it != registry.end();) {
    const auto storage = it->lock();
    if (!storage) {
      it = registry.erase(it);
      continue;
    }
    auto &arrays = storage->foreign_reference_arrays;
    for (auto array_it = arrays.begin(); array_it != arrays.end();) {
      JC_Array *array = *array_it;
      if (array == nullptr || !array->hasForeignReferenceEntries()) {
        array_it = arrays.erase(array_it);
        continue;
      }
      array->visitForeignReferencesForPackage(package_id, visitor,
                                              visitor_context);
      ++array_it;
    }
    if (arrays.empty()) {
      it = registry.erase(it);
    } else {
      ++it;
    }
  }
}

void Heap::clearForeignArrayReferencesForPackage(
    const jpackage_ID_t package_id) noexcept {
  auto &registry = foreignReferenceStorageRegistry();
  for (auto it = registry.begin(); it != registry.end();) {
    const auto storage = it->lock();
    if (!storage) {
      it = registry.erase(it);
      continue;
    }
    auto &arrays = storage->foreign_reference_arrays;
    for (auto array_it = arrays.begin(); array_it != arrays.end();) {
      JC_Array *array = *array_it;
      if (array == nullptr) {
        array_it = arrays.erase(array_it);
        continue;
      }
      array->clearForeignReferencesForPackage(package_id);
      if (!array->hasForeignReferenceEntries()) {
        array_it = arrays.erase(array_it);
      } else {
        ++array_it;
      }
    }
    if (arrays.empty()) {
      it = registry.erase(it);
    } else {
      ++it;
    }
  }
}

bool Heap::hasForeignArrayReferenceToAppletOwner(
    const jpackage_ID_t package_id, const japplet_ID_t applet_owner,
    Heap &target_heap) noexcept {
  struct Query {
    Heap *heap;
    japplet_ID_t owner;
    bool found = false;
  } query{&target_heap, applet_owner};
  const auto inspect_target = [](const jref_t reference, void *opaque) {
    auto &state = *static_cast<Query *>(opaque);
    if (state.found || reference.isNullPointer() ||
        reference.isGlobalArray() || reference.isGlobalInstance()) return;
    try {
      JC_Object &object = reference.isArray()
          ? static_cast<JC_Object &>(*state.heap->getArray(reference))
          : static_cast<JC_Object &>(*state.heap->getInstance(reference));
      bool removed_with_applet = true;
      if (reference.isArray()) {
        const auto array = state.heap->getArray(reference);
        if (array->hasPersistentTag()) {
          const fs::Tag tag = array->getPersistentTag();
          if (tag.len >= 2 && tag.value[0] == 0x02) {
            removed_with_applet = false;
          }
        }
      } else {
        const auto instance = state.heap->getInstance(reference);
        if (instance->hasPersistentTag()) {
          const fs::Tag tag = instance->getPersistentTag();
          if (tag.len >= 2 && tag.value[0] == 0x02) {
            removed_with_applet = false;
          }
        }
      }
      state.found = object.getOwner().getOwner() == state.owner &&
                    removed_with_applet;
    } catch (...) {
      state.found = true;
    }
  };
  // Same-package sibling applets share one handle table, so their references
  // are not tagged as foreign-package edges. Inspect their arrays, object
  // fields, and cached package static roots. Persistent getters resolve their
  // values from flash; malformed records fail closed below.
  for (const auto &array : target_heap.shared_storage->arrays) {
    if (!array || array->getType() != JAVA_ARRAY_T_REFERENCE ||
        array->getOwner().getOwner() == applet_owner) continue;
    try {
      for (uint16_t index = 0; index < array->size(); ++index) {
        inspect_target(array->getReferenceEntry(index), &query);
        if (query.found) return true;
      }
    } catch (...) {
      return true;
    }
  }
  for (const auto &instance : target_heap.shared_storage->instances) {
    if (!instance || instance->getOwner().getOwner() == applet_owner) continue;
    try {
      const auto fields = instance->getFields();
      for (uint16_t index = 0; index < fields->size(); ++index) {
        const jc_field_t &field = fields->at(index);
        switch (field.type) {
        case FieldType::FIELD_TYPE_OBJECT:
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
          inspect_target(jref_t(field.value), &query);
          if (query.found) return true;
          break;
        default:
          break;
        }
      }
    } catch (...) {
      return true;
    }
  }
  for (const StaticReferenceRoot &root :
       target_heap.shared_storage->static_reference_roots) {
    if (root.rollback_generation ==
        FlashMemory_Handler::persistentWriteRollbackGeneration()) {
      inspect_target(root.reference, &query);
      if (query.found) return true;
    }
  }
  visitForeignArrayReferencesForPackage(package_id, inspect_target, &query);
  return query.found;
}

jref_t Heap::addGlobalInstance(const jpackage_ID_t package_id,
                               const jclass_index_t class_index) {
  Heap &heap = globalArrayHeap();
  uint16_t slot = 0;
  for (auto &existing : heap.shared_storage->instances) {
    if (!existing) {
      existing = std::make_shared<JC_Instance>(heap, package_id, class_index);
      jref_t reference;
      reference.setOffset(static_cast<uint16_t>((slot + 1) | 0x4000u));
      reference.setAsGlobalInstance();
      return reference;
    }
    ++slot;
  }
  if (heap.shared_storage->instances.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }
  heap.shared_storage->instances.push_back(
      std::make_shared<JC_Instance>(heap, package_id, class_index));
  jref_t reference;
  reference.setOffset(static_cast<uint16_t>((heap.shared_storage->instances.size()) | 0x4000u));
  reference.setAsGlobalInstance();
  return reference;
}

std::shared_ptr<JC_Instance> Heap::getGlobalInstance(const jref_t reference) {
  if (!reference.isGlobalInstance()) {
    throw Exceptions::SecurityException;
  }
  const uint16_t index = reference.getGlobalInstanceIndex();
  Heap &heap = globalArrayHeap();
  if (index == 0 || index > heap.shared_storage->instances.size() ||
      !heap.shared_storage->instances.at(static_cast<uint16_t>(index - 1))) {
    throw Exceptions::SecurityException;
  }
  return heap.shared_storage->instances.at(static_cast<uint16_t>(index - 1));
}

void Heap::releaseGlobalInstance(const jref_t reference) noexcept {
  if (!reference.isGlobalInstance()) return;
  const uint16_t index = reference.getGlobalInstanceIndex();
  Heap &heap = globalArrayHeap();
  if (index == 0 || index > heap.shared_storage->instances.size()) return;
  heap.shared_storage->instances.at(static_cast<uint16_t>(index - 1)).reset();
  for (auto &entry : heap.global_aid_objects) {
    if (entry.reference == reference) entry = GlobalAIDObject{};
  }
}

jref_t Heap::findGlobalAIDObject(const uint8_t *aid,
                                 const uint8_t aid_length) noexcept {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) return jref_t();
  const Heap &heap = globalArrayHeap();
  for (const GlobalAIDObject &entry : heap.global_aid_objects) {
    if (entry.reference.isGlobalInstance() && entry.aid_length == aid_length &&
        std::equal(aid, aid + aid_length, entry.aid.begin())) {
      return entry.reference;
    }
  }
  return jref_t();
}

jref_t Heap::addGlobalAIDObject(const uint8_t *aid, const uint8_t aid_length,
                                const jpackage_ID_t package_id,
                                const jclass_index_t class_index) {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  const jref_t cached = findGlobalAIDObject(aid, aid_length);
  if (!cached.isNullPointer()) return cached;

  Heap &heap = globalArrayHeap();
  const jref_t object_reference = addGlobalInstance(package_id, class_index);
  jref_t bytes_reference;
  try {
    bytes_reference = addGlobalRetainedArray(aid_length, JAVA_ARRAY_T_BYTE);
    const auto bytes = getGlobalArray(bytes_reference);
    for (uint8_t index = 0; index < aid_length; ++index) {
      bytes->setByteEntry(index, static_cast<jbyte_t>(aid[index]));
    }
    const auto object = getGlobalInstance(object_reference);
    if (object->getNumberOfFields() != 1) {
      throw Exceptions::RuntimeException;
    }
    object->setField_Reference(0, bytes_reference);

    GlobalAIDObject entry{};
    std::copy_n(aid, aid_length, entry.aid.begin());
    entry.aid_length = aid_length;
    entry.package_id = package_id;
    entry.class_index = class_index;
    entry.reference = object_reference;
    heap.global_aid_objects.push_back(entry);
  } catch (...) {
    releaseGlobalInstance(object_reference);
    releaseGlobalArray(bytes_reference);
    throw;
  }
  return object_reference;
}

void Heap::releaseGlobalAIDObject(const uint8_t *aid,
                                 const uint8_t aid_length) noexcept {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) return;
  Heap &heap = globalArrayHeap();
  for (GlobalAIDObject &entry : heap.global_aid_objects) {
    if (!entry.reference.isGlobalInstance() ||
        entry.aid_length != aid_length ||
        !std::equal(aid, aid + aid_length, entry.aid.begin())) {
      continue;
    }
    jref_t bytes_reference;
    try {
      const auto object = getGlobalInstance(entry.reference);
      if (object->getNumberOfFields() == 1) {
        bytes_reference = object->getField_Reference(0);
      }
    } catch (...) {
      // Drop the canonical handle even if a damaged cached object cannot be
      // inspected; a later lookup can rebuild it from the installed registry.
    }
    const jref_t object_reference = entry.reference;
    releaseGlobalArray(bytes_reference);
    releaseGlobalInstance(object_reference);
    return;
  }
}

bool Heap::describeGlobalAIDObject(const jref_t reference, uint8_t aid[16],
                                   uint8_t &aid_length,
                                   jpackage_ID_t &package_id,
                                   jclass_index_t &class_index) noexcept {
  aid_length = 0;
  if (!reference.isGlobalInstance() || aid == nullptr) return false;
  const Heap &heap = globalArrayHeap();
  for (const GlobalAIDObject &entry : heap.global_aid_objects) {
    if (entry.reference == reference && entry.aid_length >= 5 &&
        entry.aid_length <= 16) {
      std::copy_n(entry.aid.begin(), entry.aid_length, aid);
      aid_length = entry.aid_length;
      package_id = entry.package_id;
      class_index = entry.class_index;
      return true;
    }
  }
  return false;
}

/*
 * Creating an array of primitive in the heap.
 *
 * @param[length] new array length
 * @param[type] new array type
 *
 * @return reference value.
 */
jref_t Heap::addArray(const uint16_t nb_entry, const jc_array_type type) {
  jref_t ref;

  uint16_t offset = 0;
  for (auto &slot : this->shared_storage->arrays) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Array>(*this, nb_entry, type);
      ref.setAsArray(true);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->shared_storage->arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new array in the heap.
  this->shared_storage->arrays.push_back(std::make_shared<JC_Array>(*this, nb_entry, type));

  ref.setAsArray(true);
  ref.setOffset(this->shared_storage->arrays.size());

  return ref;
}
/*
 * Creating an array in the heap.
 *
 * @param[length] new array length
 * @param[type] new array type
 * @param[array_reference_type] type of the references.
 *
 * @return reference value.
 */
jref_t Heap::addArray(const uint16_t nb_entry, const jc_array_type type,
                      const jc_cp_offset_t reference_type) {
  jref_t ref;

  uint16_t offset = 0;
  for (auto &slot : this->shared_storage->arrays) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Array>(*this, nb_entry, type, reference_type);
      ref.setAsArray(true);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->shared_storage->arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new array in the heap.
  this->shared_storage->arrays.push_back(
      std::make_shared<JC_Array>(*this, nb_entry, type, reference_type));

  ref.setAsArray(true);
  ref.setOffset(this->shared_storage->arrays.size());

  return ref;
}

jref_t Heap::addTransientArray(const uint16_t nb_entry,
                               const jc_array_type type,
                               const ClearEvent event) {
  return this->addArray(JC_Array(*this, nb_entry, type, true, event));
}

jref_t Heap::addTransientArray(const uint16_t nb_entry,
                               const jc_array_type type,
                               const jc_cp_offset_t reference_type,
                               const ClearEvent event) {
  return this->addArray(
      JC_Array(*this, nb_entry, type, reference_type, true, event));
}

/*
 * Creating add an array in the heap.
 *
 * @param[array] array to add
 *
 * @return reference value.
 */
jref_t Heap::addArray(JC_Array array) {
  jref_t ref;

  uint16_t offset = 0;
  for (auto &slot : this->shared_storage->arrays) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Array>(array);
      ref.setAsArray(true);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->shared_storage->arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Adding new array in the heap.
  this->shared_storage->arrays.push_back(std::make_shared<JC_Array>(array));

  ref.setAsArray(true);
  ref.setOffset(this->shared_storage->arrays.size());

  return ref;
}

/**
 * Clear transient arrays associated with the given lifecycle event.
 */
void Heap::clearTransientArrays(const ClearEvent event) {
  for (auto &array : this->shared_storage->arrays) {
    if (array && array->isTransientArray() &&
        array->getClearEvent() == event) {
      array->clearTransientData();
      if (!array->hasForeignReferenceEntries()) {
        this->unregisterForeignReferenceArray(array.get());
      }
    }
  }
  if (!this->global_heap && event == ClearEvent::CLEAR_ON_RESET) {
    for (auto &instance : this->shared_storage->instances) {
      if (instance) instance->clearNativeSessionFields();
    }
    Heap::clearGlobalArrays(event);
  }
}

/*
 * Adding new instance in the transient heap.
 *
 * @param[PackageID] package type ID
 * @param[isPersistent] is a persistant
 * @param[instantiated_class] instance type to add.
 */
jref_t Heap::addInstance(const jpackage_ID_t packageID,
                         const jclass_index_t instantiated_class) {
  jref_t ref;

  uint16_t offset = 0;
  for (auto &slot : this->shared_storage->instances) {
    ++offset;
    if (!slot) {
#ifndef PC_VERSION
      choupi_install_stage(3);
#endif
      slot = std::make_shared<JC_Instance>(*this, packageID,
                                           instantiated_class);
#ifndef PC_VERSION
      choupi_install_stage(10);
#endif
      ref.setAsArray(false);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->shared_storage->instances.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new instance in the heap.
#ifndef PC_VERSION
  choupi_install_stage(3);
#endif
  this->shared_storage->instances.push_back(
      std::make_shared<JC_Instance>(*this, packageID, instantiated_class));
#ifndef PC_VERSION
  choupi_install_stage(10);
#endif

  ref.setAsArray(false);
  ref.setOffset(this->shared_storage->instances.size());

  return ref;
}

/*
 * Adding an instance in the transient heap.
 *
 * @param[instance] pointer to the new instance to add.
 */
jref_t Heap::addInstance(JC_Instance &&instance) {
  jref_t ref;

  uint16_t offset = 0;
  for (auto &slot : this->shared_storage->instances) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Instance>(std::move(instance));
      ref.setAsArray(false);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->shared_storage->instances.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new instance in the heap.
  this->shared_storage->instances.push_back(
      std::make_shared<JC_Instance>(std::move(instance)));

  ref.setAsArray(false);
  ref.setOffset(this->shared_storage->instances.size());

  return ref;
}

/*
 * Get the JCRE-owned CryptoException instance for this applet context.
 * CryptoException.throwIt() allocates through the normal NEW bytecode, but
 * Java Card requires throwIt() to reuse a runtime-owned temporary instance.
 * Keeping the root here avoids storing it in a Java static field, since this
 * VM persists Java static references through flash serialization.
 */
jref_t Heap::addRuntimeCryptoException(const jpackage_ID_t packageID,
                                       const jclass_index_t classIndex) {
  // The ROM and imported class records use distinct byte offsets that change
  // with selected providers. Package identity remains stable across profiles.
  jref_t &root = packageID == 0 ? this->runtime_crypto_exception_rom
                                : this->runtime_crypto_exception_imported;
  if (root.isNullPointer()) {
    root = this->addInstance(packageID, classIndex);
  }
  return root;
}

jref_t Heap::addRuntimeException(const jpackage_ID_t packageID,
                                 const jclass_index_t classIndex) {
  for (const RuntimeExceptionRoot &root : this->shared_storage->runtime_exception_roots) {
    if (root.package_id == packageID && root.class_index == classIndex) {
      return root.reference;
    }
  }

  // Grow the root table before adding the heap object. If reserve fails under
  // memory pressure, no exception instance is left behind without a root.
  this->shared_storage->runtime_exception_roots.reserve(this->shared_storage->runtime_exception_roots.size() +
                                        1);
  const jref_t reference = this->addInstance(packageID, classIndex);
  this->shared_storage->runtime_exception_roots.push_back(
      RuntimeExceptionRoot{packageID, classIndex, reference});
  return reference;
}

bool Heap::isRuntimeExceptionReference(const jref_t reference) const noexcept {
  if (reference == this->runtime_crypto_exception_rom ||
      reference == this->runtime_crypto_exception_imported) {
    return !reference.isNullPointer();
  }
  for (const RuntimeExceptionRoot &root :
       this->shared_storage->runtime_exception_roots) {
    if (root.reference == reference) return true;
  }
  return false;
}

bool Heap::getStaticReferenceRoot(const jpackage_ID_t package_id,
                                  const uint16_t field_number,
                                  jref_t &reference) const noexcept {
  for (const StaticReferenceRoot &root : this->shared_storage->static_reference_roots) {
    if (root.package_id == package_id && root.field_number == field_number &&
        root.rollback_generation ==
            FlashMemory_Handler::persistentWriteRollbackGeneration()) {
      reference = root.reference;
      return true;
    }
  }
  return false;
}

void Heap::reserveStaticReferenceRoot(const jpackage_ID_t package_id,
                                      const uint16_t field_number) {
  for (const StaticReferenceRoot &root : this->shared_storage->static_reference_roots) {
    if (root.package_id == package_id && root.field_number == field_number) {
      return;
    }
  }
  this->shared_storage->static_reference_roots.reserve(this->shared_storage->static_reference_roots.size() +
                                       1);
}

void Heap::setStaticReferenceRoot(const jpackage_ID_t package_id,
                                  const uint16_t field_number,
                                  const jref_t reference) {
  for (StaticReferenceRoot &root : this->shared_storage->static_reference_roots) {
    if (root.package_id == package_id && root.field_number == field_number) {
      root.reference = reference;
      root.rollback_generation =
          FlashMemory_Handler::persistentWriteRollbackGeneration();
      return;
    }
  }
  this->shared_storage->static_reference_roots.push_back(
      StaticReferenceRoot{package_id, field_number, reference,
                          FlashMemory_Handler::persistentWriteRollbackGeneration()});
}

void Heap::appendRootReferences(std::vector<jref_t> &references) const {
  this->visitRootReferences(appendRootToVector, &references);
}

void Heap::visitRootReferences(const ReferenceVisitor visitor,
                               void *visitor_context) const {
  const jref_t runtime_roots[] = {this->runtime_crypto_exception_rom,
                                  this->runtime_crypto_exception_imported};
  for (const jref_t &reference : runtime_roots) {
    if (!reference.isNullPointer()) {
      visitor(reference, visitor_context);
    }
  }
  for (const RuntimeExceptionRoot &root : this->shared_storage->runtime_exception_roots) {
    if (!root.reference.isNullPointer()) {
      visitor(root.reference, visitor_context);
    }
  }
  for (const StaticReferenceRoot &root : this->shared_storage->static_reference_roots) {
    if (root.rollback_generation ==
            FlashMemory_Handler::persistentWriteRollbackGeneration() &&
        !root.reference.isNullPointer()) {
      visitor(root.reference, visitor_context);
    }
  }

  // Persistent flash records outlive their in-memory objects. Do not pin every
  // loaded tagged object: trace only references reachable from the current
  // context/runtime and loaded object fields. If an unreferenced persistent
  // object is restored later through a still-unread static field, its stored
  // tag allows that field read to restore it again.
}

bool Heap::getArrayReference(const JC_Array *array, jref_t &reference) const {
  if (array == nullptr) {
    return false;
  }
  uint16_t offset = 0;
  for (const auto &candidate : this->shared_storage->arrays) {
    if (offset == 0x7fff) {
      return false;
    }
    ++offset;
    if (candidate.get() == array) {
      reference.setAsArray(true);
      reference.setOffset(offset);
      return true;
    }
  }
  return false;
}

bool Heap::getPersistentReference(const fs::Tag &tag,
                                  jref_t &reference) const noexcept {
  if (tag.len > sizeof(tag.value)) {
    return false;
  }

  uint16_t offset = 0;
  for (const auto &array : this->shared_storage->arrays) {
    ++offset;
    if (array && array->hasPersistentTag()) {
      const fs::Tag candidate = array->getPersistentTag();
      if (candidate.len == tag.len &&
          std::memcmp(candidate.value, tag.value, tag.len) == 0) {
        reference.setAsArray(true);
        reference.setOffset(offset);
        return true;
      }
    }
  }

  offset = 0;
  for (const auto &instance : this->shared_storage->instances) {
    ++offset;
    if (instance && instance->hasPersistentTag()) {
      const fs::Tag candidate = instance->getPersistentTag();
      if (candidate.len == tag.len &&
          std::memcmp(candidate.value, tag.value, tag.len) == 0) {
        reference.setAsArray(false);
        reference.setOffset(offset);
        return true;
      }
    }
  }
  return false;
}

void Heap::visitChildReferences(const jref_t reference,
                                const ReferenceVisitor visitor,
                                void *visitor_context) {
  // JCRE-owned global objects are not reclaimed by an applet heap walk.
  if (reference.isNullPointer() || reference.isGlobalArray() ||
      reference.isGlobalInstance()) {
    return;
  }
  if (reference.isArray()) {
    const auto array = this->getArray(reference);
    this->visitChildReferences(*array, visitor, visitor_context);
    return;
  }

  const auto instance = this->getInstance(reference);
  this->visitChildReferences(*instance, visitor, visitor_context);
}

void Heap::visitChildReferences(JC_Array &array,
                                const ReferenceVisitor visitor,
                                void *visitor_context) {
  if (array.getType() != JAVA_ARRAY_T_REFERENCE) {
    return;
  }
  const uint16_t length = array.size();
  if (array.isPersistent() && array.hasPersistentTag() &&
      !array.isTransientArray()) {
    const fs::Tag array_tag = array.getPersistentTag();
    if (array_tag.len > sizeof(array_tag.value) - sizeof(uint16_t)) {
      throw Exceptions::SecurityException;
    }
    for (uint16_t index = 0; index < length; ++index) {
      const fs::Tag element_tag =
          FlashMemory_Handler::computeTag(array_tag, index);
      const jref_t child =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(element_tag,
                                                                  *this);
      if (!child.isNullPointer()) {
        visitor(child, visitor_context);
      }
    }

    return;
  }

  for (uint16_t index = 0; index < length; ++index) {
    const jref_t child = array.getReferenceEntry(index);
    if (!child.isNullPointer() &&
        array.getReferenceEntryOwner(index) == 0xff) {
      visitor(child, visitor_context);
    }
  }
}

void Heap::visitChildReferences(const JC_Instance &instance,
                                const ReferenceVisitor visitor,
                                void *visitor_context) {
  if (instance.isPersistent() && instance.hasPersistentTag()) {
    const fs::Tag instance_tag = instance.getPersistentTag();
    const uint16_t field_count = instance.getNumberOfFields();
    for (uint16_t index = 0; index < field_count; ++index) {
      if (instance_tag.len > sizeof(instance_tag.value) - sizeof(uint16_t)) {
        throw Exceptions::SecurityException;
      }
      const fs::Tag field_tag =
          FlashMemory_Handler::computeTag(instance_tag, index);
      if (fs_exists(field_tag.value, field_tag.len) == 0) {
        continue;
      }

      const FieldType type =
          FlashMemory_Handler::getPersistentField_Type(field_tag);
      switch (type) {
      case FieldType::FIELD_TYPE_OBJECT:
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
      case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT: {
        const jref_t child =
            FlashMemory_Handler::getPersistentField_ReferenceStrict(field_tag,
                                                                     *this);
        if (!child.isNullPointer()) {
          visitor(child, visitor_context);
        }
        break;
      }
      case FieldType::FIELD_TYPE_BYTE:
      case FieldType::FIELD_TYPE_BOOLEAN:
      case FieldType::FIELD_TYPE_SHORT:
#ifdef JCVM_INT_SUPPORTED
      case FieldType::FIELD_TYPE_INT:
#endif
      case FieldType::FIELD_TYPE_UNINITIALIZED:
        break;
      default:
        throw Exceptions::SecurityException;
      }
    }

    // A loaded persistent object can hold a newer in-memory reference than
    // the serialized field snapshot seen by the strict flash walk above.
    // Keep those cached children live across the APDU boundary that created
    // them, while retaining strict validation of the persistent records.
    if (!instance.hasLoadedFields()) {
      return;
    }
    const auto fields = instance.getFields();
    for (uint16_t index = 0; index < fields->size(); ++index) {
      const jc_field_t &field = fields->at(index);
      switch (field.type) {
      case FieldType::FIELD_TYPE_OBJECT:
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
      case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT: {
        const jref_t child(field.value);
        if (child.isNullPointer()) {
          break;
        }
        const bool expects_array =
            field.type != FieldType::FIELD_TYPE_OBJECT;
        if (child.isArray() != expects_array) {
          throw Exceptions::SecurityException;
        }
        visitor(child, visitor_context);
        break;
      }
      case FieldType::FIELD_TYPE_BYTE:
      case FieldType::FIELD_TYPE_BOOLEAN:
      case FieldType::FIELD_TYPE_SHORT:
#ifdef JCVM_INT_SUPPORTED
      case FieldType::FIELD_TYPE_INT:
      case FieldType::FIELD_TYPE_INT_LOW:
#endif
      case FieldType::FIELD_TYPE_UNINITIALIZED:
        break;
      default:
        throw Exceptions::SecurityException;
      }
    }
    return;
  }

  const auto fields = instance.getFields();
  for (uint16_t index = 0; index < fields->size(); ++index) {
    const jc_field_t &field = fields->at(index);
    switch (field.type) {
    case FieldType::FIELD_TYPE_OBJECT: {
      const jref_t child(field.value);
      if (!child.isNullPointer()) {
        if (child.isArray()) {
          throw Exceptions::SecurityException;
        }
        visitor(child, visitor_context);
      }
      break;
    }
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
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT: {
      const jref_t child(field.value);
      if (!child.isNullPointer()) {
        if (!child.isArray()) {
          throw Exceptions::SecurityException;
        }
        visitor(child, visitor_context);
      }
      break;
    }
    default:
      break;
    }
  }
}

Heap::ReachabilitySummary Heap::traceAndMark(
    const RootReferenceEnumerator enumerate_roots, void *root_context) {
  ReachabilitySummary summary;
  for (auto &array : this->shared_storage->arrays) {
    if (array) {
      array->heap_mark = 0;
    }
  }
  for (auto &instance : this->shared_storage->instances) {
    if (instance) {
      instance->heap_mark = 0;
    }
  }
  summary.static_roots = static_cast<uint32_t>(this->shared_storage->static_reference_roots.size());
  summary.runtime_roots = static_cast<uint32_t>(
      this->shared_storage->runtime_exception_roots.size() + 2);
  for (const auto &array : this->shared_storage->arrays) {
    summary.persistent_tagged_arrays +=
        array && array->hasPersistentTag() ? 1u : 0u;
  }
  for (const auto &instance : this->shared_storage->instances) {
    summary.persistent_tagged_instances +=
        instance && instance->hasPersistentTag() ? 1u : 0u;
  }
  bool found_new_reference = false;
  const auto mark_reference = [&](const jref_t reference) {
    // Global handles belong to the runtime-wide heap. Never interpret their
    // offset as a local applet-heap slot.
    if (reference.isNullPointer() || reference.isGlobalArray() ||
        reference.isGlobalInstance()) {
      return;
    }
    const uint16_t offset = reference.getOffset();
    const std::size_t slot_count = reference.isArray()
                                       ? this->shared_storage->arrays.size()
                                       : this->shared_storage->instances.size();
    if (offset == 0 || offset > slot_count) {
      ++summary.invalid_references;
      summary.complete = false;
      return;
    }
    JC_Object *object = nullptr;
    if (reference.isArray()) {
      const auto &array = this->shared_storage->arrays.at(static_cast<uint16_t>(offset - 1));
      if (array) {
        object = array.get();
      }
    } else {
      const auto &instance =
          this->shared_storage->instances.at(static_cast<uint16_t>(offset - 1));
      if (instance) {
        object = instance.get();
      }
    }
    if (!object) {
      ++summary.invalid_references;
      summary.complete = false;
      return;
    }
    if (object->heap_mark != 0) {
      return;
    }
    object->heap_mark = 1;
    found_new_reference = true;
  };

  struct VisitorContext {
    const decltype(mark_reference) *mark_fn;
  } visitor_context{&mark_reference};
  const auto visit_child = [](const jref_t child, void *opaque) {
    const auto &context = *static_cast<VisitorContext *>(opaque);
    (*context.mark_fn)(child);
  };

#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
  uint32_t root_count = 0;
  std::printf("CHOUPI:HEAP_GC roots_begin arrays=%u instances=%u\r\n",
              static_cast<unsigned>(this->shared_storage->arrays.size()),
              static_cast<unsigned>(this->shared_storage->instances.size()));
  std::fflush(stdout);
#endif
  try {
    if (!enumerate_roots(visit_child, &visitor_context, root_context)) {
      ++summary.root_enumeration_failures;
      summary.complete = false;
    }
  } catch (...) {
    ++summary.root_enumeration_failures;
    summary.complete = false;
  }
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
  for (const auto &array : this->shared_storage->arrays) {
    root_count += array && array->heap_mark == 1 ? 1u : 0u;
  }
  for (const auto &instance : this->shared_storage->instances) {
    root_count += instance && instance->heap_mark == 1 ? 1u : 0u;
  }
  std::printf("CHOUPI:HEAP_GC roots_end pending=%lu\r\n",
              static_cast<unsigned long>(root_count));
  std::printf("CHOUPI:HEAP_GC loop_begin array_slots=%lu first_mark=%u\r\n",
              static_cast<unsigned long>(this->shared_storage->arrays.size()), 0u);
  std::fflush(stdout);
#endif

  // A mark byte is also the work state: zero is unreachable, one is marked
  // and pending, and two is fully scanned. Repeated stable-slot passes avoid
  // heap allocations for a work queue and a per-object child buffer.
  do {
    found_new_reference = false;
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
    uint32_t scanned_arrays = 0;
#endif
    auto array = this->shared_storage->arrays.cbegin();
    const std::size_t array_scan_count = this->shared_storage->arrays.size();
    for (std::size_t index = 0; index < array_scan_count; ++index, ++array) {
      if (!*array || (*array)->heap_mark != 1) {
        continue;
      }
      (*array)->heap_mark = 2;
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
      ++scanned_arrays;
      if (scanned_arrays == 1 || (scanned_arrays % 16) == 0) {
        std::printf("CHOUPI:HEAP_GC arrays_progress scanned=%lu slot=%lu\r\n",
                    static_cast<unsigned long>(scanned_arrays),
                    static_cast<unsigned long>(index + 1));
        std::fflush(stdout);
      }
#endif
      try {
        if (*array) {
          this->visitChildReferences(**array, visit_child, &visitor_context);
        }
      } catch (Exceptions) {
        ++summary.edge_load_failures;
        summary.complete = false;
      }
    }
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
    std::printf("CHOUPI:HEAP_GC arrays_end scanned=%lu\r\n",
                static_cast<unsigned long>(scanned_arrays));
    std::fflush(stdout);
#endif
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
    uint32_t scanned_instances = 0;
#endif
    auto instance = this->shared_storage->instances.cbegin();
    const std::size_t instance_scan_count = this->shared_storage->instances.size();
    for (std::size_t index = 0; index < instance_scan_count;
         ++index, ++instance) {
      if (!*instance || (*instance)->heap_mark != 1) {
        continue;
      }
      (*instance)->heap_mark = 2;
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
      ++scanned_instances;
      if (scanned_instances == 1 || (scanned_instances % 16) == 0) {
        std::printf(
            "CHOUPI:HEAP_GC instances_progress scanned=%lu slot=%lu\r\n",
            static_cast<unsigned long>(scanned_instances),
            static_cast<unsigned long>(index + 1));
        std::fflush(stdout);
      }
#endif
      try {
        if (*instance) {
          this->visitChildReferences(**instance, visit_child,
                                     &visitor_context);
        }
      } catch (Exceptions) {
        ++summary.edge_load_failures;
        summary.complete = false;
      }
    }
#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_HEAP_GC_TRACE)
    std::printf("CHOUPI:HEAP_GC instances_end scanned=%lu\r\n",
                static_cast<unsigned long>(scanned_instances));
    std::fflush(stdout);
#endif
  } while (found_new_reference);

  summary.arrays_total = static_cast<uint32_t>(this->shared_storage->arrays.size());
  summary.instances_total = static_cast<uint32_t>(this->shared_storage->instances.size());
  for (const auto &array : this->shared_storage->arrays) {
    summary.arrays_reachable +=
        array && (array->heap_mark == 1 || array->heap_mark == 2) ? 1u : 0u;
  }
  for (const auto &instance : this->shared_storage->instances) {
    summary.instances_reachable += instance &&
                                           (instance->heap_mark == 1 ||
                                            instance->heap_mark == 2)
                                       ? 1u
                                       : 0u;
  }
  return summary;
}

Heap::ReachabilitySummary Heap::traceReachableFrom(
    const std::vector<jref_t> &roots) {
  VectorRootContext root_context{&roots};
  return this->traceAndMark(enumerateVectorRoots, &root_context);
}

Heap::ReachabilitySummary Heap::traceReachableFrom(
    const RootReferenceEnumerator enumerate_roots, void *root_context) {
  ReachabilitySummary summary;
  if (enumerate_roots == nullptr) {
    summary.complete = false;
    summary.root_enumeration_failures = 1;
    return summary;
  }
  return this->traceAndMark(enumerate_roots, root_context);
}

bool Heap::hasReachablePersistentTagPrefix(const fs::Tag &tag) const noexcept {
  if (tag.len == 0 || tag.len > sizeof(tag.value)) {
    return false;
  }

  const auto owns_tag_or_direct_record = [&tag](const fs::Tag &object_tag) {
    if (object_tag.len == 0 || object_tag.len > sizeof(object_tag.value) ||
        object_tag.len > tag.len) {
      return false;
    }
    const bool exact_object_tag = object_tag.len == tag.len;
    const bool direct_field_or_element =
        tag.len == static_cast<uint16_t>(object_tag.len + sizeof(uint16_t));
    return (exact_object_tag || direct_field_or_element) &&
           std::memcmp(tag.value, object_tag.value, object_tag.len) == 0;
  };

  for (const auto &array : this->shared_storage->arrays) {
    if (!array || array->heap_mark == 0 || !array->isPersistent() ||
        !array->hasPersistentTag()) {
      continue;
    }
    const fs::Tag object_tag = array->getPersistentTag();
    if (owns_tag_or_direct_record(object_tag)) {
      return true;
    }
  }

  for (const auto &instance : this->shared_storage->instances) {
    if (!instance || instance->heap_mark == 0 || !instance->isPersistent() ||
        !instance->hasPersistentTag()) {
      continue;
    }
    const fs::Tag object_tag = instance->getPersistentTag();
    if (owns_tag_or_direct_record(object_tag)) {
      return true;
    }
  }
  return false;
}

Heap::ReachabilitySummary Heap::collectUnreachableFrom(
    const std::vector<jref_t> &roots) {
  ReachabilitySummary summary;
  if (this->global_heap) {
    summary.complete = false;
    return summary;
  }

  VectorRootContext root_context{&roots};
  summary = this->traceAndMark(enumerateVectorRoots, &root_context);
  if (!summary.complete) {
    return summary;
  }

  for (auto &array : this->shared_storage->arrays) {
    if (array && array->heap_mark == 0) {
      this->unregisterForeignReferenceArray(array.get());
      array.reset();
      ++summary.arrays_reclaimed;
    }
  }

  for (auto &instance : this->shared_storage->instances) {
    if (instance && instance->heap_mark == 0) {
      instance.reset();
      ++summary.instances_reclaimed;
    }
  }
  return summary;
}

Heap::ReachabilitySummary Heap::collectUnreachableFrom(
    const RootReferenceEnumerator enumerate_roots, void *root_context) {
  ReachabilitySummary summary;
  if (this->global_heap) {
    summary.complete = false;
    return summary;
  }

  summary = this->traceAndMark(enumerate_roots, root_context);
  if (!summary.complete) {
    return summary;
  }

  for (auto &array : this->shared_storage->arrays) {
    if (array && array->heap_mark == 0) {
      this->unregisterForeignReferenceArray(array.get());
      array.reset();
      ++summary.arrays_reclaimed;
    }
  }

  for (auto &instance : this->shared_storage->instances) {
    if (instance && instance->heap_mark == 0) {
      instance.reset();
      ++summary.instances_reclaimed;
    }
  }
  return summary;
}

/*
 * Getting array from heap.
 *
 * @param[objectref] reference to the objectref to get.
 */
std::shared_ptr<JC_Array> Heap::getArray(const jref_t objectref)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
{

  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  if (objectref.isGlobalArray()) {
    return Heap::getGlobalArray(objectref);
  }

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (!objectref.isArray()) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  const auto array = this->shared_storage->arrays.at(objectref.getOffset() - 1);
  if (!array) {
    throw Exceptions::SecurityException;
  }
  return array;
}

/*
 * Getting instance from heap.
 *
 * @param[objectref] reference to the objectref to get.
 */
std::shared_ptr<JC_Instance> Heap::getInstance(const jref_t objectref)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
{

  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  if (objectref.isGlobalInstance()) {
    return Heap::getGlobalInstance(objectref);
  }

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (objectref.isArray()) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  const auto instance = this->shared_storage->instances.at(objectref.getOffset() - 1);
  if (!instance) {
    throw Exceptions::SecurityException;
  }
  return instance;
}

} // namespace jcvm
