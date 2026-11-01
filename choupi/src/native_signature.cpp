#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_signature_digest.hpp"

#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace jcvm {

#if JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1 || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2 || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5 || \
    JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3 || \
    JC_SUPPORT_SIGNATURE_AES_CMAC_128
std::vector<unsigned char> computeSymmetricMac(
    uint8_t algorithm, std::vector<unsigned char> &key,
    const std::vector<unsigned char> &iv,
    const std::vector<unsigned char> &message);
#endif

#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
void initializeEcdsaSignature(JC_Instance &signature, jref_t key, uint8_t mode);
jshort_t ecdsaSignatureLength(JC_Instance &signature);
jshort_t ecdsaSignatureSignHash(JC_Instance &signature,
    const std::vector<unsigned char> &hash,
    const std::shared_ptr<JC_Array> &output, jshort_t outputOffset);
jbool_t ecdsaSignatureVerifyHash(JC_Instance &signature,
    const std::vector<unsigned char> &hash,
    const std::shared_ptr<JC_Array> &expected, jshort_t expectedOffset,
    jshort_t expectedLength);
jshort_t ecdsaSignatureSign(JC_Instance &signature,
    const std::shared_ptr<JC_Array> &input, jshort_t offset, jshort_t length,
    const std::shared_ptr<JC_Array> &output, jshort_t outputOffset);
jbool_t ecdsaSignatureVerify(JC_Instance &signature,
    const std::shared_ptr<JC_Array> &input, jshort_t offset, jshort_t length,
    const std::shared_ptr<JC_Array> &expected, jshort_t expectedOffset,
    jshort_t expectedLength);
#endif

#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
void initializeRsaSignature(JC_Instance &signature, jref_t key,
                            uint8_t mode, jshort_t saltLength = -1);
jshort_t rsaSignatureLength(JC_Instance &signature);
jshort_t rsaSignatureSign(JC_Instance &signature,
                          const std::shared_ptr<JC_Array> &input,
                          jshort_t offset, jshort_t length,
                          const std::shared_ptr<JC_Array> &output,
                          jshort_t outputOffset);
jshort_t rsaSignatureSignRecovery(
    JC_Instance &signature, const std::shared_ptr<JC_Array> &input,
    jshort_t offset, jshort_t length,
    const std::shared_ptr<JC_Array> &output, jshort_t outputOffset,
    const std::shared_ptr<JC_Array> &recoveredLength,
    jshort_t recoveredLengthOffset);
jshort_t rsaSignatureBeginVerify(
    JC_Instance &signature, const std::shared_ptr<JC_Array> &signatureData,
    jshort_t offset, jshort_t length, std::vector<unsigned char> &recovered,
    std::vector<unsigned char> &hash, bool &partial);
bool rsaSignatureVerifyRecoveredMessage(
    JC_Instance &signature, const std::vector<unsigned char> &message,
    const std::vector<unsigned char> &hash);
jshort_t rsaSignatureSignHash(JC_Instance &signature,
                              const std::shared_ptr<JC_Array> &hash,
                              jshort_t offset, jshort_t length,
                              const std::shared_ptr<JC_Array> &output,
                              jshort_t outputOffset);
jbool_t rsaSignatureVerify(JC_Instance &signature,
                           const std::shared_ptr<JC_Array> &input,
                           jshort_t offset, jshort_t length,
                           const std::shared_ptr<JC_Array> &expected,
                           jshort_t expectedOffset, jshort_t expectedLength);
jbool_t rsaSignatureVerifyHash(JC_Instance &signature,
                               const std::shared_ptr<JC_Array> &hash,
                               jshort_t hashOffset, jshort_t hashLength,
                               const std::shared_ptr<JC_Array> &expected,
                               jshort_t expectedOffset, jshort_t expectedLength);
#endif

namespace {
constexpr uint8_t kModeSign = 1;
constexpr uint8_t kModeVerify = 2;
constexpr uint8_t kAesKeyTransientReset = 13;
constexpr uint8_t kAesKeyTransientDeselect = 14;
constexpr uint8_t kAesKeyPersistent = 15;
constexpr uint8_t kDesKeyTransientReset = 1;
constexpr uint8_t kDesKeyTransientDeselect = 2;
constexpr uint8_t kDesKeyPersistent = 3;
constexpr uint8_t kSignatureCipherDesMac4 = 1;
constexpr uint8_t kSignatureCipherDesMac8 = 2;
constexpr uint8_t kSignatureCipherAesMac128 = 6;
constexpr uint8_t kSignatureCipherAesCmac128 = 10;
constexpr uint8_t kSignatureCipherRsa = 3;
constexpr uint8_t kPaddingNoPad = 1;
constexpr uint8_t kPaddingIso9797M1 = 2;
constexpr uint8_t kPaddingIso9797M2 = 3;
constexpr uint8_t kPaddingIso9797M1Alg3 = 4;
constexpr uint8_t kPaddingIso9797M2Alg3 = 5;
constexpr uint8_t kPaddingPkcs5 = 6;
constexpr uint8_t kPaddingPkcs1Pss = 8;
constexpr uint8_t kPaddingIso9796 = 10;
constexpr uint8_t kPaddingIso9796Mr = 11;
constexpr uint8_t kInitialDigestState = 3;

uint16_t signatureClassIndex() {
  const uint16_t offset =
      Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
          .getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_SIGNATURE);
  return HTONS(offset);
}

std::shared_ptr<JC_Instance> signatureInstance(const jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance ||
      instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != signatureClassIndex())
    throw Exceptions::SecurityException;
  return instance;
}

std::shared_ptr<JC_Array> byteArray(JC_Instance &instance,
                                    const uint16_t field) {
  const jref_t reference = instance.getField_Reference(field);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  return array;
}

