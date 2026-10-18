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
#include "jc_handlers/jc_cp.hpp"
#include "jc_types/jc_array.hpp"

#include <cstring>

#include <algorithm>
#include <utility>

#ifndef PC_VERSION
extern "C" void choupi_install_stage(uint8_t stage);
#endif

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

void enumerateVectorRoots(Heap::ReferenceVisitor visitor,
                          void *visitor_context, void *root_context) {
  const auto &roots = *static_cast<VectorRootContext *>(root_context)->roots;
  for (const jref_t &root : roots) {
    visitor(root, visitor_context);
  }
}

} // namespace

/**
 * Default constructor.
 *
 * @param[owner] Applet ID owner
 */
Heap::Heap(const japplet_ID_t owner, const bool global_heap) noexcept
    : owner(owner), global_heap(global_heap) {}

/*
 * Getting the applet ID owner
 *
 * @return heap owner.
 */
japplet_ID_t Heap::getOwner() const { return this->owner; }

bool Heap::isGlobalHeap() const noexcept { return this->global_heap; }

jref_t Heap::addGlobalArray(const uint16_t length, const jc_array_type type) {
  Heap &heap = globalArrayHeap();
  if (heap.arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  constexpr ClearEvent clear_on_reset = ClearEvent::CLEAR_ON_RESET;
  if (type == JAVA_ARRAY_T_REFERENCE) {
    // A global Object[] has no applet-specific component class. Keep its
    // component type wildcarded, while giving it the required reset lifetime.
    heap.arrays.push_back(std::make_shared<JC_Array>(
        heap, length, type, static_cast<jc_cp_offset_t>(0xffff), true,
        clear_on_reset));
  } else {
    heap.arrays.push_back(std::make_shared<JC_Array>(
        heap, length, type, true, clear_on_reset));
  }

  jref_t reference;
  reference.setAsArray(true);
  reference.setOffset(static_cast<uint16_t>(heap.arrays.size()));
  reference.setAsGlobalArray();
  return reference;
}

std::shared_ptr<JC_Array> Heap::getGlobalArray(const jref_t reference) {
  if (!reference.isGlobalArray()) {
    throw Exceptions::SecurityException;
  }
  const uint16_t index = reference.getGlobalArrayIndex();
  if (index == 0) {
    throw Exceptions::SecurityException;
  }
  return globalArrayHeap().arrays.at(index - 1);
}

void Heap::clearGlobalArrays(const ClearEvent event) {
  if (event != ClearEvent::CLEAR_ON_RESET) {
    return;
  }
  for (const auto &array : globalArrayHeap().arrays) {
    if (array && array->isTransientArray() &&
        array->getClearEvent() == event) {
      array->clearTransientData();
    }
  }
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
  for (auto &slot : this->arrays) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Array>(*this, nb_entry, type);
      ref.setAsArray(true);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new array in the heap.
  this->arrays.push_back(std::make_shared<JC_Array>(*this, nb_entry, type));

  ref.setAsArray(true);
  ref.setOffset(this->arrays.size());

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
  for (auto &slot : this->arrays) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Array>(*this, nb_entry, type, reference_type);
      ref.setAsArray(true);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new array in the heap.
  this->arrays.push_back(
      std::make_shared<JC_Array>(*this, nb_entry, type, reference_type));

  ref.setAsArray(true);
  ref.setOffset(this->arrays.size());

  return ref;
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
  for (auto &slot : this->arrays) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Array>(array);
      ref.setAsArray(true);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->arrays.size() >= kMaximumLocalArrayHandles) {
    throw Exceptions::RuntimeException;
  }

  // Adding new array in the heap.
  this->arrays.push_back(std::make_shared<JC_Array>(array));

  ref.setAsArray(true);
  ref.setOffset(this->arrays.size());

  return ref;
}

/**
 * Clear transient arrays associated with the given lifecycle event.
 */
