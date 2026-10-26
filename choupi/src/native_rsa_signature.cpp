#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"

#include "mbedtls/bignum.h"
#include "mbedtls/sha1.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#if JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1
namespace jcvm {
namespace {
constexpr uint8_t kRsaPublic = 4;
constexpr uint8_t kRsaPrivate = 5;
constexpr uint8_t kRsaPrivateTransientReset = 22;
constexpr uint8_t kRsaPrivateTransientDeselect = 23;
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
std::vector<unsigned char> sha1(const std::vector<unsigned char> &message);

std::shared_ptr<JC_Array> keyComponentArray(JC_Instance &key,
                                            uint16_t component) {
  const uint16_t fields[] = {
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_MODULUS,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_EXPONENT};
  if (component >= 2) throwCryptoException(CryptoExceptionReason::IllegalValue);
  return byteArray(key, fields[component]);
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
  if (length <= 0) throwCryptoException(CryptoExceptionReason::UninitializedKey);
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
  const jshort_t count = signature.getField_Short(countField);
  const int32_t total = static_cast<int32_t>(count) + length;
  if (count < 0 || total > 32767)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (total > current->size()) {
    uint32_t capacity = std::max<uint16_t>(64, current->size());
    while (capacity < static_cast<uint32_t>(total)) capacity *= 2;
    capacity = std::min<uint32_t>(capacity, 32767);
    const jref_t expandedRef = Context::getActiveContext().getHeap().addArray(
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
  signature.setField_Short(countField, static_cast<jshort_t>(total));
}

std::vector<unsigned char> bufferedData(JC_Instance &signature) {
  const uint16_t bufferField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER;
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  auto buffer = byteArray(signature, bufferField);
  const jshort_t count = signature.getField_Short(countField);
  if (count < 0 || static_cast<uint16_t>(count) > buffer->size())
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return readArray(*buffer, 0, static_cast<uint16_t>(count));
}

void clearMessage(JC_Instance &signature) {
  auto buffer = byteArray(signature,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
  const uint16_t countField =
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  const jshort_t count = signature.getField_Short(countField);
  if (count < 0 || static_cast<uint16_t>(count) > buffer->size())
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  for (jshort_t i = 0; i < count; ++i)
    buffer->setByteEntry(static_cast<uint16_t>(i), 0);
  signature.setField_Short(countField, 0);
}

std::vector<unsigned char> digestInfoFromHash(const unsigned char *digest) {
  static constexpr unsigned char prefix[] = {
      0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
      0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
  std::vector<unsigned char> result(std::begin(prefix), std::end(prefix));
  result.insert(result.end(), digest, digest + 20);
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

std::vector<unsigned char> transform(JC_Instance &signature,
                                     const std::vector<unsigned char> &hash) {
  if (hash.size() != 20) throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto digestInfo = digestInfoFromHash(hash.data());
  const auto modulus = keyComponent(signature, 0);
  const auto exponent = keyComponent(signature, 1);
  const uint16_t length = static_cast<uint16_t>(signature.getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH));
  if (modulus.size() != length)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  return modExp(modulus, exponent, encodedMessage(digestInfo, length));
}

std::vector<unsigned char> transformMessage(
    JC_Instance &signature, const std::vector<unsigned char> &message) {
  const auto hash = sha1(message);
  return transform(signature, hash);
}

bool verify(JC_Instance &signature, const std::vector<unsigned char> &hash,
            const std::vector<unsigned char> &value) {
  const uint16_t length = static_cast<uint16_t>(signature.getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH));
  if (value.size() != length || hash.size() != 20) return false;
  const auto modulus = keyComponent(signature, 0);
  const auto exponent = keyComponent(signature, 1);
  if (!std::lexicographical_compare(value.begin(), value.end(),
                                    modulus.begin(), modulus.end()))
    return false;
  const auto expected = encodedMessage(digestInfoFromHash(hash.data()), length);
  const auto actual = modExp(modulus, exponent, value);
  uint8_t difference = 0;
  for (size_t i = 0; i < expected.size(); ++i)
    difference |= static_cast<uint8_t>(expected[i] ^ actual[i]);
  return difference == 0;
}

std::vector<unsigned char> sha1(const std::vector<unsigned char> &message) {
  unsigned char digest[20];
  if (mbedtls_sha1(message.data(), message.size(), digest) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return {digest, digest + sizeof(digest)};
}
} // namespace

void initializeRsaSignature(JC_Instance &signature, jref_t keyReference,
                            uint8_t mode) {
  if (mode != kModeSign && mode != kModeVerify)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto key = rsaKeyInstance(keyReference);
  const uint8_t type = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  const bool privateKey = type == kRsaPrivate ||
                          type == kRsaPrivateTransientReset ||
                          type == kRsaPrivateTransientDeselect;
  const bool publicKey = type == kRsaPublic;
  if ((mode == kModeSign && !privateKey) || (mode == kModeVerify && !publicKey))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint16_t length = keyModulusLength(*key);
  // Read both components now so init reports an uninitialized key before the
  // operation begins. They are re-read on each operation to honor clearKey.
  const jref_t lengthRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  auto lengths = Context::getActiveContext().getHeap().getArray(lengthRef);
  if (!lengths || lengths->getShortEntry(0) <= 0 || lengths->getShortEntry(1) <= 0)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  signature.setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY, keyReference);
  signature.setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_OUTPUTLENGTH,
      static_cast<jshort_t>(length));
  signature.setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH, 0);
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE,
      static_cast<jbyte_t>(mode));
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 1);
}

