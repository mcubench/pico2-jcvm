#include "exceptions.hpp"
#include "javacard-support.h"
#include "javacard-rom-metadata.h"
#include "context.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_rsa_math.hpp"

#include "mbedtls/bignum.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/sha1.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#ifndef PC_VERSION
#include "pico/rand.h"
#include "pico/time.h"

extern "C" mbedtls_ms_time_t mbedtls_ms_time(void) {
  return static_cast<mbedtls_ms_time_t>(
      to_ms_since_boot(get_absolute_time()));
}
#endif

#if JC_SUPPORT_CIPHER_RSA_PKCS1 || JC_SUPPORT_CIPHER_RSA_NOPAD || \
    JC_SUPPORT_CIPHER_RSA_OAEP
namespace jcvm {
namespace {
constexpr uint8_t kRsaPublic = 4;
constexpr uint8_t kRsaPrivate = 5;
constexpr uint8_t kRsaCrtPrivate = 6;
constexpr uint8_t kRsaPrivateTransientReset = 22;
constexpr uint8_t kRsaPrivateTransientDeselect = 23;
constexpr uint8_t kRsaCrtTransientReset = 24;
constexpr uint8_t kRsaCrtTransientDeselect = 25;

bool rsaCrtKeyType(uint8_t type) {
  return type == kRsaCrtPrivate || type == kRsaCrtTransientReset ||
         type == kRsaCrtTransientDeselect;
}

void checkArray(const JC_Array &array) {
  if (array.getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
}

std::vector<unsigned char> readBytes(JC_Array &array, uint16_t length) {
  std::vector<unsigned char> bytes(length);
  for (uint16_t i = 0; i < length; ++i)
    bytes[i] = static_cast<unsigned char>(array.getByteEntry(i));
  return bytes;
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

void mgf1Sha1(const unsigned char *seed, size_t seedLength,
              unsigned char *mask, size_t maskLength) {
  std::vector<unsigned char> input(seedLength + 4);
  RsaSensitiveBufferGuard inputGuard(input);
  std::copy(seed, seed + seedLength, input.begin());
  size_t produced = 0;
  uint32_t counter = 0;
  while (produced < maskLength) {
    const size_t offset = seedLength;
    input[offset] = static_cast<unsigned char>(counter >> 24);
    input[offset + 1] = static_cast<unsigned char>(counter >> 16);
    input[offset + 2] = static_cast<unsigned char>(counter >> 8);
    input[offset + 3] = static_cast<unsigned char>(counter);
    std::array<unsigned char, 20> digest{};
    if (mbedtls_sha1(input.data(), input.size(), digest.data()) != 0)
      throw Exceptions::RuntimeException;
    const size_t count = std::min(maskLength - produced, digest.size());
    std::copy_n(digest.begin(), count, mask + produced);
    produced += count;
    ++counter;
  }
}

void modExp(const std::vector<unsigned char> &modulus,
            const std::vector<unsigned char> &exponent,
            const std::vector<unsigned char> &input,
            std::vector<unsigned char> &output) {
  mbedtls_mpi n, e, x, rr;
  mbedtls_mpi_init(&n);
  mbedtls_mpi_init(&e);
  mbedtls_mpi_init(&x);
  mbedtls_mpi_init(&rr);
  const int importResult =
      mbedtls_mpi_read_binary(&n, modulus.data(), modulus.size()) == 0 &&
      mbedtls_mpi_read_binary(&e, exponent.data(), exponent.size()) == 0 &&
      mbedtls_mpi_read_binary(&x, input.data(), input.size()) == 0
          ? 0 : -1;
  if (importResult == 0 && mbedtls_mpi_cmp_mpi(&x, &n) >= 0) {
    mbedtls_mpi_free(&rr);
    mbedtls_mpi_free(&x);
    mbedtls_mpi_free(&e);
    mbedtls_mpi_free(&n);
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  const int result = importResult == 0
      ? mbedtls_mpi_exp_mod(&x, &x, &e, &n, &rr) : importResult;
  if (result == 0)
    mbedtls_mpi_write_binary(&x, output.data(), output.size());
  mbedtls_mpi_free(&rr);
  mbedtls_mpi_free(&x);
  mbedtls_mpi_free(&e);
  mbedtls_mpi_free(&n);
  if (result != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
}

std::vector<unsigned char> oaepEncode(
    const std::vector<unsigned char> &message, size_t modulusLength) {
  constexpr size_t hashLength = 20;
  if (modulusLength < hashLength * 2 + 2 ||
      message.size() > modulusLength - hashLength * 2 - 2)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  static constexpr std::array<unsigned char, 20> emptyLabelHash = {
      0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,0x32,0x55,
      0xbf,0xef,0x95,0x60,0x18,0x90,0xaf,0xd8,0x07,0x09};
  const size_t dbLength = modulusLength - hashLength - 1;
  std::vector<unsigned char> db(dbLength, 0);
  RsaSensitiveBufferGuard dbGuard(db);
  std::copy(emptyLabelHash.begin(), emptyLabelHash.end(), db.begin());
  const size_t delimiter = dbLength - message.size() - 1;
  db[delimiter] = 1;
  std::copy(message.begin(), message.end(), db.begin() + delimiter + 1);
  std::array<unsigned char, hashLength> seed{};
  RsaSensitiveBufferGuard seedGuard(seed);
  for (auto &byte : seed) byte = randomByte();
  std::vector<unsigned char> dbMask(dbLength);
  RsaSensitiveBufferGuard dbMaskGuard(dbMask);
  mgf1Sha1(seed.data(), seed.size(), dbMask.data(), dbMask.size());
  for (size_t i = 0; i < dbLength; ++i) db[i] ^= dbMask[i];
  std::array<unsigned char, hashLength> seedMask{};
  RsaSensitiveBufferGuard seedMaskGuard(seedMask);
  mgf1Sha1(db.data(), db.size(), seedMask.data(), seedMask.size());
  std::vector<unsigned char> encoded(modulusLength, 0);
  for (size_t i = 0; i < hashLength; ++i) encoded[1 + i] = seed[i] ^ seedMask[i];
  std::copy(db.begin(), db.end(), encoded.begin() + 1 + hashLength);
  return encoded;
}

std::vector<unsigned char> oaepDecode(
    const std::vector<unsigned char> &encoded) {
  constexpr size_t hashLength = 20;
  if (encoded.size() < hashLength * 2 + 2 || encoded[0] != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  static constexpr std::array<unsigned char, 20> emptyLabelHash = {
      0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,0x32,0x55,
      0xbf,0xef,0x95,0x60,0x18,0x90,0xaf,0xd8,0x07,0x09};
  const size_t dbLength = encoded.size() - hashLength - 1;
  std::array<unsigned char, hashLength> seed{};
  RsaSensitiveBufferGuard seedGuard(seed);
  std::copy_n(encoded.begin() + 1, hashLength, seed.begin());
  std::vector<unsigned char> db(encoded.begin() + 1 + hashLength, encoded.end());
  RsaSensitiveBufferGuard dbGuard(db);
  std::array<unsigned char, hashLength> seedMask{};
  RsaSensitiveBufferGuard seedMaskGuard(seedMask);
  mgf1Sha1(db.data(), db.size(), seedMask.data(), seedMask.size());
  for (size_t i = 0; i < hashLength; ++i) seed[i] ^= seedMask[i];
  std::vector<unsigned char> dbMask(dbLength);
  RsaSensitiveBufferGuard dbMaskGuard(dbMask);
  mgf1Sha1(seed.data(), seed.size(), dbMask.data(), dbMask.size());
  for (size_t i = 0; i < dbLength; ++i) db[i] ^= dbMask[i];
  uint8_t bad = 0;
  for (size_t i = 0; i < hashLength; ++i) bad |= db[i] ^ emptyLabelHash[i];
  size_t delimiter = hashLength;
  while (delimiter < dbLength && db[delimiter] == 0) ++delimiter;
  if (bad != 0 || delimiter == dbLength || db[delimiter] != 1)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return std::vector<unsigned char>(db.begin() + delimiter + 1, db.end());
}
} // namespace

static jshort_t transformRsa(
    jbyte_t padding, jbool_t privateOperation,
    std::shared_ptr<JC_Array> modulusArray,
    std::shared_ptr<JC_Array> exponentArray,
    std::shared_ptr<JC_Array> inputArray, jshort_t inputLength,
    std::shared_ptr<JC_Array> outputArray) {
  if (!modulusArray || !exponentArray || !inputArray || !outputArray)
    throw Exceptions::NullPointerException;
  checkArray(*modulusArray);
  checkArray(*exponentArray);
  checkArray(*inputArray);
  checkArray(*outputArray);
  const uint16_t keyLength = modulusArray->size();
  const uint16_t crtPartLength = static_cast<uint16_t>((keyLength + 1U) / 2U);
  const bool crtPrivate = privateOperation &&
      exponentArray->size() == static_cast<uint32_t>(crtPartLength) * 5U;
  if (keyLength < 64 ||
      (exponentArray->size() != keyLength && !crtPrivate) || inputLength < 0 ||
      inputLength > inputArray->size() || outputArray->size() < keyLength)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint8_t pad = static_cast<uint8_t>(padding);
  const bool nopad = pad == 1;
  const bool pkcs1 = pad == 7;
  const bool oaep = pad == 9;
  if ((!nopad && !pkcs1 && !oaep) || (privateOperation && nopad == false &&
      pad != 7 && pad != 9))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  auto modulus = readBytes(*modulusArray, keyLength);
  RsaSensitiveBufferGuard modulusGuard(modulus);
  auto exponent = readBytes(*exponentArray, exponentArray->size());
  RsaSensitiveBufferGuard exponentGuard(exponent);
  auto input = readBytes(*inputArray, static_cast<uint16_t>(inputLength));
  RsaSensitiveBufferGuard inputGuard(input);
  std::vector<unsigned char> encoded;
  RsaSensitiveBufferGuard encodedGuard(encoded);
  if (nopad) {
    if (input.size() != keyLength)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    encoded = std::move(input);
  } else if (privateOperation) {
    if (input.size() != keyLength)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    encoded = std::move(input);
  } else if (pkcs1) {
    if (input.size() > static_cast<size_t>(keyLength - 11))
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    encoded.assign(keyLength, 0xff);
    encoded[0] = 0;
    encoded[1] = 2;
    const size_t separator = keyLength - input.size() - 1;
    for (size_t i = 2; i < separator; ++i) {
      uint8_t byte = 0;
      while (byte == 0) byte = randomByte();
      encoded[i] = byte;
    }
    encoded[separator] = 0;
    std::copy(input.begin(), input.end(), encoded.begin() + separator + 1);
  } else {
    encoded = oaepEncode(input, keyLength);
  }
  std::vector<unsigned char> result(keyLength);
  RsaSensitiveBufferGuard resultGuard(result);
  if (crtPrivate) {
    std::array<std::vector<unsigned char>, 5> components;
    RsaSensitiveBufferGuard componentsGuard(components);
    for (size_t i = 0; i < components.size(); ++i) {
      const size_t offset = i * crtPartLength;
      components[i].assign(exponent.begin() + offset,
                           exponent.begin() + offset + crtPartLength);
    }
    result = rsaCrtModExp(modulus, components, encoded);
  } else {
    modExp(modulus, exponent, encoded, result);
  }
  size_t resultLength = keyLength;
  if (privateOperation && !nopad) {
    if (result[0] != 0 || (pkcs1 && result[1] != 2))
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    if (pkcs1) {
      size_t separator = 2;
      while (separator < result.size() && result[separator] != 0) ++separator;
      if (separator < 10 || separator >= result.size())
        throwCryptoException(CryptoExceptionReason::IllegalUse);
      resultLength = result.size() - separator - 1;
      std::move(result.begin() + separator + 1, result.end(), result.begin());
    } else {
      result = oaepDecode(result);
      resultLength = result.size();
    }
  }
  for (size_t i = 0; i < resultLength; ++i)
    outputArray->setByteEntry(static_cast<uint16_t>(i),
                              static_cast<jbyte_t>(result[i]));
  return static_cast<jshort_t>(resultLength);
}

namespace {
constexpr uint8_t kModeDecrypt = 1;
constexpr uint8_t kModeEncrypt = 2;

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

std::shared_ptr<JC_Array> getByteArray(JC_Instance &instance,
                                       const uint16_t field) {
  const jref_t reference = instance.getField_Reference(field);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  return array;
}

std::shared_ptr<JC_Array> getShortArray(JC_Instance &instance,
                                        const uint16_t field) {
  const jref_t reference = instance.getField_Reference(field);
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_SHORT)
    throw Exceptions::ArrayStoreException;
  return array;
}

void checkArray(const std::shared_ptr<JC_Array> &array, const jshort_t offset,
                const jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  const uint32_t size = array->size();
  if (offset < 0 || length < 0 || static_cast<uint32_t>(offset) > size ||
      static_cast<uint32_t>(length) > size - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

void allocateCipherArray(JC_Instance &cipher, const uint16_t field,
                         const uint16_t length) {
  Heap &heap = Context::getActiveContext().getHeap();
  cipher.setField_Reference(field,
      heap.addArray(length, JAVA_ARRAY_T_BYTE));
}

void loadKeyComponent(const JC_Instance &key, const uint16_t valueField,
                      const uint16_t lengthField, JC_Array &destination,
                      const uint16_t destinationOffset,
                      const uint16_t destinationLength) {
  auto value = getByteArray(const_cast<JC_Instance &>(key), valueField);
  auto lengths = getShortArray(const_cast<JC_Instance &>(key),
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  const jshort_t componentLength = lengths->getShortEntry(lengthField);
  if (componentLength <= 0)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  if (static_cast<uint32_t>(destinationOffset) + destinationLength >
          destination.size() ||
      static_cast<uint16_t>(componentLength) > destinationLength ||
      static_cast<uint16_t>(componentLength) > value->size())
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint16_t srcStart = static_cast<uint16_t>(value->size() - componentLength);
  for (uint16_t i = 0; i < destinationLength; ++i)
    destination.setByteEntry(static_cast<uint16_t>(destinationOffset + i), 0);
  for (uint16_t i = 0; i < static_cast<uint16_t>(componentLength); ++i)
    destination.setByteEntry(static_cast<uint16_t>(destinationOffset +
                             destinationLength - componentLength + i),
        value->getByteEntry(static_cast<uint16_t>(srcStart + i)));
}

void appendInput(JC_Instance &cipher, const std::shared_ptr<JC_Array> &input,
                 const jshort_t offset, const jshort_t length) {
  auto current = getByteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  const jshort_t oldLength = cipher.getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH);
  const jshort_t maxLength = cipher.getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH);
  const int32_t wanted = static_cast<int32_t>(oldLength) + length;
  if (wanted < 0 || wanted > maxLength)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (wanted > static_cast<int32_t>(current->size())) {
    allocateCipherArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED,
        static_cast<uint16_t>(wanted));
    auto expanded = getByteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
    for (jshort_t i = 0; i < oldLength; ++i)
      expanded->setByteEntry(static_cast<uint16_t>(i),
                             current->getByteEntry(static_cast<uint16_t>(i)));
    current = expanded;
  }
  for (jshort_t i = 0; i < length; ++i)
    current->setByteEntry(static_cast<uint16_t>(oldLength + i),
        input->getByteEntry(static_cast<uint16_t>(offset + i)));
  cipher.setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH,
      static_cast<jshort_t>(wanted));
}

void clearInput(JC_Instance &cipher) {
  auto buffered = getByteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  const jshort_t length = cipher.getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH);
  for (jshort_t i = 0; i < length; ++i)
    buffered->setByteEntry(static_cast<uint16_t>(i), 0);
  cipher.setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH, 0);
}
} // namespace

void initializeRsaCipher(std::shared_ptr<JC_Instance> cipher,
                         jref_t keyReference, jbyte_t requestedMode) {
  if (keyReference.isNullPointer()) throw Exceptions::NullPointerException;
  const uint8_t mode = static_cast<uint8_t>(requestedMode);
  if (mode != kModeEncrypt && mode != kModeDecrypt)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto key = rsaKeyInstance(keyReference);
  auto lengths = getShortArray(*key,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  const uint8_t type = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  const bool publicKey = type == kRsaPublic;
  const bool privateKey = type == kRsaPrivate ||
                          type == kRsaPrivateTransientReset ||
                          type == kRsaPrivateTransientDeselect ||
                          rsaCrtKeyType(type);
  if (lengths->size() < 7 ||
      (rsaCrtKeyType(type)
           ? (lengths->getShortEntry(2) <= 0 ||
              lengths->getShortEntry(3) <= 0 ||
              lengths->getShortEntry(4) <= 0 ||
              lengths->getShortEntry(5) <= 0 ||
              lengths->getShortEntry(6) <= 0)
           : (lengths->getShortEntry(0) <= 0 ||
              lengths->getShortEntry(1) <= 0)))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  const bool noPadding = cipher->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM) == 1;
  const bool standardMode = (publicKey && mode == kModeEncrypt) ||
                            (privateKey && mode == kModeDecrypt);
  const bool rawRsaMode = noPadding && (publicKey || privateKey);
  if (!standardMode && !rawRsaMode)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const jshort_t keyBits = key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE);
  if (keyBits <= 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint16_t keyLength = static_cast<uint16_t>((keyBits + 7) / 8);
  const uint16_t crtPartLength = static_cast<uint16_t>((keyLength + 1U) / 2U);
  allocateCipherArray(*cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS, keyLength);
  allocateCipherArray(*cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_EXPONENT,
      rsaCrtKeyType(type) ? static_cast<uint16_t>(5U * crtPartLength)
                          : keyLength);
  allocateCipherArray(*cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED, 0);
  auto modulus = getByteArray(*cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS);
  auto exponent = getByteArray(*cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_EXPONENT);
  if (rsaCrtKeyType(type)) {
    static constexpr uint16_t crtFields[] = {
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_P,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_Q,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DP,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DQ,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PQ};
    std::array<std::vector<unsigned char>, 2> factors;
    for (uint16_t i = 0; i < std::size(crtFields); ++i) {
      loadKeyComponent(*key, crtFields[i], static_cast<uint16_t>(i + 2),
                       *exponent, static_cast<uint16_t>(i * crtPartLength),
                       crtPartLength);
      if (i < factors.size()) {
        std::vector<unsigned char> &factor = factors[i];
        factor.resize(crtPartLength);
        const uint16_t start = static_cast<uint16_t>(i * crtPartLength);
        for (uint16_t j = 0; j < crtPartLength; ++j)
          factor[j] = static_cast<unsigned char>(exponent->getByteEntry(
              static_cast<uint16_t>(start + j)));
      }
    }
    const auto derivedModulus = rsaCrtModulus(factors[0], factors[1], keyLength);
    for (uint16_t i = 0; i < keyLength; ++i)
      modulus->setByteEntry(i, static_cast<jbyte_t>(derivedModulus[i]));
  } else {
    loadKeyComponent(*key,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_MODULUS, 0,
        *modulus, 0, keyLength);
    loadKeyComponent(*key,
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_EXPONENT, 1,
        *exponent, 0, keyLength);
  }
  cipher->setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH, 0);
  cipher->setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH,
      static_cast<jshort_t>(keyLength));
  cipher->setField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE, requestedMode);
  cipher->setField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PRIVATEOPERATION,
      privateKey ? 1 : 0);
  cipher->setField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED, 1);
}

