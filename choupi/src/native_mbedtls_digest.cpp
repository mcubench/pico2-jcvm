#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"

#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

#include <cstdint>
#include <memory>
#include <vector>

#if JC_SUPPORT_SHA1 || JC_SUPPORT_SHA224 || JC_SUPPORT_SHA384 || JC_SUPPORT_SHA512
namespace jcvm {
namespace {
void checkRange(const JC_Array &array, jshort_t offset, jshort_t length) {
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) > array.size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
}
}

jbool_t fr_gouv_ssi_nativeimpl_MbedTlsDigestNativeBridge_digest(
    jbyte_t algorithm, std::shared_ptr<JC_Array> input, jshort_t inputLength,
    std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  if (!input || !output) throw Exceptions::NullPointerException;
  if (input->getType() != JAVA_ARRAY_T_BYTE || output->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  checkRange(*input, 0, inputLength);

  const uint8_t alg = static_cast<uint8_t>(algorithm);
  uint16_t digestLength = 0;
  switch (alg) {
  case 1: digestLength = 20; break;
  case 7: digestLength = 28; break;
  case 5: digestLength = 48; break;
  case 6: digestLength = 64; break;
  default: throw Exceptions::CryptoException;
  }
  checkRange(*output, outputOffset, static_cast<jshort_t>(digestLength));

  std::vector<unsigned char> source(static_cast<size_t>(inputLength));
  std::vector<unsigned char> digest(digestLength);
  for (jshort_t i = 0; i < inputLength; ++i)
    source[static_cast<size_t>(i)] = static_cast<uint8_t>(input->getByteEntry(static_cast<uint16_t>(i)));
  int result = -1;
  switch (alg) {
#if JC_SUPPORT_SHA1
  case 1: result = mbedtls_sha1(source.data(), source.size(), digest.data()); break;
#endif
#if JC_SUPPORT_SHA224
  case 7: result = mbedtls_sha256(source.data(), source.size(), digest.data(), 1); break;
#endif
#if JC_SUPPORT_SHA384
  case 5: result = mbedtls_sha512(source.data(), source.size(), digest.data(), 1); break;
#endif
#if JC_SUPPORT_SHA512
  case 6: result = mbedtls_sha512(source.data(), source.size(), digest.data(), 0); break;
#endif
  default: break;
  }
  if (result != 0) return FALSE;
  for (uint16_t i = 0; i < digestLength; ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i), static_cast<jbyte_t>(digest[i]));
  return TRUE;
}
} // namespace jcvm
#endif