jshort_t rsaSignatureLength(JC_Instance &signature) {
  auto key = rsaKeyInstance(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  const jref_t lengthsRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS);
  auto lengths = Context::getActiveContext().getHeap().getArray(lengthsRef);
  if (!lengths || lengths->getShortEntry(0) <= 0 || lengths->getShortEntry(1) <= 0)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  return static_cast<jshort_t>(keyModulusLength(*key));
}

jshort_t rsaSignatureSign(JC_Instance &signature,
                          const std::shared_ptr<JC_Array> &input,
                          jshort_t offset, jshort_t length,
                          const std::shared_ptr<JC_Array> &output,
                          jshort_t outputOffset) {
  append(signature, input, offset, length);
  const auto message = bufferedData(signature);
  const auto result = transformMessage(signature, message);
  checkRange(output, outputOffset, static_cast<jshort_t>(result.size()));
  for (size_t i = 0; i < result.size(); ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i), result[i]);
  clearMessage(signature);
  return static_cast<jshort_t>(result.size());
}

jshort_t rsaSignatureSignHash(JC_Instance &signature,
                              const std::shared_ptr<JC_Array> &hash,
                              jshort_t offset, jshort_t length,
                              const std::shared_ptr<JC_Array> &output,
                              jshort_t outputOffset) {
  checkRange(hash, offset, length);
  if (length != 20) throwCryptoException(CryptoExceptionReason::IllegalValue);
  const auto value = transform(signature, readArray(*hash, offset, 20));
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
  append(signature, input, offset, length);
  checkRange(expected, expectedOffset, expectedLength);
  const auto message = bufferedData(signature);
  const auto hash = sha1(message);
  const auto value = readArray(*expected, expectedOffset, expectedLength);
  const bool valid = verify(signature, hash, value);
  clearMessage(signature);
  return valid ? TRUE : FALSE;
}

jbool_t rsaSignatureVerifyHash(JC_Instance &signature,
                               const std::shared_ptr<JC_Array> &hash,
                               jshort_t hashOffset, jshort_t hashLength,
                               const std::shared_ptr<JC_Array> &expected,
                               jshort_t expectedOffset, jshort_t expectedLength) {
  checkRange(hash, hashOffset, hashLength);
  checkRange(expected, expectedOffset, expectedLength);
  if (hashLength != 20) throwCryptoException(CryptoExceptionReason::IllegalValue);
  return verify(signature, readArray(*hash, hashOffset, 20),
      readArray(*expected, expectedOffset, expectedLength)) ? TRUE : FALSE;
}
} // namespace jcvm
#endif