void checkRange(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) >
          array->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

bool isMessageRecoverySignature(JC_Instance &signature) {
  return static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM)) == 30;
}

std::vector<unsigned char> readBytes(JC_Array &array, uint16_t offset,
                                     uint16_t length) {
  std::vector<unsigned char> result(length);
  for (uint16_t index = 0; index < length; ++index)
    result[index] = static_cast<uint8_t>(array.getByteEntry(offset + index));
  return result;
}

[[maybe_unused]] uint32_t readBigEndianWord(
    const std::vector<unsigned char> &state, size_t offset) {
  return (static_cast<uint32_t>(state[offset]) << 24) |
         (static_cast<uint32_t>(state[offset + 1]) << 16) |
         (static_cast<uint32_t>(state[offset + 2]) << 8) |
         static_cast<uint32_t>(state[offset + 3]);
}

struct DigestByteLength {
  uint64_t low;
  uint64_t high;
};

DigestByteLength readInitialMessageLength(JC_Array &lengthBytes) {
  DigestByteLength result{0, 0};
  for (uint16_t i = 0; i < 8; ++i) {
    result.high = (result.high << 8) |
        static_cast<uint8_t>(lengthBytes.getByteEntry(i));
    result.low = (result.low << 8) |
        static_cast<uint8_t>(lengthBytes.getByteEntry(8 + i));
  }
  return result;
}

void storeInitialDigestState(JC_Instance &signature,
                             const std::vector<unsigned char> &state,
                             DigestByteLength digestedLength) {
  auto recoveryHash = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  auto lengthBytes = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR);
  if (state.size() > recoveryHash->size() || lengthBytes->size() < 16)
    throw Exceptions::ArrayStoreException;
  for (uint16_t i = 0; i < recoveryHash->size(); ++i)
    recoveryHash->setByteEntry(i, i < state.size() ? state[i] : 0);
  for (uint16_t i = 0; i < 8; ++i) {
    const unsigned shift = static_cast<unsigned>((7 - i) * 8);
    lengthBytes->setByteEntry(i,
        static_cast<jbyte_t>(digestedLength.high >> shift));
    lengthBytes->setByteEntry(8 + i,
        static_cast<jbyte_t>(digestedLength.low >> shift));
  }
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE,
      static_cast<jbyte_t>(kInitialDigestState));
}

std::vector<unsigned char> continueDigestFromState(
    uint8_t algorithm, const std::vector<unsigned char> &state,
    DigestByteLength digestedLength,
    const std::vector<unsigned char> &message) {
  std::vector<unsigned char> result;
  int rc = -1;
  switch (algorithm) {
#if JC_SUPPORT_SHA1
  case 1: {
    if (state.size() != 20) break;
    mbedtls_sha1_context context;
    mbedtls_sha1_init(&context);
    rc = mbedtls_sha1_starts(&context);
    for (size_t i = 0; rc == 0 && i < 5; ++i)
      context.MBEDTLS_PRIVATE(state)[i] = readBigEndianWord(state, i * 4);
    context.MBEDTLS_PRIVATE(total)[0] = static_cast<uint32_t>(digestedLength.low);
    context.MBEDTLS_PRIVATE(total)[1] = static_cast<uint32_t>(digestedLength.low >> 32);
    std::fill(std::begin(context.MBEDTLS_PRIVATE(buffer)),
              std::end(context.MBEDTLS_PRIVATE(buffer)), 0);
    if (rc == 0 && !message.empty())
      rc = mbedtls_sha1_update(&context, message.data(), message.size());
    result.resize(20);
    if (rc == 0) rc = mbedtls_sha1_finish(&context, result.data());
    mbedtls_sha1_free(&context);
    break;
  }
#endif
#if JC_SUPPORT_SHA224 || JC_SUPPORT_SHA256
  case 4:
  case 7: {
    if (state.size() != 32) break;
    const bool sha224 = algorithm == 7;
#if !JC_SUPPORT_SHA224
    if (sha224) break;
#endif
#if !JC_SUPPORT_SHA256
    if (!sha224) break;
#endif
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    rc = mbedtls_sha256_starts(&context, sha224 ? 1 : 0);
    for (size_t i = 0; rc == 0 && i < 8; ++i)
      context.MBEDTLS_PRIVATE(state)[i] = readBigEndianWord(state, i * 4);
    context.MBEDTLS_PRIVATE(total)[0] = static_cast<uint32_t>(digestedLength.low);
    context.MBEDTLS_PRIVATE(total)[1] = static_cast<uint32_t>(digestedLength.low >> 32);
    std::fill(std::begin(context.MBEDTLS_PRIVATE(buffer)),
              std::end(context.MBEDTLS_PRIVATE(buffer)), 0);
    if (rc == 0 && !message.empty())
      rc = mbedtls_sha256_update(&context, message.data(), message.size());
    result.resize(sha224 ? 28 : 32);
    if (rc == 0) rc = mbedtls_sha256_finish(&context, result.data());
    mbedtls_sha256_free(&context);
    break;
  }
#endif
#if JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
  case 5:
  case 6: {
    if (state.size() != 64) break;
    const bool sha384 = algorithm == 5;
#if !JC_SUPPORT_SHA384
    if (sha384) break;
#endif
#if !JC_SUPPORT_SHA512
    if (!sha384) break;
#endif
    mbedtls_sha512_context context;
    mbedtls_sha512_init(&context);
    rc = mbedtls_sha512_starts(&context, sha384 ? 1 : 0);
    for (size_t i = 0; rc == 0 && i < 8; ++i) {
      uint64_t word = 0;
      for (size_t j = 0; j < 8; ++j)
        word = (word << 8) | state[i * 8 + j];
      context.MBEDTLS_PRIVATE(state)[i] = word;
    }
    context.MBEDTLS_PRIVATE(total)[0] = digestedLength.low;
    context.MBEDTLS_PRIVATE(total)[1] = digestedLength.high;
    std::fill(std::begin(context.MBEDTLS_PRIVATE(buffer)),
              std::end(context.MBEDTLS_PRIVATE(buffer)), 0);
    if (rc == 0 && !message.empty())
      rc = mbedtls_sha512_update(&context, message.data(), message.size());
    result.resize(sha384 ? 48 : 64);
    if (rc == 0) rc = mbedtls_sha512_finish(&context, result.data());
    mbedtls_sha512_free(&context);
    break;
  }
#endif
  default:
    break;
  }
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  return result;
}

