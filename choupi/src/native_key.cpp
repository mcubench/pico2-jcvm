#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "heap.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace jcvm {
namespace {
constexpr uint8_t kDesTransientReset = 1;
constexpr uint8_t kDesTransientDeselect = 2;
constexpr uint8_t kDesPersistent = 3;
constexpr uint8_t kRsaPublic = 4;
constexpr uint8_t kRsaPrivate = 5;
constexpr uint8_t kRsaCrtPrivate = 6;
constexpr uint8_t kAesTransientReset = 13;
constexpr uint8_t kAesTransientDeselect = 14;
constexpr uint8_t kAesPersistent = 15;
constexpr uint8_t kRsaPrivateTransientReset = 22;
constexpr uint8_t kRsaPrivateTransientDeselect = 23;
constexpr uint8_t kRsaCrtTransientReset = 24;
constexpr uint8_t kRsaCrtTransientDeselect = 25;
constexpr uint8_t kMemoryPersistent = 0;
constexpr uint8_t kMemoryTransientReset = 1;
constexpr uint8_t kMemoryTransientDeselect = 2;
constexpr uint16_t kComponentCount = 7;

constexpr std::array<uint16_t, kComponentCount> kComponentFields = {
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_MODULUS,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_EXPONENT,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_P,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_Q,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DP,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DQ,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PQ,
};

uint16_t nativeKeyClassIndex() {
  const uint16_t offset =
      Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
          .getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_NATIVEKEY);
  return HTONS(offset);
}

std::shared_ptr<JC_Instance> nativeKey(const jref_t reference) {
  if (reference.isNullPointer() || reference.isArray())
    throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != nativeKeyClassIndex()) {
    throw Exceptions::SecurityException;
  }
  return instance;
}

std::shared_ptr<JC_Array> arrayField(JC_Instance &instance, uint16_t field,
                                     jc_array_type type) {
  const jref_t reference = instance.getField_Reference(field);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != type) throw Exceptions::ArrayStoreException;
  return array;
}

jref_t allocateArray(Heap &heap, uint16_t length, jc_array_type type,
                     bool transient, ClearEvent event) {
  if (!transient) return heap.addArray(length, type);
  return heap.addArray(JC_Array(heap, length, type, true, event));
}

bool rsaCrt(uint8_t type) {
  return type == kRsaCrtPrivate || type == kRsaCrtTransientReset ||
         type == kRsaCrtTransientDeselect;
}
bool rsaPrivate(uint8_t type) {
  return type == kRsaPrivate || type == kRsaPrivateTransientReset ||
         type == kRsaPrivateTransientDeselect || rsaCrt(type);
}
bool rsaType(uint8_t type) {
  return type == kRsaPublic || rsaPrivate(type);
}
bool transientType(uint8_t type) {
  return type == kDesTransientReset || type == kDesTransientDeselect ||
         type == kAesTransientReset || type == kAesTransientDeselect ||
         type == kRsaPrivateTransientReset || type == kRsaPrivateTransientDeselect ||
         type == kRsaCrtTransientReset || type == kRsaCrtTransientDeselect;
}
ClearEvent clearEvent(uint8_t type) {
  return type == kDesTransientDeselect || type == kAesTransientDeselect ||
                 type == kRsaPrivateTransientDeselect ||
                 type == kRsaCrtTransientDeselect
             ? ClearEvent::CLEAR_ON_DESELECT
             : ClearEvent::CLEAR_ON_RESET;
}

bool rsaLengthEnabled(uint8_t family, uint16_t bits) {
#define RSA_LENGTH_CASE(length) \
  case length: \
    if (family == 0) return JC_SUPPORT_KEY_RSA_PUBLIC_##length; \
    if (family == 1) return JC_SUPPORT_KEY_RSA_PRIVATE_##length; \
    return JC_SUPPORT_KEY_RSA_CRT_PRIVATE_##length
  switch (bits) {
    RSA_LENGTH_CASE(512);
    RSA_LENGTH_CASE(736);
    RSA_LENGTH_CASE(768);
    RSA_LENGTH_CASE(896);
    RSA_LENGTH_CASE(1024);
    RSA_LENGTH_CASE(1280);
    RSA_LENGTH_CASE(1536);
    RSA_LENGTH_CASE(1984);
    RSA_LENGTH_CASE(2048);
    RSA_LENGTH_CASE(3072);
    RSA_LENGTH_CASE(4096);
  default: return false;
  }
#undef RSA_LENGTH_CASE
}

