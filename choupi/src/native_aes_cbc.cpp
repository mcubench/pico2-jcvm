#include "javacard-support.h"
#if JC_SUPPORT_CIPHER_AES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD || \
    JC_SUPPORT_CIPHER_AES_ECB_NOPAD || JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M1 || \
    JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M2 || JC_SUPPORT_CIPHER_AES_CBC_PKCS5 || \
    JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M1 || JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M2 || \
    JC_SUPPORT_CIPHER_AES_ECB_PKCS5 || JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD || \
    JC_SUPPORT_SIGNATURE_AES_CMAC_128
#include "mbedtls/aes.h"
#include <vector>

namespace jcvm {
int cipherAesCbc(bool encrypt, std::vector<unsigned char> &key,
                 std::vector<unsigned char> &iv,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output) {
  mbedtls_aes_context context;
  mbedtls_aes_init(&context);
  int result = encrypt
      ? mbedtls_aes_setkey_enc(&context, key.data(),
                               static_cast<unsigned>(key.size() * 8))
      : mbedtls_aes_setkey_dec(&context, key.data(),
                               static_cast<unsigned>(key.size() * 8));
  if (result == 0 && !input.empty()) {
    result = mbedtls_aes_crypt_cbc(
        &context, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
        input.size(), iv.data(), input.data(), output.data());
  }
  mbedtls_aes_free(&context);
  return result;
}

int cipherAesEcb(bool encrypt, std::vector<unsigned char> &key,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output) {
  mbedtls_aes_context context;
  mbedtls_aes_init(&context);
  int result = encrypt
      ? mbedtls_aes_setkey_enc(&context, key.data(), static_cast<unsigned>(key.size() * 8))
      : mbedtls_aes_setkey_dec(&context, key.data(), static_cast<unsigned>(key.size() * 8));
  for (size_t offset = 0; result == 0 && offset < input.size(); offset += 16)
    result = mbedtls_aes_crypt_ecb(&context, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
                                   input.data() + offset, output.data() + offset);
  mbedtls_aes_free(&context);
  return result;
}
} // namespace jcvm
#endif
