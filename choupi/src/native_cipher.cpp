#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "heap.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_rsa_cipher.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace jcvm {
#if JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_AES_ECB_NOPAD || JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M1 || \
    JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M2 || JC_SUPPORT_CIPHER_AES_CBC_PKCS5 || \
    JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M1 || JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_AES_ECB_PKCS5
int cipherAesCbc(bool encrypt, std::vector<unsigned char> &key,
                 std::vector<unsigned char> &iv,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
int cipherAesEcb(bool encrypt, std::vector<unsigned char> &key,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
#endif
#if JC_SUPPORT_CIPHER_AES_CTR || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CTR
int cipherAesCtr(std::vector<unsigned char> &key,
                 std::vector<unsigned char> &counter, size_t &offset,
                 std::vector<unsigned char> &streamBlock,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
#endif
#if JC_SUPPORT_CIPHER_DES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M1 || JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_DES_CBC_PKCS5 || JC_SUPPORT_CIPHER_DES_ECB_NOPAD || \
    JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M1 || JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_DES_ECB_PKCS5
int cipherDesCbc(bool encrypt, std::vector<unsigned char> &key,
                 std::vector<unsigned char> &iv,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
int cipherDesEcb(bool encrypt, std::vector<unsigned char> &key,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
#endif

namespace {
constexpr uint8_t kModeDecrypt = 1;
constexpr uint8_t kModeEncrypt = 2;
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
constexpr jbyte_t kCipherKeyWorkspaceMarker = static_cast<jbyte_t>(0xA5);

struct CipherSpec {
  uint8_t algorithm;
  uint8_t cipher;
  uint8_t padding;
  bool aes;
  bool cbc;
  bool rsa;
};

uint16_t cipherClassIndex() {
  const uint16_t offset =
      Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO))
          .getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARDX_CRYPTO_CIPHER);
  return HTONS(offset);
}

std::shared_ptr<JC_Instance> cipherInstance(jref_t reference) {
  if (reference.isNullPointer() || reference.isArray())
    throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO ||
      instance->getClassIndex() != cipherClassIndex())
    throw Exceptions::SecurityException;
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

jref_t allocateByteArray(uint16_t length) {
  // Cipher's mutable workspaces share the native-session lifetime of its
  // counters: transaction rollback must not restore bytes independently of
  // those counters, and reset must wipe any buffered plaintext/ciphertext.
  return Context::getActiveContext().getHeap().addTransientArray(
      length, JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET);
}

void setByteArray(JC_Instance &instance, uint16_t field, uint16_t length) {
  instance.setField_Reference(field, allocateByteArray(length));
}

void ensureTransientByteArray(JC_Instance &instance, const uint16_t field,
                              const uint16_t minimumLength) {
  const jref_t reference = instance.getField_Reference(field);
  if (!reference.isNullPointer()) {
    auto array = Context::getActiveContext().getHeap().getArray(reference);
    if (array && array->getType() == JAVA_ARRAY_T_BYTE &&
        array->isTransientArray() && array->size() >= minimumLength) {
      return;
    }
  }
  setByteArray(instance, field, minimumLength);
}

void ensureCapacity(JC_Instance &cipher, uint16_t wanted) {
  auto current = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  if (wanted <= current->size()) return;
  if (wanted > 32767) throw Exceptions::ArrayIndexOutOfBoundsException;
  uint32_t capacity = std::max<uint32_t>(32, current->size());
  while (capacity < wanted) capacity = std::min<uint32_t>(32767, capacity * 2);
  if (capacity < wanted) throw Exceptions::ArrayIndexOutOfBoundsException;
  auto nextRef = allocateByteArray(static_cast<uint16_t>(capacity));
  auto next = Context::getActiveContext().getHeap().getArray(nextRef);
  const uint16_t oldLength = static_cast<uint16_t>(cipher.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
  for (uint16_t i = 0; i < oldLength; ++i)
    next->setByteEntry(i, current->getByteEntry(i));
  cipher.setField_Reference(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED, nextRef);
}

void clearBuffer(JC_Instance &cipher) {
  const jshort_t length = cipher.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH);
  auto buffer = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  for (jshort_t i = 0; i < length; ++i)
    buffer->setByteEntry(static_cast<uint16_t>(i), 0);
  cipher.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH, 0);
}

void checkArray(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) > array->size() ||
      static_cast<uint32_t>(length) > array->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

std::vector<unsigned char> readArray(const std::shared_ptr<JC_Array> &array,
                                     uint16_t offset, uint16_t length) {
  std::vector<unsigned char> result(length);
  for (uint16_t i = 0; i < length; ++i)
    result[i] = static_cast<unsigned char>(array->getByteEntry(offset + i));
  return result;
}

void writeArray(const std::shared_ptr<JC_Array> &array, uint16_t offset,
                const std::vector<unsigned char> &bytes, uint16_t length) {
  for (uint16_t i = 0; i < length; ++i)
    array->setByteEntry(offset + i, static_cast<jbyte_t>(bytes[i]));
}

bool isAesAlgorithm(uint8_t algorithm) {
  return algorithm == 13 || algorithm == 14 ||
         (algorithm >= 22 && algorithm <= 27) || algorithm == 0xF0;
}
bool isDesAlgorithm(uint8_t algorithm) {
  return algorithm >= 1 && algorithm <= 8;
}
bool isRsaAlgorithm(uint8_t algorithm) {
  return algorithm == 10 || algorithm == 12 || algorithm == 15;
}

bool singleAlgorithmEnabled(uint8_t algorithm) {
  switch (algorithm) {
  case 1: return JC_SUPPORT_CIPHER_DES_CBC_NOPAD;
  case 2: return JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M1;
  case 3: return JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M2;
  case 4: return JC_SUPPORT_CIPHER_DES_CBC_PKCS5;
  case 5: return JC_SUPPORT_CIPHER_DES_ECB_NOPAD;
  case 6: return JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M1;
  case 7: return JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M2;
  case 8: return JC_SUPPORT_CIPHER_DES_ECB_PKCS5;
  case 10: return JC_SUPPORT_CIPHER_RSA_PKCS1;
  case 12: return JC_SUPPORT_CIPHER_RSA_NOPAD;
  case 13: return JC_SUPPORT_CIPHER_AES_CBC_NOPAD;
  case 0xF0: return JC_SUPPORT_CIPHER_AES_CTR;
  case 14: return JC_SUPPORT_CIPHER_AES_ECB_NOPAD;
  case 15: return JC_SUPPORT_CIPHER_RSA_OAEP;
  case 22: return JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M1;
  case 23: return JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M2;
  case 24: return JC_SUPPORT_CIPHER_AES_CBC_PKCS5;
  case 25: return JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M1;
  case 26: return JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M2;
  case 27: return JC_SUPPORT_CIPHER_AES_ECB_PKCS5;
  default: return false;
  }
}

bool pairAlgorithmEnabled(uint8_t algorithm) {
  switch (algorithm) {
  case 1: return JC_SUPPORT_CIPHER_DES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD;
  case 2: return JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M1;
  case 3: return JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M2;
  case 4: return JC_SUPPORT_CIPHER_DES_CBC_PKCS5;
  case 5: return JC_SUPPORT_CIPHER_DES_ECB_NOPAD;
  case 6: return JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M1;
  case 7: return JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M2;
  case 8: return JC_SUPPORT_CIPHER_DES_ECB_PKCS5;
  case 10: return JC_SUPPORT_CIPHER_RSA_PKCS1;
  case 12: return JC_SUPPORT_CIPHER_RSA_NOPAD;
  case 13: return JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD;
  case 0xF0: return JC_SUPPORT_CIPHER_GETINSTANCE_AES_CTR;
  case 14: return JC_SUPPORT_CIPHER_AES_ECB_NOPAD;
  case 15: return JC_SUPPORT_CIPHER_RSA_OAEP;
  case 22: return JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M1;
  case 23: return JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M2;
  case 24: return JC_SUPPORT_CIPHER_AES_CBC_PKCS5;
  case 25: return JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M1;
  case 26: return JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M2;
  case 27: return JC_SUPPORT_CIPHER_AES_ECB_PKCS5;
  default: return false;
  }
}

CipherSpec specForAlgorithmUnchecked(uint8_t algorithm);

CipherSpec specForAlgorithm(uint8_t algorithm, bool pairFactory) {
  if (!(pairFactory ? pairAlgorithmEnabled(algorithm)
                    : singleAlgorithmEnabled(algorithm)))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  switch (algorithm) {
  case 1: return {1, 3, 1, false, true, false};
  case 2: return {2, 3, 2, false, true, false};
  case 3: return {3, 3, 3, false, true, false};
  case 4: return {4, 3, 6, false, true, false};
  case 5: return {5, 4, 1, false, false, false};
  case 6: return {6, 4, 2, false, false, false};
  case 7: return {7, 4, 3, false, false, false};
  case 8: return {8, 4, 6, false, false, false};
  case 10: return {10, 7, 7, false, false, true};
  case 12: return {12, 7, 1, false, false, true};
  case 13: return {13, 1, 1, true, true, false};
  case 0xF0: return {0xF0, 8, 1, true, false, false};
  case 14: return {14, 2, 1, true, false, false};
  case 15: return {15, 7, 9, false, false, true};
  case 22: return {22, 1, 2, true, true, false};
  case 23: return {23, 1, 3, true, true, false};
  case 24: return {24, 1, 6, true, true, false};
  case 25: return {25, 2, 2, true, false, false};
  case 26: return {26, 2, 3, true, false, false};
  case 27: return {27, 2, 6, true, false, false};
  default: throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
}

uint8_t algorithmForPair(uint8_t cipher, uint8_t padding) {
  const uint8_t algorithms[] = {1,2,3,4,5,6,7,8,13,14,22,23,24,25,26,27,10,12,15,0xF0};
  for (uint8_t algorithm : algorithms) {
    const CipherSpec spec = specForAlgorithmUnchecked(algorithm);
    if (spec.cipher == cipher && spec.padding == padding &&
        pairAlgorithmEnabled(algorithm)) return algorithm;
  }
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}

CipherSpec specForAlgorithmUnchecked(uint8_t algorithm) {
  switch (algorithm) {
  case 1: return {1, 3, 1, false, true, false};
  case 2: return {2, 3, 2, false, true, false};
  case 3: return {3, 3, 3, false, true, false};
  case 4: return {4, 3, 6, false, true, false};
  case 5: return {5, 4, 1, false, false, false};
  case 6: return {6, 4, 2, false, false, false};
  case 7: return {7, 4, 3, false, false, false};
  case 8: return {8, 4, 6, false, false, false};
  case 10: return {10, 7, 7, false, false, true};
  case 12: return {12, 7, 1, false, false, true};
  case 13: return {13, 1, 1, true, true, false};
  case 0xF0: return {0xF0, 8, 1, true, false, false};
  case 14: return {14, 2, 1, true, false, false};
  case 15: return {15, 7, 9, false, false, true};
  case 22: return {22, 1, 2, true, true, false};
  case 23: return {23, 1, 3, true, true, false};
  case 24: return {24, 1, 6, true, true, false};
  case 25: return {25, 2, 2, true, false, false};
  case 26: return {26, 2, 3, true, false, false};
  case 27: return {27, 2, 6, true, false, false};
  default: return {0,0,0,false,false,false};
  }
}

jref_t newCipher(const CipherSpec &spec) {
  Heap &heap = Context::getActiveContext().getHeap();
  const jref_t reference = heap.addInstance(ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO,
                                            cipherClassIndex());
  auto cipher = heap.getInstance(reference);
  if (!cipher) throw Exceptions::RuntimeException;
  cipher->setField_Byte(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM,
                        spec.algorithm);
  cipher->setField_Byte(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_CIPHERALGORITHM,
                        spec.cipher);
  cipher->setField_Byte(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM,
                        spec.padding);
  cipher->setNativeSessionField_Byte(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE, 0);
  cipher->setNativeSessionField_Byte(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PRIVATEOPERATION, 0);
  cipher->setNativeSessionField_Byte(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED, 0);
  cipher->setNativeSessionField_Short(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH, 0);
  cipher->setNativeSessionField_Short(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH, 0);
  setByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA, 0);
  setByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_IV, 16);
  setByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIV, 16);
  setByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS,
               spec.algorithm == 0xF0 ? 16 : 0);
  setByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_EXPONENT, 0);
  setByteArray(*cipher, ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED, 32);
  return reference;
}

