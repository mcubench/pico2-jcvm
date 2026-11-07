#include "javacard-support.h"
#include "native_crypto_exception.hpp"

#include "mbedtls/md.h"

#include <cstdint>
#include <vector>

#if JC_SUPPORT_SIGNATURE_HMAC_SHA1 || JC_SUPPORT_SIGNATURE_HMAC_SHA_256 || \
    JC_SUPPORT_SIGNATURE_HMAC_SHA_384 || JC_SUPPORT_SIGNATURE_HMAC_SHA_512 || \
    JC_SUPPORT_SIGNATURE_HMAC_MD5 || JC_SUPPORT_SIGNATURE_HMAC_RIPEMD160
namespace jcvm {
namespace {
mbedtls_md_type_t digestForAlgorithm(uint8_t algorithm) {
  switch (algorithm) {
#if JC_SUPPORT_SIGNATURE_HMAC_SHA1
  case 24: return MBEDTLS_MD_SHA1;
#endif
#if JC_SUPPORT_SIGNATURE_HMAC_SHA_256
  case 25: return MBEDTLS_MD_SHA256;
#endif
#if JC_SUPPORT_SIGNATURE_HMAC_SHA_384
  case 26: return MBEDTLS_MD_SHA384;
#endif
#if JC_SUPPORT_SIGNATURE_HMAC_SHA_512
  case 27: return MBEDTLS_MD_SHA512;
#endif
#if JC_SUPPORT_SIGNATURE_HMAC_MD5
  case 28: return MBEDTLS_MD_MD5;
#endif
#if JC_SUPPORT_SIGNATURE_HMAC_RIPEMD160
  case 29: return MBEDTLS_MD_RIPEMD160;
#endif
  default: throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
}
}  // namespace

std::vector<unsigned char> computeHmac(
    uint8_t algorithm, const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& message) {
  const auto* info = mbedtls_md_info_from_type(digestForAlgorithm(algorithm));
  if (!info) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const size_t length = mbedtls_md_get_size(info);
  std::vector<unsigned char> result(length);
  const int rc = mbedtls_md_hmac(info, key.data(), key.size(),
                                 message.data(), message.size(), result.data());
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  return result;
}
}  // namespace jcvm
#endif
