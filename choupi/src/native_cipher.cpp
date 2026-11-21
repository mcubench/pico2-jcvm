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

#if JC_SUPPORT_CIPHER_AES_CFB || JC_SUPPORT_CIPHER_AES_XTS
#include "mbedtls/aes.h"
#endif

#if JC_SUPPORT_CIPHER_AES_GCM
#include "mbedtls/gcm.h"
#endif
#if JC_SUPPORT_CIPHER_AES_CCM
#include "mbedtls/ccm.h"
#endif
#if JC_SUPPORT_CIPHER_AES_GCM || JC_SUPPORT_CIPHER_AES_CCM
#include <array>
#endif

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
constexpr uint8_t kAlgorithmAesCfb = 28; // Java Card 3.1
constexpr uint8_t kAlgorithmAesXts = 29;
constexpr uint8_t kAlgorithmAesGcm = 0xF3;
constexpr uint8_t kAlgorithmAesCcm = 0xF4;
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

uint16_t aeadCipherClassIndex() {
  return HTONS(Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO))
      .getExportedClassOffset(ROM_CLASS_EXPORT_TOKEN_JAVACARDX_CRYPTO_AEADCIPHER));
}

std::shared_ptr<JC_Instance> cipherInstance(jref_t reference) {
  if (reference.isNullPointer() || reference.isArray())
    throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO ||
      (instance->getClassIndex() != cipherClassIndex() &&
       instance->getClassIndex() != aeadCipherClassIndex()))
    throw Exceptions::SecurityException;
  return instance;
}

std::shared_ptr<JC_Instance> aeadCipherInstance(jref_t reference) {
  auto instance = cipherInstance(reference);
  if (instance->getClassIndex() != aeadCipherClassIndex())
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
  array->copyByteEntriesTo(offset, length, result.data());
  return result;
}

void writeArray(const std::shared_ptr<JC_Array> &array, uint16_t offset,
                const std::vector<unsigned char> &bytes, uint16_t length) {
  for (uint16_t i = 0; i < length; ++i)
    array->setByteEntry(offset + i, static_cast<jbyte_t>(bytes[i]));
}

bool isAesStreamAlgorithm(uint8_t algorithm) {
  return algorithm == 0xF0 || algorithm == kAlgorithmAesCfb;
}