bool rsaLengthEnabledForType(uint8_t type, uint16_t bits) {
  uint8_t family = 0;
  if (type == kRsaPrivate || type == kRsaPrivateTransientReset ||
      type == kRsaPrivateTransientDeselect) family = 1;
  else if (type == kRsaCrtPrivate || type == kRsaCrtTransientReset ||
           type == kRsaCrtTransientDeselect) family = 2;
  if (!rsaLengthEnabled(family, bits)) return false;
  if (type == kRsaPrivateTransientReset)
    return JC_SUPPORT_KEY_RSA_PRIVATE_TRANSIENT_RESET;
  if (type == kRsaPrivateTransientDeselect)
    return JC_SUPPORT_KEY_RSA_PRIVATE_TRANSIENT_DESELECT;
  if (type == kRsaCrtTransientReset)
    return JC_SUPPORT_KEY_RSA_CRT_PRIVATE_TRANSIENT_RESET;
  if (type == kRsaCrtTransientDeselect)
    return JC_SUPPORT_KEY_RSA_CRT_PRIVATE_TRANSIENT_DESELECT;
  return true;
}

bool supportedRsa(uint8_t type, uint16_t bits) {
  return rsaType(type) && rsaLengthEnabledForType(type, bits);
}

bool supportedSymmetric(uint8_t type, uint16_t bits) {
  switch (type) {
  case kAesPersistent:
  case kAesTransientReset:
  case kAesTransientDeselect:
    switch (bits) {
    case 128: return JC_SUPPORT_KEY_AES_128 &&
        (type == kAesPersistent || (type == kAesTransientReset && JC_SUPPORT_KEY_AES_TRANSIENT_RESET) ||
         (type == kAesTransientDeselect && JC_SUPPORT_KEY_AES_TRANSIENT_DESELECT));
    case 192: return JC_SUPPORT_KEY_AES_192 &&
        (type == kAesPersistent || (type == kAesTransientReset && JC_SUPPORT_KEY_AES_TRANSIENT_RESET) ||
         (type == kAesTransientDeselect && JC_SUPPORT_KEY_AES_TRANSIENT_DESELECT));
    case 256: return JC_SUPPORT_KEY_AES_256 &&
        (type == kAesPersistent || (type == kAesTransientReset && JC_SUPPORT_KEY_AES_TRANSIENT_RESET) ||
         (type == kAesTransientDeselect && JC_SUPPORT_KEY_AES_TRANSIENT_DESELECT));
    default: return false;
    }
  case kDesPersistent:
  case kDesTransientReset:
  case kDesTransientDeselect:
    switch (bits) {
    case 64: return JC_SUPPORT_KEY_DES &&
        (type == kDesPersistent || (type == kDesTransientReset && JC_SUPPORT_KEY_DES_TRANSIENT_RESET) ||
         (type == kDesTransientDeselect && JC_SUPPORT_KEY_DES_TRANSIENT_DESELECT));
    case 128: return JC_SUPPORT_KEY_DES3_2KEY &&
        (type == kDesPersistent || (type == kDesTransientReset && JC_SUPPORT_KEY_DES_TRANSIENT_RESET) ||
         (type == kDesTransientDeselect && JC_SUPPORT_KEY_DES_TRANSIENT_DESELECT));
    case 192: return JC_SUPPORT_KEY_DES3_3KEY &&
        (type == kDesPersistent || (type == kDesTransientReset && JC_SUPPORT_KEY_DES_TRANSIENT_RESET) ||
         (type == kDesTransientDeselect && JC_SUPPORT_KEY_DES_TRANSIENT_DESELECT));
    default: return false;
    }
  default: return false;
  }
}

uint8_t typeForAlgorithm(uint8_t algorithm, uint8_t memory) {
  if (algorithm == 1 || algorithm == 2) {
    if (memory == kMemoryPersistent) return algorithm == 1 ? kDesPersistent : kAesPersistent;
    if (memory == kMemoryTransientReset) return algorithm == 1 ? kDesTransientReset : kAesTransientReset;
    if (memory == kMemoryTransientDeselect) return algorithm == 1 ? kDesTransientDeselect : kAesTransientDeselect;
  } else if (algorithm == 11 && memory == kMemoryPersistent) return kRsaPublic;
  else if (algorithm == 12 && memory == kMemoryPersistent) return kRsaPrivate;
  else if (algorithm == 13 && memory == kMemoryPersistent) return kRsaCrtPrivate;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}