void appendBytes(JC_Instance &signature, const unsigned char *bytes,
                 uint16_t length) {
  const uint16_t messageField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER;
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  auto message = byteArray(signature, messageField);
  const jshort_t oldLength = signature.getField_Short(countField);
  const uint32_t required = static_cast<uint32_t>(oldLength) + length;
  if (oldLength < 0 || required > 32767)
    throw Exceptions::ArrayIndexOutOfBoundsException;
  if (required > message->size()) {
    uint32_t capacity = std::max<uint16_t>(message->size(), 64);
    while (capacity < required) capacity *= 2;
    capacity = std::min<uint32_t>(capacity, 32767);
    const jref_t expandedReference = Context::getActiveContext().getHeap()
        .addArray(static_cast<uint16_t>(capacity), JAVA_ARRAY_T_BYTE,
                  ClearEvent::CLEAR_ON_RESET);
    auto expanded = Context::getActiveContext().getHeap().getArray(expandedReference);
    for (jshort_t index = 0; index < oldLength; ++index)
      expanded->setByteEntry(static_cast<uint16_t>(index),
                             message->getByteEntry(static_cast<uint16_t>(index)));
    signature.setField_Reference(messageField, expandedReference);
    message = expanded;
  }
  for (uint16_t index = 0; index < length; ++index)
    message->setByteEntry(static_cast<uint16_t>(oldLength + index), bytes[index]);
  signature.setField_Short(countField, static_cast<jshort_t>(required));
}