uint8_t keyType(JC_Instance &key) {
  return static_cast<uint8_t>(key.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
}

void checkKeyInitialized(JC_Instance &key, uint8_t type) {
  if (type == kAesTransientReset || type == kAesTransientDeselect ||
      type == kDesTransientReset || type == kDesTransientDeselect ||
      type == kRsaPrivateTransientReset || type == kRsaPrivateTransientDeselect ||
      type == kRsaCrtTransientReset || type == kRsaCrtTransientDeselect) {
    const jref_t state = key.getField_Reference(
        ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TRANSIENTINITIALIZED);
    if (state.isNullPointer()) throwCryptoException(CryptoExceptionReason::UninitializedKey);
    auto array = Context::getActiveContext().getHeap().getArray(state);
    if (!array || array->getType() != JAVA_ARRAY_T_BYTE || array->size() < 1 ||
        array->getByteEntry(0) == 0)
      throwCryptoException(CryptoExceptionReason::UninitializedKey);
  } else if (key.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED) == 0) {
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  }
}

void checkRangeForOutput(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                         uint16_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  if (offset < 0 || static_cast<uint32_t>(offset) > array->size() ||
      length > array->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

void appendInput(JC_Instance &cipher, const std::shared_ptr<JC_Array> &input,
                 jshort_t offset, jshort_t length) {
  const uint16_t oldLength = static_cast<uint16_t>(cipher.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
  const uint32_t wanted = static_cast<uint32_t>(oldLength) +
                          static_cast<uint16_t>(length);
  if (wanted > 32767) throw Exceptions::ArrayIndexOutOfBoundsException;
  ensureCapacity(cipher, static_cast<uint16_t>(wanted));
  auto buffer = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  for (jshort_t i = 0; i < length; ++i)
    buffer->setByteEntry(static_cast<uint16_t>(oldLength + i),
                         input->getByteEntry(static_cast<uint16_t>(offset + i)));
  cipher.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH,
      static_cast<jshort_t>(wanted));
}

std::vector<unsigned char> symmetricKey(JC_Instance &cipher) {
  auto key = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA);
  if (key->size() < 2 || key->getByteEntry(0) != kCipherKeyWorkspaceMarker)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  return readArray(key, 1, static_cast<uint16_t>(key->size() - 1));
}

void allocateKeyWorkspace(JC_Instance &cipher, uint16_t field,
                          uint16_t length, ClearEvent event) {
  Heap &heap = Context::getActiveContext().getHeap();
  cipher.setField_Reference(field, heap.addArray(
      JC_Array(heap, length, JAVA_ARRAY_T_BYTE, true, event)));
}

void checkKeyWorkspace(JC_Instance &cipher) {
  auto key = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA);
  if (key->size() == 0 || key->getByteEntry(0) != kCipherKeyWorkspaceMarker)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
}