void rejectRsaCipherParameters(std::shared_ptr<JC_Instance> cipher,
                               jref_t key, jbyte_t mode,
                               std::shared_ptr<JC_Array> parameters,
                               jshort_t offset, jshort_t length) {
  (void)cipher; (void)key; (void)mode; (void)parameters; (void)offset; (void)length;
  throwCryptoException(CryptoExceptionReason::IllegalValue);
}

jshort_t updateRsaCipher(std::shared_ptr<JC_Instance> cipher,
                         std::shared_ptr<JC_Array> input, jshort_t inputOffset,
                         jshort_t inputLength, std::shared_ptr<JC_Array> output,
                         jshort_t outputOffset) {
  if (!cipher->getField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED))
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  checkArray(input, inputOffset, inputLength);
  checkArray(output, outputOffset, 0);
  appendInput(*cipher, input, inputOffset, inputLength);
  return 0;
}

jshort_t doFinalRsaCipher(std::shared_ptr<JC_Instance> cipher,
                          std::shared_ptr<JC_Array> input, jshort_t inputOffset,
                          jshort_t inputLength, std::shared_ptr<JC_Array> output,
                          jshort_t outputOffset) {
  if (!cipher->getField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED))
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  checkArray(input, inputOffset, inputLength);
  checkArray(output, outputOffset, 0);
  appendInput(*cipher, input, inputOffset, inputLength);
  const jshort_t modulusLength = cipher->getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH);
  Heap &heap = Context::getActiveContext().getHeap();
  const jref_t resultReference = heap.addArray(
      static_cast<uint16_t>(modulusLength), JAVA_ARRAY_T_BYTE);
  auto result = heap.getArray(resultReference);
  const bool privateOperation = cipher->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PRIVATEOPERATION) != 0;
  const jshort_t resultLength = transformRsa(
      cipher->getField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM),
      privateOperation ? TRUE : FALSE,
      getByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS),
      getByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_EXPONENT),
      getByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED),
      cipher->getField_Short(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH),
      result);
  checkArray(output, outputOffset, resultLength);
  for (jshort_t i = 0; i < resultLength; ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         result->getByteEntry(static_cast<uint16_t>(i)));
  clearInput(*cipher);
  return resultLength;
}
} // namespace jcvm
#endif