uint16_t resultLength(const uint8_t algorithm) {
  switch (algorithm) {
  case 1:
  case 3:
  case 5:
  case 7:
  case 19:
  case 47:
    return 4;
  case 2:
  case 4:
  case 6:
  case 8:
  case 20:
  case 48:
    return 8;
  case 18:
  case 49:
    return 16;
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  case 10:
    return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796
  case 9: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796_MR
  case 30: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_224_PKCS1
  case 39: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_256_PKCS1
  case 40: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_384_PKCS1
  case 41: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_512_PKCS1
  case 42: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_PSS
  case 21: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_224_PKCS1_PSS
  case 43: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_256_PKCS1_PSS
  case 44: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_384_PKCS1_PSS
  case 45: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_512_PKCS1_PSS
  case 46: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA
  case 17: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_224
  case 37: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_256
  case 33: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_384
  case 34: return 0;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  case 38: return 0;
#endif
  default:
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
}

bool supportedAlgorithm(const uint8_t algorithm) {
  switch (algorithm) {
#if JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD
  case 2: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1
  case 3: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1
  case 4: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2
  case 5: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2
  case 6: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5
  case 7: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5
  case 8: return true;
#endif
#if JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD
  case 18: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3
  case 19: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3
  case 20: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3
  case 47: return true;
#endif
#if JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3
  case 48: return true;
#endif
#if JC_SUPPORT_SIGNATURE_AES_CMAC_128
  case 49: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  case 10: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796
  case 9: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796_MR
  case 30: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_224_PKCS1
  case 39: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_256_PKCS1
  case 40: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_384_PKCS1
  case 41: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_512_PKCS1
  case 42: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_PSS
  case 21: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_224_PKCS1_PSS
  case 43: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_256_PKCS1_PSS
  case 44: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_384_PKCS1_PSS
  case 45: return true;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_512_PKCS1_PSS
  case 46: return true;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA
  case 17: return true;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_224
  case 37: return true;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_256
  case 33: return true;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_384
  case 34: return true;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  case 38: return true;
#endif
  default: return false;
  }
}

bool tupleForAlgorithm(uint8_t algorithm, uint8_t &digest, uint8_t &cipher,
                       uint8_t &padding) {
  digest = 0;
  switch (algorithm) {
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796
  case 9: digest = 1; cipher = kSignatureCipherRsa; padding = kPaddingIso9796; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796_MR
  case 30: digest = 1; cipher = kSignatureCipherRsa; padding = kPaddingIso9796Mr; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  case 10: digest = 1; cipher = kSignatureCipherRsa; padding = 7; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_224_PKCS1
  case 39: digest = 7; cipher = kSignatureCipherRsa; padding = 7; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_256_PKCS1
  case 40: digest = 4; cipher = kSignatureCipherRsa; padding = 7; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_384_PKCS1
  case 41: digest = 5; cipher = kSignatureCipherRsa; padding = 7; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_512_PKCS1
  case 42: digest = 6; cipher = kSignatureCipherRsa; padding = 7; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_PSS
  case 21: digest = 1; cipher = kSignatureCipherRsa; padding = kPaddingPkcs1Pss; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_224_PKCS1_PSS
  case 43: digest = 7; cipher = kSignatureCipherRsa; padding = kPaddingPkcs1Pss; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_256_PKCS1_PSS
  case 44: digest = 4; cipher = kSignatureCipherRsa; padding = kPaddingPkcs1Pss; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_384_PKCS1_PSS
  case 45: digest = 5; cipher = kSignatureCipherRsa; padding = kPaddingPkcs1Pss; break;
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_512_PKCS1_PSS
  case 46: digest = 6; cipher = kSignatureCipherRsa; padding = kPaddingPkcs1Pss; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA
  case 17: digest = 1; cipher = 5; padding = 0; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_224
  case 37: digest = 7; cipher = 5; padding = 0; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_256
  case 33: digest = 4; cipher = 5; padding = 0; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_384
  case 34: digest = 5; cipher = 5; padding = 0; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  case 38: digest = 6; cipher = 5; padding = 0; break;
#endif
  case 2: cipher = kSignatureCipherDesMac8; padding = kPaddingNoPad; break;
  case 3: cipher = kSignatureCipherDesMac4; padding = kPaddingIso9797M1; break;
  case 4: cipher = kSignatureCipherDesMac8; padding = kPaddingIso9797M1; break;
  case 5: cipher = kSignatureCipherDesMac4; padding = kPaddingIso9797M2; break;
  case 6: cipher = kSignatureCipherDesMac8; padding = kPaddingIso9797M2; break;
  case 7: cipher = kSignatureCipherDesMac4; padding = kPaddingPkcs5; break;
  case 8: cipher = kSignatureCipherDesMac8; padding = kPaddingPkcs5; break;
  case 18: cipher = kSignatureCipherAesMac128; padding = kPaddingNoPad; break;
  case 19:
    cipher = kSignatureCipherDesMac4; padding = kPaddingIso9797M2Alg3; break;
  case 20:
    cipher = kSignatureCipherDesMac8; padding = kPaddingIso9797M2Alg3; break;
  case 47:
    cipher = kSignatureCipherDesMac4; padding = kPaddingIso9797M1Alg3; break;
  case 48:
    cipher = kSignatureCipherDesMac8; padding = kPaddingIso9797M1Alg3; break;
  case 49:
    cipher = kSignatureCipherAesCmac128; padding = kPaddingIso9797M2; break;
  default: return false;
  }
  return supportedAlgorithm(algorithm);
}

uint8_t algorithmForTuple(uint8_t digest, uint8_t cipher, uint8_t padding) {
  for (uint8_t algorithm : {uint8_t(2), uint8_t(3), uint8_t(4), uint8_t(5),
                            uint8_t(6), uint8_t(7), uint8_t(8), uint8_t(18),
                            uint8_t(19), uint8_t(20), uint8_t(39), uint8_t(40),
                            uint8_t(41), uint8_t(42), uint8_t(47), uint8_t(48),
                            uint8_t(49), uint8_t(10), uint8_t(21),
                            uint8_t(43), uint8_t(44), uint8_t(45),
                            uint8_t(46), uint8_t(9), uint8_t(30),
                            uint8_t(17), uint8_t(37), uint8_t(33),
                            uint8_t(34), uint8_t(38)}) {
    uint8_t candidateDigest = 0, candidateCipher = 0, candidatePadding = 0;
    if (tupleForAlgorithm(algorithm, candidateDigest, candidateCipher,
                          candidatePadding) &&
        candidateDigest == digest && candidateCipher == cipher &&
        candidatePadding == padding)
      return algorithm;
  }
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}

#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
bool isRsaSignatureAlgorithm(uint8_t algorithm) {
  uint8_t digest = 0, cipher = 0, padding = 0;
  return tupleForAlgorithm(algorithm, digest, cipher, padding) &&
         cipher == kSignatureCipherRsa;
}
#endif

#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
bool isEcdsaSignatureAlgorithm(uint8_t algorithm) {
  return algorithm == 17 || algorithm == 33 || algorithm == 34 ||
         algorithm == 37 || algorithm == 38;
}
#endif

jref_t createSignature(uint8_t algorithm, uint8_t digest, uint8_t cipher,
                       uint8_t padding, jbool_t externalAccess) {
  if (externalAccess || !supportedAlgorithm(algorithm))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  Heap &heap = Context::getActiveContext().getHeap();
  const uint16_t classIndex = signatureClassIndex();
  const jref_t reference = heap.addInstance(
      ROM_PACKAGE_INDEX_JAVACARD_SECURITY, classIndex);
  auto instance = heap.getInstance(reference);
  if (!instance) throw Exceptions::RuntimeException;
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM,
      static_cast<jbyte_t>(algorithm));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM,
      static_cast<jbyte_t>(digest));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_CIPHERALGORITHM,
      static_cast<jbyte_t>(cipher));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_PADDINGALGORITHM,
      static_cast<jbyte_t>(padding));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 0);
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE, 0);
  instance->setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH,
      static_cast<jshort_t>(resultLength(algorithm)));
  instance->setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH, 0);
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY, jref_t());
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR,
      heap.addArray(16, JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET));
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER,
      heap.addArray(64, JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET));
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH,
      heap.addArray(64, JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET));
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE, 0);
  return reference;
}

std::shared_ptr<JC_Instance> keyInstance(jref_t keyReference) {
  if (keyReference.isNullPointer() || keyReference.isArray())
    throw Exceptions::NullPointerException;
  auto key = Context::getActiveContext().getHeap().getInstance(keyReference);
  if (!key || key->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  return key;
}

std::shared_ptr<JC_Array> keyBytes(JC_Instance &key) {
  const jref_t keyDataReference = key.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_KEYDATA);
  if (keyDataReference.isNullPointer())
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  auto data = Context::getActiveContext().getHeap().getArray(keyDataReference);
  if (!data || data->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  const jref_t transientReference = key.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TRANSIENTINITIALIZED);
  bool initialized = key.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED) != 0;
  if (!transientReference.isNullPointer()) {
    auto flag = Context::getActiveContext().getHeap().getArray(transientReference);
    if (!flag || flag->getType() != JAVA_ARRAY_T_BYTE || flag->size() < 1)
      throw Exceptions::ArrayStoreException;
    initialized = flag->getByteEntry(0) != 0;
  }
  if (!initialized) throwCryptoException(CryptoExceptionReason::UninitializedKey);
  return data;
}

