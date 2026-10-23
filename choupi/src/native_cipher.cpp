#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace jcvm {

#if JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD
int cipherAesCbc(bool encrypt, std::vector<unsigned char> &key,
                 std::vector<unsigned char> &iv,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
#endif
#if JC_SUPPORT_CIPHER_DES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD
int cipherDesCbc(bool encrypt, std::vector<unsigned char> &key,
                 std::vector<unsigned char> &iv,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output);
#endif

namespace {
void checkArrayRange(const JC_Array &array, const jshort_t offset,
                     const jshort_t length) {
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) >
          array.size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
}

} // namespace

#if JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_DES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD
void fr_gouv_ssi_nativeimpl_CbcCipherNativeBridge_cipherCbc(
    const jbyte_t algorithm, const jbyte_t mode,
    std::shared_ptr<JC_Array> key, std::shared_ptr<JC_Array> iv,
    std::shared_ptr<JC_Array> input, const jshort_t inputOffset,
    const jshort_t inputLength, std::shared_ptr<JC_Array> output,
    const jshort_t outputOffset) {
  if (!key || !iv || !input || !output) {
    throw Exceptions::NullPointerException;
  }
  if (key->getType() != JAVA_ARRAY_T_BYTE || iv->getType() != JAVA_ARRAY_T_BYTE ||
      input->getType() != JAVA_ARRAY_T_BYTE ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (inputLength < 0 || inputOffset < 0 || outputOffset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*input, inputOffset, inputLength);
  checkArrayRange(*output, outputOffset, inputLength);

  const bool aes = static_cast<uint8_t>(algorithm) == 13;
  const bool des = static_cast<uint8_t>(algorithm) == 1;
  if (!aes && !des) throw Exceptions::CryptoException;
  const uint16_t block_size = aes ? 16 : 8;
  if ((aes && !JC_SUPPORT_CIPHER_AES_CBC_NOPAD &&
       !JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD) ||
      (des && !JC_SUPPORT_CIPHER_DES_CBC_NOPAD &&
       !JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD)) {
    throw Exceptions::CryptoException;
  }
  if (inputLength % block_size != 0 || iv->size() < block_size ||
      (aes && (key->size() != 16 && key->size() != 24 && key->size() != 32)) ||
      (des && key->size() != 8 && key->size() != 16 && key->size() != 24)) {
    throw Exceptions::CryptoException;
  }
  const bool encrypt = static_cast<uint8_t>(mode) == 2;
  if (static_cast<uint8_t>(mode) != 1 && !encrypt) {
    throw Exceptions::CryptoException;
  }

  std::vector<unsigned char> key_bytes(key->size());
  std::vector<unsigned char> iv_bytes(block_size);
  std::vector<unsigned char> input_bytes(inputLength);
  std::vector<unsigned char> output_bytes(inputLength);
  for (uint16_t index = 0; index < key->size(); ++index) {
    key_bytes[index] = static_cast<unsigned char>(key->getByteEntry(index));
  }
  for (uint16_t index = 0; index < block_size; ++index) {
    iv_bytes[index] = static_cast<unsigned char>(iv->getByteEntry(index));
  }
  for (uint16_t index = 0; index < static_cast<uint16_t>(inputLength); ++index) {
    input_bytes[index] = static_cast<unsigned char>(input->getByteEntry(
        static_cast<uint16_t>(inputOffset + index)));
  }

  int result = -1;
  if (aes) {
#if JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD
    result = cipherAesCbc(encrypt, key_bytes, iv_bytes, input_bytes, output_bytes);
#endif
  } else {
#if JC_SUPPORT_CIPHER_DES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD
    result = cipherDesCbc(encrypt, key_bytes, iv_bytes, input_bytes, output_bytes);
#endif
  }
  if (result != 0) throw Exceptions::RuntimeException;
  for (uint16_t index = 0; index < static_cast<uint16_t>(inputLength); ++index) {
    output->setByteEntry(static_cast<uint16_t>(outputOffset + index),
                         static_cast<jbyte_t>(output_bytes[index]));
  }
  for (uint16_t index = 0; index < block_size; ++index) {
    iv->setByteEntry(index, static_cast<jbyte_t>(iv_bytes[index]));
  }
}
#endif

} // namespace jcvm
