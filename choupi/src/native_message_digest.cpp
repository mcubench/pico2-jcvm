#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace jcvm {

#if JC_SUPPORT_SHA256
void fr_gouv_ssi_nativeimpl_Sha256NativeBridge_doFinal(
    std::shared_ptr<JC_Array>, jshort_t, std::shared_ptr<JC_Array>, jshort_t);
#endif

#if JC_SUPPORT_SHA1 || JC_SUPPORT_SHA224 || JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
jbool_t fr_gouv_ssi_nativeimpl_MbedTlsDigestNativeBridge_digest(
    jbyte_t, std::shared_ptr<JC_Array>, jshort_t,
    std::shared_ptr<JC_Array>, jshort_t);
#endif

namespace {
constexpr uint16_t kMaximumDigestInput = 2046;

jclass_index_t messageDigestClassIndex() {
  const uint16_t classOffset =
      Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
          .getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_MESSAGEDIGEST);
  return HTONS(classOffset);
}

std::shared_ptr<JC_Instance> digestInstance(const jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  const uint16_t expectedClass = messageDigestClassIndex();
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != expectedClass)
    throw Exceptions::SecurityException;
  return instance;
}

std::shared_ptr<JC_Array> digestBuffer(JC_Instance &instance) {
  const jref_t reference = instance.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTBUFFER);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  return array;
}

uint16_t digestLength(const jbyte_t algorithm) {
  switch (static_cast<uint8_t>(algorithm)) {
#if JC_SUPPORT_SHA1
  case 1: return 20;
#endif
#if JC_SUPPORT_SHA224
  case 7: return 28;
#endif
#if JC_SUPPORT_SHA256
  case 4: return 32;
#endif
#if JC_SUPPORT_SHA384
  case 5: return 48;
#endif
#if JC_SUPPORT_SHA512
  case 6: return 64;
#endif
  default: throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
}

void checkRange(const std::shared_ptr<JC_Array> &array,
                const jshort_t offset, const jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) >
          array->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

void appendInput(JC_Instance &instance,
                 const std::shared_ptr<JC_Array> &source,
                 const jshort_t offset, const jshort_t length) {
  checkRange(source, offset, length);
  const jbyte_t algorithm = instance.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM);
  auto destination = digestBuffer(instance);
  const jshort_t current = instance.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH);
  const uint16_t maximumInput = static_cast<uint8_t>(algorithm) == 4
                                    ? destination->size()
                                    : kMaximumDigestInput;
  if (current < 0 || static_cast<uint32_t>(current) +
                          static_cast<uint16_t>(length) > maximumInput)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  for (jshort_t i = 0; i < length; ++i)
    destination->setByteEntry(static_cast<uint16_t>(current + i),
                              source->getByteEntry(
                                  static_cast<uint16_t>(offset + i)));
  instance.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH,
      static_cast<jshort_t>(current + length));
}

void clearInput(JC_Instance &instance) {
  auto buffer = digestBuffer(instance);
  const jshort_t length = instance.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH);
  if (length < 0 || static_cast<uint16_t>(length) > buffer->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
  for (jshort_t i = 0; i < length; ++i)
    buffer->setByteEntry(static_cast<uint16_t>(i), 0);
  instance.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH,
      0);
}
} // namespace

jref_t javacard_security_MessageDigest_getInstance(
    const jbyte_t algorithm, const jbool_t externalAccess) {
  if (externalAccess) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint16_t outputLength = digestLength(algorithm);
  (void)outputLength;
  const uint16_t capacity = kMaximumDigestInput;
  Heap &heap = Context::getActiveContext().getHeap();
  const uint16_t classOffset = messageDigestClassIndex();
  const jref_t reference = heap.addInstance(
      ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
      classOffset);
  auto instance = heap.getInstance(reference);
  if (!instance) throw Exceptions::RuntimeException;
  const jref_t buffer = heap.addArray(
      JC_Array(heap, capacity, JAVA_ARRAY_T_BYTE, true,
               ClearEvent::CLEAR_ON_RESET));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM,
      algorithm);
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTBUFFER,
      buffer);
  instance->setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH,
      0);
  return reference;
}

jbyte_t javacard_security_MessageDigest_getAlgorithm(const jref_t receiver) {
  auto instance = digestInstance(receiver);
  return instance->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM);
}

jbyte_t javacard_security_MessageDigest_getLength(const jref_t receiver) {
  auto instance = digestInstance(receiver);
  return static_cast<jbyte_t>(digestLength(instance->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM)));
}

void javacard_security_MessageDigest_update(
    const jref_t receiver, std::shared_ptr<JC_Array> input,
    const jshort_t inputOffset, const jshort_t inputLength) {
  auto instance = digestInstance(receiver);
  appendInput(*instance, input, inputOffset, inputLength);
}

jshort_t javacard_security_MessageDigest_doFinal(
    const jref_t receiver, std::shared_ptr<JC_Array> input,
    const jshort_t inputOffset, const jshort_t inputLength,
    std::shared_ptr<JC_Array> output, const jshort_t outputOffset) {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto instance = digestInstance(receiver);
  if (!output) throw Exceptions::NullPointerException;
  if (output->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  const jbyte_t algorithm = instance->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM);
  const uint16_t resultLength = digestLength(algorithm);
  checkRange(output, outputOffset, static_cast<jshort_t>(resultLength));
  appendInput(*instance, input, inputOffset, inputLength);
  auto buffer = digestBuffer(*instance);
  bool success = false;
#if JC_SUPPORT_SHA256
  if (static_cast<uint8_t>(algorithm) == 4) {
    fr_gouv_ssi_nativeimpl_Sha256NativeBridge_doFinal(
        buffer,
        instance->getNativeSessionField_Short(
            ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH),
        output, outputOffset);
    success = true;
  }
#endif
#if JC_SUPPORT_SHA1 || JC_SUPPORT_SHA224 || JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
  if (!success)
    success = fr_gouv_ssi_nativeimpl_MbedTlsDigestNativeBridge_digest(
        algorithm, buffer,
        instance->getNativeSessionField_Short(
            ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH),
        output, outputOffset) != FALSE;
#endif
  if (!success) throwCryptoException(CryptoExceptionReason::IllegalUse);
  clearInput(*instance);
  context.setSensitiveResultShort(static_cast<jshort_t>(resultLength));
  return static_cast<jshort_t>(resultLength);
}

void javacard_security_MessageDigest_reset(const jref_t receiver) {
  auto instance = digestInstance(receiver);
  clearInput(*instance);
}

} // namespace jcvm