std::vector<unsigned char> cipherIv(JC_Instance &cipher, uint16_t blockSize) {
  auto iv = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_IV);
  return readArray(iv, 0, blockSize);
}

void saveIv(JC_Instance &cipher, const std::vector<unsigned char> &iv) {
  writeArray(byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_IV), 0, iv,
      static_cast<uint16_t>(iv.size()));
}

void discardBuffered(JC_Instance &cipher, uint16_t count) {
  auto buffer = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  const uint16_t length = static_cast<uint16_t>(cipher.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
  for (uint16_t i = count; i < length; ++i)
    buffer->setByteEntry(i - count, buffer->getByteEntry(i));
  for (uint16_t i = length - count; i < length; ++i)
    buffer->setByteEntry(i, 0);
  cipher.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH,
      static_cast<jshort_t>(length - count));
}

std::vector<unsigned char> transformSymmetric(JC_Instance &cipher,
                                               uint16_t length) {
  const uint8_t algorithm = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  const bool aes = isAesAlgorithm(algorithm);
  const bool des = isDesAlgorithm(algorithm);
  if (!aes && !des) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const bool ecb = (algorithm >= 5 && algorithm <= 8) || algorithm == 14 ||
                   (algorithm >= 25 && algorithm <= 27);
  const uint16_t blockSize = aes ? 16 : 8;
  auto buffered = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
  std::vector<unsigned char> input = readArray(buffered, 0, length);
  std::vector<unsigned char> output(length);
  std::vector<unsigned char> key = symmetricKey(cipher);
  std::vector<unsigned char> iv = ecb ? std::vector<unsigned char>(blockSize, 0)
                                      : cipherIv(cipher, blockSize);
  [[maybe_unused]] const bool encrypt = cipher.getNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE) == kModeEncrypt;
  int result = -1;
  if (aes) {
#if JC_SUPPORT_CIPHER_AES_CTR || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CTR
    if (algorithm == 0xF0) {
      auto streamArray = byteArray(cipher,
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS);
      // These RSA workspace fields are otherwise unused by symmetric ciphers.
      std::vector<unsigned char> streamBlock =
          readArray(streamArray, 0, static_cast<uint16_t>(streamArray->size()));
      size_t offset = static_cast<uint16_t>(cipher.getNativeSessionField_Short(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH));
      result = cipherAesCtr(key, iv, offset, streamBlock, input, output);
      if (result == 0) {
        saveIv(cipher, iv);
        writeArray(streamArray, 0, streamBlock,
                   static_cast<uint16_t>(streamBlock.size()));
        cipher.setNativeSessionField_Short(
            ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH,
            static_cast<jshort_t>(offset));
      }
      if (result != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
      return output;
    }
#endif
#if JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_AES_ECB_NOPAD || JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M1 || \
    JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M2 || JC_SUPPORT_CIPHER_AES_CBC_PKCS5 || \
    JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M1 || JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_AES_ECB_PKCS5
    result = ecb ? cipherAesEcb(encrypt, key, input, output)
                 : cipherAesCbc(encrypt, key, iv, input, output);
#endif
  } else {
#if JC_SUPPORT_CIPHER_DES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M1 || JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_DES_CBC_PKCS5 || JC_SUPPORT_CIPHER_DES_ECB_NOPAD || \
    JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M1 || JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_DES_ECB_PKCS5
    result = ecb ? cipherDesEcb(encrypt, key, input, output)
                 : cipherDesCbc(encrypt, key, iv, input, output);
#endif
  }
  if (result != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (!ecb) saveIv(cipher, iv);
  return output;
}

void restoreInitialIv(JC_Instance &cipher) {
  const uint8_t algorithm = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  auto initial = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIV);
  auto current = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_IV);
  const bool cbc = (algorithm >= 1 && algorithm <= 4) || algorithm == 13 ||
                   (algorithm >= 22 && algorithm <= 24);
  for (uint16_t i = 0; i < current->size(); ++i) {
    current->setByteEntry(i, cbc ? 0 : initial->getByteEntry(i));
  }
  if (algorithm == 0xF0) {
    auto stream = byteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS);
    for (uint16_t i = 0; i < stream->size(); ++i)
      stream->setByteEntry(i, 0);
    cipher.setNativeSessionField_Short(
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH, 0);
  }
}