void initializeMac(JC_Instance &signature, jref_t keyReference, uint8_t mode) {
  if (mode != kModeSign && mode != kModeVerify)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto key = keyInstance(keyReference);
  const uint8_t keyType = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
  const uint8_t algorithm = static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM));
  const bool aes = algorithm == 18 || algorithm == 49;
  const bool aesType = keyType == kAesKeyTransientReset ||
                       keyType == kAesKeyTransientDeselect ||
                       keyType == kAesKeyPersistent;
  const bool desType = keyType == kDesKeyTransientReset ||
                       keyType == kDesKeyTransientDeselect ||
                       keyType == kDesKeyPersistent;
  if ((aes && (!aesType || bits != 128)) ||
      (!aes && (!desType || (bits != 64 && bits != 128 && bits != 192))))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if ((algorithm == 19 || algorithm == 20 || algorithm == 47 ||
       algorithm == 48) && bits != 128)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto bytes = keyBytes(*key);
  if (bytes->size() != bits / 8)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  signature.setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY, keyReference);
  auto iv = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR);
  for (uint16_t i = 0; i < iv->size(); ++i) iv->setByteEntry(i, 0);
  signature.setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH, 0);
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE, 0);
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE,
      static_cast<jbyte_t>(mode));
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 1);
}

void appendMessage(JC_Instance &signature,
                   const std::shared_ptr<JC_Array> &input,
                   jshort_t offset, jshort_t length) {
  checkRange(input, offset, length);
  const auto bytes = readBytes(*input, static_cast<uint16_t>(offset),
                               static_cast<uint16_t>(length));
  appendBytes(signature, bytes.data(), static_cast<uint16_t>(bytes.size()));
}

void clearMessage(JC_Instance &signature) {
  auto message = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  const jshort_t length = signature.getField_Short(countField);
  if (length < 0 || static_cast<uint16_t>(length) > message->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
  for (jshort_t i = 0; i < length; ++i)
    if (message->getByteEntry(static_cast<uint16_t>(i)) != 0)
      message->setByteEntry(static_cast<uint16_t>(i), 0);
  if (length != 0) signature.setField_Short(countField, 0);
  const uint16_t recoveryStateField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE;
  if (signature.getField_Byte(recoveryStateField) != 0)
    signature.setField_Byte(recoveryStateField, 0);
  auto recoveryHash = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  for (uint16_t i = 0; i < recoveryHash->size(); ++i)
    if (recoveryHash->getByteEntry(i) != 0)
      recoveryHash->setByteEntry(i, 0);
}

void clearMacMessage(JC_Instance &signature) {
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  const jshort_t length = signature.getField_Short(countField);
  if (length < 0) throw Exceptions::ArrayIndexOutOfBoundsException;
  if (length == 0) return;

  auto message = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
  if (static_cast<uint16_t>(length) > message->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
  for (jshort_t i = 0; i < length; ++i)
    message->setByteEntry(static_cast<uint16_t>(i), 0);
  signature.setField_Short(countField, 0);
}

void requireInitialized(JC_Instance &signature, uint8_t mode = 0) {
  if (!signature.getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED))
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  if (mode != 0 && signature.getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE) != mode)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
}

std::vector<unsigned char> runMac(
    JC_Instance &signature, const std::shared_ptr<JC_Array> &inputArray,
    jshort_t inputOffset, jshort_t inputLength) {
  const uint8_t algorithm = static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM));
  const jref_t keyReference = signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY);
  auto keyObject = keyInstance(keyReference);
  auto key = keyBytes(*keyObject);
  const jshort_t bufferedLength = signature.getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH);
  if (bufferedLength < 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  std::shared_ptr<JC_Array> message;
  if (bufferedLength != 0) {
    message = byteArray(signature,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
    if (static_cast<uint16_t>(bufferedLength) > message->size())
      throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  checkRange(inputArray, inputOffset, inputLength);
  const uint32_t combinedLength = static_cast<uint32_t>(bufferedLength) +
                                  static_cast<uint16_t>(inputLength);
  if (combinedLength > 32767)
    throw Exceptions::ArrayIndexOutOfBoundsException;
  std::vector<unsigned char> keyBytes(key->size());
  const bool aes = algorithm == 18 || algorithm == 49;
  const size_t blockSize = aes ? 16 : 8;
  std::vector<unsigned char> ivBytes(blockSize, 0);
  auto initialVector = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR);
  if (!initialVector || initialVector->size() < blockSize)
    throw Exceptions::ArrayStoreException;
  for (size_t i = 0; i < blockSize; ++i)
    ivBytes[i] = static_cast<uint8_t>(initialVector->getByteEntry(
        static_cast<uint16_t>(i)));
  std::vector<unsigned char> input(combinedLength);
  for (uint16_t i = 0; i < key->size(); ++i)
    keyBytes[i] = static_cast<uint8_t>(key->getByteEntry(i));
  for (jshort_t i = 0; i < bufferedLength; ++i)
    input[static_cast<size_t>(i)] = static_cast<uint8_t>(
        message->getByteEntry(static_cast<uint16_t>(i)));
  for (jshort_t i = 0; i < inputLength; ++i)
    input[static_cast<size_t>(bufferedLength + i)] = static_cast<uint8_t>(
        inputArray->getByteEntry(static_cast<uint16_t>(inputOffset + i)));
#if JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1 || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2 || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5 || \
    JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3 || \
    JC_SUPPORT_SIGNATURE_AES_CMAC_128
  return computeSymmetricMac(algorithm, keyBytes, ivBytes, input);
#else
  (void)algorithm; (void)keyBytes; (void)ivBytes; (void)input;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

} // namespace

bool signatureHasInitialDigest(JC_Instance &signature) {
  return static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE)) ==
      kInitialDigestState;
}

