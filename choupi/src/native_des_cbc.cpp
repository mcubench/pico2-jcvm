#include "javacard-support.h"
#if JC_SUPPORT_CIPHER_DES_CBC_NOPAD || JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD
#include "mbedtls/des.h"
#include <vector>

namespace jcvm {
int cipherDesCbc(bool encrypt, std::vector<unsigned char> &key,
                 std::vector<unsigned char> &iv,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output) {
  int result = 0;
  if (key.size() == 8) {
    mbedtls_des_context context;
    mbedtls_des_init(&context);
    result = encrypt ? mbedtls_des_setkey_enc(&context, key.data())
                     : mbedtls_des_setkey_dec(&context, key.data());
    if (result == 0 && !input.empty()) {
      result = mbedtls_des_crypt_cbc(
          &context, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT,
          input.size(), iv.data(), input.data(), output.data());
    }
    mbedtls_des_free(&context);
  } else {
    mbedtls_des3_context context;
    mbedtls_des3_init(&context);
    if (key.size() == 16) {
      result = encrypt ? mbedtls_des3_set2key_enc(&context, key.data())
                       : mbedtls_des3_set2key_dec(&context, key.data());
    } else {
      result = encrypt ? mbedtls_des3_set3key_enc(&context, key.data())
                       : mbedtls_des3_set3key_dec(&context, key.data());
    }
    if (result == 0 && !input.empty()) {
      result = mbedtls_des3_crypt_cbc(
          &context, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT,
          input.size(), iv.data(), input.data(), output.data());
    }
    mbedtls_des3_free(&context);
  }
  return result;
}
} // namespace jcvm
#endif