void initializeSymmetricCipher(JC_Instance &cipher, jref_t keyReference,
                               jbyte_t requestedMode, bool withParameters,
                               const std::shared_ptr<JC_Array> &parameters,
                               jshort_t offset, jshort_t length) {
  if (keyReference.isNullPointer()) throw Exceptions::NullPointerException;
  const uint8_t mode = static_cast<uint8_t>(requestedMode);
  if (mode != kModeEncrypt && mode != kModeDecrypt)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto key = Context::getActiveContext().getHeap().getInstance(keyReference);
  if (!key || key->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      key->getClassIndex() != HTONS(Export_Handler(
          Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY)).getExportedClassOffset(
              ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_NATIVEKEY)))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint8_t algorithm = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  const bool aes = isAesAlgorithm(algorithm);
  const bool des = isDesAlgorithm(algorithm);
  if (!aes && !des) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint8_t type = keyType(*key);
  checkKeyInitialized(*key, type);
  if (aes ? !(type == kAesPersistent || type == kAesTransientReset ||
              type == kAesTransientDeselect)
          : !(type == kDesPersistent || type == kDesTransientReset ||
              type == kDesTransientDeselect))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint16_t bytes = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE) / 8);
  if (aes ? (bytes != 16 && bytes != 24 && bytes != 32)
          : (bytes != 8 && bytes != 16 && bytes != 24))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const jref_t keyDataRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_KEYDATA);
  auto keyData = Context::getActiveContext().getHeap().getArray(keyDataRef);
  if (!keyData || keyData->getType() != JAVA_ARRAY_T_BYTE || keyData->size() != bytes)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  const bool ctr = algorithm == 0xF0;
  // Rebind persisted objects from earlier firmware versions and retained
  // Cipher instances to transient workspaces before processing new input.
  cipher.setNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH, 0);
  ensureTransientByteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_IV, 16);
  ensureTransientByteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIV, 16);
  ensureTransientByteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED, 32);
  if (ctr) {
    ensureTransientByteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS, 16);
  }
  auto cipherKey = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA);
  for (uint16_t i = 0; i < cipherKey->size(); ++i)
    cipherKey->setByteEntry(i, 0);
  const ClearEvent keyClearEvent =
      (type == kAesTransientDeselect || type == kDesTransientDeselect)
          ? ClearEvent::CLEAR_ON_DESELECT : ClearEvent::CLEAR_ON_RESET;
  allocateKeyWorkspace(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA,
      static_cast<uint16_t>(bytes + 1), keyClearEvent);
  cipherKey = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA);
  cipherKey->setByteEntry(0, kCipherKeyWorkspaceMarker);
  for (uint16_t i = 0; i < bytes; ++i)
    cipherKey->setByteEntry(static_cast<uint16_t>(i + 1),
                            keyData->getByteEntry(i));
  const uint16_t blockSize = aes ? 16 : 8;
  auto iv = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_IV);
  auto initial = byteArray(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIV);
  const bool cbc = (algorithm >= 1 && algorithm <= 4) ||
                   algorithm == 13 || (algorithm >= 22 && algorithm <= 24);
  if (withParameters) {
    if ((!cbc && !ctr) || !parameters || offset < 0 || length != blockSize)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    checkArray(parameters, offset, length);
  }
  for (uint16_t i = 0; i < iv->size(); ++i) {
    const uint8_t value = withParameters && i < blockSize
        ? static_cast<uint8_t>(parameters->getByteEntry(
              static_cast<uint16_t>(offset + i))) : 0;
    iv->setByteEntry(i, static_cast<jbyte_t>(value));
    initial->setByteEntry(i, static_cast<jbyte_t>(value));
  }
  if (ctr) {
    auto stream = byteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS);
    for (uint16_t i = 0; i < stream->size(); ++i)
      stream->setByteEntry(i, 0);
    cipher.setNativeSessionField_Short(
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH, 0);
  }
  clearBuffer(cipher);
  cipher.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE, requestedMode);
  cipher.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PRIVATEOPERATION, 0);
  cipher.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED, 1);
}

