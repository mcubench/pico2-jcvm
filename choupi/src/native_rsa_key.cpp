#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "jc_types/jc_instance.hpp"
#include "jc_handlers/jc_export.hpp"
#include "native_crypto_exception.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace jcvm {
namespace {
constexpr uint8_t kRsaCrtPrivate = 6;
constexpr uint8_t kRsaCrtPrivateTransientReset = 24;
constexpr uint8_t kRsaCrtPrivateTransientDeselect = 25;

constexpr std::array<uint16_t, 7> kComponentFields = {
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_MODULUS,
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_EXPONENT,
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_P,
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_Q,
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_DP,
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_DQ,
    ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_PQ,
};
constexpr std::array<uint16_t, 7> kComponentLengths = {0, 1, 2, 3, 4, 5, 6};

bool isCrtType(const uint8_t type) {
  return type == kRsaCrtPrivate || type == kRsaCrtPrivateTransientReset ||
         type == kRsaCrtPrivateTransientDeselect;
}

std::shared_ptr<JC_Instance> instanceFor(const jref_t receiver) {
  if (receiver.isNullPointer()) throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(receiver);
  const jclass_index_t expected_class =
      HTONS(Export_Handler(Package(ROM_PACKAGE_INDEX_FR_GOUV_SSI_NATIVEIMPL))
                .getExportedClassOffset(
                    ROM_CLASS_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER));
  if (!instance || instance->getPackageID() !=
                       ROM_PACKAGE_INDEX_FR_GOUV_SSI_NATIVEIMPL ||
      instance->getClassIndex() != expected_class) {
    throw Exceptions::SecurityException;
  }
  return instance;
}

std::shared_ptr<JC_Array> componentArray(JC_Instance &instance,
                                         const uint16_t field) {
  const jref_t reference = instance.getField_Reference(field);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  return array;
}

std::shared_ptr<JC_Array> lengthsArray(JC_Instance &instance) {
  const jref_t reference = instance.getField_Reference(
      ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_LENGTHS);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_SHORT || array->size() < 7)
    throw Exceptions::ArrayStoreException;
  return array;
}

void setComponent(JC_Instance &instance, const uint16_t component,
                  const std::shared_ptr<JC_Array> &buffer,
                  const jshort_t offset, const jshort_t length) {
  if (!buffer) throw Exceptions::NullPointerException;
  auto destination = componentArray(instance, kComponentFields[component]);
  const uint32_t source_size = buffer->size();
  if (offset < 0 || length <= 0 ||
      static_cast<uint32_t>(offset) > source_size ||
      static_cast<uint32_t>(length) > source_size -
                                            static_cast<uint32_t>(offset) ||
      static_cast<uint16_t>(length) > destination->size()) {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  for (uint16_t i = 0; i < destination->size(); ++i)
    destination->setByteEntry(i, 0);
  const uint16_t start = static_cast<uint16_t>(destination->size() - length);
  for (uint16_t i = 0; i < static_cast<uint16_t>(length); ++i) {
    destination->setByteEntry(static_cast<uint16_t>(start + i),
                              buffer->getByteEntry(
                                  static_cast<uint16_t>(offset + i)));
  }
  lengthsArray(instance)->setShortEntry(kComponentLengths[component], length);
}

jshort_t getComponent(JC_Instance &instance, const uint16_t component,
                      const std::shared_ptr<JC_Array> &buffer,
                      const jshort_t offset) {
  if (!buffer) throw Exceptions::NullPointerException;
  if (buffer->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  const jshort_t length = lengthsArray(instance)->getShortEntry(
      kComponentLengths[component]);
  if (length == 0)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  if (length < 0 || offset < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint16_t>(length) >
          buffer->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  auto source = componentArray(instance, kComponentFields[component]);
  const uint16_t start = static_cast<uint16_t>(source->size() - length);
  std::vector<uint8_t> contents(static_cast<size_t>(length));
  source->copyPrimitiveEntriesTo(start, static_cast<uint16_t>(length),
                                 contents.data());
  buffer->copyPrimitiveEntriesFrom(static_cast<uint16_t>(offset),
                                   static_cast<uint16_t>(length),
                                   contents.data(), true);
  return length;
}

} // namespace

jbool_t fr_gouv_ssi_nativeimpl_RSAKeyProvider_isInitialized(jref_t receiver) {
  auto instance = instanceFor(receiver);
  auto lengths = lengthsArray(*instance);
  const uint8_t type = static_cast<uint8_t>(instance->getField_Byte(
      ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_TYPE));
  if (isCrtType(type)) {
    for (uint16_t i = 2; i < 7; ++i)
      if (lengths->getShortEntry(i) == 0) return FALSE;
    return TRUE;
  }
  return lengths->getShortEntry(0) != 0 &&
                 lengths->getShortEntry(1) != 0
             ? TRUE
             : FALSE;
}

void fr_gouv_ssi_nativeimpl_RSAKeyProvider_clearKey(jref_t receiver) {
  auto instance = instanceFor(receiver);
  for (uint16_t i = 0; i < kComponentFields.size(); ++i) {
    auto array = componentArray(*instance, kComponentFields[i]);
    for (uint16_t j = 0; j < array->size(); ++j)
      array->setByteEntry(j, 0);
  }
  auto lengths = lengthsArray(*instance);
  for (uint16_t i = 0; i < 7; ++i) lengths->setShortEntry(i, 0);
}

jbyte_t fr_gouv_ssi_nativeimpl_RSAKeyProvider_getType(jref_t receiver) {
  return instanceFor(receiver)->getField_Byte(
      ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_TYPE);
}

jshort_t fr_gouv_ssi_nativeimpl_RSAKeyProvider_getSize(jref_t receiver) {
  return instanceFor(receiver)->getField_Short(
      ROM_FIELD_INDEX_FR_GOUV_SSI_NATIVEIMPL_RSAKEYPROVIDER_RSAKEYPROVIDER_SIZE);
}

#define RSA_KEY_COMPONENT_METHODS(name, index)                                  \
  void fr_gouv_ssi_nativeimpl_RSAKeyProvider_set##name(                              \
      jref_t receiver, std::shared_ptr<JC_Array> buffer, jshort_t offset,       \
      jshort_t length) {                                                        \
    auto instance = instanceFor(receiver);                                      \
    setComponent(*instance, index, buffer, offset, length);                     \
  }                                                                             \
  jshort_t fr_gouv_ssi_nativeimpl_RSAKeyProvider_get##name(                          \
      jref_t receiver, std::shared_ptr<JC_Array> buffer, jshort_t offset) {     \
    auto instance = instanceFor(receiver);                                      \
    return getComponent(*instance, index, buffer, offset);                      \
  }

RSA_KEY_COMPONENT_METHODS(Modulus, 0)
RSA_KEY_COMPONENT_METHODS(Exponent, 1)
RSA_KEY_COMPONENT_METHODS(P, 2)
RSA_KEY_COMPONENT_METHODS(Q, 3)
RSA_KEY_COMPONENT_METHODS(DP1, 4)
RSA_KEY_COMPONENT_METHODS(DQ1, 5)
RSA_KEY_COMPONENT_METHODS(PQ, 6)

#undef RSA_KEY_COMPONENT_METHODS
} // namespace jcvm
