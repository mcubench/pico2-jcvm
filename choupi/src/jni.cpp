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
#include "interpretor.hpp"
#include "heap.hpp"
#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_handlers/jc_method.hpp"
#include "jc_handlers/package.hpp"
#include "jc_utils.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "jc_types/jc_instance.hpp"
#include "jc_types/jref_t.hpp"
#include "types.hpp"
#include "javacard-class-token-map.h"
#include "../../src/cap_store.hpp"
#include "../../src/vm_lifecycle.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace jcvm {

#ifndef PC_VERSION
extern "C" uint32_t choupi_available_transient_memory_bytes(void);
extern "C" uint32_t choupi_available_persistent_memory_bytes(void);
#endif

namespace {
class ScopedRemoteArrays {
  std::array<jref_t, 128> references{};
  uint8_t count = 0;

public:
  ~ScopedRemoteArrays() {
    for (uint8_t index = 0; index < count; ++index) {
      Heap::releaseGlobalArray(references[index]);
    }
  }

  jref_t add(const uint16_t length, const jc_array_type type) {
    if (count >= references.size()) {
      throw Exceptions::RuntimeException;
    }
    const jref_t reference = Heap::addGlobalArray(length, type);
    references[count++] = reference;
    return reference;
  }
};

struct RemoteSignatureType {
  uint8_t code = 0;
  jc_cap_class_ref class_ref{};
};

bool getRemoteExceptionType(const uint8_t exception_id, uint8_t &type) {
  switch (static_cast<Exceptions>(exception_id)) {
  case Exceptions::StackUnderflowException:
  case Exceptions::StackOverflowException:
  case Exceptions::NotYetImplemented:
    return false;
  case Exceptions::CardException:
    type = 0x21;
    return true;
  case Exceptions::UserException:
    type = 0x27;
    return true;
  case Exceptions::IOException:
    type = 0x0b;
    return true;
  case Exceptions::RemoteException:
    type = 0x0c;
    return true;
  case Exceptions::RuntimeException:
    type = 0x09;
    return true;
  case Exceptions::ArithmeticException:
    type = 0x01;
    return true;
  case Exceptions::ArrayStoreException:
    type = 0x03;
    return true;
  case Exceptions::CardRuntimeException:
    type = 0x22;
    return true;
  case Exceptions::APDUException:
    type = 0x20;
    return true;
  case Exceptions::Bio1NException:
  case Exceptions::BioException:
    type = 0x50;
    return true;
  case Exceptions::CryptoException:
    type = 0x30;
    return true;
  case Exceptions::ExternalException:
    type = 0x60;
    return true;
  case Exceptions::ISOException:
    type = 0x23;
    return true;
  case Exceptions::PINException:
    type = 0x24;
    return true;
  case Exceptions::StringException:
    type = 0x22; // StringException extends CardRuntimeException.
    return true;
  case Exceptions::ServiceException:
    type = 0x40;
    return true;
  case Exceptions::SystemException:
    type = 0x25;
    return true;
  case Exceptions::TLVException:
    type = 0x70;
    return true;
  case Exceptions::TransactionException:
    type = 0x26;
    return true;
  case Exceptions::UtilException:
    type = 0x80;
    return true;
  case Exceptions::ClassCastException:
    type = 0x04;
    return true;
  case Exceptions::IndexOutOfBoundsException:
    type = 0x06;
    return true;
  case Exceptions::ArrayIndexOutOfBoundsException:
    type = 0x02;
    return true;
  case Exceptions::NegativeArraySizeException:
    type = 0x07;
    return true;
  case Exceptions::NullPointerException:
    type = 0x08;
    return true;
  case Exceptions::SecurityException:
    type = 0x0a;
    return true;
  case Exceptions::FullMemoryException:
    type = 0x00; // java.lang.Throwable is the closest defined API type.
    return true;
  }
  return false;
}

struct RemoteJavaExceptionInfo {
  uint8_t type = 0;
  uint16_t reason = 0;
  bool is_subclass = false;
};

bool getRemoteJavaExceptionInfo(Context &context,
                                const jref_t exception_reference,
                                RemoteJavaExceptionInfo &result) {
  if (exception_reference.isNullPointer() || exception_reference.isArray()) {
    return false;
  }
  const auto instance = context.getHeap().getInstance(exception_reference);
  if (instance == nullptr) {
    return false;
  }

  static constexpr uint8_t java_lang_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x00, 0x01};
  static constexpr uint8_t javacard_framework_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x01};
  static constexpr uint8_t javacard_security_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x02};
  struct JavaExceptionClass {
    uint8_t token;
    uint8_t type;
  };
  struct FrameworkExceptionClass {
    uint8_t embedded_token;
    uint8_t type;
  };
  static constexpr JavaExceptionClass java_exception_classes[] = {
      {0, 0x00},  // Throwable
      {1, 0x05},  // Exception
      {3, 0x09},  // RuntimeException
      {4, 0x06},  // IndexOutOfBoundsException
      {5, 0x02},  // ArrayIndexOutOfBoundsException
      {6, 0x07},  // NegativeArraySizeException
      {7, 0x08},  // NullPointerException
      {8, 0x04},  // ClassCastException
      {9, 0x01},  // ArithmeticException
      {10, 0x0a}, // SecurityException
      {11, 0x03}, // ArrayStoreException
  };
  static constexpr FrameworkExceptionClass framework_exception_classes[] = {
      {8, 0x22},  // CardRuntimeException
      {10, 0x20}, // APDUException
      {18, 0x21}, // CardException
      {19, 0x24}, // PINException
      {21, 0x23}, // ISOException
      {22, 0x25}, // SystemException
      {23, 0x26}, // TransactionException
      {24, 0x27}, // UserException
  };

  Package package(instance->getPackageID());
  const auto class_component = package.getCap().getClass();
  if (class_component == nullptr) {
    return false;
  }
  const auto class_bytes = class_component->claz();
  uint16_t class_index = instance->getClassIndex();
  if (class_index >= class_bytes.size()) {
    return false;
  }
  auto *class_info = reinterpret_cast<const jc_cap_class_info *>(
      class_bytes.data() + class_index);

  for (uint8_t depth = 0; depth < 64 && class_info != nullptr; ++depth) {
    const auto *header = package.getCap().getHeader();
    if (header == nullptr) {
      return false;
    }
    // Java Card API exceptions are regular Java objects when an applet calls
    // their throwIt() method. Recognize them before walking to java.lang
    // superclasses and carry their reason field in the RMI response. These are
    // embedded framework export tokens; their JC305 token mappings differ.
    if (header->package.AID_length == sizeof(javacard_framework_aid) &&
        std::memcmp(header->package.AID, javacard_framework_aid,
                    sizeof(javacard_framework_aid)) == 0) {
      Export_Handler exports(package);
      ConstantPool_Handler resolver(package);
      for (const auto &candidate : framework_exception_classes) {
        const uint16_t candidate_index = HTONS(
            exports.getExportedClassOffset(candidate.embedded_token));
        const auto *candidate_class =
            resolver.getClassFromClassIndex(candidate_index);
        if (candidate_class == class_info) {
          result.type = candidate.type;
          result.reason = static_cast<uint16_t>(instance->getField_Short(0));
          result.is_subclass =
              instance->getPackageID() != package.getPackageID() ||
              instance->getClassIndex() != class_index;
          return true;
        }
      }
    }
    // CryptoException belongs to javacard.security rather than the framework
    // package. Resolve it from the export table because optional providers
    // change byte offsets in the security Class component.
    if (header->package.AID_length == sizeof(javacard_security_aid) &&
        std::memcmp(header->package.AID, javacard_security_aid,
                    sizeof(javacard_security_aid)) == 0) {
      constexpr uint8_t jc305_crypto_exception_class_token = 12;
      const uint8_t crypto_exception_export_token =
          jc305_javacard_security_class_tokens[
              jc305_crypto_exception_class_token];
      Export_Handler exports(package);
      ConstantPool_Handler resolver(package);
      const uint16_t crypto_exception_index = HTONS(
          exports.getExportedClassOffset(crypto_exception_export_token));
      const auto *crypto_exception_class =
          resolver.getClassFromClassIndex(crypto_exception_index);
      if (crypto_exception_class == class_info) {
        result.type = 0x30; // CryptoException
        result.reason = static_cast<uint16_t>(instance->getField_Short(0));
        result.is_subclass =
            instance->getPackageID() != package.getPackageID() ||
            instance->getClassIndex() != class_index;
        return true;
      }
    }
    if (header->package.AID_length == sizeof(java_lang_aid) &&
        std::memcmp(header->package.AID, java_lang_aid,
                    sizeof(java_lang_aid)) == 0) {
      ConstantPool_Handler resolver(package);
      Export_Handler exports(package);
      for (const auto &candidate : java_exception_classes) {
        const uint16_t candidate_index =
            HTONS(exports.getExportedClassOffset(candidate.token));
        const auto *candidate_class =
            resolver.getClassFromClassIndex(candidate_index);
        if (candidate_class == class_info) {
          result.type = candidate.type;
          result.reason = 0;
          result.is_subclass =
              instance->getPackageID() != package.getPackageID() ||
              instance->getClassIndex() != class_index;
          return true;
        }
      }
    }

    if (class_info->isObjectClass()) {
      break;
    }
    const auto superclass = ConstantPool_Handler(package)
                                .classref2class(class_info->super_class_ref);
    package = superclass.first;
    class_info = superclass.second;
    if (class_info == nullptr) {
      return false;
    }
    const auto superclass_component = package.getCap().getClass();
    if (superclass_component == nullptr) {
      return false;
    }
    const auto superclass_bytes = superclass_component->claz();
    const auto *class_pointer = reinterpret_cast<const uint8_t *>(class_info);
    if (class_pointer < superclass_bytes.data() ||
        class_pointer >= superclass_bytes.data() + superclass_bytes.size()) {
      return false;
    }
    class_index = static_cast<uint16_t>(class_pointer - superclass_bytes.data());
  }
  return false;
}

