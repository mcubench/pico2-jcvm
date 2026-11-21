#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#if JC_SUPPORT_SHA3_ANY
#include "mbedtls/sha3.h"
#include <type_traits>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace jcvm {

namespace {
constexpr uint16_t kDigestContextBytes = 1 + std::max({
    sizeof(mbedtls_sha1_context), sizeof(mbedtls_sha256_context),
    sizeof(mbedtls_sha512_context)});
#if JC_SUPPORT_SHA3_ANY
static_assert(sizeof(mbedtls_sha3_context) + 1 <= kDigestContextBytes);
#endif

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

std::shared_ptr<JC_Instance> digestInstance(const jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      (instance->getClassIndex() != messageDigestClassIndex() &&
       !isInitializedDigest(*instance)))
    throw Exceptions::SecurityException;
  return instance;
}

std::shared_ptr<JC_Array> digestBuffer(JC_Instance &instance) {
  const jref_t reference = instance.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTBUFFER);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE ||
      !array->isTransientArray() || array->size() < kDigestContextBytes)
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
#if JC_SUPPORT_SHA3_224
  case 8: return 28;
#endif
#if JC_SUPPORT_SHA3_256
  case 9: return 32;
#endif
#if JC_SUPPORT_SHA3_384
  case 10: return 48;
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

// Byte zero marks a started context. A card reset clears the transient array,
// and the existing ROM field also accepts the older, larger transient buffer.
template <typename State, typename Start, typename Update, typename Finish,
          typename Operation>