jref_t buildNativeKey(uint8_t type, uint16_t bits, bool encrypted) {
  if (encrypted || (!supportedRsa(type, bits) && !supportedSymmetric(type, bits)))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  Heap &heap = Context::getActiveContext().getHeap();
  const jref_t reference = heap.addInstance(ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
                                            nativeKeyClassIndex());
  auto instance = heap.getInstance(reference);
  if (!instance) throw Exceptions::RuntimeException;
  instance->setField_Byte(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE, type);
  instance->setField_Short(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE,
                           static_cast<jshort_t>(bits));
  const bool transient = transientType(type);
  const ClearEvent event = transient ? clearEvent(type) : ClearEvent::None;
  if (!rsaType(type)) {
    const uint16_t bytes = bits / 8;
    instance->setField_Reference(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_KEYDATA,
        allocateArray(heap, bytes, JAVA_ARRAY_T_BYTE, transient, event));
    if (transient) {
      instance->setField_Reference(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TRANSIENTINITIALIZED,
          allocateArray(heap, 1, JAVA_ARRAY_T_BYTE, true, event));
    }
  } else {
    const uint16_t bytes = static_cast<uint16_t>((bits + 7) / 8);
    const uint16_t half = static_cast<uint16_t>((bytes + 1) / 2);
    for (uint16_t i = 0; i < kComponentCount; ++i) {
      const uint16_t capacity = i < 2 ? bytes : half;
      instance->setField_Reference(kComponentFields[i],
          allocateArray(heap, capacity, JAVA_ARRAY_T_BYTE, transient, event));
    }
    instance->setField_Reference(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS,
        allocateArray(heap, kComponentCount, JAVA_ARRAY_T_SHORT, transient, event));
  }
  return reference;
}

