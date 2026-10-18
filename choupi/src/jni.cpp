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
#include "jc_handlers/jc_cp.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_handlers/package.hpp"
#include "jc_utils.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "jc_types/jref_t.hpp"
#include "types.hpp"

#include <cstring>
#include <algorithm>
#include "mbedtls/sha256.h"
#ifndef PC_VERSION
#include "pico/sha256.h"
#endif
#include <vector>

namespace jcvm {

#ifndef PC_VERSION
extern "C" uint32_t choupi_available_transient_memory_bytes(void);
extern "C" uint32_t choupi_available_persistent_memory_bytes(void);
#endif

namespace {
ClearEvent getTransientClearEvent(const jbyte_t event) {
  switch (event) {
  case static_cast<jbyte_t>(ClearEvent::CLEAR_ON_RESET):
    return ClearEvent::CLEAR_ON_RESET;
  case static_cast<jbyte_t>(ClearEvent::CLEAR_ON_DESELECT):
    return ClearEvent::CLEAR_ON_DESELECT;
  default:
    throw Exceptions::SystemException;
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

bool isArrayLogicPrimitive(const jc_array_type type) {
  switch (type) {
  case JAVA_ARRAY_T_BYTE:
  case JAVA_ARRAY_T_SHORT:
#ifdef JCVM_INT_SUPPORTED
  case JAVA_ARRAY_T_INT:
#endif
    return true;
  case JAVA_ARRAY_T_BOOLEAN:
  case JAVA_ARRAY_T_REFERENCE:
    return false;
  }
  return false;
}

int32_t readSignedArrayElement(JC_Array &array, const uint16_t index) {
  switch (array.getType()) {
  case JAVA_ARRAY_T_BYTE:
    return array.getByteEntry(index);
  case JAVA_ARRAY_T_SHORT:
    return array.getShortEntry(index);
#ifdef JCVM_INT_SUPPORTED
  case JAVA_ARRAY_T_INT:
    return array.getIntEntry(index);
#endif
  case JAVA_ARRAY_T_BOOLEAN:
  case JAVA_ARRAY_T_REFERENCE:
    break;
  }
  throw Exceptions::UtilException;
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
  if (patternOffset < 0 ||
      static_cast<uint32_t>(patternOffset) >= pattern->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  if (!isArrayLogicPrimitive(destination->getType()) ||
      !isArrayLogicPrimitive(pattern->getType())) {
    throw Exceptions::UtilException;
  }
  if (destination->getType() != pattern->getType()) {
    throw Exceptions::UtilException;
  }

  const uint32_t value =
      readArrayElement(*pattern, static_cast<uint16_t>(patternOffset));
  for (jshort_t index = 0; index < length; ++index) {
    writeArrayElement(*destination,
                      static_cast<uint16_t>(destinationOffset + index), value);
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
  if (!isArrayLogicPrimitive(first->getType()) ||
      !isArrayLogicPrimitive(second->getType())) {
    throw Exceptions::UtilException;
  }
  if (first->getType() != second->getType()) {
    throw Exceptions::UtilException;
  }

  for (jshort_t index = 0; index < length; ++index) {
    const int32_t firstValue = readSignedArrayElement(
        *first, static_cast<uint16_t>(firstOffset + index));
    const int32_t secondValue = readSignedArrayElement(
        *second, static_cast<uint16_t>(secondOffset + index));
    if (firstValue < secondValue) {
      return -1;
    }
    if (firstValue > secondValue) {
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
      value_offset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  if (value_array->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::UtilException;
  }
  if (!isArrayLogicPrimitive(array->getType())) {
    throw Exceptions::UtilException;
  }

  const uint16_t element_bytes = array->getEntrySize();
  if (static_cast<uint32_t>(value_offset) + element_bytes >
      value_array->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  uint32_t value = 0;
  for (uint16_t byte_index = 0; byte_index < element_bytes; ++byte_index) {
    value = (value << 8) | static_cast<uint8_t>(value_array->getByteEntry(
                                static_cast<uint16_t>(value_offset + byte_index)));
  }
  for (uint16_t index = static_cast<uint16_t>(offset);
       index < array->size(); ++index) {
    if (readArrayElement(*array, index) == value) {
      return static_cast<jshort_t>(index);
    }
  }
  return -1;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet() {
  // The current VM has no applet selection lifecycle or pending-select state.
  return FALSE;
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_getAPDUBuffer() {
  Context &context = Context::getActiveContext();
  const jref_t reference = context.getAPDUBufferRef();
  if (reference.isNullPointer() || !reference.isArray()) {
    throw Exceptions::RuntimeException;
  }
  return context.getHeap().getArray(reference);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_setAPDUBuffer(
    std::shared_ptr<JC_Array> buffer) {
  Context::getActiveContext().setAPDUJavaBuffer(std::move(buffer));
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAPDUIncomingLength() {
  return static_cast<jshort_t>(
      Context::getActiveContext().getAPDUIncomingLength());
}

jshort_t
fr_gouv_ssi_nativeimpl_NativeImplementation_getAPDUExpectedResponseLength() {
  return static_cast<jshort_t>(
      Context::getActiveContext().getAPDUExpectedResponseLength());
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_beginAPDUOutgoing(
    const jbool_t no_chaining) {
  return static_cast<jshort_t>(
      Context::getActiveContext().beginAPDUOutgoing(no_chaining != FALSE));
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_setAPDUOutgoingLength(
    const jshort_t length) {
  if (length < 0) {
    throw Exceptions::APDUException;
  }
  Context::getActiveContext().setAPDUOutgoingLength(
      static_cast<uint16_t>(length));
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAPDUState() {
  return static_cast<jbyte_t>(Context::getActiveContext().getAPDUState());
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_receiveAPDUIncoming() {
  Context &context = Context::getActiveContext();
  context.markAPDUIncomingReceived();
  return static_cast<jshort_t>(context.getAPDUIncomingLength());
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_setAPDUOutgoing(
    std::shared_ptr<JC_Array> buffer, const jshort_t offset,
    const jshort_t length) {
  if (offset < 0 || length < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  Context &context = Context::getActiveContext();
  context.setAPDUOutgoing(std::move(buffer), static_cast<uint16_t>(offset),
                          static_cast<uint16_t>(length));
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

void fr_gouv_ssi_nativeimpl_NativeImplementation_sha256(
    std::shared_ptr<JC_Array> input, const jshort_t inputOffset,
    const jshort_t inputLength, std::shared_ptr<JC_Array> output,
    const jshort_t outputOffset) {
  if (!input || !output) {
    throw Exceptions::NullPointerException;
  }
  if (input->getType() != JAVA_ARRAY_T_BYTE ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (inputOffset < 0 || inputLength < 0 || outputOffset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*input, inputOffset, inputLength);
  checkArrayRange(*output, outputOffset, 32);

#ifdef PC_VERSION
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  int result = mbedtls_sha256_starts(&context, 0);
  uint8_t block[64];
  for (jshort_t offset = 0; result == 0 && offset < inputLength;) {
    const jshort_t count = static_cast<jshort_t>(
        std::min<jshort_t>(sizeof(block), inputLength - offset));
    for (jshort_t i = 0; i < count; ++i) {
      block[i] = static_cast<uint8_t>(input->getByteEntry(
          static_cast<uint16_t>(inputOffset + offset + i)));
    }
    result = mbedtls_sha256_update(&context, block,
                                   static_cast<size_t>(count));
    offset = static_cast<jshort_t>(offset + count);
  }
  uint8_t digest[32];
  if (result == 0) {
    result = mbedtls_sha256_finish(&context, digest);
  }
  mbedtls_sha256_free(&context);
  if (result != 0) {
    throw Exceptions::CryptoException;
  }
  for (uint16_t i = 0; i < sizeof(digest); ++i) {
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         static_cast<jbyte_t>(digest[i]));
  }
#else
  pico_sha256_state_t state{};
  if (pico_sha256_start_blocking_until(
          &state, SHA256_BIG_ENDIAN, false, make_timeout_time_ms(100)) !=
      PICO_OK) {
    throw Exceptions::CryptoException;
  }
  uint8_t block[64];
  for (jshort_t offset = 0; offset < inputLength;) {
    const jshort_t count = static_cast<jshort_t>(
        std::min<jshort_t>(sizeof(block), inputLength - offset));
    for (jshort_t i = 0; i < count; ++i) {
      block[i] = static_cast<uint8_t>(input->getByteEntry(
          static_cast<uint16_t>(inputOffset + offset + i)));
    }
    pico_sha256_update_blocking(&state, block, static_cast<size_t>(count));
    offset = static_cast<jshort_t>(offset + count);
  }
  sha256_result_t digest{};
  pico_sha256_finish(&state, &digest);
  for (uint16_t i = 0; i < sizeof(digest.bytes); ++i) {
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         static_cast<jbyte_t>(digest.bytes[i]));
  }
#endif
}

jbool_t fr_gouv_ssi_nativeimpl_Sha256NativeBridge_update(
    std::shared_ptr<JC_Array> state, std::shared_ptr<JC_Array> input,
    const jshort_t inputOffset, const jshort_t inputLength) {
  constexpr uint16_t state_capacity = 2048;
  constexpr uint16_t max_input_length = state_capacity - 2;
  if (!state || !input) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE ||
      input->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (inputOffset < 0 || inputLength < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*state, 0, state_capacity);
  checkArrayRange(*input, inputOffset, inputLength);

  const uint16_t current_length = static_cast<uint16_t>(
      (static_cast<uint8_t>(state->getByteEntry(max_input_length)) << 8) |
      static_cast<uint8_t>(state->getByteEntry(max_input_length + 1)));
  if (current_length > max_input_length ||
      static_cast<uint32_t>(current_length) +
              static_cast<uint16_t>(inputLength) >
          max_input_length) {
    return FALSE;
  }
  for (uint16_t index = 0; index < static_cast<uint16_t>(inputLength);
       ++index) {
    state->setByteEntry(
        static_cast<uint16_t>(current_length + index),
        input->getByteEntry(static_cast<uint16_t>(inputOffset + index)));
  }
  const uint16_t updated_length = static_cast<uint16_t>(
      current_length + static_cast<uint16_t>(inputLength));
  state->setByteEntry(max_input_length,
                      static_cast<jbyte_t>(updated_length >> 8));
  state->setByteEntry(max_input_length + 1,
                      static_cast<jbyte_t>(updated_length));
  return TRUE;
}

void fr_gouv_ssi_nativeimpl_Sha256NativeBridge_doFinal(
    std::shared_ptr<JC_Array> state, std::shared_ptr<JC_Array> output,
    const jshort_t outputOffset) {
  constexpr uint16_t state_capacity = 2048;
  constexpr uint16_t max_input_length = state_capacity - 2;
  if (!state || !output) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (outputOffset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*state, 0, state_capacity);
  checkArrayRange(*output, outputOffset, 32);
  const uint16_t input_length = static_cast<uint16_t>(
      (static_cast<uint8_t>(state->getByteEntry(max_input_length)) << 8) |
      static_cast<uint8_t>(state->getByteEntry(max_input_length + 1)));
  if (input_length > max_input_length) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  fr_gouv_ssi_nativeimpl_NativeImplementation_sha256(
      state, 0, static_cast<jshort_t>(input_length), output, outputOffset);
  for (uint16_t index = 0; index < input_length; ++index) {
    state->setByteEntry(index, 0);
  }
  state->setByteEntry(max_input_length, 0);
  state->setByteEntry(max_input_length + 1, 0);
}

void fr_gouv_ssi_nativeimpl_Sha256NativeBridge_reset(
    std::shared_ptr<JC_Array> state) {
  constexpr uint16_t state_capacity = 2048;
  constexpr uint16_t max_input_length = state_capacity - 2;
  if (!state) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  checkArrayRange(*state, 0, state_capacity);
  const uint16_t input_length = static_cast<uint16_t>(
      (static_cast<uint8_t>(state->getByteEntry(max_input_length)) << 8) |
      static_cast<uint8_t>(state->getByteEntry(max_input_length + 1)));
  if (input_length <= max_input_length) {
    for (uint16_t index = 0; index < input_length; ++index) {
      state->setByteEntry(index, 0);
    }
  }
  state->setByteEntry(max_input_length, 0);
  state->setByteEntry(max_input_length + 1, 0);
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
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(
    const jshort_t length, const jbyte_t event) {
  if (length < 0) {
    throw Exceptions::NegativeArraySizeException;
  }

  Heap &heap = Context::getActiveContext().getHeap();
  // Object[] has no applet-specific component class. The all-ones class
  // reference is the wildcard used for an array that accepts any reference.
  constexpr jc_cp_offset_t any_reference_type = 0xffff;
  return std::make_shared<JC_Array>(heap, static_cast<uint16_t>(length),
                                    JAVA_ARRAY_T_REFERENCE,
                                    any_reference_type, true,
                                    getTransientClearEvent(event));
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(jbyte_t,
                                                                   jshort_t) {
  // TODO: to implement;
  throw Exceptions::NotYetImplemented;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAID() {
  Context &context = Context::getActiveContext();
  const jref_t cached_aid = context.getAppletAIDObjectRef();
  if (!cached_aid.isNullPointer()) {
    return cached_aid;
  }

  uint8_t aid[16]{};
  uint8_t aid_length = 0;
  if (!context.copyAppletAID(aid, aid_length)) {
    throw Exceptions::RuntimeException;
  }

  constexpr uint8_t framework_aid[] = {0xa0, 0x00, 0x00, 0x00,
                                       0x62, 0x01, 0x01};
  Package applet_package(context.getAppletPackageID());
  const JC_Cap applet_cap = applet_package.getCap();
  const auto *imports = applet_cap.getImport();
  if (imports == nullptr) {
    throw Exceptions::RuntimeException;
  }

  Import_Handler import_handler(applet_package);
  const jc_cap_package_info *framework_info = nullptr;
  for (uint8_t index = 0; index < imports->count; ++index) {
    const auto *candidate = import_handler.getPackageAID(index);
    if (candidate != nullptr && candidate->AID_length == sizeof(framework_aid) &&
        std::memcmp(candidate->AID, framework_aid, sizeof(framework_aid)) == 0) {
      framework_info = candidate;
      break;
    }
  }
  if (framework_info == nullptr) {
    throw Exceptions::RuntimeException;
  }

  const jpackage_ID_t framework_package_id =
      import_handler.getPackageIndex(framework_info);
  Package framework_package(framework_package_id);
  constexpr uint8_t aid_class_export_token = 6;
  const uint8_t aid_class_token =
      remap_imported_class_token(*framework_info, aid_class_export_token);
  const jclass_index_t aid_class_index = HTONS(
      Export_Handler(framework_package).getExportedClassOffset(aid_class_token));

  Heap &heap = context.getHeap();
  const jref_t aid_bytes_ref = heap.addArray(aid_length, JAVA_ARRAY_T_BYTE);
  const auto aid_bytes = heap.getArray(aid_bytes_ref);
  for (uint8_t index = 0; index < aid_length; ++index) {
    aid_bytes->setByteEntry(index, static_cast<jbyte_t>(aid[index]));
  }

  const jref_t aid_object_ref =
      heap.addInstance(framework_package_id, aid_class_index);
  const auto aid_object = heap.getInstance(aid_object_ref);
  if (aid_object->getNumberOfFields() != 1) {
    throw Exceptions::RuntimeException;
  }
  aid_object->setField_Reference(0, aid_bytes_ref);
  context.setAppletAIDObjectRef(aid_object_ref);
  return aid_object_ref;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_beginTransaction() {
  return FlashMemory_Handler::beginPersistentWriteTransaction() ? 0 : 1;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_abortTransaction() {
  if (!FlashMemory_Handler::persistentWriteTransactionActive()) return 1;
  return FlashMemory_Handler::rollbackPersistentWriteTransaction() ? 0 : 2;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_commitTransaction() {
  if (!FlashMemory_Handler::persistentWriteTransactionActive()) return 1;
  return FlashMemory_Handler::commitPersistentWriteTransaction() ? 0 : 2;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_getTransactionDepth() {
  return FlashMemory_Handler::persistentWriteTransactionActive() ? 1 : 0;
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
  // This runtime has no cross-applet shareable-interface context switches.
  // Consequently there is no previous applet context to report.
  return jref_t{};
}

jshort_t
fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(jbyte_t type) {
  if (type < 0 || type > 2) {
    throw Exceptions::SystemException;
  }
#ifdef PC_VERSION
  const uint32_t available = 0;
#else
  const uint32_t available =
      type == 0 ? choupi_available_persistent_memory_bytes()
                : choupi_available_transient_memory_bytes();
#endif
  return static_cast<jshort_t>(
      std::min<uint32_t>(available, 0x7fffu));
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
    std::shared_ptr<JC_Array> output, jshort_t offset, jbyte_t type) {
  if (output == nullptr) {
    throw Exceptions::NullPointerException;
  }
  if (offset < 0 || static_cast<uint32_t>(offset) + 2 > output->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  if (type < 0 || type > 2) {
    throw Exceptions::SystemException;
  }
#ifdef PC_VERSION
  const uint32_t available = 0;
#else
  const uint32_t available =
      type == 0 ? choupi_available_persistent_memory_bytes()
                : choupi_available_transient_memory_bytes();
#endif
  output->setShortEntry(static_cast<uint16_t>(offset),
                        static_cast<jshort_t>(available >> 16));
  output->setShortEntry(static_cast<uint16_t>(offset + 1),
                        static_cast<jshort_t>(available & 0xffffu));
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
