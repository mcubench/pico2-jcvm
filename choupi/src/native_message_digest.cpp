#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_signature_digest.hpp"

#include <algorithm>
#include <vector>
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

jclass_index_t initializedMessageDigestClassIndex() {
  const uint16_t classOffset =
      Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
          .getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_INITIALIZEDMESSAGEDIGEST);
  return HTONS(classOffset);
}

bool isInitializedDigest(const JC_Instance &instance) {
  return instance.getClassIndex() == initializedMessageDigestClassIndex();
}

// MessageDigest natives accept both runtime classes; InitializedMessageDigest
// only adds the starting-state array.
std::shared_ptr<JC_Instance> digestInstance(const jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      (instance->getClassIndex() != messageDigestClassIndex() &&
       !isInitializedDigest(*instance)))
    throw Exceptions::SecurityException;
  return instance;
}

// [0] 1 when setInitialDigest() supplied a starting state, [1..64] the
// intermediate hash value (32 bytes for SHA-1/224/256 state words, 64 for
// SHA-384/512), [65..80] the digested byte length (big-endian, 128 bits).
constexpr uint16_t kInitialStateFlag = 0;
constexpr uint16_t kInitialStateValue = 1;
constexpr uint16_t kInitialStateLength = 65;
constexpr uint16_t kInitialStateBytes = 81;

// ROM field indexes are local to the declaring class; the instance stores
// the superclass (MessageDigest) fields first, as resolveInstanceFieldToken()
// does for bytecode access.
uint16_t initialStateField() {
  return static_cast<uint16_t>(
      Class_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
          .getInstanceFieldsSize(messageDigestClassIndex()) +
      ROM_FIELD_INDEX_JAVACARD_SECURITY_INITIALIZEDMESSAGEDIGEST_INITIALIZEDMESSAGEDIGEST_INITIALSTATE);
}

std::shared_ptr<JC_Array> initialStateArray(JC_Instance &instance) {
  const jref_t reference = instance.getField_Reference(initialStateField());
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE ||
      array->size() < kInitialStateBytes)
    throw Exceptions::ArrayStoreException;
  return array;
}

