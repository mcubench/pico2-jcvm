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

#include "context.hpp"
#include "exceptions.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "jc_types/jref_t.hpp"
#include "types.hpp"

#include <vector>

namespace jcvm {

namespace {
ClearEvent getTransientClearEvent(const jbyte_t event) {
  switch (event) {
  case static_cast<jbyte_t>(ClearEvent::CLEAR_ON_RESET):
    return ClearEvent::CLEAR_ON_RESET;
  case static_cast<jbyte_t>(ClearEvent::CLEAR_ON_DESELECT):
    return ClearEvent::CLEAR_ON_DESELECT;
  default:
    throw Exceptions::SecurityException;
  }
}

std::shared_ptr<JC_Array> makeTransientArray(const jshort_t length,
                                             const jbyte_t event,
                                             const jc_array_type type) {
  if (length < 0) {
    throw Exceptions::NegativeArraySizeException;
  }

  Heap &heap = Context::getActiveContext().getHeap();
  return std::make_shared<JC_Array>(
      heap, static_cast<uint16_t>(length), type, true,
      getTransientClearEvent(event));
}

void checkArrayRange(const JC_Array &array, const jshort_t offset,
                     const jshort_t length) {
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) >
          array.size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
}

uint32_t readArrayElement(JC_Array &array, const uint16_t index) {
  switch (array.getType()) {
  case JAVA_ARRAY_T_BOOLEAN:
  case JAVA_ARRAY_T_BYTE:
    return static_cast<uint8_t>(array.getByteEntry(index));
  case JAVA_ARRAY_T_SHORT:
    return static_cast<uint16_t>(array.getShortEntry(index));
#ifdef JCVM_INT_SUPPORTED
  case JAVA_ARRAY_T_INT:
    return static_cast<uint32_t>(array.getIntEntry(index));
#endif
  case JAVA_ARRAY_T_REFERENCE:
    throw Exceptions::NotYetImplemented;
  }

  throw Exceptions::SecurityException;
}

void writeArrayElement(JC_Array &array, const uint16_t index,
                       const uint32_t value) {
  switch (array.getType()) {
  case JAVA_ARRAY_T_BOOLEAN:
  case JAVA_ARRAY_T_BYTE:
    array.setByteEntry(index, static_cast<jbyte_t>(value));
    return;
  case JAVA_ARRAY_T_SHORT:
    array.setShortEntry(index, static_cast<jshort_t>(value));
    return;
#ifdef JCVM_INT_SUPPORTED
  case JAVA_ARRAY_T_INT:
    array.setIntEntry(index, static_cast<jint_t>(value));
    return;
#endif
  case JAVA_ARRAY_T_REFERENCE:
    throw Exceptions::NotYetImplemented;
  }

  throw Exceptions::SecurityException;
}

jshort_t copyArrayRange(const jref_t sourceRef, const jshort_t sourceOffset,
                        const jshort_t length, const jref_t destinationRef,
                        const jshort_t destinationOffset) {
  Heap &heap = Context::getActiveContext().getHeap();
  auto source = heap.getArray(sourceRef);
  auto destination = heap.getArray(destinationRef);
  checkArrayRange(*source, sourceOffset, length);
  checkArrayRange(*destination, destinationOffset, length);
  if (source->getType() != destination->getType()) {
    throw Exceptions::ArrayStoreException;
  }

  std::vector<uint32_t> elements;
  elements.reserve(static_cast<size_t>(length));
  for (jshort_t index = 0; index < length; ++index) {
    elements.push_back(readArrayElement(
        *source, static_cast<uint16_t>(sourceOffset + index)));
  }
  for (jshort_t index = 0; index < length; ++index) {
    writeArrayElement(*destination,
                      static_cast<uint16_t>(destinationOffset + index),
                      elements[static_cast<size_t>(index)]);
  }

  return static_cast<jshort_t>(destinationOffset + length);
}

jshort_t fillArrayRange(const jref_t destinationRef,
                        const jshort_t destinationOffset,
                        const jshort_t length, const jref_t patternRef,
                        const jshort_t patternOffset) {
  Heap &heap = Context::getActiveContext().getHeap();
  auto destination = heap.getArray(destinationRef);
  auto pattern = heap.getArray(patternRef);
  checkArrayRange(*destination, destinationOffset, length);
  checkArrayRange(*pattern, patternOffset, length == 0 ? 0 : 1);
  if (destination->getType() != JAVA_ARRAY_T_BYTE ||
      pattern->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::SecurityException;
  }

  const uint16_t patternLength =
      static_cast<uint16_t>(pattern->size() - patternOffset);
  if (length > 0 && patternLength == 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }

  for (jshort_t index = 0; index < length; ++index) {
    const uint16_t patternIndex = static_cast<uint16_t>(
        patternOffset + (static_cast<uint16_t>(index) % patternLength));
    destination->setByteEntry(
        static_cast<uint16_t>(destinationOffset + index),
        pattern->getByteEntry(patternIndex));
  }
  return static_cast<jshort_t>(destinationOffset + length);
}
} // namespace

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
    const jref_t source, const jshort_t sourceOffset, const jshort_t length,
    const jref_t destination, const jshort_t destinationOffset) {
  return copyArrayRange(source, sourceOffset, length, destination,
                        destinationOffset);
};

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepackNonAtomic(
    const jref_t source, const jshort_t sourceOffset, const jshort_t length,
    const jref_t destination, const jshort_t destinationOffset) {
  return copyArrayRange(source, sourceOffset, length, destination,
                        destinationOffset);
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
    const jref_t destination, const jshort_t destinationOffset,
    const jshort_t length, const jref_t pattern,
    const jshort_t patternOffset) {
  return fillArrayRange(destination, destinationOffset, length, pattern,
                        patternOffset);
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGenericNonAtomic(
    const jref_t destination, const jshort_t destinationOffset,
    const jshort_t length, const jref_t pattern,
    const jshort_t patternOffset) {
  return fillArrayRange(destination, destinationOffset, length, pattern,
                        patternOffset);
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
    const jref_t firstRef, const jshort_t firstOffset,
    const jref_t secondRef, const jshort_t secondOffset,
    const jshort_t length) {
  Heap &heap = Context::getActiveContext().getHeap();
  auto first = heap.getArray(firstRef);
  auto second = heap.getArray(secondRef);
  checkArrayRange(*first, firstOffset, length);
  checkArrayRange(*second, secondOffset, length);
  if (first->getType() != JAVA_ARRAY_T_BYTE ||
      second->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::SecurityException;
  }

  for (jshort_t index = 0; index < length; ++index) {
    const jbyte_t firstByte =
        first->getByteEntry(static_cast<uint16_t>(firstOffset + index));
    const jbyte_t secondByte =
        second->getByteEntry(static_cast<uint16_t>(secondOffset + index));
    if (firstByte < secondByte) {
      return -1;
    }
    if (firstByte > secondByte) {
      return 1;
    }
  }
  return 0;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
    const jref_t array_ref, const jshort_t offset,
    std::shared_ptr<JC_Array> value_array, const jshort_t value_offset) {
  if (array_ref.isNullPointer() || value_array == nullptr) {
    throw Exceptions::NullPointerException;
  }

  Heap &heap = Context::getActiveContext().getHeap();
  auto array = heap.getArray(array_ref);
  if (offset < 0 || static_cast<uint32_t>(offset) > array->size() ||
      value_offset < 0 ||
      static_cast<uint32_t>(value_offset) >= value_array->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  if (array->getType() != JAVA_ARRAY_T_BYTE ||
      value_array->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::UtilException;
  }

  const jbyte_t value = value_array->getByteEntry(
      static_cast<uint16_t>(value_offset));
  for (uint16_t index = static_cast<uint16_t>(offset);
       index < array->size(); ++index) {
    if (array->getByteEntry(index) == value) {
      return static_cast<jshort_t>(index);
    }
  }
  return -1;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet() {
  // The current VM has no applet selection lifecycle or pending-select state.
  return FALSE;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_isTransient(
    const jref_t objectref) {
  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  Heap &heap = Context::getActiveContext().getHeap();
  if (objectref.isArray()) {
    auto array = heap.getArray(objectref);
    if (array->isTransientArray()) {
      return static_cast<jbyte_t>(array->getClearEvent());
    }
    return 0;
  }

  return 0;
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientBooleanArray(
    const jshort_t length, const jbyte_t event) {
  return makeTransientArray(length, event, JAVA_ARRAY_T_BOOLEAN);
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
    const jshort_t length, const jbyte_t event) {
  return makeTransientArray(length, event, JAVA_ARRAY_T_BYTE);
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientShortArray(
    const jshort_t length, const jbyte_t event) {
  return makeTransientArray(length, event, JAVA_ARRAY_T_SHORT);
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(jshort_t,
                                                                     jbyte_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(jbyte_t,
                                                                   jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAID() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_beginTransaction() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_abortTransaction() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_commitTransaction() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_getTransactionDepth() {
  // Transactions are not implemented, so a transaction is never active.
  return 0;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_getUnusedCommitCapacity() {
  // No commit buffer is available until transactional persistence is added.
  return 0;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_getMaxCommitCapacity() {
  // Report the absence of a transaction commit buffer to capability probes.
  return 0;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getPreviousContextAID() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jshort_t
fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(jbyte_t type) {
  // The VM does not yet account for free heap or persistent-store capacity.
  // Report zero rather than inventing a capacity estimate to Java Card code.
  if (type < 0 || type > 2) {
    throw Exceptions::SystemException;
  }
  return 0;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
    std::shared_ptr<JC_Array> output, jshort_t offset, jbyte_t type) {
  if (output == nullptr || type < 0 || type > 2 || offset < 0 ||
      static_cast<uint32_t>(offset) + 2 > output->size()) {
    throw Exceptions::SystemException;
  }
  // The short[2] overload returns persistent and transient capacity. Neither
  // capacity is measured by this port yet, so both are explicitly zero.
  output->setShortEntry(static_cast<uint16_t>(offset), 0);
  output->setShortEntry(static_cast<uint16_t>(offset + 1), 0);
}

jref_t
fr_gouv_ssi_nativeimpl_NativeImplementation_getAppletShareableInterfaceObject(
    jref_t, jbyte_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isObjectDeletionSupported() {
  return FALSE;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_requestObjectDeletion() {
  // Object deletion is not supported; the Java Card API specifies this as a
  // request and callers can query the unsupported capability above.
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAssignedChannel() {
  // The current transport has no logical-channel support; use the basic channel.
  return 0;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isAppletActive(jref_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_assertIntegrity(jref_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitive(
    const jref_t objectref) {
  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  // This VM does not create or track integrity-sensitive arrays.
  return FALSE;
}

jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitiveArraysSupported() {
  // Integrity-sensitive array storage and verification are not implemented.
  return FALSE;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_makeIntegritySensitiveArray(
    jbyte_t, jbyte_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_clearArray(jref_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientIntArray(
    const jshort_t length, const jbyte_t event) {
  return makeTransientArray(length, event, JAVA_ARRAY_T_INT);
}

} // namespace jcvm