std::vector<unsigned char> continueSignatureDigest(
    JC_Instance &signature, const std::vector<unsigned char> &message) {
  if (!signatureHasInitialDigest(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  auto stateArray = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  auto lengthBytes = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR);
  const uint8_t algorithm = static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM));
  uint16_t stateLength = 0;
  switch (algorithm) {
  case 1: stateLength = 20; break;
  case 4:
  case 7: stateLength = 32; break;
  case 5:
  case 6: stateLength = 64; break;
  default: throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  if (stateLength > stateArray->size() || lengthBytes->size() < 16)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const auto state = readBytes(*stateArray, 0, stateLength);
  const DigestByteLength digestedLength =
      readInitialMessageLength(*lengthBytes);
  return continueDigestFromState(algorithm, state, digestedLength, message);
}

void clearSignatureInitialDigest(JC_Instance &signature) {
  if (!signatureHasInitialDigest(signature)) return;
  auto stateArray = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  auto lengthBytes = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR);
  for (uint16_t i = 0; i < stateArray->size(); ++i)
    stateArray->setByteEntry(i, 0);
  for (uint16_t i = 0; i < lengthBytes->size(); ++i)
    lengthBytes->setByteEntry(i, 0);
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE, 0);
}

jref_t javacard_security_Signature_getInstance(jbyte_t algorithm,
                                               jbool_t externalAccess) {
  uint8_t digest = 0, cipher = 0, padding = 0;
  const uint8_t selected = static_cast<uint8_t>(algorithm);
  if (!tupleForAlgorithm(selected, digest, cipher, padding))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  return createSignature(selected, digest, cipher, padding, externalAccess);
}

jref_t javacard_security_Signature_getInstance(
    jbyte_t digest, jbyte_t cipher, jbyte_t padding, jbool_t externalAccess) {
  const uint8_t selected = algorithmForTuple(static_cast<uint8_t>(digest),
      static_cast<uint8_t>(cipher), static_cast<uint8_t>(padding));
  return createSignature(selected, static_cast<uint8_t>(digest),
      static_cast<uint8_t>(cipher), static_cast<uint8_t>(padding), externalAccess);
}

void javacard_security_Signature_init(jref_t receiver, jref_t key,
                                      jbyte_t mode) {
  auto signature = signatureInstance(receiver);
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  const uint8_t algorithm = static_cast<uint8_t>(signature->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM));
  if (isEcdsaSignatureAlgorithm(algorithm)) {
    initializeEcdsaSignature(*signature, key, static_cast<uint8_t>(mode));
    return;
  }
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM)))) {
    initializeRsaSignature(*signature, key, static_cast<uint8_t>(mode));
    return;
  }
#endif
  initializeMac(*signature, key, static_cast<uint8_t>(mode));
}

