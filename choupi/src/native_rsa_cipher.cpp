#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"
#include "native_crypto_exception.hpp"

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
  std::copy(emptyLabelHash.begin(), emptyLabelHash.end(), db.begin());
  const size_t delimiter = dbLength - message.size() - 1;
  db[delimiter] = 1;
  std::copy(message.begin(), message.end(), db.begin() + delimiter + 1);
  std::array<unsigned char, hashLength> seed{};
  for (auto &byte : seed) byte = randomByte();
  std::vector<unsigned char> dbMask(dbLength);
  mgf1Sha1(seed.data(), seed.size(), dbMask.data(), dbMask.size());
  for (size_t i = 0; i < dbLength; ++i) db[i] ^= dbMask[i];
  std::array<unsigned char, hashLength> seedMask{};
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
  std::copy_n(encoded.begin() + 1, hashLength, seed.begin());
  std::vector<unsigned char> db(encoded.begin() + 1 + hashLength, encoded.end());
  std::array<unsigned char, hashLength> seedMask{};
  mgf1Sha1(db.data(), db.size(), seedMask.data(), seedMask.size());
  for (size_t i = 0; i < hashLength; ++i) seed[i] ^= seedMask[i];
  std::vector<unsigned char> dbMask(dbLength);
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

jshort_t fr_gouv_ssi_nativeimpl_RsaCipherNativeBridge_transform(
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
  if (keyLength < 64 || exponentArray->size() != keyLength || inputLength < 0 ||
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
  auto exponent = readBytes(*exponentArray, keyLength);
  auto input = readBytes(*inputArray, static_cast<uint16_t>(inputLength));
  std::vector<unsigned char> encoded;
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
  modExp(modulus, exponent, encoded, result);
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
} // namespace jcvm
#endif