std::shared_ptr<JC_Array> componentArray(JC_Instance &key, uint16_t component) {
  if (component >= kComponentCount) throwCryptoException(CryptoExceptionReason::IllegalValue);
  return arrayField(key, kComponentFields[component], JAVA_ARRAY_T_BYTE);
}
std::shared_ptr<JC_Array> lengthsArray(JC_Instance &key) {
  return arrayField(key, ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS,
                    JAVA_ARRAY_T_SHORT);
}
void checkInput(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 || static_cast<uint32_t>(offset) > array->size() ||
      static_cast<uint32_t>(length) > array->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
}
void setComponent(JC_Instance &key, uint16_t component,
                  const std::shared_ptr<JC_Array> &source, jshort_t offset,
                  jshort_t length) {
  checkInput(source, offset, length);
  auto destination = componentArray(key, component);
  if (length == 0 || static_cast<uint16_t>(length) > destination->size())
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  for (uint16_t i = 0; i < destination->size(); ++i) destination->setByteEntry(i, 0);
  const uint16_t start = static_cast<uint16_t>(destination->size() - length);
  for (uint16_t i = 0; i < static_cast<uint16_t>(length); ++i)
    destination->setByteEntry(start + i, source->getByteEntry(static_cast<uint16_t>(offset + i)));
  lengthsArray(key)->setShortEntry(component, length);
}
jshort_t getComponent(JC_Instance &key, uint16_t component,
                      const std::shared_ptr<JC_Array> &destination,
                      jshort_t offset) {
  if (!destination) throw Exceptions::NullPointerException;
  if (destination->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  auto lengths = lengthsArray(key);
  const jshort_t length = lengths->getShortEntry(component);
  if (length <= 0) throwCryptoException(CryptoExceptionReason::UninitializedKey);
  if (offset < 0 || static_cast<uint32_t>(offset) > destination->size() ||
      static_cast<uint16_t>(length) > destination->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
  auto source = componentArray(key, component);
  const uint16_t start = static_cast<uint16_t>(source->size() - length);
  for (uint16_t i = 0; i < static_cast<uint16_t>(length); ++i)
    destination->setByteEntry(static_cast<uint16_t>(offset + i),
                              source->getByteEntry(static_cast<uint16_t>(start + i)));
  return length;
}
} // namespace

jref_t javacard_security_KeyBuilder_buildKey(jbyte_t type, jshort_t length,
                                              jbool_t encrypted) {
  return buildNativeKey(static_cast<uint8_t>(type), static_cast<uint16_t>(length),
                        encrypted != FALSE);
}
jref_t javacard_security_KeyBuilder_buildKey(jbyte_t algorithm, jbyte_t memory,
                                              jshort_t length, jbool_t encrypted) {
  const uint8_t type = typeForAlgorithm(static_cast<uint8_t>(algorithm),
                                        static_cast<uint8_t>(memory));
  return buildNativeKey(type, static_cast<uint16_t>(length), encrypted != FALSE);
}

jbool_t javacard_security_NativeKey_isInitialized(jref_t receiver) {
  auto key = nativeKey(receiver);
  const uint8_t type = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  if (rsaType(type)) {
    auto lengths = lengthsArray(*key);
    if (rsaCrt(type)) {
      for (uint16_t i = 2; i < 7; ++i)
        if (lengths->getShortEntry(i) == 0) return FALSE;
      return TRUE;
    }
    return lengths->getShortEntry(0) != 0 && lengths->getShortEntry(1) != 0 ? TRUE : FALSE;
  }
  const jref_t transientRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TRANSIENTINITIALIZED);
  if (!transientRef.isNullPointer()) {
    auto flag = Context::getActiveContext().getHeap().getArray(transientRef);
    if (!flag || flag->getType() != JAVA_ARRAY_T_BYTE || flag->size() < 1)
      throw Exceptions::ArrayStoreException;
    return flag->getByteEntry(0) ? TRUE : FALSE;
  }
  return key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED) ? TRUE : FALSE;
}
void javacard_security_NativeKey_clearKey(jref_t receiver) {
  auto key = nativeKey(receiver);
  const uint8_t type = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  if (rsaType(type)) {
    for (uint16_t i = 0; i < kComponentCount; ++i) {
      auto component = componentArray(*key, i);
      for (uint16_t j = 0; j < component->size(); ++j) component->setByteEntry(j, 0);
    }
    auto lengths = lengthsArray(*key);
    for (uint16_t i = 0; i < kComponentCount; ++i) lengths->setShortEntry(i, 0);
    return;
  }
  auto data = arrayField(*key, ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_KEYDATA,
                         JAVA_ARRAY_T_BYTE);
  for (uint16_t i = 0; i < data->size(); ++i) data->setByteEntry(i, 0);
  const jref_t transientRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TRANSIENTINITIALIZED);
  if (!transientRef.isNullPointer()) {
    auto flag = Context::getActiveContext().getHeap().getArray(transientRef);
    if (!flag || flag->getType() != JAVA_ARRAY_T_BYTE || flag->size() < 1)
      throw Exceptions::ArrayStoreException;
    flag->setByteEntry(0, 0);
  } else {
    key->setField_Byte(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED, 0);
  }
}
jbyte_t javacard_security_NativeKey_getType(jref_t receiver) {
  return nativeKey(receiver)->getField_Byte(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE);
}
jshort_t javacard_security_NativeKey_getSize(jref_t receiver) {
  return nativeKey(receiver)->getField_Short(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE);
}
void javacard_security_NativeKey_setKey(jref_t receiver,
    std::shared_ptr<JC_Array> source, jshort_t offset) {
  auto key = nativeKey(receiver);
  auto target = arrayField(*key, ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_KEYDATA,
                           JAVA_ARRAY_T_BYTE);
  checkInput(source, offset, static_cast<jshort_t>(target->size()));
  for (uint16_t i = 0; i < target->size(); ++i)
    target->setByteEntry(i, source->getByteEntry(static_cast<uint16_t>(offset + i)));
  const jref_t transientRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TRANSIENTINITIALIZED);
  if (!transientRef.isNullPointer()) {
    auto flag = Context::getActiveContext().getHeap().getArray(transientRef);
    if (!flag || flag->getType() != JAVA_ARRAY_T_BYTE || flag->size() < 1)
      throw Exceptions::ArrayStoreException;
    flag->setByteEntry(0, 1);
  } else {
    key->setField_Byte(ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED, 1);
  }
}
jbyte_t javacard_security_NativeKey_getKey(jref_t receiver,
    std::shared_ptr<JC_Array> destination, jshort_t offset) {
  auto key = nativeKey(receiver);
  if (!javacard_security_NativeKey_isInitialized(receiver))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  auto source = arrayField(*key, ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_KEYDATA,
                           JAVA_ARRAY_T_BYTE);
  checkInput(destination, offset, static_cast<jshort_t>(source->size()));
  for (uint16_t i = 0; i < source->size(); ++i)
    destination->setByteEntry(static_cast<uint16_t>(offset + i), source->getByteEntry(i));
  return static_cast<jbyte_t>(source->size());
}

#define KEY_COMPONENT_METHODS(name, index) \
void javacard_security_NativeKey_set##name(jref_t receiver, std::shared_ptr<JC_Array> source, jshort_t offset, jshort_t length) { \
  setComponent(*nativeKey(receiver), index, source, offset, length); \
} \
jshort_t javacard_security_NativeKey_get##name(jref_t receiver, std::shared_ptr<JC_Array> destination, jshort_t offset) { \
  return getComponent(*nativeKey(receiver), index, destination, offset); \
}
KEY_COMPONENT_METHODS(Modulus, 0)
KEY_COMPONENT_METHODS(Exponent, 1)
KEY_COMPONENT_METHODS(P, 2)
KEY_COMPONENT_METHODS(Q, 3)
KEY_COMPONENT_METHODS(DP1, 4)
KEY_COMPONENT_METHODS(DQ1, 5)
KEY_COMPONENT_METHODS(PQ, 6)
#undef KEY_COMPONENT_METHODS

} // namespace jcvm