void javacard_security_Signature_init(
    jref_t receiver, jref_t key, jbyte_t mode,
    std::shared_ptr<JC_Array> parameters, jshort_t offset, jshort_t length) {
  auto signature = signatureInstance(receiver);
  if (!parameters) throw Exceptions::NullPointerException;
  const uint8_t algorithm = static_cast<uint8_t>(signature->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM));
  if (isMessageRecoverySignature(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  if (isEcdsaSignatureAlgorithm(algorithm))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  if (isRsaSignatureAlgorithm(algorithm)) {
    checkRange(parameters, offset, length);
    if (signature->getField_Byte(
            ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_PADDINGALGORITHM)
        != kPaddingPkcs1Pss || length != 2)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    const uint16_t saltLength =
        (static_cast<uint16_t>(static_cast<uint8_t>(parameters->getByteEntry(offset))) << 8) |
        static_cast<uint8_t>(parameters->getByteEntry(offset + 1));
    if (saltLength > 32767)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    initializeRsaSignature(*signature, key, static_cast<uint8_t>(mode),
                           static_cast<jshort_t>(saltLength));
    return;
  }
#endif
  const uint16_t blockLength = (algorithm == 18 || algorithm == 49) ? 16 : 8;
  if (algorithm == 49 || length != blockLength)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkRange(parameters, offset, length);
  initializeMac(*signature, key, static_cast<uint8_t>(mode));
  auto iv = byteArray(*signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALVECTOR);
  for (jshort_t i = 0; i < length; ++i)
    iv->setByteEntry(static_cast<uint16_t>(i),
        parameters->getByteEntry(static_cast<uint16_t>(offset + i)));
}

jbyte_t javacard_security_Signature_getAlgorithm(jref_t receiver) {
  return signatureInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM);
}
jbyte_t javacard_security_Signature_getMessageDigestAlgorithm(jref_t receiver) {
  return signatureInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM);
}
jbyte_t javacard_security_Signature_getCipherAlgorithm(jref_t receiver) {
  return signatureInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_CIPHERALGORITHM);
}
jbyte_t javacard_security_Signature_getPaddingAlgorithm(jref_t receiver) {
  return signatureInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_PADDINGALGORITHM);
}
jshort_t javacard_security_Signature_getLength(jref_t receiver) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return rsaSignatureLength(*signature);
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  if (isEcdsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return ecdsaSignatureLength(*signature);
#endif
  auto key = keyInstance(signature->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  (void)keyBytes(*key);
  return signature->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH);
}
void javacard_security_Signature_setInitialDigest(
    jref_t receiver, std::shared_ptr<JC_Array> digest, jshort_t digestOffset,
    jshort_t digestLength, std::shared_ptr<JC_Array> messageLength,
    jshort_t messageLengthOffset, jshort_t messageLengthLength) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature);
  [[maybe_unused]] const uint8_t algorithm = static_cast<uint8_t>(signature->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM));
  const uint8_t digestAlgorithm = static_cast<uint8_t>(signature->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM));
  uint16_t expectedStateLength = 0;
  uint16_t blockLength = 0;
  switch (digestAlgorithm) {
#if JC_SUPPORT_SHA1
  case 1: expectedStateLength = 20; blockLength = 64; break;
#endif
#if JC_SUPPORT_SHA224 || JC_SUPPORT_SHA256
  case 4: expectedStateLength = 32; blockLength = 64; break;
  case 7: expectedStateLength = 32; blockLength = 64; break;
#endif
#if JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
  case 5: expectedStateLength = 64; blockLength = 128; break;
  case 6: expectedStateLength = 64; blockLength = 128; break;
#endif
  default:
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  bool isSupportedInitialDigestAlgorithm = false;
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  isSupportedInitialDigestAlgorithm = isRsaSignatureAlgorithm(algorithm);
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  isSupportedInitialDigestAlgorithm =
      isSupportedInitialDigestAlgorithm || isEcdsaSignatureAlgorithm(algorithm);
#endif
  if (isMessageRecoverySignature(*signature) ||
      !isSupportedInitialDigestAlgorithm)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const jshort_t existingLength = signature->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH);
  if (existingLength != 0 || signatureHasInitialDigest(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  checkRange(digest, digestOffset, digestLength);
  checkRange(messageLength, messageLengthOffset, messageLengthLength);
  const jshort_t maxMessageLengthBytes =
      (digestAlgorithm == 5 || digestAlgorithm == 6) ? 16 : 8;
  if (digestLength != expectedStateLength || messageLengthLength == 0 ||
      messageLengthLength > maxMessageLengthBytes)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  DigestByteLength digestedLength{0, 0};
  const jshort_t highLengthBytes =
      std::max<jshort_t>(0, static_cast<jshort_t>(messageLengthLength - 8));
  for (jshort_t i = 0; i < messageLengthLength; ++i) {
    const uint8_t byte = static_cast<uint8_t>(messageLength->getByteEntry(
        static_cast<uint16_t>(messageLengthOffset + i)));
    if (i < highLengthBytes)
      digestedLength.high = (digestedLength.high << 8) | byte;
    else
      digestedLength.low = (digestedLength.low << 8) | byte;
  }
  const bool sha512Family = digestAlgorithm == 5 || digestAlgorithm == 6;
  if ((sha512Family && digestedLength.high >= (uint64_t{1} << 61)) ||
      (!sha512Family && (digestedLength.high != 0 ||
                         digestedLength.low > 0x1FFFFFFFFFFFFFFFULL)) ||
      (digestedLength.high == 0 && digestedLength.low == 0) ||
      digestedLength.low % blockLength != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const auto state = readBytes(*digest, static_cast<uint16_t>(digestOffset),
                               static_cast<uint16_t>(digestLength));
  storeInitialDigestState(*signature, state, digestedLength);
}
void javacard_security_Signature_update(
    jref_t receiver, std::shared_ptr<JC_Array> input, jshort_t offset,
    jshort_t length) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature);
  if (isMessageRecoverySignature(*signature)) {
    const uint8_t state = static_cast<uint8_t>(signature->getField_Byte(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE));
    const uint8_t mode = static_cast<uint8_t>(signature->getField_Byte(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE));
    if (mode == kModeVerify && state == 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    if (state == 1 && length != 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  appendMessage(*signature, input, offset, length);
}

jshort_t javacard_security_Signature_beginVerify(
    jref_t receiver, std::shared_ptr<JC_Array> signatureData,
    jshort_t offset, jshort_t length) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature);
  if (signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE) !=
      kModeVerify)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (!isMessageRecoverySignature(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  clearMessage(*signature);
  std::vector<unsigned char> recovered;
  std::vector<unsigned char> hash;
  bool partial = false;
  jshort_t recoveredLength = 0;
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796_MR
  recoveredLength = rsaSignatureBeginVerify(
      *signature, signatureData, offset, length, recovered, hash, partial);
#else
  (void)signatureData; (void)offset; (void)length;
  throwCryptoException(CryptoExceptionReason::IllegalUse);
#endif
  if (hash.size() != 20 || recovered.size() !=
          static_cast<size_t>(recoveredLength))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  appendBytes(*signature, recovered.data(), static_cast<uint16_t>(recovered.size()));
  auto recoveryHash = byteArray(*signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  if (recoveryHash->size() < hash.size())
    throw Exceptions::ArrayStoreException;
  for (uint16_t index = 0; index < recoveryHash->size(); ++index)
    recoveryHash->setByteEntry(index,
        index < hash.size() ? static_cast<jbyte_t>(hash[index]) : 0);
  signature->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE,
      static_cast<jbyte_t>(partial ? 2 : 1));
  checkRange(signatureData, offset, recoveredLength);
  for (jshort_t index = 0; index < recoveredLength; ++index)
    signatureData->setByteEntry(static_cast<uint16_t>(offset + index),
                                recovered[static_cast<size_t>(index)]);
  return recoveredLength;
}
jshort_t javacard_security_Signature_sign(
    jref_t receiver, std::shared_ptr<JC_Array> input, jshort_t offset,
    jshort_t length, std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature, kModeSign);
  if (isMessageRecoverySignature(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  jshort_t resultSize = signature->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM)))) {
    resultSize = rsaSignatureLength(*signature);
  }
#endif
  checkRange(output, outputOffset, resultSize);
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  if (isEcdsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return ecdsaSignatureSign(*signature, input, offset, length, output,
                              outputOffset);
#endif
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return rsaSignatureSign(*signature, input, offset, length, output,
                            outputOffset);
#endif
  const auto result = runMac(*signature, input, offset, length);
  for (jshort_t i = 0; i < resultSize; ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         static_cast<jbyte_t>(result[static_cast<size_t>(i)]));
  clearMacMessage(*signature);
  return resultSize;
}

