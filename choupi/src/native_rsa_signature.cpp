#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_signature_digest.hpp"
#include "native_rsa_math.hpp"

#include "mbedtls/bignum.h"
#if JC_SUPPORT_SHA1
#include "mbedtls/sha1.h"
#endif
#if JC_SUPPORT_SHA224 || JC_SUPPORT_SHA256
#include "mbedtls/sha256.h"
#endif
#if JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
#include "mbedtls/sha512.h"
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#ifndef PC_VERSION
#include "pico/rand.h"
#endif

#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY
namespace jcvm {
namespace {
constexpr uint8_t kRsaPublic = 4;
constexpr uint8_t kRsaPrivate = 5;
constexpr uint8_t kRsaPrivateTransientReset = 22;
constexpr uint8_t kRsaPrivateTransientDeselect = 23;
constexpr uint8_t kRsaCrtPrivate = 6;
constexpr uint8_t kRsaCrtTransientReset = 24;
constexpr uint8_t kRsaCrtTransientDeselect = 25;
constexpr uint8_t kModeSign = 1;
constexpr uint8_t kModeVerify = 2;

std::shared_ptr<JC_Instance> rsaKeyInstance(const jref_t reference) {
  if (reference.isNullPointer() || reference.isArray())
    throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  const uint16_t expectedClass = HTONS(
      Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
          .getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_NATIVEKEY));
  if (!instance || instance->getPackageID() !=
                       ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != expectedClass)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  return instance;
}

std::shared_ptr<JC_Array> byteArray(JC_Instance &instance, uint16_t field) {
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
  const uint32_t size = array->size();
  if (offset < 0 || length < 0 || static_cast<uint32_t>(offset) > size ||
      static_cast<uint32_t>(length) > size - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

std::vector<unsigned char> readArray(JC_Array &array, uint16_t offset,
                                     uint16_t length) {
  std::vector<unsigned char> result(length);
  for (uint16_t i = 0; i < length; ++i)
    result[i] = static_cast<unsigned char>(array.getByteEntry(offset + i));
  return result;
}

uint16_t keyModulusLength(JC_Instance &key);

std::shared_ptr<JC_Array> keyComponentArray(JC_Instance &key,
                                            uint16_t component) {
  const uint16_t fields[] = {
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_MODULUS,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_EXPONENT,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_P,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_Q,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DP,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DQ,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PQ};
  if (component >= std::size(fields))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  return byteArray(key, fields[component]);
}

bool rsaCrtKey(JC_Instance &key) {
  const uint8_t type = static_cast<uint8_t>(key.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  return type == kRsaCrtPrivate || type == kRsaCrtTransientReset ||
         type == kRsaCrtTransientDeselect;
}

jshort_t keyComponentLength(JC_Instance &key, uint16_t component) {
  const jref_t reference = key.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto lengths = Context::getActiveContext().getHeap().getArray(reference);
  if (!lengths || lengths->getType() != JAVA_ARRAY_T_SHORT ||
      lengths->size() <= component)
    throw Exceptions::ArrayStoreException;
  return lengths->getShortEntry(component);
}

std::vector<unsigned char> keyComponent(JC_Instance &signature,
                                        uint16_t component) {
  auto key = rsaKeyInstance(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  auto storage = keyComponentArray(*key, component);
  const jshort_t length = keyComponentLength(*key, component);
  if (length <= 0) {
    if (component == 0 && length == 0 && rsaCrtKey(*key)) {
      auto p = keyComponent(signature, 2);
      RsaSensitiveBufferGuard pGuard(p);
      auto q = keyComponent(signature, 3);
      RsaSensitiveBufferGuard qGuard(q);
      return rsaCrtModulus(p, q, keyModulusLength(*key));
    }
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  }
  if (static_cast<uint16_t>(length) > storage->size())
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto result = readArray(*storage,
      static_cast<uint16_t>(storage->size() - static_cast<uint16_t>(length)),
      static_cast<uint16_t>(length));
  if (component == 0) {
    const uint16_t capacity = keyModulusLength(*key);
    if (result.size() > capacity)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    result.insert(result.begin(), capacity - result.size(), 0);
  }
  return result;
}

uint16_t keyModulusLength(JC_Instance &key) {
  const jshort_t bits = key.getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE);
  if (bits < 512 || bits > 4096)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  return static_cast<uint16_t>((bits + 7) / 8);
}

void append(JC_Instance &signature, const std::shared_ptr<JC_Array> &input,
            jshort_t offset, jshort_t length) {
  checkRange(input, offset, length);
  const uint16_t bufferField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER;
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  auto current = byteArray(signature, bufferField);
  const jshort_t count = signature.getNativeSessionField_Short(countField);
  const int32_t total = static_cast<int32_t>(count) + length;
  if (count < 0 || total > 32767)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (total > current->size()) {
    uint32_t capacity = std::max<uint16_t>(64, current->size());
    while (capacity < static_cast<uint32_t>(total)) capacity *= 2;
    capacity = std::min<uint32_t>(capacity, 32767);
    const jref_t expandedRef =
        Context::getActiveContext().getHeap().addTransientArray(
            static_cast<uint16_t>(capacity), JAVA_ARRAY_T_BYTE,
            ClearEvent::CLEAR_ON_RESET);
    auto expanded = Context::getActiveContext().getHeap().getArray(expandedRef);
    for (jshort_t i = 0; i < count; ++i)
      expanded->setByteEntry(static_cast<uint16_t>(i),
                             current->getByteEntry(static_cast<uint16_t>(i)));
    signature.setField_Reference(bufferField, expandedRef);
    current = expanded;
  }
  for (jshort_t i = 0; i < length; ++i)
    current->setByteEntry(static_cast<uint16_t>(count + i),
                          input->getByteEntry(static_cast<uint16_t>(offset + i)));
  signature.setNativeSessionField_Short(countField, static_cast<jshort_t>(total));
}

std::vector<unsigned char> bufferedData(JC_Instance &signature) {
  const uint16_t bufferField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER;
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  auto buffer = byteArray(signature, bufferField);
  const jshort_t count = signature.getNativeSessionField_Short(countField);
  if (count < 0 || static_cast<uint16_t>(count) > buffer->size())
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return readArray(*buffer, 0, static_cast<uint16_t>(count));
}

void clearMessage(JC_Instance &signature) {
  auto buffer = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  const jshort_t count = signature.getNativeSessionField_Short(countField);
  if (count < 0 || static_cast<uint16_t>(count) > buffer->size())
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  for (jshort_t i = 0; i < count; ++i)
    buffer->setByteEntry(static_cast<uint16_t>(i), 0);
  signature.setNativeSessionField_Short(countField, 0);
}

uint8_t signatureDigestAlgorithm(JC_Instance &signature) {
  return static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM));
}

uint16_t digestLength(uint8_t algorithm) {
  switch (algorithm) {
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

std::vector<unsigned char> digestMessage(JC_Instance &signature,
                                         const std::vector<unsigned char> &message) {
  const uint8_t algorithm = signatureDigestAlgorithm(signature);
  std::vector<unsigned char> digest(digestLength(algorithm));
  int result = -1;
  switch (algorithm) {
#if JC_SUPPORT_SHA1
  case 1: result = mbedtls_sha1(message.data(), message.size(), digest.data()); break;
#endif
#if JC_SUPPORT_SHA224
  case 7: result = mbedtls_sha256(message.data(), message.size(), digest.data(), 1); break;
#endif
#if JC_SUPPORT_SHA256
  case 4: result = mbedtls_sha256(message.data(), message.size(), digest.data(), 0); break;
#endif
#if JC_SUPPORT_SHA384
  case 5: result = mbedtls_sha512(message.data(), message.size(), digest.data(), 1); break;
#endif
#if JC_SUPPORT_SHA512
  case 6: result = mbedtls_sha512(message.data(), message.size(), digest.data(), 0); break;
#endif
  default: break;
  }
  if (result != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  return digest;
}

std::vector<unsigned char> digestInfoFromHash(uint8_t algorithm,
                                              const unsigned char *digest) {
  const unsigned char *prefix = nullptr;
  size_t prefixLength = 0;
  static constexpr unsigned char sha1Prefix[] = {
      0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
      0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
  static constexpr unsigned char sha224Prefix[] = {
      0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c};
  static constexpr unsigned char sha256Prefix[] = {
      0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
  static constexpr unsigned char sha384Prefix[] = {
      0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
  static constexpr unsigned char sha512Prefix[] = {
      0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
  switch (algorithm) {
  case 1: prefix = sha1Prefix; prefixLength = sizeof(sha1Prefix); break;
  case 7: prefix = sha224Prefix; prefixLength = sizeof(sha224Prefix); break;
  case 4: prefix = sha256Prefix; prefixLength = sizeof(sha256Prefix); break;
  case 5: prefix = sha384Prefix; prefixLength = sizeof(sha384Prefix); break;
  case 6: prefix = sha512Prefix; prefixLength = sizeof(sha512Prefix); break;
  default: throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
  std::vector<unsigned char> result(prefix, prefix + prefixLength);
  result.insert(result.end(), digest, digest + digestLength(algorithm));
  return result;
}

std::vector<unsigned char> encodedMessage(
    const std::vector<unsigned char> &digest, uint16_t modulusLength) {
  if (modulusLength < digest.size() + 11)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  std::vector<unsigned char> encoded(modulusLength, 0xff);
  encoded[0] = 0;
  encoded[1] = 1;
  const size_t separator = modulusLength - digest.size() - 1;
  encoded[separator] = 0;
  std::copy(digest.begin(), digest.end(), encoded.begin() + separator + 1);
  return encoded;
}

bool isPss(JC_Instance &signature) {
  return signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_PADDINGALGORITHM) == 8;
}

bool isIso9796(JC_Instance &signature) {
  const uint8_t padding = static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_PADDINGALGORITHM));
  return padding == 10 || padding == 11;
}

bool isIso9796MessageRecovery(JC_Instance &signature) {
  return signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_PADDINGALGORITHM) == 11;
}

std::vector<unsigned char> digestMessageForIso9796(
    const std::vector<unsigned char> &message) {
  std::vector<unsigned char> digest(20);
  if (mbedtls_sha1(message.data(), message.size(), digest.data()) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return digest;
}

std::vector<unsigned char> encodedIso9796(
    const std::vector<unsigned char> &message,
    const std::vector<unsigned char> &hash, uint16_t modulusLength,
    uint16_t modulusBits, size_t *recoverableLengthOut = nullptr) {
  constexpr size_t digestSize = 20;
  constexpr size_t trailerBits = 8;
  if (hash.size() != digestSize || modulusLength < digestSize + 2 ||
      message.size() > 32767)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const int32_t excessBits = static_cast<int32_t>((digestSize + message.size()) * 8 +
      trailerBits + 4) - modulusBits;
  const size_t recoverableLength = excessBits > 0
      ? (message.size() > static_cast<size_t>((excessBits + 7) / 8)
             ? message.size() - static_cast<size_t>((excessBits + 7) / 8) : 0)
      : message.size();
  const size_t digestOffset = modulusLength - digestSize - 1;
  if (recoverableLength == 0 || recoverableLength > digestOffset)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (recoverableLengthOut != nullptr)
    *recoverableLengthOut = recoverableLength;
  const size_t messageOffset = digestOffset - recoverableLength;
  std::vector<unsigned char> encoded(modulusLength, 0);
  std::copy(hash.begin(), hash.end(), encoded.begin() + digestOffset);
  std::copy_n(message.begin(), recoverableLength,
              encoded.begin() + messageOffset);
  if (messageOffset > 1) {
    std::fill(encoded.begin() + 1, encoded.begin() + messageOffset, 0xbb);
    encoded[messageOffset - 1] ^= 1;
    encoded[0] = static_cast<unsigned char>(excessBits > 0 ? 0x6b : 0x4b);
  } else {
    encoded[0] = static_cast<unsigned char>(excessBits > 0 ? 0x6a : 0x4a);
  }
  encoded.back() = 0xbc;
  return encoded;
}

struct Iso9796Block {
  std::vector<unsigned char> recoveredMessage;
  std::vector<unsigned char> hash;
  bool partial = false;
};

bool decodeIso9796(const std::vector<unsigned char> &encoded,
                   Iso9796Block &decoded) {
  constexpr size_t digestSize = 20;
  if (encoded.size() < digestSize + 2 ||
      encoded.back() != 0xbc || (encoded[0] & 0xc0) != 0x40)
    return false;
  const bool partial = (encoded[0] & 0x20) != 0;
  const uint8_t fullHeader = partial ? 0x6b : 0x4b;
  const uint8_t shortHeader = partial ? 0x6a : 0x4a;
  const bool shortHeaderUsed = encoded[0] == shortHeader;
  if (!shortHeaderUsed && encoded[0] != fullHeader) return false;
  const size_t digestOffset = encoded.size() - digestSize - 1;
  size_t messageStart;
  if (shortHeaderUsed) {
    messageStart = 1;
  } else {
    messageStart = 1;
    while (messageStart < digestOffset &&
           (encoded[messageStart] & 0x0f) != 0x0a)
      ++messageStart;
    if (messageStart >= digestOffset || encoded[messageStart] != 0xba)
      return false;
    for (size_t i = 1; i < messageStart; ++i)
      if (encoded[i] != 0xbb) return false;
    ++messageStart;
  }
  if (messageStart >= digestOffset) return false;
  decoded.recoveredMessage.assign(encoded.begin() + messageStart,
                                  encoded.begin() + digestOffset);
  decoded.hash.assign(encoded.begin() + digestOffset,
                      encoded.begin() + digestOffset + digestSize);
  decoded.partial = partial;
  return true;
}

bool verifyIso9796(const std::vector<unsigned char> &message,
                   const std::vector<unsigned char> &hash,
                   const std::vector<unsigned char> &encoded) {
  constexpr size_t digestSize = 20;
  if (hash.size() != digestSize) return false;
  Iso9796Block decoded;
  RsaSensitiveBufferGuard recoveredGuard(decoded.recoveredMessage);
  RsaSensitiveBufferGuard decodedHashGuard(decoded.hash);
  if (!decodeIso9796(encoded, decoded) ||
      decoded.recoveredMessage.size() > message.size() ||
      !std::equal(decoded.recoveredMessage.begin(),
                  decoded.recoveredMessage.end(), message.begin()))
    return false;
  std::vector<unsigned char> digestToCheck = hash;
  RsaSensitiveBufferGuard digestToCheckGuard(digestToCheck);
  if (!decoded.partial) {
    if (decoded.recoveredMessage.size() != message.size()) return false;
    digestToCheck = digestMessageForIso9796(message);
  }
  uint8_t difference = 0;
  for (size_t i = 0; i < digestSize; ++i)
    difference |= static_cast<uint8_t>(decoded.hash[i] ^ digestToCheck[i]);
  return difference == 0;
}

uint8_t randomByte() {
#ifdef PC_VERSION
  static std::random_device source;
  return static_cast<uint8_t>(source());
#else
  static uint32_t word = 0;
  static uint8_t remaining = 0;
  if (remaining == 0) {
    word = get_rand_32();
    remaining = 4;
  }
  const uint8_t value = static_cast<uint8_t>(word);
  word >>= 8;
  --remaining;
  return value;
#endif
}

std::vector<unsigned char> mgf1(JC_Instance &signature,
                                const std::vector<unsigned char> &seed,
                                size_t maskLength) {
  std::vector<unsigned char> mask(maskLength);
  std::vector<unsigned char> input(seed.size() + 4);
  RsaSensitiveBufferGuard inputGuard(input);
  std::copy(seed.begin(), seed.end(), input.begin());
  size_t produced = 0;
  uint32_t counter = 0;
  while (produced < maskLength) {
    const size_t offset = seed.size();
    input[offset] = static_cast<unsigned char>(counter >> 24);
    input[offset + 1] = static_cast<unsigned char>(counter >> 16);
    input[offset + 2] = static_cast<unsigned char>(counter >> 8);
    input[offset + 3] = static_cast<unsigned char>(counter);
    auto digest = digestMessage(signature, input);
    RsaSensitiveBufferGuard digestGuard(digest);
    const size_t count = std::min(maskLength - produced, digest.size());
    std::copy_n(digest.begin(), count, mask.begin() + produced);
    produced += count;
    ++counter;
  }
  return mask;
}

std::vector<unsigned char> encodedPss(JC_Instance &signature,
                                     const std::vector<unsigned char> &hash,
                                     uint16_t modulusLength,
                                     uint16_t modulusBits) {
  const size_t hashLength = digestLength(signatureDigestAlgorithm(signature));
  const jshort_t configuredSaltLength = signature.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH);
  if (configuredSaltLength < 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const size_t saltLength = static_cast<size_t>(configuredSaltLength);
  const size_t encodedLength = (static_cast<size_t>(modulusBits - 1) + 7) / 8;
  if (hash.size() != hashLength || encodedLength < hashLength + saltLength + 2)
    throwCryptoException(CryptoExceptionReason::IllegalValue);

  std::vector<unsigned char> salt(saltLength);
  RsaSensitiveBufferGuard saltGuard(salt);
  for (auto &byte : salt) byte = randomByte();
  std::vector<unsigned char> hashInput(8, 0);
  RsaSensitiveBufferGuard hashInputGuard(hashInput);
  // Avoid freeing an earlier allocation that already held the digest or salt.
  hashInput.reserve(8 + hash.size() + salt.size());
  hashInput.insert(hashInput.end(), hash.begin(), hash.end());
  hashInput.insert(hashInput.end(), salt.begin(), salt.end());
  auto h = digestMessage(signature, hashInput);
  RsaSensitiveBufferGuard hGuard(h);
  const size_t dbLength = encodedLength - hashLength - 1;
  std::vector<unsigned char> db(dbLength, 0);
  RsaSensitiveBufferGuard dbGuard(db);
  db[dbLength - saltLength - 1] = 1;
  std::copy(salt.begin(), salt.end(), db.end() - saltLength);
  auto mask = mgf1(signature, h, dbLength);
  RsaSensitiveBufferGuard maskGuard(mask);
  for (size_t i = 0; i < dbLength; ++i) db[i] ^= mask[i];
  const uint8_t unusedBits = static_cast<uint8_t>(8 * encodedLength - (modulusBits - 1));
  if (unusedBits != 0)
    db[0] &= static_cast<unsigned char>(0xffu >> unusedBits);

  std::vector<unsigned char> encoded(modulusLength, 0);
  const size_t leading = modulusLength - encodedLength;
  std::copy(db.begin(), db.end(), encoded.begin() + leading);
  std::copy(h.begin(), h.end(), encoded.begin() + leading + dbLength);
  encoded.back() = 0xbc;
  return encoded;
}

bool verifyPss(JC_Instance &signature, const std::vector<unsigned char> &hash,
               const std::vector<unsigned char> &encoded,
               uint16_t modulusLength, uint16_t modulusBits) {
  const size_t hashLength = digestLength(signatureDigestAlgorithm(signature));
  const jshort_t configuredSaltLength = signature.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH);
  if (configuredSaltLength < 0 || hash.size() != hashLength ||
      encoded.size() != modulusLength || encoded.back() != 0xbc)
    return false;
  const size_t saltLength = static_cast<size_t>(configuredSaltLength);
  const size_t encodedLength = (static_cast<size_t>(modulusBits - 1) + 7) / 8;
  if (encodedLength < hashLength + saltLength + 2 || encodedLength > encoded.size())
    return false;
  const size_t leading = encoded.size() - encodedLength;
  const size_t dbLength = encodedLength - hashLength - 1;
  const uint8_t unusedBits = static_cast<uint8_t>(8 * encodedLength - (modulusBits - 1));
  const unsigned char *maskedDb = encoded.data() + leading;
  const unsigned char *h = maskedDb + dbLength;
  if (unusedBits != 0 && (maskedDb[0] & static_cast<unsigned char>(0xffu << (8 - unusedBits))))
    return false;

  std::vector<unsigned char> hVector(h, h + hashLength);
  RsaSensitiveBufferGuard hVectorGuard(hVector);
  auto mask = mgf1(signature, hVector, dbLength);
  RsaSensitiveBufferGuard maskGuard(mask);
  std::vector<unsigned char> db(dbLength);
  RsaSensitiveBufferGuard dbGuard(db);
  for (size_t i = 0; i < dbLength; ++i) db[i] = maskedDb[i] ^ mask[i];
  if (unusedBits != 0)
    db[0] &= static_cast<unsigned char>(0xffu >> unusedBits);
  const size_t delimiter = dbLength - saltLength - 1;
  uint8_t difference = 0;
  for (size_t i = 0; i < delimiter; ++i) difference |= db[i];
  difference |= static_cast<uint8_t>(db[delimiter] ^ 1);
  if (difference != 0) return false;

  std::vector<unsigned char> hashInput(8, 0);
  RsaSensitiveBufferGuard hashInputGuard(hashInput);
  // The second append must not reallocate and leave a copied digest in freed
  // allocator storage.
  hashInput.reserve(8 + hash.size() + saltLength);
  hashInput.insert(hashInput.end(), hash.begin(), hash.end());
  hashInput.insert(hashInput.end(), db.begin() + delimiter + 1, db.end());
  auto expectedH = digestMessage(signature, hashInput);
  RsaSensitiveBufferGuard expectedHGuard(expectedH);
  difference = 0;
  for (size_t i = 0; i < hashLength; ++i) difference |= expectedH[i] ^ h[i];
  return difference == 0;
}

std::vector<unsigned char> modExp(const std::vector<unsigned char> &modulus,
                                  const std::vector<unsigned char> &exponent,
                                  const std::vector<unsigned char> &input) {
  mbedtls_mpi n, e, x, y;
  mbedtls_mpi_init(&n); mbedtls_mpi_init(&e);
  mbedtls_mpi_init(&x); mbedtls_mpi_init(&y);
  const int rc = mbedtls_mpi_read_binary(&n, modulus.data(), modulus.size()) |
                 mbedtls_mpi_read_binary(&e, exponent.data(), exponent.size()) |
                 mbedtls_mpi_read_binary(&x, input.data(), input.size());
  if (rc != 0 || mbedtls_mpi_cmp_mpi(&x, &n) >= 0 ||
      mbedtls_mpi_exp_mod(&y, &x, &e, &n, nullptr) != 0) {
    mbedtls_mpi_free(&n); mbedtls_mpi_free(&e);
    mbedtls_mpi_free(&x); mbedtls_mpi_free(&y);
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  std::vector<unsigned char> result(modulus.size());
  const int writeResult = mbedtls_mpi_write_binary(&y, result.data(), result.size());
  mbedtls_mpi_free(&n); mbedtls_mpi_free(&e);
  mbedtls_mpi_free(&x); mbedtls_mpi_free(&y);
  if (writeResult != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  return result;
}

std::vector<unsigned char> privateModExp(
    JC_Instance &signature, const std::vector<unsigned char> &modulus,
    const std::vector<unsigned char> &input) {
  if (rsaCrtKey(*rsaKeyInstance(signature.getField_Reference(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY)))) {
    std::array<std::vector<unsigned char>, 5> components;
    RsaSensitiveBufferGuard componentsGuard(components);
    for (uint16_t i = 0; i < components.size(); ++i)
      components[i] = keyComponent(signature, static_cast<uint16_t>(i + 2));
    return rsaCrtModExp(modulus, components, input);
  }
  auto exponent = keyComponent(signature, 1);
  RsaSensitiveBufferGuard exponentGuard(exponent);
  return modExp(modulus, exponent, input);
}

std::vector<unsigned char> transform(JC_Instance &signature,
                                     const std::vector<unsigned char> &hash) {
  const uint8_t algorithm = signatureDigestAlgorithm(signature);
  if (hash.size() != digestLength(algorithm))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto digestInfo = digestInfoFromHash(algorithm, hash.data());
  RsaSensitiveBufferGuard digestInfoGuard(digestInfo);
  auto modulus = keyComponent(signature, 0);
  RsaSensitiveBufferGuard modulusGuard(modulus);
  const uint16_t length = static_cast<uint16_t>(modulus.size());
  if (modulus.size() != length)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (isPss(signature)) {
    const auto key = rsaKeyInstance(signature.getField_Reference(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
    const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
    auto encoded = encodedPss(signature, hash, length, bits);
    RsaSensitiveBufferGuard encodedGuard(encoded);
    return privateModExp(signature, modulus, encoded);
  }
  if (isIso9796(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  auto encoded = encodedMessage(digestInfo, length);
  RsaSensitiveBufferGuard encodedGuard(encoded);
  return privateModExp(signature, modulus, encoded);
}

std::vector<unsigned char> transformMessage(
    JC_Instance &signature, const std::vector<unsigned char> &message,
    size_t *recoverableLength = nullptr) {
  auto hash = signatureHasInitialDigest(signature)
      ? continueSignatureDigest(signature, message)
      : digestMessage(signature, message);
  RsaSensitiveBufferGuard hashGuard(hash);
  if (isIso9796(signature)) {
    const auto key = rsaKeyInstance(signature.getField_Reference(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
    const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
    auto modulus = keyComponent(signature, 0);
    RsaSensitiveBufferGuard modulusGuard(modulus);
    auto encoded = encodedIso9796(message, hash, modulus.size(), bits,
                                  recoverableLength);
    RsaSensitiveBufferGuard encodedGuard(encoded);
    return privateModExp(signature, modulus, encoded);
  }
  return transform(signature, hash);
}

bool verify(JC_Instance &signature, const std::vector<unsigned char> &hash,
            const std::vector<unsigned char> &value,
            const std::vector<unsigned char> *message = nullptr) {
  const auto key = rsaKeyInstance(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  const uint16_t length = keyModulusLength(*key);
  const uint8_t algorithm = signatureDigestAlgorithm(signature);
  if (value.size() != length || hash.size() != digestLength(algorithm)) return false;
  auto modulus = keyComponent(signature, 0);
  RsaSensitiveBufferGuard modulusGuard(modulus);
  auto exponent = keyComponent(signature, 1);
  RsaSensitiveBufferGuard exponentGuard(exponent);
  if (!std::lexicographical_compare(value.begin(), value.end(),
                                    modulus.begin(), modulus.end()))
    return false;
  auto actual = modExp(modulus, exponent, value);
  RsaSensitiveBufferGuard actualGuard(actual);
  if (isIso9796(signature))
    return message && verifyIso9796(*message, hash, actual);
  if (isPss(signature)) {
    const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
    return verifyPss(signature, hash, actual, length, bits);
  }
  auto digestInfo = digestInfoFromHash(algorithm, hash.data());
  RsaSensitiveBufferGuard digestInfoGuard(digestInfo);
  auto expected = encodedMessage(digestInfo, length);
  RsaSensitiveBufferGuard expectedGuard(expected);
  uint8_t difference = 0;
  for (size_t i = 0; i < expected.size(); ++i)
    difference |= static_cast<uint8_t>(expected[i] ^ actual[i]);
  return difference == 0;
}

} // namespace

void initializeRsaSignature(JC_Instance &signature, jref_t keyReference,
                            uint8_t mode, jshort_t saltLength) {
  if (mode != kModeSign && mode != kModeVerify)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto key = rsaKeyInstance(keyReference);
  const uint8_t type = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  const bool privateKey = type == kRsaPrivate ||
                          type == kRsaPrivateTransientReset ||
                          type == kRsaPrivateTransientDeselect || rsaCrtKey(*key);
  const bool publicKey = type == kRsaPublic;
  if ((mode == kModeSign && !privateKey) || (mode == kModeVerify && !publicKey))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint16_t length = keyModulusLength(*key);
  if (isPss(signature)) {
    if (saltLength < 0)
      saltLength = static_cast<jshort_t>(digestLength(
          signatureDigestAlgorithm(signature)));
    const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
    const size_t encodedLength = (static_cast<size_t>(bits - 1) + 7) / 8;
    const size_t hashLength = digestLength(signatureDigestAlgorithm(signature));
    if (encodedLength < hashLength + static_cast<uint16_t>(saltLength) + 2)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  // Read both components now so init reports an uninitialized key before the
  // operation begins. They are re-read on each operation to honor clearKey.
  const jref_t lengthRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  auto lengths = Context::getActiveContext().getHeap().getArray(lengthRef);
  if (!lengths || lengths->size() < 7 || lengths->getShortEntry(0) < 0 ||
      (rsaCrtKey(*key)
           ? (lengths->getShortEntry(2) <= 0 ||
              lengths->getShortEntry(3) <= 0 ||
              lengths->getShortEntry(4) <= 0 ||
              lengths->getShortEntry(5) <= 0 ||
              lengths->getShortEntry(6) <= 0)
           : (lengths->getShortEntry(0) <= 0 ||
              lengths->getShortEntry(1) <= 0)))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  signature.setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY, keyReference);
  signature.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH,
      isPss(signature) ? saltLength : static_cast<jshort_t>(length));
  signature.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH, 0);
  signature.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYSTATE, 0);
  auto recoveryHash = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_RECOVERYHASH);
  for (uint16_t i = 0; i < recoveryHash->size(); ++i)
    recoveryHash->setByteEntry(i, 0);
  signature.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE,
      static_cast<jbyte_t>(mode));
  signature.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 1);
}

jshort_t rsaSignatureLength(JC_Instance &signature) {
  auto key = rsaKeyInstance(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  const jref_t lengthsRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  auto lengths = Context::getActiveContext().getHeap().getArray(lengthsRef);
  if (!lengths || lengths->size() < 7 ||
      (rsaCrtKey(*key)
           ? (lengths->getShortEntry(2) <= 0 ||
              lengths->getShortEntry(3) <= 0 ||
              lengths->getShortEntry(4) <= 0 ||
              lengths->getShortEntry(5) <= 0 ||
              lengths->getShortEntry(6) <= 0)
           : (lengths->getShortEntry(0) <= 0 ||
              lengths->getShortEntry(1) <= 0)))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  return static_cast<jshort_t>(keyModulusLength(*key));
}

jshort_t rsaSignatureSign(JC_Instance &signature,
                          const std::shared_ptr<JC_Array> &input,
                          jshort_t offset, jshort_t length,
                          const std::shared_ptr<JC_Array> &output,
                          jshort_t outputOffset) {
  if (isIso9796MessageRecovery(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  append(signature, input, offset, length);
  auto message = bufferedData(signature);
  RsaSensitiveBufferGuard messageGuard(message);
  auto result = transformMessage(signature, message);
  RsaSensitiveBufferGuard resultGuard(result);
  checkRange(output, outputOffset, static_cast<jshort_t>(result.size()));
  for (size_t i = 0; i < result.size(); ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i), result[i]);
  clearMessage(signature);
  clearSignatureInitialDigest(signature);
  return static_cast<jshort_t>(result.size());
}

jshort_t rsaSignatureSignRecovery(
    JC_Instance &signature, const std::shared_ptr<JC_Array> &input,
    jshort_t offset, jshort_t length,
    const std::shared_ptr<JC_Array> &output, jshort_t outputOffset,
    const std::shared_ptr<JC_Array> &recoveredLength,
    jshort_t recoveredLengthOffset) {
  if (!isIso9796MessageRecovery(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (!recoveredLength) throw Exceptions::NullPointerException;
  if (recoveredLength->getType() != JAVA_ARRAY_T_SHORT)
    throw Exceptions::ArrayStoreException;
  if (recoveredLengthOffset < 0 ||
      static_cast<uint16_t>(recoveredLengthOffset) >= recoveredLength->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
  append(signature, input, offset, length);
  auto message = bufferedData(signature);
  RsaSensitiveBufferGuard messageGuard(message);
  size_t recoveredCount = 0;
  auto result = transformMessage(signature, message, &recoveredCount);
  RsaSensitiveBufferGuard resultGuard(result);
  if (recoveredCount > 32767)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkRange(output, outputOffset, static_cast<jshort_t>(result.size()));
  for (size_t i = 0; i < result.size(); ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i), result[i]);
  recoveredLength->setShortEntry(static_cast<uint16_t>(recoveredLengthOffset),
                                 static_cast<jshort_t>(recoveredCount));
  clearMessage(signature);
  return static_cast<jshort_t>(result.size());
}

jshort_t rsaSignatureBeginVerify(
    JC_Instance &signature, const std::shared_ptr<JC_Array> &signatureData,
    jshort_t offset, jshort_t length, std::vector<unsigned char> &recovered,
    std::vector<unsigned char> &hash, bool &partial) {
  if (!isIso9796MessageRecovery(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  checkRange(signatureData, offset, length);
  const auto key = rsaKeyInstance(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  auto modulus = keyComponent(signature, 0);
  RsaSensitiveBufferGuard modulusGuard(modulus);
  auto exponent = keyComponent(signature, 1);
  RsaSensitiveBufferGuard exponentGuard(exponent);
  if (length != static_cast<jshort_t>(modulus.size()))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  auto value = readArray(*signatureData, static_cast<uint16_t>(offset),
                         static_cast<uint16_t>(length));
  RsaSensitiveBufferGuard valueGuard(value);
  auto encoded = modExp(modulus, exponent, value);
  RsaSensitiveBufferGuard encodedGuard(encoded);
  Iso9796Block decoded;
  RsaSensitiveBufferGuard recoveredGuard(decoded.recoveredMessage);
  RsaSensitiveBufferGuard hashGuard(decoded.hash);
  if (!decodeIso9796(encoded, decoded))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  recovered = std::move(decoded.recoveredMessage);
  hash = std::move(decoded.hash);
  partial = decoded.partial;
  return static_cast<jshort_t>(recovered.size());
}

bool rsaSignatureVerifyRecoveredMessage(
    JC_Instance &signature, const std::vector<unsigned char> &message,
    const std::vector<unsigned char> &hash) {
  if (!isIso9796MessageRecovery(signature) || hash.size() != 20)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  auto actual = digestMessageForIso9796(message);
  RsaSensitiveBufferGuard actualGuard(actual);
  uint8_t difference = 0;
  for (size_t index = 0; index < actual.size(); ++index)
    difference |= static_cast<uint8_t>(actual[index] ^ hash[index]);
  return difference == 0;
}

jshort_t rsaSignatureSignHash(JC_Instance &signature,
                              const std::shared_ptr<JC_Array> &hash,
                              jshort_t offset, jshort_t length,
                              const std::shared_ptr<JC_Array> &output,
                              jshort_t outputOffset) {
  checkRange(hash, offset, length);
  if (isIso9796(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (length != digestLength(signatureDigestAlgorithm(signature)))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  auto hashBytes = readArray(*hash, offset, static_cast<uint16_t>(length));
  RsaSensitiveBufferGuard hashGuard(hashBytes);
  auto value = transform(signature, hashBytes);
  RsaSensitiveBufferGuard valueGuard(value);
  checkRange(output, outputOffset, static_cast<jshort_t>(value.size()));
  for (size_t i = 0; i < value.size(); ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i), value[i]);
  return static_cast<jshort_t>(value.size());
}

jbool_t rsaSignatureVerify(JC_Instance &signature,
                           const std::shared_ptr<JC_Array> &input,
                           jshort_t offset, jshort_t length,
                           const std::shared_ptr<JC_Array> &expected,
                           jshort_t expectedOffset, jshort_t expectedLength) {
  if (isIso9796MessageRecovery(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  append(signature, input, offset, length);
  checkRange(expected, expectedOffset, expectedLength);
  auto message = bufferedData(signature);
  RsaSensitiveBufferGuard messageGuard(message);
  auto hash = signatureHasInitialDigest(signature)
      ? continueSignatureDigest(signature, message)
      : digestMessage(signature, message);
  RsaSensitiveBufferGuard hashGuard(hash);
  auto value = readArray(*expected, expectedOffset, expectedLength);
  RsaSensitiveBufferGuard valueGuard(value);
  const bool valid = verify(signature, hash, value, &message);
  clearMessage(signature);
  clearSignatureInitialDigest(signature);
  return valid ? TRUE : FALSE;
}

jbool_t rsaSignatureVerifyHash(JC_Instance &signature,
                               const std::shared_ptr<JC_Array> &hash,
                               jshort_t hashOffset, jshort_t hashLength,
                               const std::shared_ptr<JC_Array> &expected,
                               jshort_t expectedOffset, jshort_t expectedLength) {
  checkRange(hash, hashOffset, hashLength);
  checkRange(expected, expectedOffset, expectedLength);
  if (isIso9796(signature))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (hashLength != digestLength(signatureDigestAlgorithm(signature)))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  auto hashBytes = readArray(*hash, hashOffset, static_cast<uint16_t>(hashLength));
  RsaSensitiveBufferGuard hashGuard(hashBytes);
  auto value = readArray(*expected, expectedOffset, expectedLength);
  RsaSensitiveBufferGuard valueGuard(value);
  return verify(signature, hashBytes, value) ? TRUE : FALSE;
}
} // namespace jcvm
#endif
