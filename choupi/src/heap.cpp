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
#include "jc_handlers/jc_cp.hpp"
#include "jc_types/jc_array.hpp"

#include <algorithm>
#include <utility>

#ifndef PC_VERSION
extern "C" void choupi_install_stage(uint8_t stage);
#endif

namespace jcvm {

/**
 * Default constructor.
 *
 * @param[owner] Applet ID owner
 */
Heap::Heap(const japplet_ID_t owner) noexcept : owner(owner) {}

/*
 * Getting the applet ID owner
 *
 * @return heap owner.
 */
japplet_ID_t Heap::getOwner() const { return this->owner; }

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
    if (array->isTransientArray() && array->getClearEvent() == event) {
      array->clearTransientData();
    }
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
  const jref_t runtime_roots[] = {this->runtime_crypto_exception_rom,
                                  this->runtime_crypto_exception_imported};
  for (const jref_t &reference : runtime_roots) {
    if (!reference.isNullPointer()) {
      references.push_back(reference);
    }
  }
  for (const StaticReferenceRoot &root : this->static_reference_roots) {
    if (!root.reference.isNullPointer()) {
      references.push_back(root.reference);
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

void Heap::appendChildReferences(const jref_t reference,
                                 std::vector<jref_t> &references) {
  if (reference.isNullPointer()) {
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
        references.push_back(child);
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
        references.push_back(child);
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
        references.push_back(child);
      }
      break;
    }
    default:
      break;
    }
  }
}

Heap::ReachabilitySummary Heap::traceReachableFrom(
    const std::vector<jref_t> &roots) {
  ReachabilitySummary summary;
  std::vector<uint8_t> marked_arrays(this->arrays.size(), 0);
  std::vector<uint8_t> marked_instances(this->instances.size(), 0);
  std::vector<jref_t> pending;
  pending.reserve(roots.size());

  const auto enqueue = [&](const jref_t reference) {
    if (reference.isNullPointer()) {
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
    if (marks.size() < slot_count) {
      marks.resize(slot_count, 0);
    }
    const std::size_t mark_index = offset - 1;
    if (marks[mark_index] == 0) {
      marks[mark_index] = 1;
      pending.push_back(reference);
    }
  };

  for (const jref_t &root : roots) {
    enqueue(root);
  }

  std::vector<jref_t> children;
  for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
    children.clear();
    try {
      this->appendChildReferences(pending[cursor], children);
    } catch (Exceptions) {
      ++summary.edge_load_failures;
      summary.complete = false;
    }
    for (const jref_t &child : children) {
      enqueue(child);
    }
  }

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

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (!objectref.isArray()) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  return this->arrays.at(objectref.getOffset() - 1);
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

  return this->instances.at(objectref.getOffset() - 1);
}

} // namespace jcvm