// Back to the state upon construction (MessageDigest.reset()/doFinal()).
void clearInitialState(JC_Instance &instance) {
  if (!isInitializedDigest(instance)) return;
  auto state = initialStateArray(instance);
  for (uint16_t i = 0; i < kInitialStateBytes; ++i) state->setByteEntry(i, 0);
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

namespace {
jref_t newDigest(const jbyte_t algorithm, const jbool_t externalAccess,
                 const bool initialized) {
  if (externalAccess) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint16_t outputLength = digestLength(algorithm);
  (void)outputLength;
  const uint16_t capacity = kMaximumDigestInput;
  Heap &heap = Context::getActiveContext().getHeap();
  const uint16_t classOffset = initialized
                                   ? initializedMessageDigestClassIndex()
                                   : messageDigestClassIndex();
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
  if (initialized) {
    // Transient like the input buffer: the starting state is per-session
    // data and is never written to flash.
    instance->setField_Reference(
        initialStateField(),
        heap.addArray(JC_Array(heap, kInitialStateBytes, JAVA_ARRAY_T_BYTE,
                               true, ClearEvent::CLEAR_ON_RESET)));
  }
  return reference;
}
} // namespace

jref_t javacard_security_MessageDigest_getInstance(
    const jbyte_t algorithm, const jbool_t externalAccess) {
  return newDigest(algorithm, externalAccess, false);
}

jref_t javacard_security_MessageDigest_getInitializedMessageDigestInstance(
    const jbyte_t algorithm, const jbool_t externalAccess) {
  return newDigest(algorithm, externalAccess, true);
}

void javacard_security_InitializedMessageDigest_setInitialDigest(
    const jref_t receiver, std::shared_ptr<JC_Array> initialDigest,
    const jshort_t initialDigestOffset, const jshort_t initialDigestLength,
    std::shared_ptr<JC_Array> digestedLength,
    const jshort_t digestedLengthOffset, const jshort_t digestedLengthLength) {
  auto instance = digestInstance(receiver);
  if (!isInitializedDigest(*instance)) throw Exceptions::SecurityException;
  checkRange(initialDigest, initialDigestOffset, initialDigestLength);
  checkRange(digestedLength, digestedLengthOffset, digestedLengthLength);
  const uint8_t algorithm = static_cast<uint8_t>(instance->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM));
  const bool sha512Family = algorithm == 5 || algorithm == 6;
  // Intermediate hash value size: SHA-1 20, SHA-224/256 32 (the full state,
  // as for Signature.setInitialDigest), SHA-384/512 64.
  const jshort_t stateLength = algorithm == 1 ? 20 : (sha512Family ? 64 : 32);
  const uint64_t blockLength = sha512Family ? 128 : 64;
  const jshort_t maximumLengthBytes = sha512Family ? 16 : 8;
  if (initialDigestLength != stateLength || digestedLengthLength <= 0 ||
      digestedLengthLength > maximumLengthBytes)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  uint64_t low = 0;
  uint64_t high = 0;
  const jshort_t highBytes = digestedLengthLength > 8
                                 ? static_cast<jshort_t>(digestedLengthLength - 8)
                                 : 0;
  for (jshort_t i = 0; i < digestedLengthLength; ++i) {
    const uint8_t byte = static_cast<uint8_t>(digestedLength->getByteEntry(
        static_cast<uint16_t>(digestedLengthOffset + i)));
    if (i < highBytes) high = (high << 8) | byte;
    else low = (low << 8) | byte;
  }
  // The first part must be a non-zero whole number of blocks and below the
  // algorithm's message length limit (2^64 bits, 2^128 bits for SHA-384/512).
  if ((low == 0 && high == 0) || (low % blockLength) != 0 ||
      (sha512Family ? high >= (uint64_t{1} << 61)
                    : (high != 0 || low >= (uint64_t{1} << 61))))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  clearInput(*instance);
  auto state = initialStateArray(*instance);
  state->setByteEntry(kInitialStateFlag, 1);
  for (jshort_t i = 0; i < 64; ++i) {
    state->setByteEntry(static_cast<uint16_t>(kInitialStateValue + i),
                        i < initialDigestLength
                            ? initialDigest->getByteEntry(
                                  static_cast<uint16_t>(initialDigestOffset + i))
                            : static_cast<jbyte_t>(0));
  }
  for (uint16_t i = 0; i < 8; ++i) {
    const unsigned shift = static_cast<unsigned>((7 - i) * 8);
    state->setByteEntry(static_cast<uint16_t>(kInitialStateLength + i),
                        static_cast<jbyte_t>(high >> shift));
    state->setByteEntry(static_cast<uint16_t>(kInitialStateLength + 8 + i),
                        static_cast<jbyte_t>(low >> shift));
  }
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
  if (isInitializedDigest(*instance)) {
    auto state = initialStateArray(*instance);
    if (state->getByteEntry(kInitialStateFlag) != 0) {
      const uint8_t selected = static_cast<uint8_t>(algorithm);
      const uint16_t stateLength =
          selected == 1 ? 20 : ((selected == 5 || selected == 6) ? 64 : 32);
      std::vector<unsigned char> startingState(stateLength);
      for (uint16_t i = 0; i < stateLength; ++i)
        startingState[i] = static_cast<uint8_t>(
            state->getByteEntry(static_cast<uint16_t>(kInitialStateValue + i)));
      uint64_t high = 0;
      uint64_t low = 0;
      for (uint16_t i = 0; i < 8; ++i) {
        high = (high << 8) | static_cast<uint8_t>(state->getByteEntry(
                                 static_cast<uint16_t>(kInitialStateLength + i)));
        low = (low << 8) | static_cast<uint8_t>(state->getByteEntry(
                               static_cast<uint16_t>(kInitialStateLength + 8 + i)));
      }
      const jshort_t messageLength = instance->getNativeSessionField_Short(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTLENGTH);
      std::vector<unsigned char> message(static_cast<uint16_t>(messageLength));
      for (jshort_t i = 0; i < messageLength; ++i)
        message[static_cast<uint16_t>(i)] = static_cast<uint8_t>(
            buffer->getByteEntry(static_cast<uint16_t>(i)));
      const auto digest = continueDigestFromInitialState(
          selected, startingState, low, high, message);
      std::fill(startingState.begin(), startingState.end(), 0);
      if (digest.size() != resultLength)
        throwCryptoException(CryptoExceptionReason::IllegalUse);
      for (uint16_t i = 0; i < resultLength; ++i)
        output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                             static_cast<jbyte_t>(digest[i]));
      success = true;
    }
  }
#if JC_SUPPORT_SHA256
  if (!success && static_cast<uint8_t>(algorithm) == 4) {
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
  clearInitialState(*instance);
  context.setSensitiveResultShort(static_cast<jshort_t>(resultLength));
  return static_cast<jshort_t>(resultLength);
}

void javacard_security_MessageDigest_reset(const jref_t receiver) {
  auto instance = digestInstance(receiver);
  clearInput(*instance);
  clearInitialState(*instance);
}

} // namespace jcvm