void withDigestContext(JC_Array &buffer, Start start, Update update,
                       Finish finish, Operation operation, bool restart) {
  State state{};
  if (!restart && buffer.getByteEntry(0) != 0) {
    buffer.copyByteEntriesTo(1, sizeof(state),
                             reinterpret_cast<uint8_t *>(&state));
  } else if (start(&state) != 0) {
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  operation(state, update, finish);
  buffer.copyByteEntriesFrom(1, sizeof(state),
                             reinterpret_cast<const uint8_t *>(&state));
  buffer.setByteEntry(0, 1);
}

template <typename Operation>
void withDigestContext(JC_Instance &instance, Operation operation,
                       bool restart = false) {
  const auto algorithm = static_cast<uint8_t>(instance.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM));
  auto buffer = digestBuffer(instance);
  switch (algorithm) {
#if JC_SUPPORT_SHA1
  case 1:
    return withDigestContext<mbedtls_sha1_context>(
        *buffer, mbedtls_sha1_starts, mbedtls_sha1_update, mbedtls_sha1_finish,
        operation, restart);
#endif
#if JC_SUPPORT_SHA224 || JC_SUPPORT_SHA256
#if JC_SUPPORT_SHA256
  case 4:
#endif
#if JC_SUPPORT_SHA224
  case 7:
#endif
    return withDigestContext<mbedtls_sha256_context>(
        *buffer, [algorithm](auto *state) {
          return mbedtls_sha256_starts(state, algorithm == 7);
        }, mbedtls_sha256_update, mbedtls_sha256_finish, operation, restart);
#endif
#if JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
#if JC_SUPPORT_SHA384
  case 5:
#endif
#if JC_SUPPORT_SHA512
  case 6:
#endif
    return withDigestContext<mbedtls_sha512_context>(
        *buffer, [algorithm](auto *state) {
          return mbedtls_sha512_starts(state, algorithm == 5);
        }, mbedtls_sha512_update, mbedtls_sha512_finish, operation, restart);
#endif
#if JC_SUPPORT_SHA3_ANY
#if JC_SUPPORT_SHA3_224
  case 8:
#endif
#if JC_SUPPORT_SHA3_256
  case 9:
#endif
#if JC_SUPPORT_SHA3_384
  case 10:
#endif
    return withDigestContext<mbedtls_sha3_context>(
        *buffer, [algorithm](auto *state) {
          constexpr mbedtls_sha3_id ids[] = {
              MBEDTLS_SHA3_224, MBEDTLS_SHA3_256, MBEDTLS_SHA3_384,
              MBEDTLS_SHA3_512};
          return mbedtls_sha3_starts(state, ids[algorithm - 8]);
        }, mbedtls_sha3_update, [algorithm](auto *state, uint8_t *output) {
          return mbedtls_sha3_finish(state, output,
                                    digestLength(static_cast<jbyte_t>(algorithm)));
        }, operation, restart);
#endif
  default:
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
}

template <typename State, typename Update>
void updateDigest(State &state, Update update, const uint8_t *data,
                  uint16_t length) {
  const auto &total = state.MBEDTLS_PRIVATE(total);
  const uint64_t low = total[0];
  const uint64_t high = total[1];
  const uint64_t lengthWord = sizeof(total[0]) == 8
                                  ? high + (low + length < low)
                                  : ((high << 32) | low) + length;
  if (lengthWord >= (uint64_t{1} << 61) ||
      (length != 0 && update(&state, data, length) != 0))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
}

#if JC_SUPPORT_SHA3_ANY
template <typename Update>
void updateDigest(mbedtls_sha3_context &state, Update update,
                  const uint8_t *data, uint16_t length) {
  if (length != 0 && update(&state, data, length) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
}
#endif

void clearInput(JC_Instance &instance) {
  const std::array<uint8_t, kDigestContextBytes> empty{};
  digestBuffer(instance)->copyByteEntriesFrom(0, empty.size(), empty.data());
}

jref_t newDigest(const jbyte_t algorithm, const jbool_t externalAccess,
                 const bool initialized) {
  if (externalAccess) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  digestLength(algorithm);
  if (initialized && algorithm >= 8)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
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
      JC_Array(heap, kDigestContextBytes, JAVA_ARRAY_T_BYTE, true,
               ClearEvent::CLEAR_ON_RESET));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_ALGORITHM,
      algorithm);
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_MESSAGEDIGEST_MESSAGEDIGEST_INPUTBUFFER,
      buffer);
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
  withDigestContext(*instance, [&](auto &state, auto, auto) {
#if JC_SUPPORT_SHA3_ANY
    if constexpr (std::is_same_v<std::decay_t<decltype(state)>, mbedtls_sha3_context>) {
      throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
    } else
#endif
    {
      const auto *bytes = initialDigest->getByteDataView() + initialDigestOffset;
      auto &words = state.MBEDTLS_PRIVATE(state);
      auto &total = state.MBEDTLS_PRIVATE(total);
      for (auto &word : words) {
        word = 0;
        for (size_t i = 0; i < sizeof(word); ++i)
          word = (word << 8) | *bytes++;
      }
      total[0] = low;
      total[1] = sizeof(total[0]) == 8 ? high : low >> 32;
    }
  }, true);
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
  checkRange(input, inputOffset, inputLength);
  if (inputLength == 0) return;
  withDigestContext(*instance, [&](auto &state, auto update, auto) {
    updateDigest(state, update, input->getByteDataView() + inputOffset,
                 inputLength);
  });
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
  checkRange(input, inputOffset, inputLength);
  uint8_t digest[64];
  withDigestContext(*instance, [&](auto &state, auto update, auto finish) {
    const auto *bytes =
        inputLength == 0 ? nullptr : input->getByteDataView() + inputOffset;
    updateDigest(state, update, bytes, inputLength);
    auto finalState = state;
    if (finish(&finalState, digest) != 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
  });
  for (uint16_t i = 0; i < resultLength; ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         static_cast<jbyte_t>(digest[i]));
  clearInput(*instance);
  context.setSensitiveResultShort(static_cast<jshort_t>(resultLength));
  return static_cast<jshort_t>(resultLength);
}

void javacard_security_MessageDigest_reset(const jref_t receiver) {
  auto instance = digestInstance(receiver);
  clearInput(*instance);
}

} // namespace jcvm