bool isAesAlgorithm(uint8_t algorithm) {
  return algorithm == 13 || algorithm == 14 ||
         (algorithm >= 22 && algorithm <= 27) ||
         isAesStreamAlgorithm(algorithm) || algorithm == kAlgorithmAesXts ||
         algorithm == kAlgorithmAesGcm || algorithm == kAlgorithmAesCcm;
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
  case kAlgorithmAesCfb: return JC_SUPPORT_CIPHER_AES_CFB;
  case kAlgorithmAesXts: return JC_SUPPORT_CIPHER_AES_XTS;
  case kAlgorithmAesGcm: return JC_SUPPORT_CIPHER_AES_GCM;
  case kAlgorithmAesCcm: return JC_SUPPORT_CIPHER_AES_CCM;
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
  case kAlgorithmAesCfb: return JC_SUPPORT_CIPHER_AES_CFB;
  case kAlgorithmAesXts: return JC_SUPPORT_CIPHER_AES_XTS;
  case kAlgorithmAesGcm: return JC_SUPPORT_CIPHER_AES_GCM;
  case kAlgorithmAesCcm: return JC_SUPPORT_CIPHER_AES_CCM;
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
  case kAlgorithmAesCfb: return {kAlgorithmAesCfb, 9, 0, true, false, false};
  case kAlgorithmAesXts: return {kAlgorithmAesXts, 10, 0, true, false, false};
  case kAlgorithmAesGcm: return {kAlgorithmAesGcm, 0xF1, 0, true, false, false};
  case kAlgorithmAesCcm: return {kAlgorithmAesCcm, 0xF2, 0, true, false, false};
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
  const uint8_t algorithms[] = {1,2,3,4,5,6,7,8,13,14,22,23,24,25,26,27,10,12,15,0xF0,kAlgorithmAesCfb,kAlgorithmAesXts,kAlgorithmAesGcm,
                                kAlgorithmAesCcm};
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
  case kAlgorithmAesCfb: return {kAlgorithmAesCfb, 9, 0, true, false, false};
  case kAlgorithmAesXts: return {kAlgorithmAesXts, 10, 0, true, false, false};
  case kAlgorithmAesGcm: return {kAlgorithmAesGcm, 0xF1, 0, true, false, false};
  case kAlgorithmAesCcm: return {kAlgorithmAesCcm, 0xF2, 0, true, false, false};
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
      spec.algorithm == kAlgorithmAesGcm || spec.algorithm == kAlgorithmAesCcm
          ? aeadCipherClassIndex() : cipherClassIndex());
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
  const jref_t currentReference = cipher.getField_Reference(field);
  if (!currentReference.isNullPointer()) {
    auto current = Context::getActiveContext().getHeap().getArray(
        currentReference);
    if (current && current->getType() == JAVA_ARRAY_T_BYTE &&
        current->isTransientArray() && current->size() == length) {
      return;
    }
  }
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
#if JC_SUPPORT_CIPHER_AES_XTS
    if (algorithm == kAlgorithmAesXts) {
      mbedtls_aes_xts_context context;
      mbedtls_aes_xts_init(&context);
      const auto bits = static_cast<unsigned>(key.size() * 8);
      result = encrypt ? mbedtls_aes_xts_setkey_enc(&context, key.data(), bits)
                       : mbedtls_aes_xts_setkey_dec(&context, key.data(), bits);
      if (result == 0)
        result = mbedtls_aes_crypt_xts(
            &context, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
            length, iv.data(), input.data(), output.data());
      mbedtls_aes_xts_free(&context);
      if (result != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
      return output;
    }
#endif
#if JC_SUPPORT_CIPHER_AES_CFB
    if (algorithm == kAlgorithmAesCfb) {
      const auto offsetField =
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH;
      size_t offset = static_cast<uint16_t>(cipher.getNativeSessionField_Short(offsetField));
      if (offset > 15) throwCryptoException(CryptoExceptionReason::IllegalUse);
      mbedtls_aes_context context;
      mbedtls_aes_init(&context);
      result = mbedtls_aes_setkey_enc(&context, key.data(),
                                      static_cast<unsigned>(key.size() * 8));
      if (result == 0)
        result = mbedtls_aes_crypt_cfb128(
            &context, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
            length, &offset, iv.data(), input.data(), output.data());
      mbedtls_aes_free(&context);
      if (result != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
      saveIv(cipher, iv);
      cipher.setNativeSessionField_Short(offsetField, static_cast<jshort_t>(offset));
      return output;
    }
#endif
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
    current->setByteEntry(i, cbc || algorithm == kAlgorithmAesCfb ||
                                 algorithm == kAlgorithmAesXts
                                 ? 0 : initial->getByteEntry(i));
  }
  if (algorithm == 0xF0) {
    auto stream = byteArray(cipher,
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS);
    for (uint16_t i = 0; i < stream->size(); ++i)
      stream->setByteEntry(i, 0);
  }
  if (isAesStreamAlgorithm(algorithm)) {
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
  const bool xts = algorithm == kAlgorithmAesXts;
  if (xts ? (bytes != 32 && bytes != 64)
      : aes ? (bytes != 16 && bytes != 24 && bytes != 32)
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
  const ClearEvent keyClearEvent =
      (type == kAesTransientDeselect || type == kDesTransientDeselect)
          ? ClearEvent::CLEAR_ON_DESELECT : ClearEvent::CLEAR_ON_RESET;
  allocateKeyWorkspace(cipher,
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_KEYDATA,
      static_cast<uint16_t>(bytes + 1), keyClearEvent);
  auto cipherKey = byteArray(cipher,
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
    if ((!cbc && !xts && !isAesStreamAlgorithm(algorithm)) || !parameters || offset < 0 || length != blockSize)
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
  }
  if (isAesStreamAlgorithm(algorithm)) {
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
  if (algorithm == kAlgorithmAesXts) {
    // Mbed TLS XTS consumes a whole data unit for ciphertext stealing.
    appendToVector(cipher, input, inputOffset, inputLength);
    return 0;
  }
  const uint16_t blockSize = isAesAlgorithm(algorithm) ? 16 : 8;
  const uint16_t oldLength = static_cast<uint16_t>(
      cipher.getNativeSessionField_Short(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH));
  const uint32_t totalLength = static_cast<uint32_t>(oldLength) +
                               static_cast<uint16_t>(inputLength);
  if (totalLength > 32767)
    throw Exceptions::ArrayIndexOutOfBoundsException;
  const bool stream = isAesStreamAlgorithm(algorithm);
  const uint8_t padding = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PADDINGALGORITHM));
  const bool encrypt = cipher.getNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE) == kModeEncrypt;
  uint16_t expectedOutput = stream
      ? static_cast<uint16_t>(totalLength)
      : static_cast<uint16_t>(totalLength - totalLength % blockSize);
  if (!stream && !encrypt && padding != 1 && expectedOutput != 0)
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
    uint16_t processLength = stream
        ? currentLength
        : static_cast<uint16_t>(currentLength - currentLength % blockSize);
    if (!stream && !encrypt && padding != 1 && processLength != 0)
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
    if (length == 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
    const int32_t blockStart = static_cast<int32_t>(length - blockSize);
    int32_t i = static_cast<int32_t>(length) - 1;
    while (i >= blockStart && data[static_cast<size_t>(i)] == 0) --i;
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
  if (algorithm == kAlgorithmAesXts && dataLength < 16)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (isAesStreamAlgorithm(algorithm) || algorithm == kAlgorithmAesXts) {
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
  // update() may already have emitted every complete block. NOPAD and ISO9797
  // method 1 add no final bytes for a block-aligned message, so an empty
  // doFinal() must complete the operation instead of rejecting the empty
  // workspace as an invalid block-cipher input.
  if (dataLength == 0 && (padding == 1 || (encrypt && padding == 2))) {
    clearBuffer(cipher);
    restoreInitialIv(cipher);
    return 0;
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

#if JC_SUPPORT_CIPHER_AES_GCM || JC_SUPPORT_CIPHER_AES_CCM
constexpr auto kAeadState =
    ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUS;
constexpr auto kAeadTag =
    ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_EXPONENT;
constexpr auto kAeadTagReady =
    ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_PRIVATEOPERATION;
constexpr auto kModeField =
    ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODE;

constexpr size_t kAeadTagSize = 16;
constexpr auto kCcmAadRemaining =
    ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_BUFFEREDLENGTH;
constexpr auto kCcmTagLength =
    ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_MODULUSLENGTH;

void checkAeadResult(int status) {
  if (status != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
}

void checkAeadTag(JC_Instance &cipher, uint8_t mode, jshort_t length) {
  if (cipher.getNativeSessionField_Byte(kAeadTagReady) == 0 ||
      static_cast<uint8_t>(cipher.getNativeSessionField_Byte(kModeField)) != mode)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const auto algorithm = static_cast<uint8_t>(cipher.getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM));
  if (algorithm == kAlgorithmAesCcm) {
    if (length != cipher.getNativeSessionField_Short(kCcmTagLength))
      throwCryptoException(CryptoExceptionReason::IllegalValue);
  } else if (length != 4 && length != 8 && (length < 12 || length > 16)) {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
}
#endif

#if JC_SUPPORT_CIPHER_AES_GCM
// Only the pointer-free running state is saved. The library cipher context
// and key schedule are reconstructed from the existing transient key cache.
constexpr size_t kGcmStateStart = offsetof(mbedtls_gcm_context, MBEDTLS_PRIVATE(len));
constexpr size_t kGcmStateSize =
    offsetof(mbedtls_gcm_context, MBEDTLS_PRIVATE(acceleration)) - kGcmStateStart;

class GcmContext {
public:
  mbedtls_gcm_context value;
  GcmContext() { mbedtls_gcm_init(&value); }
  ~GcmContext() { mbedtls_gcm_free(&value); }
  GcmContext(const GcmContext &) = delete;
  GcmContext &operator=(const GcmContext &) = delete;

  uint8_t *progress() {
    return reinterpret_cast<uint8_t *>(&value) + kGcmStateStart;
  }
};

void setGcmKey(JC_Instance &cipher, GcmContext &context) {
  const auto key = symmetricKey(cipher);
  checkAeadResult(mbedtls_gcm_setkey(&context.value, MBEDTLS_CIPHER_ID_AES,
      key.data(), static_cast<unsigned>(key.size() * 8)));
}

void startGcm(JC_Instance &cipher, GcmContext &context,
              const uint8_t *nonce, size_t nonceLength) {
  const auto mode = cipher.getNativeSessionField_Byte(kModeField) == kModeEncrypt
      ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT;
  checkAeadResult(mbedtls_gcm_starts(&context.value, mode, nonce, nonceLength));
}

void loadGcm(JC_Instance &cipher, GcmContext &context) {
  if (cipher.getNativeSessionField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED) == 0)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  setGcmKey(cipher, context);
  byteArray(cipher, kAeadState)->copyByteEntriesTo(
      0, kGcmStateSize, context.progress());
}

void saveGcm(JC_Instance &cipher, GcmContext &context) {
  byteArray(cipher, kAeadState)->copyByteEntriesFrom(
      0, kGcmStateSize, context.progress());
}

void initializeGcm(JC_Instance &cipher, jref_t key, jbyte_t mode,
                   const std::shared_ptr<JC_Array> &nonce,
                   jshort_t offset, jshort_t length) {
  static constexpr std::array<uint8_t, 12> zeroNonce{};
  if (nonce) {
    if (length <= 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
    checkArray(nonce, offset, length);
  }
  initializeSymmetricCipher(cipher, key, mode, false, nullptr, 0, 0);
  const auto initialized = ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED;
  cipher.setNativeSessionField_Byte(initialized, 0);
  ensureTransientByteArray(cipher, kAeadState, kGcmStateSize);
  ensureTransientByteArray(cipher, kAeadTag, kAeadTagSize);
  GcmContext context;
  setGcmKey(cipher, context);
  const auto *nonceBytes = nonce ? nonce->getByteDataView() + offset : zeroNonce.data();
  startGcm(cipher, context, nonceBytes, nonce ? length : zeroNonce.size());
  saveGcm(cipher, context);
  cipher.setNativeSessionField_Byte(initialized, 1);
}

jshort_t processGcm(JC_Instance &cipher,
    const std::shared_ptr<JC_Array> &input, jshort_t inOffset, jshort_t length,
    const std::shared_ptr<JC_Array> &output, jshort_t outOffset, bool final) {
  checkArray(input, inOffset, length);
  checkRangeForOutput(output, outOffset, length);
  GcmContext context;
  loadGcm(cipher, context);
  std::vector<uint8_t> result(static_cast<uint16_t>(length));
  const auto *data = length == 0 ? nullptr : input->getByteDataView() + inOffset;
  size_t written = 0;
  checkAeadResult(mbedtls_gcm_update(&context.value, data, length, result.data(),
                                   result.size(), &written));
  if (written != result.size()) throwCryptoException(CryptoExceptionReason::IllegalUse);

  std::array<uint8_t, kAeadTagSize> tag{};
  if (final) {
    size_t extra = 0;
    checkAeadResult(mbedtls_gcm_finish(&context.value, nullptr, 0, &extra,
                                      tag.data(), tag.size()));
    if (extra != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
    constexpr std::array<uint8_t, 12> zeroNonce{};
    startGcm(cipher, context, zeroNonce.data(), zeroNonce.size());
  }
  writeArray(output, outOffset, result, length);
  saveGcm(cipher, context);
  if (final)
    byteArray(cipher, kAeadTag)->copyByteEntriesFrom(0, tag.size(), tag.data());
  if (final || length != 0)
    cipher.setNativeSessionField_Byte(kAeadTagReady, final ? 1 : 0);
  return length;
}

#endif

#if JC_SUPPORT_CIPHER_AES_CCM
#if defined(MBEDTLS_BLOCK_CIPHER_C)
constexpr size_t kCcmPrefixSize =
    offsetof(mbedtls_ccm_context, MBEDTLS_PRIVATE(block_cipher_ctx));
#else
constexpr size_t kCcmPrefixSize =
    offsetof(mbedtls_ccm_context, MBEDTLS_PRIVATE(cipher_ctx));
#endif

class CcmContext {
public:
  mbedtls_ccm_context value;
  CcmContext() { mbedtls_ccm_init(&value); }
  ~CcmContext() { mbedtls_ccm_free(&value); }
  CcmContext(const CcmContext &) = delete;
  CcmContext &operator=(const CcmContext &) = delete;

  static constexpr size_t stateSize = kCcmPrefixSize + sizeof(value.MBEDTLS_PRIVATE(state));

  void transferState(JC_Array &workspace, bool save) {
    // Save the progress fields on either side of the live library cipher.
    auto *prefix = reinterpret_cast<uint8_t *>(&value);
    auto *state = reinterpret_cast<uint8_t *>(&value.MBEDTLS_PRIVATE(state));
    if (save) {
      workspace.copyByteEntriesFrom(0, kCcmPrefixSize, prefix);
      workspace.copyByteEntriesFrom(kCcmPrefixSize, sizeof(value.MBEDTLS_PRIVATE(state)), state);
    } else {
      workspace.copyByteEntriesTo(0, kCcmPrefixSize, prefix);
      workspace.copyByteEntriesTo(kCcmPrefixSize, sizeof(value.MBEDTLS_PRIVATE(state)), state);
    }
  }
};

void setCcmKey(JC_Instance &cipher, CcmContext &context) {
  const auto key = symmetricKey(cipher);
  checkAeadResult(mbedtls_ccm_setkey(&context.value, MBEDTLS_CIPHER_ID_AES,
      key.data(), static_cast<unsigned>(key.size() * 8)));
}

void loadCcm(JC_Instance &cipher, CcmContext &context) {
  if (cipher.getNativeSessionField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED) == 0)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  setCcmKey(cipher, context);
  context.transferState(*byteArray(cipher, kAeadState), false);
}

void initializeCcm(JC_Instance &cipher, jref_t key, jbyte_t mode,
    const std::shared_ptr<JC_Array> &nonce, jshort_t offset, jshort_t length,
    jshort_t aadLength, jshort_t messageLength, jshort_t tagLength) {
  if (length < 7 || length > 13 || aadLength < 0 || messageLength < 0 ||
      tagLength < 4 || tagLength > 16 || (tagLength & 1) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkArray(nonce, offset, length);
  initializeSymmetricCipher(cipher, key, mode, false, nullptr, 0, 0);
  const auto initialized = ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED;
  cipher.setNativeSessionField_Byte(initialized, 0);
  ensureTransientByteArray(cipher, kAeadState, CcmContext::stateSize);
  ensureTransientByteArray(cipher, kAeadTag, kAeadTagSize);
  CcmContext context;
  setCcmKey(cipher, context);
  checkAeadResult(mbedtls_ccm_starts(&context.value,
      mode == kModeEncrypt ? MBEDTLS_CCM_ENCRYPT : MBEDTLS_CCM_DECRYPT,
      nonce->getByteDataView() + offset, length));
  checkAeadResult(mbedtls_ccm_set_lengths(&context.value,
      aadLength, messageLength, tagLength));
  context.transferState(*byteArray(cipher, kAeadState), true);
  cipher.setNativeSessionField_Short(kCcmAadRemaining, aadLength);
  cipher.setNativeSessionField_Short(kCcmTagLength, tagLength);
  cipher.setNativeSessionField_Byte(initialized, 1);
}

void updateCcmAad(JC_Instance &cipher, const JC_Array &input,
                  jshort_t offset, jshort_t length) {
  CcmContext context;
  loadCcm(cipher, context);
  const jshort_t remaining = cipher.getNativeSessionField_Short(kCcmAadRemaining);
  if (length > remaining)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (length == 0) return;
  checkAeadResult(mbedtls_ccm_update_ad(&context.value,
      input.getByteDataView() + offset, length));
  context.transferState(*byteArray(cipher, kAeadState), true);
  cipher.setNativeSessionField_Short(kCcmAadRemaining, remaining - length);
}

jshort_t processCcm(JC_Instance &cipher,
    const std::shared_ptr<JC_Array> &input, jshort_t inOffset, jshort_t length,
    const std::shared_ptr<JC_Array> &output, jshort_t outOffset, bool final) {
  checkArray(input, inOffset, length);
  checkRangeForOutput(output, outOffset, length);
  CcmContext context;
  loadCcm(cipher, context);
  if (cipher.getNativeSessionField_Short(kCcmAadRemaining) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  std::vector<uint8_t> result(static_cast<uint16_t>(length));
  const auto *data = length == 0 ? nullptr : input->getByteDataView() + inOffset;
  size_t written = 0;
  checkAeadResult(mbedtls_ccm_update(&context.value, data, length,
                                    result.data(), result.size(), &written));
  if (written != result.size()) throwCryptoException(CryptoExceptionReason::IllegalUse);
  std::array<uint8_t, kAeadTagSize> tag{};
  const auto tagLength = cipher.getNativeSessionField_Short(kCcmTagLength);
  if (final)
    checkAeadResult(mbedtls_ccm_finish(&context.value, tag.data(), tagLength));
  writeArray(output, outOffset, result, length);
  if (final) {
    byteArray(cipher, kAeadTag)->copyByteEntriesFrom(0, tagLength, tag.data());
    cipher.setNativeSessionField_Byte(kAeadTagReady, 1);
    cipher.setNativeSessionField_Byte(
        ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_INITIALIZED, 0);
  } else {
    context.transferState(*byteArray(cipher, kAeadState), true);
  }
  return length;
}
#endif

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
  if (algorithm == kAlgorithmAesCcm)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
#if JC_SUPPORT_CIPHER_AES_GCM
  if (algorithm == kAlgorithmAesGcm) {
    initializeGcm(*cipher, key, mode, nullptr, 0, 0);
    return;
  }
#endif
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
  if (algorithm == kAlgorithmAesCcm)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
#if JC_SUPPORT_CIPHER_AES_GCM
  if (algorithm == kAlgorithmAesGcm) {
    if (!parameters) throw Exceptions::NullPointerException;
    initializeGcm(*cipher, key, mode, parameters, offset, length);
    return;
  }
#endif
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
#if JC_SUPPORT_CIPHER_AES_CCM
  if (algorithm == kAlgorithmAesCcm) {
    const auto result = processCcm(*cipher, input, inputOffset, inputLength,
                                   output, outputOffset, false);
    context.setSensitiveResultShort(result);
    return result;
  }
#endif
#if JC_SUPPORT_CIPHER_AES_GCM
  if (algorithm == kAlgorithmAesGcm) {
    const auto result = processGcm(*cipher, input, inputOffset, inputLength, output, outputOffset, false);
    context.setSensitiveResultShort(result);
    return result;
  }
#endif
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
#if JC_SUPPORT_CIPHER_AES_CCM
  if (algorithm == kAlgorithmAesCcm) {
    const auto result = processCcm(*cipher, input, inputOffset, inputLength,
                                   output, outputOffset, true);
    context.setSensitiveResultShort(result);
    return result;
  }
#endif
#if JC_SUPPORT_CIPHER_AES_GCM
  if (algorithm == kAlgorithmAesGcm) {
    const auto result = processGcm(*cipher, input, inputOffset, inputLength, output, outputOffset, true);
    context.setSensitiveResultShort(result);
    return result;
  }
#endif
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
void javacardx_crypto_AEADCipher_init(jref_t receiver, jref_t key, jbyte_t mode) {
  aeadCipherInstance(receiver);
  javacardx_crypto_Cipher_init(receiver, key, mode);
}

void javacardx_crypto_AEADCipher_init(jref_t receiver, jref_t key, jbyte_t mode,
    std::shared_ptr<JC_Array> nonce, jshort_t offset, jshort_t length) {
  aeadCipherInstance(receiver);
  javacardx_crypto_Cipher_init(receiver, key, mode, nonce, offset, length);
}

void javacardx_crypto_AEADCipher_init(jref_t receiver, jref_t key, jbyte_t mode,
    std::shared_ptr<JC_Array> nonce, jshort_t offset, jshort_t length,
    jshort_t aadLength, jshort_t messageLength, jshort_t tagLength) {
#if JC_SUPPORT_CIPHER_AES_CCM
  auto cipher = aeadCipherInstance(receiver);
  if (static_cast<uint8_t>(cipher->getField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM)) == kAlgorithmAesCcm) {
    initializeCcm(*cipher, key, mode, nonce, offset, length,
                  aadLength, messageLength, tagLength);
    return;
  }
#else
  (void)receiver; (void)key; (void)mode; (void)nonce; (void)offset; (void)length;
  (void)aadLength; (void)messageLength; (void)tagLength;
#endif
  throwCryptoException(CryptoExceptionReason::InvalidInit);
}

void javacardx_crypto_AEADCipher_updateAAD(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t length) {
#if JC_SUPPORT_CIPHER_AES_GCM || JC_SUPPORT_CIPHER_AES_CCM
  auto cipher = aeadCipherInstance(receiver);
  checkArray(input, offset, length);
#if JC_SUPPORT_CIPHER_AES_CCM
  if (static_cast<uint8_t>(cipher->getField_Byte(
          ROM_FIELD_INDEX_JAVACARDX_CRYPTO_CIPHER_CIPHER_ALGORITHM)) == kAlgorithmAesCcm) {
    updateCcmAad(*cipher, *input, offset, length);
    return;
  }
#endif
#if JC_SUPPORT_CIPHER_AES_GCM
  GcmContext context;
  loadGcm(*cipher, context);
  if (length == 0) return;
  if (context.value.MBEDTLS_PRIVATE(len) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  checkAeadResult(mbedtls_gcm_update_ad(&context.value,
      input->getByteDataView() + offset, length));
  saveGcm(*cipher, context);
  cipher->setNativeSessionField_Byte(kAeadTagReady, 0);
  return;
#endif
#else
  (void)receiver; (void)input; (void)offset; (void)length;
#endif
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}

jshort_t javacardx_crypto_AEADCipher_update(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t inOffset, jshort_t length,
    std::shared_ptr<JC_Array> output, jshort_t outOffset) {
  aeadCipherInstance(receiver);
  return javacardx_crypto_Cipher_update(receiver, input, inOffset, length,
                                         output, outOffset);
}

jshort_t javacardx_crypto_AEADCipher_doFinal(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t inOffset, jshort_t length,
    std::shared_ptr<JC_Array> output, jshort_t outOffset) {
  aeadCipherInstance(receiver);
  return javacardx_crypto_Cipher_doFinal(receiver, input, inOffset, length,
                                          output, outOffset);
}

jshort_t javacardx_crypto_AEADCipher_retrieveTag(jref_t receiver,
    std::shared_ptr<JC_Array> output, jshort_t offset, jshort_t length) {
#if JC_SUPPORT_CIPHER_AES_GCM || JC_SUPPORT_CIPHER_AES_CCM
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto cipher = aeadCipherInstance(receiver);
  checkAeadTag(*cipher, kModeEncrypt, length);
  checkArray(output, offset, length);
  const auto tag = readArray(byteArray(*cipher, kAeadTag), 0, length);
  writeArray(output, offset, tag, length);
  context.setSensitiveResultShort(length);
  return length;
#else
  (void)receiver; (void)output; (void)offset; (void)length;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

jbool_t javacardx_crypto_AEADCipher_verifyTag(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t receivedLength,
    jshort_t requiredLength) {
#if JC_SUPPORT_CIPHER_AES_GCM || JC_SUPPORT_CIPHER_AES_CCM
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto cipher = aeadCipherInstance(receiver);
  checkAeadTag(*cipher, kModeDecrypt, requiredLength);
  checkArray(input, offset, receivedLength);
  const auto expected = readArray(byteArray(*cipher, kAeadTag), 0, requiredLength);
  uint8_t difference = receivedLength < requiredLength;
  if (receivedLength >= requiredLength) {
    const auto *received = input->getByteDataView() + offset;
    for (jshort_t i = 0; i < requiredLength; ++i)
      difference |= expected[i] ^ received[i];
  }
  context.setSensitiveResultBoolean(difference == 0);
  return difference == 0 ? TRUE : FALSE;
#else
  (void)receiver; (void)input; (void)offset; (void)receivedLength; (void)requiredLength;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}
} // namespace jcvm