jshort_t javacard_security_Signature_sign(
    jref_t receiver, std::shared_ptr<JC_Array> input, jshort_t offset,
    jshort_t length, std::shared_ptr<JC_Array> output, jshort_t outputOffset,
    std::shared_ptr<JC_Array> recoveredLength, jshort_t recoveredLengthOffset) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature, kModeSign);
  if (!isMessageRecoverySignature(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796_MR
  return rsaSignatureSignRecovery(*signature, input, offset, length, output,
                                  outputOffset, recoveredLength,
                                  recoveredLengthOffset);
#else
  (void)input; (void)offset; (void)length; (void)output; (void)outputOffset;
  (void)recoveredLength; (void)recoveredLengthOffset;
  throwCryptoException(CryptoExceptionReason::IllegalUse);
#endif
}
jshort_t javacard_security_Signature_signPreComputedHash(
    jref_t receiver, std::shared_ptr<JC_Array> hash, jshort_t offset,
    jshort_t length, std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  auto signature = signatureInstance(receiver);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  requireInitialized(*signature, kModeSign);
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM)))) {
    const jshort_t resultSize = rsaSignatureLength(*signature);
    checkRange(output, outputOffset, resultSize);
    return rsaSignatureSignHash(*signature, hash, offset, length, output,
                                outputOffset);
  }
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  requireInitialized(*signature, kModeSign);
  if (isEcdsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM)))) {
    if (signatureHasInitialDigest(*signature))
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    checkRange(hash, offset, length);
    return ecdsaSignatureSignHash(*signature,
        readBytes(*hash, static_cast<uint16_t>(offset),
                  static_cast<uint16_t>(length)), output, outputOffset);
  }
#endif
  (void)hash; (void)offset; (void)length; (void)output; (void)outputOffset;
  throwCryptoException(CryptoExceptionReason::IllegalUse);
}
jbool_t javacard_security_Signature_verify(
    jref_t receiver, std::shared_ptr<JC_Array> input, jshort_t offset,
    jshort_t length, std::shared_ptr<JC_Array> expected, jshort_t expectedOffset,
    jshort_t expectedLength) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature, kModeVerify);
  if (isMessageRecoverySignature(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  checkRange(expected, expectedOffset, expectedLength);
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  if (isEcdsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return ecdsaSignatureVerify(*signature, input, offset, length, expected,
                                expectedOffset, expectedLength);
#endif
  const jshort_t resultSize = signature->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return rsaSignatureVerify(*signature, input, offset, length, expected,
                              expectedOffset, expectedLength);
#endif
  const auto result = runMac(*signature, input, offset, length);
  uint8_t difference = expectedLength == resultSize ? 0 : 1;
  if (expectedLength == resultSize)
    for (jshort_t i = 0; i < resultSize; ++i)
      difference |= static_cast<uint8_t>(result[static_cast<size_t>(i)] ^
          static_cast<uint8_t>(expected->getByteEntry(
              static_cast<uint16_t>(expectedOffset + i))));
  clearMacMessage(*signature);
  return difference == 0 ? TRUE : FALSE;
}

jbool_t javacard_security_Signature_verify(
    jref_t receiver, std::shared_ptr<JC_Array> input, jshort_t offset,
    jshort_t length) {
  auto signature = signatureInstance(receiver);
  requireInitialized(*signature, kModeVerify);
  if (!isMessageRecoverySignature(*signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const uint8_t state = static_cast<uint8_t>(signature->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE));
  if (state == 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  checkRange(input, offset, length);
  if (state == 1 && length != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  appendMessage(*signature, input, offset, length);
  const jshort_t messageLength = signature->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH);
  auto messageBuffer = byteArray(*signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
  const auto message = readBytes(*messageBuffer, 0,
                                 static_cast<uint16_t>(messageLength));
  auto recoveryHash = byteArray(*signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  if (recoveryHash->size() < 20)
    throw Exceptions::ArrayStoreException;
  const auto hash = readBytes(*recoveryHash, 0, 20);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_ISO9796_MR
  const bool valid = rsaSignatureVerifyRecoveredMessage(
      *signature, message, hash);
#else
  const bool valid = false;
#endif
  clearMessage(*signature);
  return valid ? TRUE : FALSE;
}
jbool_t javacard_security_Signature_verifyPreComputedHash(
    jref_t receiver, std::shared_ptr<JC_Array> hash, jshort_t hashOffset,
    jshort_t hashLength, std::shared_ptr<JC_Array> expected,
    jshort_t expectedOffset, jshort_t expectedLength) {
  auto signature = signatureInstance(receiver);
#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
  requireInitialized(*signature, kModeVerify);
  if (isRsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM))))
    return rsaSignatureVerifyHash(*signature, hash, hashOffset, hashLength,
                                  expected, expectedOffset, expectedLength);
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  requireInitialized(*signature, kModeVerify);
  if (isEcdsaSignatureAlgorithm(static_cast<uint8_t>(signature->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_ALGORITHM)))) {
    if (signatureHasInitialDigest(*signature))
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    checkRange(hash, hashOffset, hashLength);
    return ecdsaSignatureVerifyHash(*signature,
        readBytes(*hash, static_cast<uint16_t>(hashOffset),
                  static_cast<uint16_t>(hashLength)), expected,
        expectedOffset, expectedLength);
  }
#endif
  (void)hash; (void)hashOffset; (void)hashLength; (void)expected;
  (void)expectedOffset; (void)expectedLength;
  throwCryptoException(CryptoExceptionReason::IllegalUse);
}

} // namespace jcvm
