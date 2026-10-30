#include "javacard-support.h"

#if JC_SUPPORT_CIPHER_AES_CTR || JC_SUPPORT_CIPHER_GETINSTANCE_AES_CTR
#include "mbedtls/aes.h"

#include <cstddef>
#include <vector>

namespace jcvm {
int cipherAesCtr(std::vector<unsigned char> &key,
                 std::vector<unsigned char> &counter, size_t &offset,
                 std::vector<unsigned char> &streamBlock,
                 const std::vector<unsigned char> &input,
                 std::vector<unsigned char> &output) {
  if (counter.size() != 16 || streamBlock.size() != 16 || offset > 15)
    return -1;
  mbedtls_aes_context context;
  mbedtls_aes_init(&context);
  int result = mbedtls_aes_setkey_enc(
      &context, key.data(), static_cast<unsigned>(key.size() * 8));
  if (result == 0 && !input.empty()) {
    result = mbedtls_aes_crypt_ctr(
        &context, input.size(), &offset, counter.data(), streamBlock.data(),
        input.data(), output.data());
  }
  mbedtls_aes_free(&context);
  return result;
}
} // namespace jcvm
#endif