void appendToVector(JC_Instance &cipher, const std::shared_ptr<JC_Array> &input,
                    jshort_t offset, jshort_t length) {
  checkArray(input, offset, length);
  appendInput(cipher, input, offset, length);
}

jshort_t updateSymmetric(JC_Instance &cipher,
                         const std::shared_ptr<JC_Array> &input,
                         jshort_t inputOffset, jshort_t inputLength,
                         const std::shared_ptr<JC_Array> &output,
                         jshort_t outputOffset) {
  if (cipher.getNativeSessionField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED) == 0)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  checkKeyWorkspace(cipher);
  checkArray(input, inputOffset, inputLength);
  checkArray(output, outputOffset, 0);
  const uint8_t algorithm = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  const uint16_t blockSize = isAesAlgorithm(algorithm) ? 16 : 8;
  const uint16_t oldLength = static_cast<uint16_t>(
      cipher.getNativeSessionField_Short(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
  const uint32_t totalLength = static_cast<uint32_t>(oldLength) +
                               static_cast<uint16_t>(inputLength);
  if (totalLength > 32767)
    throw Exceptions::ArrayIndexOutOfBoundsException;
  const bool ctr = algorithm == 0xF0;
  const uint8_t padding = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM));
  const bool encrypt = cipher.getNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE) == kModeEncrypt;
  uint16_t expectedOutput = ctr
      ? static_cast<uint16_t>(totalLength)
      : static_cast<uint16_t>(totalLength - totalLength % blockSize);
  if (!ctr && !encrypt && padding != 1 && expectedOutput != 0)
    expectedOutput = static_cast<uint16_t>(expectedOutput - blockSize);
  checkRangeForOutput(output, outputOffset, expectedOutput);

  uint16_t consumed = 0;
  uint16_t produced = 0;
  bool firstPass = true;
  while (firstPass || consumed < static_cast<uint16_t>(inputLength)) {
    firstPass = false;
    auto buffer = byteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
    const uint16_t bufferedLength = static_cast<uint16_t>(
        cipher.getNativeSessionField_Short(
            ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
    if (bufferedLength > buffer->size())
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    const uint16_t available = static_cast<uint16_t>(
        buffer->size() - bufferedLength);
    const uint16_t remaining = static_cast<uint16_t>(
        static_cast<uint16_t>(inputLength) - consumed);
    const uint16_t chunk = std::min(available, remaining);
    if (remaining != 0 && chunk == 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    if (chunk != 0) {
      appendToVector(cipher, input,
          static_cast<jshort_t>(inputOffset + consumed),
          static_cast<jshort_t>(chunk));
      consumed = static_cast<uint16_t>(consumed + chunk);
    }

    const uint16_t currentLength = static_cast<uint16_t>(
        cipher.getNativeSessionField_Short(
            ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
    uint16_t processLength = ctr
        ? currentLength
        : static_cast<uint16_t>(currentLength - currentLength % blockSize);
    if (!ctr && !encrypt && padding != 1 && processLength != 0)
      processLength = static_cast<uint16_t>(processLength - blockSize);
    if (processLength != 0) {
      if (static_cast<uint32_t>(produced) + processLength > expectedOutput)
        throwCryptoException(CryptoExceptionReason::IllegalUse);
      const auto result = transformSymmetric(cipher, processLength);
      writeArray(output,
          static_cast<uint16_t>(static_cast<uint16_t>(outputOffset) + produced),
          result, processLength);
      discardBuffered(cipher, processLength);
      produced = static_cast<uint16_t>(produced + processLength);
    }
    if (consumed == static_cast<uint16_t>(inputLength)) break;
  }
  if (produced != expectedOutput)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return static_cast<jshort_t>(produced);
}

uint16_t unpad(std::vector<unsigned char> &data, uint16_t length,
               uint8_t padding, uint16_t blockSize) {
  if (padding == 6) {
    if (length == 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
    const uint16_t count = data[length - 1];
    if (count == 0 || count > blockSize || count > length)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    for (uint16_t i = static_cast<uint16_t>(length - count); i < length; ++i)
      if (data[i] != count) throwCryptoException(CryptoExceptionReason::IllegalUse);
    return static_cast<uint16_t>(length - count);
  }
  if (padding == 3) {
    int32_t i = static_cast<int32_t>(length) - 1;
    while (i >= 0 && data[static_cast<size_t>(i)] == 0) --i;
    if (i < 0 || data[static_cast<size_t>(i)] != 0x80)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    return static_cast<uint16_t>(i);
  }
  return length;
}

jshort_t doFinalSymmetric(JC_Instance &cipher,
                          const std::shared_ptr<JC_Array> &input,
                          jshort_t inputOffset, jshort_t inputLength,
                          const std::shared_ptr<JC_Array> &output,
                          jshort_t outputOffset) {
  if (cipher.getNativeSessionField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED) == 0)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  checkKeyWorkspace(cipher);
  checkArray(input, inputOffset, inputLength);
  checkArray(output, outputOffset, 0);
  appendToVector(cipher, input, inputOffset, inputLength);
  const uint8_t algorithm = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  const uint16_t blockSize = isAesAlgorithm(algorithm) ? 16 : 8;
  const uint8_t padding = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM));
  const bool encrypt = cipher.getNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE) == kModeEncrypt;
  uint16_t dataLength = static_cast<uint16_t>(cipher.getNativeSessionField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
  if (algorithm == 0xF0) {
    checkRangeForOutput(output, outputOffset, dataLength);
    if (dataLength != 0) {
      const auto result = transformSymmetric(cipher, dataLength);
      writeArray(output, static_cast<uint16_t>(outputOffset), result, dataLength);
    }
    clearBuffer(cipher);
    restoreInitialIv(cipher);
    return static_cast<jshort_t>(dataLength);
  }
  if (encrypt && padding != 1) {
    const uint16_t remainder = static_cast<uint16_t>(dataLength % blockSize);
    const uint16_t pad = remainder == 0 && padding == 2
        ? 0 : static_cast<uint16_t>(blockSize - remainder);
    const uint16_t paddedLength = static_cast<uint16_t>(dataLength + pad);
    ensureCapacity(cipher, paddedLength);
    auto buffer = byteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFERED);
    if (padding == 3 && pad != 0)
      buffer->setByteEntry(dataLength, static_cast<jbyte_t>(0x80));
    if (padding == 6)
      for (uint16_t i = dataLength; i < paddedLength; ++i)
        buffer->setByteEntry(i, static_cast<jbyte_t>(pad));
    dataLength = paddedLength;
    cipher.setNativeSessionField_Short(
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH,
        static_cast<jshort_t>(dataLength));
  }
  if (dataLength == 0 || dataLength % blockSize != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (encrypt || padding == 1)
    checkRangeForOutput(output, outputOffset, dataLength);
  const auto transformed = transformSymmetric(cipher, dataLength);
  uint16_t resultLength = dataLength;
  std::vector<unsigned char> result = transformed;
  if (!encrypt && padding != 1 && padding != 2)
    resultLength = unpad(result, dataLength, padding, blockSize);
  checkRangeForOutput(output, outputOffset, resultLength);
  writeArray(output, static_cast<uint16_t>(outputOffset), result, resultLength);
  clearBuffer(cipher);
  restoreInitialIv(cipher);
  return static_cast<jshort_t>(resultLength);
}

} // namespace

jref_t javacardx_crypto_Cipher_getInstance(jbyte_t algorithm,
                                            jbool_t externalAccess) {
  if (externalAccess) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const CipherSpec spec = specForAlgorithm(static_cast<uint8_t>(algorithm), false);
  return newCipher(spec);
}

jref_t javacardx_crypto_Cipher_getInstance(jbyte_t cipherAlgorithm,
                                            jbyte_t paddingAlgorithm,
                                            jbool_t externalAccess) {
  if (externalAccess) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint8_t algorithm = algorithmForPair(
      static_cast<uint8_t>(cipherAlgorithm), static_cast<uint8_t>(paddingAlgorithm));
  return newCipher(specForAlgorithm(algorithm, true));
}

void javacardx_crypto_Cipher_init(jref_t receiver, jref_t key, jbyte_t mode) {
  auto cipher = cipherInstance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(cipher->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  if (isRsaAlgorithm(algorithm)) {
#if JC_SUPPORT_CIPHER_RSA_PKCS1 || JC_SUPPORT_CIPHER_RSA_NOPAD || JC_SUPPORT_CIPHER_RSA_OAEP
    initializeRsaCipher(cipher, key, mode);
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  } else {
    initializeSymmetricCipher(*cipher, key, mode, false, nullptr, 0, 0);
  }
}

void javacardx_crypto_Cipher_init(jref_t receiver, jref_t key, jbyte_t mode,
                                   std::shared_ptr<JC_Array> parameters,
                                   jshort_t offset, jshort_t length) {
  auto cipher = cipherInstance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(cipher->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  if (isRsaAlgorithm(algorithm)) {
#if JC_SUPPORT_CIPHER_RSA_PKCS1 || JC_SUPPORT_CIPHER_RSA_NOPAD || JC_SUPPORT_CIPHER_RSA_OAEP
    rejectRsaCipherParameters(cipher, key, mode, parameters, offset, length);
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  } else {
    initializeSymmetricCipher(*cipher, key, mode, true, parameters, offset, length);
  }
}

jbyte_t javacardx_crypto_Cipher_getAlgorithm(jref_t receiver) {
  return cipherInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM);
}
jbyte_t javacardx_crypto_Cipher_getCipherAlgorithm(jref_t receiver) {
  return cipherInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_CIPHERALGORITHM);
}
jbyte_t javacardx_crypto_Cipher_getPaddingAlgorithm(jref_t receiver) {
  return cipherInstance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM);
}