ClearEvent getTransientClearEvent(const jbyte_t event) {
  switch (event) {
  case static_cast<jbyte_t>(ClearEvent::CLEAR_ON_RESET):
    return ClearEvent::CLEAR_ON_RESET;
  case static_cast<jbyte_t>(ClearEvent::CLEAR_ON_DESELECT):
    return ClearEvent::CLEAR_ON_DESELECT;
  default:
    Context::getActiveContext().setPendingSystemExceptionReason(1);
    throw Exceptions::SystemException;
  }
}

std::shared_ptr<JC_Array> makeTransientArray(const jshort_t length,
                                             const jbyte_t event,
                                             const jc_array_type type) {
#ifndef PC_VERSION
  std::printf("CHOUPI:JC_TRANSIENT_ALLOC begin length=%d event=%u type=%u\r\n",
              static_cast<int>(length), static_cast<unsigned>(event),
              static_cast<unsigned>(type));
  std::fflush(stdout);
#endif
  if (length < 0) {
    throw Exceptions::NegativeArraySizeException;
  }

  Heap &heap = Context::getActiveContext().getHeap();
  std::shared_ptr<JC_Array> array;
  try {
    array = std::make_shared<JC_Array>(
        heap, static_cast<uint16_t>(length), type, true,
        getTransientClearEvent(event));
  } catch (const std::bad_alloc &) {
    Context::getActiveContext().setPendingSystemExceptionReason(2);
    throw Exceptions::SystemException;
  }
#ifndef PC_VERSION
  std::printf("CHOUPI:JC_TRANSIENT_ALLOC ok length=%d event=%u type=%u\r\n",
              static_cast<int>(length), static_cast<unsigned>(event),
              static_cast<unsigned>(type));
  std::fflush(stdout);
#endif
  return array;
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

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet(
    const jref_t) {
  return choupi_vm_is_selecting_applet() != 0 ? TRUE : FALSE;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isAppletReselecting() {
  return choupi_vm_is_reselecting_applet() != 0 ? TRUE : FALSE;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isAppletSelecting() {
  return choupi_vm_is_selecting_applet() != 0 ? TRUE : FALSE;
}

namespace {
void requireAppletRegistrationAllowed(Context &context,
                                     const jref_t applet_reference) {
  if (applet_reference.isNullPointer() || applet_reference.isArray()) {
    throw Exceptions::NullPointerException;
  }
  if (!context.isAppletInstallationActive() || context.wasAppletRegistered()) {
    context.setPendingSystemExceptionReason(4); // SystemException.ILLEGAL_AID
    throw Exceptions::SystemException;
  }
}
} // namespace

void fr_gouv_ssi_nativeimpl_NativeImplementation_registerAppletUsingInstallAID(
    const jref_t applet_reference) {
  Context &context = Context::getActiveContext();
  requireAppletRegistrationAllowed(context, applet_reference);
  context.markAppletRegistered(applet_reference);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_registerApplet(
    const jref_t applet_reference, std::shared_ptr<JC_Array> requested_aid,
    const jshort_t offset, const jbyte_t length) {
  Context &context = Context::getActiveContext();
  requireAppletRegistrationAllowed(context, applet_reference);
  if (!requested_aid) {
    throw Exceptions::NullPointerException;
  }
  if (requested_aid->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  const uint8_t aid_length = static_cast<uint8_t>(length);
  if (aid_length < 5 || aid_length > 16) {
    context.setPendingSystemExceptionReason(1); // SystemException.ILLEGAL_VALUE
    throw Exceptions::SystemException;
  }
  if (offset < 0 || static_cast<uint32_t>(offset) + aid_length >
                        requested_aid->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  uint8_t installed_aid[16]{};
  uint8_t installed_aid_length = 0;
  if (!context.copyAppletAID(installed_aid, installed_aid_length) ||
      installed_aid_length != aid_length) {
    context.setPendingSystemExceptionReason(4); // SystemException.ILLEGAL_AID
    throw Exceptions::SystemException;
  }
  for (uint8_t index = 0; index < aid_length; ++index) {
    if (static_cast<uint8_t>(requested_aid->getByteEntry(
            static_cast<uint16_t>(offset + index))) != installed_aid[index]) {
      context.setPendingSystemExceptionReason(4); // SystemException.ILLEGAL_AID
      throw Exceptions::SystemException;
    }
  }
  context.markAppletRegistered(applet_reference);
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_exportRemoteObject(
    const jref_t object_reference) {
  Context &context = Context::getActiveContext();
  if (object_reference.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }
  if (object_reference.isArray() ||
      !context.getHeap().getInstance(object_reference)) {
    throw Exceptions::SecurityException;
  }
  const uint16_t object_id = context.exportRemoteObject(object_reference);
  if (object_id == 0xffff) {
    context.setPendingSystemExceptionReason(5); // SystemException.NO_RESOURCE
    throw Exceptions::SystemException;
  }
  return static_cast<jshort_t>(object_id);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_unexportRemoteObject(
    const jref_t object_reference) {
  Context &context = Context::getActiveContext();
  if (object_reference.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }
  if (object_reference.isArray() ||
      !context.getHeap().getInstance(object_reference)) {
    throw Exceptions::SecurityException;
  }
  context.unexportRemoteObject(object_reference);
}

jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_retainRemoteObjectForSession(
    const jref_t object_reference) {
  Context &context = Context::getActiveContext();
  if (object_reference.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }
  if (object_reference.isArray() ||
      !context.getHeap().getInstance(object_reference)) {
    throw Exceptions::SecurityException;
  }
  return context.retainRemoteObjectForSession(object_reference) ? TRUE : FALSE;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_getRemoteObjectId(
    const jref_t object_reference) {
  Context &context = Context::getActiveContext();
  if (object_reference.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }
  if (object_reference.isArray() ||
      !context.getHeap().getInstance(object_reference)) {
    throw Exceptions::SecurityException;
  }
  return static_cast<jshort_t>(context.remoteObjectId(object_reference));
}

static uint16_t encodeRemoteReferenceDescriptor(
    Context &context, const jref_t object_reference,
    const bool with_interfaces, uint8_t *output,
    const uint16_t output_capacity) {
  if (object_reference.isNullPointer() || object_reference.isArray() ||
      output == nullptr) {
    return 0;
  }
  const uint16_t object_id = context.remoteObjectId(object_reference);
  if (object_id == 0xffff) {
    return 0;
  }

  const auto instance = context.getHeap().getInstance(object_reference);
  if (instance == nullptr) {
    return 0;
  }
  Package metadata_package(instance->getPackageID());
  const auto class_component = metadata_package.getCap().getClass();
  if (class_component == nullptr) {
    return 0;
  }
  const auto class_bytes = class_component->claz();
  const uint16_t class_index = instance->getClassIndex();
  if (class_index >= class_bytes.size()) {
    return 0;
  }

  auto *class_info = reinterpret_cast<const jc_cap_class_info *>(
      class_bytes.data() + class_index);
  const jc_cap_remote_interface_info *remote_info = nullptr;
  for (uint8_t depth = 0; depth < 64 && class_info != nullptr; ++depth) {
    if ((class_info->flags & JC_CAP_CLASS_ACC_REMOTE) != 0) {
      remote_info = class_info->remoteInterfaceInfo();
      break;
    }
    if (class_info->isObjectClass()) {
      break;
    }
    const auto superclass =
        ConstantPool_Handler(metadata_package)
            .classref2class(class_info->super_class_ref);
    metadata_package = superclass.first;
    class_info = superclass.second;
  }
  if (class_info == nullptr || remote_info == nullptr) {
    return 0;
  }

  const auto *header = metadata_package.getCap().getHeader();
  if (header == nullptr || header->major_version != 2 ||
      header->minor_version < 2 || header->packageName() == nullptr ||
      header->packageNameLength() == 0) {
    return 0;
  }

  std::array<uint8_t, 255> descriptor{};
  uint16_t descriptor_length = 0;
  if (with_interfaces) {
    const uint8_t count = remote_info->remoteInterfacesCount();
    if (count == 0 || count >= 16) {
      return 0;
    }
    std::array<jc_cap_remote_interface_name, 15> names{};
    ConstantPool_Handler resolver(metadata_package);
    for (uint8_t index = 0; index < count; ++index) {
      const auto resolved =
          resolver.classref2interface(remote_info->remoteInterfaces()[index]);
      const auto *interface_info = resolved.second;
      const auto *interface_header = resolved.first.getCap().getHeader();
      if (interface_info == nullptr || interface_header == nullptr ||
          interface_header->packageName() == nullptr ||
          interface_header->packageNameLength() == 0 ||
          interface_info->interfaceName() == nullptr ||
          interface_info->interfaceNameLength() == 0) {
        return 0;
      }
      names[index] = {interface_header->packageName(),
                      interface_header->packageNameLength(),
                      interface_info->interfaceName(),
                      interface_info->interfaceNameLength()};
    }
    descriptor_length = encodeRemoteReferenceWithInterfaces(
        object_id, remote_info->hashModifier(),
        remote_info->hashModifierLength(), names.data(), count,
        descriptor.data(), static_cast<uint16_t>(descriptor.size()));
  } else {
    descriptor_length = encodeRemoteReferenceWithClass(
        object_id, header->packageName(), header->packageNameLength(),
        *remote_info, descriptor.data(),
        static_cast<uint16_t>(descriptor.size()));
  }
  if (descriptor_length == 0 || descriptor_length > output_capacity) {
    return 0;
  }
  std::memcpy(output, descriptor.data(), descriptor_length);
  return descriptor_length;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_encodeRemoteReference(
    const jref_t object_reference, const jbool_t with_interfaces,
    std::shared_ptr<JC_Array> output) {
  Context &context = Context::getActiveContext();
  if (object_reference.isNullPointer() || output == nullptr) {
    throw Exceptions::NullPointerException;
  }
  if (object_reference.isArray() ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::SecurityException;
  }

  uint8_t descriptor[255]{};
  const uint16_t descriptor_length = encodeRemoteReferenceDescriptor(
      context, object_reference, with_interfaces != 0, descriptor,
      static_cast<uint16_t>(sizeof(descriptor)));
  if (descriptor_length == 0 || descriptor_length > output->size()) {
    return 0;
  }
  for (uint16_t index = 0; index < descriptor_length; ++index) {
    output->setByteEntry(index, static_cast<jbyte_t>(descriptor[index]));
  }
  return static_cast<jshort_t>(descriptor_length);
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_processRemoteInvoke(
    std::shared_ptr<JC_Array> apdu_buffer,
    const jshort_t incoming_length, const jbool_t with_interfaces) {
  if (apdu_buffer == nullptr) {
    throw Exceptions::NullPointerException;
  }
  if (apdu_buffer->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }

  const auto write_error = [&apdu_buffer](const uint16_t detail) {
    apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x99));
    apdu_buffer->setByteEntry(6, static_cast<jbyte_t>(detail >> 8));
    apdu_buffer->setByteEntry(7, static_cast<jbyte_t>(detail));
    return static_cast<jshort_t>(3);
  };
  if (incoming_length < 4 ||
      static_cast<uint32_t>(incoming_length) + 9 > apdu_buffer->size()) {
    return write_error(0x0003);
  }

  const auto read_u8 = [&apdu_buffer](const uint16_t index) {
    return static_cast<uint8_t>(apdu_buffer->getByteEntry(index));
  };
  const uint16_t object_id = static_cast<uint16_t>(
      (static_cast<uint16_t>(read_u8(5)) << 8) | read_u8(6));
  const uint16_t method_hash = static_cast<uint16_t>(
      (static_cast<uint16_t>(read_u8(7)) << 8) | read_u8(8));

  Context &context = Context::getActiveContext();
  jref_t object_reference;
  if (!context.remoteObject(object_id, object_reference)) {
    return write_error(0x0001);
  }
  const auto instance = context.getHeap().getInstance(object_reference);
  if (instance == nullptr) {
    return write_error(0x0001);
  }

  Package metadata_package(instance->getPackageID());
  const jc_cap_remote_interface_info *remote_info = nullptr;
  const jc_cap_class_info *remote_class_info = nullptr;
  uint16_t remote_class_index = 0xffff;
  for (uint8_t depth = 0; depth < 64; ++depth) {
    const auto cap = metadata_package.getCap();
    const auto *class_component = cap.getClass();
    if (class_component == nullptr) {
      return write_error(0x0002);
    }
    const auto class_bytes = class_component->claz();
    const uint16_t class_index =
        metadata_package.getPackageID() == instance->getPackageID()
            ? instance->getClassIndex()
            : remote_class_index;
    if (class_index >= class_bytes.size()) {
      return write_error(0x0002);
    }
    const auto *class_info = reinterpret_cast<const jc_cap_class_info *>(
        class_bytes.data() + class_index);
    if ((class_info->flags & JC_CAP_CLASS_ACC_REMOTE) != 0) {
      remote_info = class_info->remoteInterfaceInfo();
      remote_class_info = class_info;
      remote_class_index = class_index;
      break;
    }
    if (class_info->isObjectClass()) {
      break;
    }
    const auto superclass = ConstantPool_Handler(metadata_package)
                                .classref2class(class_info->super_class_ref);
    metadata_package = superclass.first;
    if (superclass.second == nullptr) {
      return write_error(0x0002);
    }
    const auto *superclass_component = metadata_package.getCap().getClass();
    if (superclass_component == nullptr) {
      return write_error(0x0002);
    }
    const auto superclass_bytes = superclass_component->claz();
    const auto *bytes = reinterpret_cast<const uint8_t *>(superclass.second);
    if (bytes < superclass_bytes.data() ||
        bytes >= superclass_bytes.data() + superclass_bytes.size()) {
      return write_error(0x0002);
    }
    remote_class_index = static_cast<uint16_t>(
        bytes - superclass_bytes.data());
  }
  if (remote_info == nullptr || remote_class_info == nullptr) {
    return write_error(0x0002);
  }

  const jc_cap_remote_method_info *remote_method = nullptr;
  for (uint8_t index = 0; index < remote_info->remote_methods_count; ++index) {
    const auto *candidate = &remote_info->remote_methods[index];
    if (candidate->getMethodHash() == method_hash) {
      remote_method = candidate;
      break;
    }
  }
  if (remote_method == nullptr) {
    return write_error(0x0002);
  }

  const auto *class_component = metadata_package.getCap().getClass();
  if (class_component == nullptr || !class_component->hasValidSignaturePool()) {
    return write_error(0x0003);
  }
  const uint16_t signature_offset = remote_method->getSignatureOffset();
  const uint16_t signature_pool_length = class_component->signaturePoolLength();
  if (signature_offset >= signature_pool_length) {
    return write_error(0x0003);
  }
  std::array<uint8_t, 128> nibbles{};
  const uint8_t *signature = class_component->signaturePool() + signature_offset;
  const uint8_t nibble_count = signature[0];
  const uint16_t signature_bytes =
      static_cast<uint16_t>((nibble_count + 1) / 2);
  if (nibble_count == 0 || nibble_count > nibbles.size() ||
      static_cast<uint32_t>(signature_offset) + 1 + signature_bytes >
          signature_pool_length) {
    return write_error(0x0003);
  }

  for (uint16_t index = 0; index < nibble_count; ++index) {
    const uint8_t packed = signature[1 + index / 2];
    nibbles[index] = static_cast<uint8_t>(
        (index & 1) == 0 ? packed >> 4 : packed & 0x0f);
  }

  std::array<RemoteSignatureType, 64> signature_types{};
  uint8_t signature_type_count = 0;
  uint8_t nibble_cursor = 0;
  while (nibble_cursor < nibble_count) {
    if (signature_type_count >= signature_types.size()) {
      return write_error(0x0003);
    }
    auto &type = signature_types[signature_type_count++];
    type.code = nibbles[nibble_cursor++];
    if (type.code == 0x06 || type.code == 0x0e) {
      if (nibble_cursor + 4 > nibble_count) {
        return write_error(0x0003);
      }
      const uint8_t first = static_cast<uint8_t>(
          (nibbles[nibble_cursor] << 4) | nibbles[nibble_cursor + 1]);
      const uint8_t second = static_cast<uint8_t>(
          (nibbles[nibble_cursor + 2] << 4) | nibbles[nibble_cursor + 3]);
      type.class_ref.internal_classref = HTONS(
          static_cast<uint16_t>((static_cast<uint16_t>(first) << 8) | second));
      nibble_cursor = static_cast<uint8_t>(nibble_cursor + 4);
    } else if ((type.code < 1 || type.code > 5) &&
               (type.code < 0x0a || type.code > 0x0d)) {
      return write_error(0x0003);
    }
  }
  if (signature_type_count == 0) {
    return write_error(0x0003);
  }
  const uint8_t parameter_count =
      static_cast<uint8_t>(signature_type_count - 1);
  const uint8_t return_type = signature_types[signature_type_count - 1].code;

  std::array<InterpretorInitialLocal, 128> locals{};
  ScopedRemoteArrays remote_arrays;
  locals[0] = {InterpretorLocalType::Reference, object_reference.compact()};
  uint8_t local_count = 1;
  uint16_t local_slots = 1;
  uint16_t cursor = 9;
  const uint16_t command_end = static_cast<uint16_t>(9 + incoming_length - 4);
  for (uint8_t index = 0; index < parameter_count; ++index) {
    const uint8_t type = signature_types[index].code;
    if (type == 2 || type == 3) {
      if (cursor >= command_end || local_count >= locals.size()) {
        return write_error(0x0003);
      }
      const uint8_t value = read_u8(cursor++);
      if (type == 2 && value > 1) {
        return write_error(0x0003);
      }
      locals[local_count++] = {InterpretorLocalType::Byte, value};
      ++local_slots;
    } else if (type == 4) {
      if (cursor + 2 > command_end || local_count >= locals.size()) {
        return write_error(0x0003);
      }
      const uint16_t value = static_cast<uint16_t>(
          (static_cast<uint16_t>(read_u8(cursor)) << 8) |
          read_u8(static_cast<uint16_t>(cursor + 1)));
      cursor += 2;
      locals[local_count++] = {InterpretorLocalType::Short, value};
      ++local_slots;
    } else if (type == 5) {
#ifdef JCVM_INT_SUPPORTED
      if (cursor + 4 > command_end || local_count >= locals.size()) {
        return write_error(0x0003);
      }
      const uint32_t value = (static_cast<uint32_t>(read_u8(cursor)) << 24) |
                             (static_cast<uint32_t>(read_u8(cursor + 1)) << 16) |
                             (static_cast<uint32_t>(read_u8(cursor + 2)) << 8) |
                             static_cast<uint32_t>(read_u8(cursor + 3));
      cursor += 4;
      locals[local_count++] = {InterpretorLocalType::Int, value};
      local_slots += 2;
#else
      return write_error(0x0003);
#endif
    } else if (type >= 0x0a && type <= 0x0d) {
      if (cursor >= command_end || local_count >= locals.size()) {
        return write_error(0x0003);
      }
      const uint8_t length = read_u8(cursor++);
      if (length == 0xff) {
        locals[local_count++] = {InterpretorLocalType::Reference,
                                 jref_t().compact()};
        ++local_slots;
        continue;
      }

      jc_array_type array_type;
      uint8_t element_width;
      switch (type) {
      case 0x0a:
        array_type = JAVA_ARRAY_T_BOOLEAN;
        element_width = 1;
        break;
      case 0x0b:
        array_type = JAVA_ARRAY_T_BYTE;
        element_width = 1;
        break;
      case 0x0c:
        array_type = JAVA_ARRAY_T_SHORT;
        element_width = 2;
        break;
      case 0x0d:
#ifdef JCVM_INT_SUPPORTED
        array_type = JAVA_ARRAY_T_INT;
        element_width = 4;
        break;
#else
        return write_error(0x0003);
#endif
      default:
        return write_error(0x0003);
      }
      const uint16_t input_bytes =
          static_cast<uint16_t>(length * element_width);
      if (cursor + input_bytes > command_end) {
        return write_error(0x0003);
      }

      jref_t array_reference;
      std::shared_ptr<JC_Array> array;
      try {
        array_reference = remote_arrays.add(length, array_type);
        array = Heap::getGlobalArray(array_reference);
      } catch (...) {
        return write_error(0x0004);
      }
      for (uint16_t element = 0; element < length; ++element) {
        if (array_type == JAVA_ARRAY_T_BOOLEAN ||
            array_type == JAVA_ARRAY_T_BYTE) {
          const uint8_t value = read_u8(cursor++);
          if (array_type == JAVA_ARRAY_T_BOOLEAN && value > 1) {
            return write_error(0x0003);
          }
          array->setByteEntry(element, static_cast<jbyte_t>(value));
        } else if (array_type == JAVA_ARRAY_T_SHORT) {
          const uint16_t value = static_cast<uint16_t>(
              (static_cast<uint16_t>(read_u8(cursor)) << 8) |
              read_u8(static_cast<uint16_t>(cursor + 1)));
          cursor += 2;
          array->setShortEntry(element, static_cast<jshort_t>(value));
        } else {
#ifdef JCVM_INT_SUPPORTED
          const uint32_t value = (static_cast<uint32_t>(read_u8(cursor)) << 24) |
                                 (static_cast<uint32_t>(read_u8(cursor + 1))
                                  << 16) |
                                 (static_cast<uint32_t>(read_u8(cursor + 2))
                                  << 8) |
                                 static_cast<uint32_t>(read_u8(cursor + 3));
          cursor += 4;
          array->setIntEntry(element, static_cast<jint_t>(value));
#endif
        }
      }
      locals[local_count++] = {InterpretorLocalType::Reference,
                               array_reference.compact()};
      ++local_slots;
    } else {
      // Java Card RMI does not allow object or remote-reference parameters.
      return write_error(0x0003);
    }
  }
  if (cursor != command_end) {
    return write_error(0x0003);
  }

  try {
    ConstantPool_Handler class_resolver(metadata_package);
    const jc_cap_virtual_method_ref_info method_reference{
        class_resolver.getClassRefFromClassIndex(remote_class_index),
        remote_method->virtual_method_token};
    const auto resolved = Class_Handler(metadata_package).getMethodOffset(
        method_reference, instance->getPackageID(), instance->getClassIndex());
    Method_Handler method_handler(context);
    method_handler.setPackage(resolved.first);
    const uint8_t resolved_argument_count =
        method_handler.getArgumentCount(resolved.second);
    if (resolved_argument_count != local_slots) {
      return write_error(0x0003);
    }

    Interpretor interpreter(context, resolved.second,
                            resolved.first.getPackageID(), locals.data(),
                            local_count, true);
    if (interpreter.run() != 0x9000) {
      uint8_t exception_type = 0;
      uint8_t exception_tag = 0x82;
      uint16_t reason = interpreter.lastExceptionReason();
      if (!getRemoteExceptionType(interpreter.lastExceptionId(),
                                  exception_type)) {
        RemoteJavaExceptionInfo java_exception;
        if (!getRemoteJavaExceptionInfo(
                context,
                jref_t(interpreter.lastUncaughtExceptionReference()),
                java_exception)) {
          return write_error(0xffff);
        }
        exception_type = java_exception.type;
        exception_tag = java_exception.is_subclass ? 0x83 : 0x82;
        reason = java_exception.reason;
      }
      apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(exception_tag));
      apdu_buffer->setByteEntry(6, static_cast<jbyte_t>(exception_type));
      apdu_buffer->setByteEntry(7, static_cast<jbyte_t>(reason >> 8));
      apdu_buffer->setByteEntry(8, static_cast<jbyte_t>(reason));
      return 4;
    }

    uint32_t return_value = 0;
    uint8_t return_width = 0;
    Stack &stack = context.getStack();
    switch (return_type) {
    case 1:
      break;
    case 2:
    case 3:
      return_value = static_cast<uint8_t>(stack.pop_Byte());
      return_width = 1;
      break;
    case 4:
      return_value = static_cast<uint16_t>(stack.pop_Short());
      return_width = 2;
      break;
    case 5:
#ifdef JCVM_INT_SUPPORTED
      return_value = static_cast<uint32_t>(stack.pop_Int());
      return_width = 4;
#else
      return write_error(0x0003);
#endif
      break;
    case 6: {
      const jref_t reference = stack.pop_Reference();
      if (reference.isNullPointer()) {
        apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x81));
        apdu_buffer->setByteEntry(6, static_cast<jbyte_t>(0xff));
        apdu_buffer->setByteEntry(7, static_cast<jbyte_t>(0xff));
        return 3;
      }
      if (reference.isArray()) {
        return write_error(0x0005);
      }
      const auto returned_instance = context.getHeap().getInstance(reference);
      if (returned_instance == nullptr) {
        return write_error(0x0005);
      }

      const auto declared_interface =
          class_resolver.classref2interface(
              signature_types[signature_type_count - 1].class_ref);
      const auto *actual_class_component =
          Package(returned_instance->getPackageID()).getCap().getClass();
      if (declared_interface.second == nullptr ||
          actual_class_component == nullptr) {
        return write_error(0x0003);
      }
      const auto actual_class_bytes = actual_class_component->claz();
      if (returned_instance->getClassIndex() >= actual_class_bytes.size()) {
        return write_error(0x0005);
      }
      const auto *actual_class = reinterpret_cast<const jc_cap_class_info *>(
          actual_class_bytes.data() + returned_instance->getClassIndex());
      if (!Class_Handler::docheckcast(
              {Package(returned_instance->getPackageID()),
               reinterpret_cast<const uint8_t *>(actual_class)},
              {declared_interface.first,
               reinterpret_cast<const uint8_t *>(declared_interface.second)})) {
        return write_error(0x0005);
      }

      const uint16_t object_id = context.remoteObjectId(reference);
      if (object_id == 0xffff) {
        apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x82));
        apdu_buffer->setByteEntry(6, static_cast<jbyte_t>(0x40));
        apdu_buffer->setByteEntry(7, static_cast<jbyte_t>(0x00));
        apdu_buffer->setByteEntry(8, static_cast<jbyte_t>(0x07));
        return 4;
      }
      if (!context.retainRemoteObjectForSession(reference)) {
        apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x82));
        apdu_buffer->setByteEntry(6, static_cast<jbyte_t>(0x40));
        apdu_buffer->setByteEntry(7, static_cast<jbyte_t>(0x00));
        apdu_buffer->setByteEntry(8, static_cast<jbyte_t>(0x07));
        return 4;
      }

      std::array<uint8_t, 255> descriptor{};
      const uint16_t descriptor_length = encodeRemoteReferenceDescriptor(
          context, reference, with_interfaces != 0, descriptor.data(),
          static_cast<uint16_t>(std::min<size_t>(
              descriptor.size(), apdu_buffer->size() - 7)));
      if (descriptor_length == 0 ||
          static_cast<uint32_t>(descriptor_length) + 7 >
              apdu_buffer->size()) {
        return write_error(0x0005);
      }
      apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x81));
      for (uint16_t index = 0; index < descriptor_length; ++index) {
        apdu_buffer->setByteEntry(static_cast<uint16_t>(6 + index),
                                  static_cast<jbyte_t>(descriptor[index]));
      }
      return static_cast<jshort_t>(1 + descriptor_length);
    }
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d: {
      const jref_t reference = stack.pop_Reference();
      if (reference.isNullPointer()) {
        apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x81));
        apdu_buffer->setByteEntry(6, static_cast<jbyte_t>(0xff));
        apdu_buffer->setByteEntry(7, static_cast<jbyte_t>(0xff));
        return 3;
      }

      const auto array = reference.isGlobalArray()
                             ? Heap::getGlobalArray(reference)
                             : context.getHeap().getArray(reference);
      jc_array_type expected_type;
      uint8_t element_width;
      switch (return_type) {
      case 0x0a:
        expected_type = JAVA_ARRAY_T_BOOLEAN;
        element_width = 1;
        break;
      case 0x0b:
        expected_type = JAVA_ARRAY_T_BYTE;
        element_width = 1;
        break;
      case 0x0c:
        expected_type = JAVA_ARRAY_T_SHORT;
        element_width = 2;
        break;
      default:
#ifdef JCVM_INT_SUPPORTED
        expected_type = JAVA_ARRAY_T_INT;
        element_width = 4;
#else
        return write_error(0x0005);
#endif
        break;
      }
      if (!array || array->getType() != expected_type ||
          array->size() >= 0xff) {
        return write_error(0x0005);
      }
      const uint16_t payload_length =
          static_cast<uint16_t>(1 + array->size() * element_width);
      if (static_cast<uint32_t>(6) + payload_length > apdu_buffer->size()) {
        return write_error(0x0005);
      }
      apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x81));
      apdu_buffer->setByteEntry(6,
                                static_cast<jbyte_t>(array->size()));
      uint16_t output = 7;
      for (uint16_t element = 0; element < array->size(); ++element) {
        if (element_width == 1) {
          const uint8_t value =
              static_cast<uint8_t>(array->getByteEntry(element));
          apdu_buffer->setByteEntry(output++, static_cast<jbyte_t>(value));
        } else if (element_width == 2) {
          const uint16_t value =
              static_cast<uint16_t>(array->getShortEntry(element));
          apdu_buffer->setByteEntry(output++,
                                    static_cast<jbyte_t>(value >> 8));
          apdu_buffer->setByteEntry(output++,
                                    static_cast<jbyte_t>(value));
        } else {
#ifdef JCVM_INT_SUPPORTED
          const uint32_t value =
              static_cast<uint32_t>(array->getIntEntry(element));
          apdu_buffer->setByteEntry(output++,
                                    static_cast<jbyte_t>(value >> 24));
          apdu_buffer->setByteEntry(output++,
                                    static_cast<jbyte_t>(value >> 16));
          apdu_buffer->setByteEntry(output++,
                                    static_cast<jbyte_t>(value >> 8));
          apdu_buffer->setByteEntry(output++,
                                    static_cast<jbyte_t>(value));
#endif
        }
      }
      return static_cast<jshort_t>(2 + array->size() * element_width);
    }
    default:
      return write_error(0x0003);
    }

    apdu_buffer->setByteEntry(5, static_cast<jbyte_t>(0x81));
    for (uint8_t index = 0; index < return_width; ++index) {
      const uint8_t shift = static_cast<uint8_t>(
          (return_width - index - 1) * 8);
      apdu_buffer->setByteEntry(static_cast<uint16_t>(6 + index),
                                static_cast<jbyte_t>(return_value >> shift));
    }
    return static_cast<jshort_t>(1 + return_width);
  } catch (Exceptions) {
    return write_error(0x0002);
  } catch (...) {
    return write_error(0x0002);
  }
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getRemoteObject(
    const jshort_t object_id) {
  Context &context = Context::getActiveContext();
  jref_t reference;
  if (!context.remoteObject(static_cast<uint16_t>(object_id), reference)) {
    return jref_t();
  }
  return reference;
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

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getCurrentAPDU() {
  Context &context = Context::getActiveContext();
  const jref_t reference = context.getAPDUObjectRef();
  if (!context.isAPDUProcessActive() || reference.isNullPointer() ||
      reference.isArray()) {
    throw Exceptions::SecurityException;
  }
  return reference;
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

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isAPDUOutgoingNoChaining() {
  return Context::getActiveContext().isAPDUOutgoingNoChaining() ? TRUE : FALSE;
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

void fr_gouv_ssi_nativeimpl_NativeImplementation_resetSensitiveResult() {
  Context::getActiveContext().resetSensitiveResult();
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_setSensitiveResultBoolean(
    const jbool_t value) {
  Context::getActiveContext().setSensitiveResultBoolean(value != FALSE);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_setSensitiveResultShort(
    const jshort_t value) {
  Context::getActiveContext().setSensitiveResultShort(value);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_setSensitiveResultObject(
    const jref_t value) {
  Context::getActiveContext().setSensitiveResultObject(value);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_assertSensitiveResultObject(
    const jref_t value) {
  Context::getActiveContext().assertSensitiveResultObject(value);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_assertSensitiveResultBoolean(
    const jbool_t value) {
  Context::getActiveContext().assertSensitiveResultBoolean(value != FALSE);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_assertSensitiveResultShort(
    const jshort_t value, const jbyte_t comparison) {
  Context::getActiveContext().assertSensitiveResultShort(
      value, static_cast<uint8_t>(comparison));
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
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(
    const jshort_t length, const jbyte_t event) {
  if (length < 0) {
    throw Exceptions::NegativeArraySizeException;
  }

  Heap &heap = Context::getActiveContext().getHeap();
  // Object[] has no applet-specific component class. The all-ones class
  // reference is the wildcard used for an array that accepts any reference.
  constexpr jc_cp_offset_t any_reference_type = 0xffff;
  try {
    return std::make_shared<JC_Array>(heap, static_cast<uint16_t>(length),
                                      JAVA_ARRAY_T_REFERENCE,
                                      any_reference_type, true,
                                      getTransientClearEvent(event));
  } catch (const std::bad_alloc &) {
    Context::getActiveContext().setPendingSystemExceptionReason(2);
    throw Exceptions::SystemException;
  }
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(
    const jbyte_t type, const jshort_t length) {
  Context &context = Context::getActiveContext();
  if (length < 0) {
    throw Exceptions::NegativeArraySizeException;
  }

  jc_array_type array_type;
  switch (static_cast<uint8_t>(type)) {
  case 1:
    array_type = JAVA_ARRAY_T_BOOLEAN;
    break;
  case 2:
    array_type = JAVA_ARRAY_T_BYTE;
    break;
  case 3:
    array_type = JAVA_ARRAY_T_SHORT;
    break;
  case 4:
#ifdef JCVM_INT_SUPPORTED
    array_type = JAVA_ARRAY_T_INT;
    break;
#else
    context.setPendingSystemExceptionReason(1);
    throw Exceptions::SystemException;
#endif
  case 5:
    array_type = JAVA_ARRAY_T_REFERENCE;
    break;
  default:
    context.setPendingSystemExceptionReason(1);
    throw Exceptions::SystemException;
  }

  try {
    return Heap::addGlobalArray(static_cast<uint16_t>(length), array_type);
  } catch (const std::bad_alloc &) {
    context.setPendingSystemExceptionReason(2);
    throw Exceptions::SystemException;
  }
}

namespace {
jref_t runtimeAIDObject(Context &context, const uint8_t *aid,
                         const uint8_t aid_length) {
  const jref_t cached = Heap::findGlobalAIDObject(aid, aid_length);
  if (!cached.isNullPointer()) return cached;

  constexpr uint8_t framework_aid[] = {0xa0, 0x00, 0x00, 0x00,
                                       0x62, 0x01, 0x01};
  Package applet_package(context.getAppletPackageID());
  const JC_Cap applet_cap = applet_package.getCap();
  const auto *imports = applet_cap.getImport();
  if (imports == nullptr) throw Exceptions::RuntimeException;

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
  if (framework_info == nullptr) throw Exceptions::RuntimeException;

  const jpackage_ID_t framework_package_id =
      import_handler.getPackageIndex(framework_info);
  Package framework_package(framework_package_id);
  constexpr uint8_t aid_class_export_token = 6;
  const uint8_t aid_class_token =
      remap_imported_class_token(*framework_info, aid_class_export_token);
  const jclass_index_t aid_class_index = HTONS(
      Export_Handler(framework_package).getExportedClassOffset(aid_class_token));
  return Heap::addGlobalAIDObject(aid, aid_length, framework_package_id,
                                  aid_class_index);
}
} // namespace

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAID() {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  if (!context.wasAppletRegistered() && !context.hasRestoredAppletInstance()) {
    const jref_t result{};
    context.setSensitiveResultObject(result);
    return result;
  }
  uint8_t aid[16]{};
  uint8_t aid_length = 0;
  if (!context.copyAppletAID(aid, aid_length)) {
    throw Exceptions::RuntimeException;
  }

  const jref_t aid_object_ref = runtimeAIDObject(context, aid, aid_length);
  context.setAppletAIDObjectRef(aid_object_ref);
  context.setSensitiveResultObject(aid_object_ref);
  return aid_object_ref;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_lookupAID(
    const std::shared_ptr<JC_Array> buffer, const jshort_t offset,
    const jbyte_t length) {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  if (!buffer) {
    throw Exceptions::NullPointerException;
  }
  if (buffer->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint8_t>(length) >
          buffer->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  if (length > 16) {
    const jref_t result{};
    context.setSensitiveResultObject(result);
    return result;
  }

  uint8_t requested_aid[16]{};
  for (uint8_t index = 0; index < static_cast<uint8_t>(length); ++index) {
    requested_aid[index] = static_cast<uint8_t>(
        buffer->getByteEntry(static_cast<uint16_t>(offset + index)));
  }

  uint8_t active_aid[16]{};
  uint8_t active_aid_length = 0;
  if (context.copyAppletAID(active_aid, active_aid_length) &&
      active_aid_length == static_cast<uint8_t>(length) &&
      std::equal(requested_aid, requested_aid + active_aid_length,
                 active_aid)) {
    return fr_gouv_ssi_nativeimpl_NativeImplementation_getAID();
  }

  for (uint8_t package_id = 0; package_id < 64; ++package_id) {
    const uint8_t instance_count =
        cap_store_package_instance_count(package_id);
    for (uint8_t instance_index = 0; instance_index < instance_count;
         ++instance_index) {
      uint8_t applet_aid[16]{};
      uint8_t instance_aid[16]{};
      uint8_t applet_aid_length = 0;
      uint8_t instance_aid_length = 0;
      uint16_t class_index = 0;
      if (!cap_store_package_instance_at(
              package_id, instance_index, applet_aid, applet_aid_length,
              instance_aid, instance_aid_length, class_index)) {
        throw Exceptions::RuntimeException;
      }
      if (instance_aid_length != static_cast<uint8_t>(length) ||
          !std::equal(requested_aid, requested_aid + instance_aid_length,
                      instance_aid)) {
        continue;
      }

      const jref_t cached = Heap::findGlobalAIDObject(
          instance_aid, instance_aid_length);
      if (!cached.isNullPointer()) {
        context.setSensitiveResultObject(cached);
        return cached;
      }

      const jref_t aid_object_ref = runtimeAIDObject(
          context, instance_aid, instance_aid_length);
      context.setSensitiveResultObject(aid_object_ref);
      return aid_object_ref;
    }
  }

  const jref_t result{};
  context.setSensitiveResultObject(result);
  return result;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_beginTransaction() {
  return FlashMemory_Handler::beginPersistentWriteTransaction(true) ? 0 : 1;
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
  return static_cast<jshort_t>(FlashMemory_Handler::unusedCommitCapacity());
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_getMaxCommitCapacity() {
  return static_cast<jshort_t>(FlashMemory_Handler::maxCommitCapacity());
}

extern "C" void choupi_record_transaction_buffer_full(void) {
  Context::getActiveContext().setPendingTransactionExceptionReason(3);
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getPreviousContextAID() {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  uint8_t previous_aid[16]{};
  uint8_t previous_aid_length = 0;
  if (!context.copyPreviousContextAID(previous_aid, previous_aid_length)) {
    const jref_t result{};
    context.setSensitiveResultObject(result);
    return result;
  }

  const jref_t result =
      runtimeAIDObject(context, previous_aid, previous_aid_length);
  context.setSensitiveResultObject(result);
  return result;
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
fr_gouv_ssi_nativeimpl_NativeImplementation_getCurrentAppletInstance() {
  Context &context = Context::getActiveContext();
  if (context.hasRestoredAppletInstance()) {
    return context.getRestoredAppletRef();
  }
  if (context.wasAppletRegistered()) {
    return context.getRegisteredAppletRef();
  }
  return jref_t{};
}

jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isObjectDeletionSupported() {
  return FALSE;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_requestObjectDeletion() {
  // The runtime reports deletion unsupported, so the API requires ILLEGAL_USE.
  Context::getActiveContext().setPendingSystemExceptionReason(6);
  throw Exceptions::SystemException;
}

jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAssignedChannel() {
  // The current transport has no logical-channel support; use the basic channel.
  return 0;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_isAppletActive(
    const jref_t aid_reference) {
  if (aid_reference.isNullPointer()) {
    return FALSE;
  }

  Context &context = Context::getActiveContext();
  if ((!context.wasAppletRegistered() && !context.hasRestoredAppletInstance()) ||
      (!context.isAppletActive() &&
       !context.isAppletInstallationActive())) {
    return FALSE;
  }

  Heap &heap = context.getHeap();
  const auto aid_object = heap.getInstance(aid_reference);
  if (aid_object->getNumberOfFields() == 0) {
    return FALSE;
  }
  const jref_t bytes_reference = aid_object->getField_Reference(0);
  if (bytes_reference.isNullPointer() || !bytes_reference.isArray()) {
    return FALSE;
  }
  const auto bytes = heap.getArray(bytes_reference);
  if (bytes->getType() != JAVA_ARRAY_T_BYTE || bytes->size() < 5 ||
      bytes->size() > 16) {
    return FALSE;
  }

  uint8_t active_aid[16]{};
  uint8_t active_aid_length = 0;
  if (!context.copyAppletAID(active_aid, active_aid_length) ||
      active_aid_length != bytes->size()) {
    return FALSE;
  }
  for (uint8_t index = 0; index < active_aid_length; ++index) {
    if (static_cast<uint8_t>(bytes->getByteEntry(index)) != active_aid[index]) {
      return FALSE;
    }
  }
  return TRUE;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_assertIntegrity(
    const jref_t objectref) {
  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  // No integrity-sensitive arrays can be created by this implementation.
  Context::getActiveContext().setPendingSystemExceptionReason(1);
  throw Exceptions::SystemException;
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
    const jbyte_t type, const jbyte_t memory, const jshort_t length) {
  Context &context = Context::getActiveContext();
  if (length < 0) {
    throw Exceptions::NegativeArraySizeException;
  }

  const uint8_t array_type = static_cast<uint8_t>(type);
  const uint8_t memory_type = static_cast<uint8_t>(memory);
  if (array_type < 1 || array_type > 5 || memory_type > 2) {
    context.setPendingSystemExceptionReason(1);
    throw Exceptions::SystemException;
  }
#ifndef JCVM_INT_SUPPORTED
  if (array_type == 4) {
    context.setPendingSystemExceptionReason(1);
    throw Exceptions::SystemException;
  }
#endif

  // The API specifies ILLEGAL_USE when integrity-sensitive arrays are not
  // supported. The advertised capability remains false.
  context.setPendingSystemExceptionReason(6);
  throw Exceptions::SystemException;
}

jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_clearArray(
    const jref_t objectref) {
  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  // No integrity-sensitive arrays can exist while support is disabled.
  Context::getActiveContext().setPendingSystemExceptionReason(1);
  throw Exceptions::SystemException;
}

std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientIntArray(
    const jshort_t length, const jbyte_t event) {
  return makeTransientArray(length, event, JAVA_ARRAY_T_INT);
}

} // namespace jcvm
