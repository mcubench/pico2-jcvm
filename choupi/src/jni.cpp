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
} // namespace

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
};

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepackNonAtomic(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGenericNonAtomic(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
    jref_t, jshort_t, jref_t, jshort_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
    jref_t, jshort_t, std::shared_ptr<JC_Array>, jbyte_t, jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
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
fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(jbyte_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
    std::shared_ptr<JC_Array>, jshort_t, jshort_t, jbyte_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
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
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isAppletActive(jref_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_assertIntegrity(jref_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitive(jref_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitiveArraysSupported() {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
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