jshort_t javacardx_crypto_Cipher_update(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t inputOffset, jshort_t inputLength,
    std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto cipher = cipherInstance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(cipher->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  if (isRsaAlgorithm(algorithm)) {
#if JC_SUPPORT_CIPHER_RSA_PKCS1 || JC_SUPPORT_CIPHER_RSA_NOPAD || JC_SUPPORT_CIPHER_RSA_OAEP
    const jshort_t result = updateRsaCipher(
        cipher, input, inputOffset, inputLength, output, outputOffset);
    context.setSensitiveResultShort(result);
    return result;
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  }
  const jshort_t result = updateSymmetric(
      *cipher, input, inputOffset, inputLength, output, outputOffset);
  context.setSensitiveResultShort(result);
  return result;
}

jshort_t javacardx_crypto_Cipher_doFinal(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t inputOffset, jshort_t inputLength,
    std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto cipher = cipherInstance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(cipher->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  if (isRsaAlgorithm(algorithm)) {
#if JC_SUPPORT_CIPHER_RSA_PKCS1 || JC_SUPPORT_CIPHER_RSA_NOPAD || JC_SUPPORT_CIPHER_RSA_OAEP
    const jshort_t result = doFinalRsaCipher(
        cipher, input, inputOffset, inputLength, output, outputOffset);
    context.setSensitiveResultShort(result);
    return result;
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  }
  const jshort_t result = doFinalSymmetric(
      *cipher, input, inputOffset, inputLength, output, outputOffset);
  context.setSensitiveResultShort(result);
  return result;
}
} // namespace jcvm