void Heap::clearTransientArrays(const ClearEvent event) {
  for (auto &array : this->arrays) {
    if (array && array->isTransientArray() &&
        array->getClearEvent() == event) {
      array->clearTransientData();
    }
  }
  if (!this->global_heap && event == ClearEvent::CLEAR_ON_RESET) {
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
  for (auto &slot : this->instances) {
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
  if (this->instances.size() >= 0x7fff) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new instance in the heap.
#ifndef PC_VERSION
  choupi_install_stage(3);
#endif
  this->instances.push_back(
      std::make_shared<JC_Instance>(*this, packageID, instantiated_class));
#ifndef PC_VERSION
  choupi_install_stage(10);
#endif

  ref.setAsArray(false);
  ref.setOffset(this->instances.size());

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
  for (auto &slot : this->instances) {
    ++offset;
    if (!slot) {
      slot = std::make_shared<JC_Instance>(std::move(instance));
      ref.setAsArray(false);
      ref.setOffset(offset);
      return ref;
    }
  }
  if (this->instances.size() >= 0x7fff) {
    throw Exceptions::RuntimeException;
  }

  // Creating and adding new instance in the heap.
  this->instances.push_back(
      std::make_shared<JC_Instance>(std::move(instance)));

  ref.setAsArray(false);
  ref.setOffset(this->instances.size());

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
  jref_t &root = classIndex == 234 ? this->runtime_crypto_exception_rom
                                   : this->runtime_crypto_exception_imported;
  if (root.isNullPointer()) {
    root = this->addInstance(packageID, classIndex);
  }
  return root;
}

jref_t Heap::addRuntimeException(const jpackage_ID_t packageID,
                                 const jclass_index_t classIndex) {
  for (const RuntimeExceptionRoot &root : this->runtime_exception_roots) {
    if (root.package_id == packageID && root.class_index == classIndex) {
      return root.reference;
    }
  }
  const jref_t reference = this->addInstance(packageID, classIndex);
  this->runtime_exception_roots.push_back(
      RuntimeExceptionRoot{packageID, classIndex, reference});
  return reference;
}

bool Heap::getStaticReferenceRoot(const jpackage_ID_t package_id,
                                  const uint16_t field_number,
                                  jref_t &reference) const noexcept {
  for (const StaticReferenceRoot &root : this->static_reference_roots) {
    if (root.package_id == package_id && root.field_number == field_number) {
      reference = root.reference;
      return true;
    }
  }
  return false;
}

void Heap::setStaticReferenceRoot(const jpackage_ID_t package_id,
                                  const uint16_t field_number,
                                  const jref_t reference) {
  for (StaticReferenceRoot &root : this->static_reference_roots) {
    if (root.package_id == package_id && root.field_number == field_number) {
      root.reference = reference;
      return;
    }
  }
  this->static_reference_roots.push_back(
      StaticReferenceRoot{package_id, field_number, reference});
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
  for (const RuntimeExceptionRoot &root : this->runtime_exception_roots) {
    if (!root.reference.isNullPointer()) {
      visitor(root.reference, visitor_context);
    }
  }
  for (const StaticReferenceRoot &root : this->static_reference_roots) {
    if (!root.reference.isNullPointer()) {
      visitor(root.reference, visitor_context);
    }
  }

  // Persisted static fields are deserialized lazily, so an object loaded by
  // another reference may not yet be represented in static_reference_roots.
  // Conservatively pin every loaded persistent object and array until the
  // runtime can enumerate persisted roots without loading them on demand.
  uint16_t offset = 0;
  for (const auto &array : this->arrays) {
    ++offset;
    if (array && array->hasPersistentTag()) {
      jref_t reference;
      reference.setAsArray(true);
      reference.setOffset(offset);
      visitor(reference, visitor_context);
    }
  }

  offset = 0;
  for (const auto &instance : this->instances) {
    ++offset;
    if (instance && instance->hasPersistentTag()) {
      jref_t reference;
      reference.setAsArray(false);
      reference.setOffset(offset);
      visitor(reference, visitor_context);
    }
  }
}

bool Heap::getArrayReference(const JC_Array *array, jref_t &reference) const {
  if (array == nullptr) {
    return false;
  }
  uint16_t offset = 0;
  for (const auto &candidate : this->arrays) {
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
  for (const auto &array : this->arrays) {
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
  for (const auto &instance : this->instances) {
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
  // Global arrays belong to the separate JCRE heap and are reclaimed by the
  // reset lifecycle, not by an applet heap walk.
  if (reference.isNullPointer() || reference.isGlobalArray()) {
    return;
  }
  if (reference.isArray()) {
    const auto array = this->getArray(reference);
    if (array->getType() != JAVA_ARRAY_T_REFERENCE) {
      return;
    }
    for (uint16_t index = 0; index < array->size(); ++index) {
      const jref_t child = array->getReferenceEntry(index);
      if (!child.isNullPointer()) {
        visitor(child, visitor_context);
      }
    }
    return;
  }

  const auto instance = this->getInstance(reference);
  const auto fields = instance->getFields();
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
    const RootReferenceEnumerator enumerate_roots, void *root_context,
    std::vector<uint8_t> &marked_arrays,
    std::vector<uint8_t> &marked_instances) {
  ReachabilitySummary summary;
  marked_arrays.assign(this->arrays.size(), 0);
  marked_instances.assign(this->instances.size(), 0);
  bool found_new_reference = false;
  const auto mark_reference = [&](const jref_t reference) {
    // Global-array handles use the same reference bit as local arrays, but
    // their offset belongs to the runtime-wide global heap. Never interpret
    // that offset as a local applet-heap slot.
    if (reference.isNullPointer() || reference.isGlobalArray()) {
      return;
    }
    const uint16_t offset = reference.getOffset();
    auto &marks = reference.isArray() ? marked_arrays : marked_instances;
    const std::size_t slot_count =
        reference.isArray() ? this->arrays.size() : this->instances.size();
    if (offset == 0 || offset > slot_count) {
      ++summary.invalid_references;
      summary.complete = false;
      return;
    }
    const bool slot_is_live = reference.isArray()
                                  ? static_cast<bool>(this->arrays.at(offset - 1))
                                  : static_cast<bool>(this->instances.at(offset - 1));
    if (!slot_is_live) {
      ++summary.invalid_references;
      summary.complete = false;
      return;
    }
    const std::size_t mark_index = offset - 1;
    if (marks[mark_index] == 0) {
      marks[mark_index] = 1;
      found_new_reference = true;
    }
  };

  struct VisitorContext {
    const decltype(mark_reference) *mark_fn;
  } visitor_context{&mark_reference};
  const auto visit_child = [](const jref_t child, void *opaque) {
    const auto &context = *static_cast<VisitorContext *>(opaque);
    (*context.mark_fn)(child);
  };

  enumerate_roots(visit_child, &visitor_context, root_context);

  // A mark byte is also the work state: zero is unreachable, one is marked
  // and pending, and two is fully scanned. Repeated stable-slot passes avoid
  // heap allocations for a work queue and a per-object child buffer.
  do {
    found_new_reference = false;
    for (std::size_t index = 0; index < marked_arrays.size(); ++index) {
      if (marked_arrays[index] != 1) {
        continue;
      }
      marked_arrays[index] = 2;
      jref_t reference;
      reference.setAsArray(true);
      reference.setOffset(static_cast<uint16_t>(index + 1));
      try {
        this->visitChildReferences(reference, visit_child, &visitor_context);
      } catch (Exceptions) {
        ++summary.edge_load_failures;
        summary.complete = false;
      }
    }
    for (std::size_t index = 0; index < marked_instances.size(); ++index) {
      if (marked_instances[index] != 1) {
        continue;
      }
      marked_instances[index] = 2;
      jref_t reference;
      reference.setAsArray(false);
      reference.setOffset(static_cast<uint16_t>(index + 1));
      try {
        this->visitChildReferences(reference, visit_child, &visitor_context);
      } catch (Exceptions) {
        ++summary.edge_load_failures;
        summary.complete = false;
      }
    }
  } while (found_new_reference);

  summary.arrays_total = static_cast<uint32_t>(this->arrays.size());
  summary.instances_total = static_cast<uint32_t>(this->instances.size());
  for (const uint8_t mark : marked_arrays) {
    summary.arrays_reachable += mark != 0 ? 1u : 0u;
  }
  for (const uint8_t mark : marked_instances) {
    summary.instances_reachable += mark != 0 ? 1u : 0u;
  }
  return summary;
}

Heap::ReachabilitySummary Heap::traceReachableFrom(
    const std::vector<jref_t> &roots) {
  std::vector<uint8_t> marked_arrays;
  std::vector<uint8_t> marked_instances;
  VectorRootContext root_context{&roots};
  return this->traceAndMark(enumerateVectorRoots, &root_context,
                            marked_arrays, marked_instances);
}

Heap::ReachabilitySummary Heap::collectUnreachableFrom(
    const std::vector<jref_t> &roots) {
  ReachabilitySummary summary;
  if (this->global_heap) {
    summary.complete = false;
    return summary;
  }

  std::vector<uint8_t> marked_arrays;
  std::vector<uint8_t> marked_instances;
  VectorRootContext root_context{&roots};
  summary = this->traceAndMark(enumerateVectorRoots, &root_context,
                               marked_arrays, marked_instances);
  if (!summary.complete) {
    return summary;
  }

  std::size_t index = 0;
  for (auto &array : this->arrays) {
    if (array && (index >= marked_arrays.size() || marked_arrays[index] == 0)) {
      array.reset();
      ++summary.arrays_reclaimed;
    }
    ++index;
  }

  index = 0;
  for (auto &instance : this->instances) {
    if (instance &&
        (index >= marked_instances.size() || marked_instances[index] == 0)) {
      instance.reset();
      ++summary.instances_reclaimed;
    }
    ++index;
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

  std::vector<uint8_t> marked_arrays;
  std::vector<uint8_t> marked_instances;
  summary = this->traceAndMark(enumerate_roots, root_context, marked_arrays,
                               marked_instances);
  if (!summary.complete) {
    return summary;
  }

  std::size_t index = 0;
  for (auto &array : this->arrays) {
    if (array && (index >= marked_arrays.size() || marked_arrays[index] == 0)) {
      array.reset();
      ++summary.arrays_reclaimed;
    }
    ++index;
  }

  index = 0;
  for (auto &instance : this->instances) {
    if (instance &&
        (index >= marked_instances.size() || marked_instances[index] == 0)) {
      instance.reset();
      ++summary.instances_reclaimed;
    }
    ++index;
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

  const auto array = this->arrays.at(objectref.getOffset() - 1);
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

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (objectref.isArray()) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  const auto instance = this->instances.at(objectref.getOffset() - 1);
  if (!instance) {
    throw Exceptions::SecurityException;
  }
  return instance;
}

} // namespace jcvm
