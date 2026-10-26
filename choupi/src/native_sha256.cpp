#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"
#include "native_crypto_exception.hpp"
#include <cstdint>
#include <memory>
#if JC_SUPPORT_SHA256
#include <algorithm>
#ifdef PC_VERSION
#include "mbedtls/sha256.h"
#else
#include "pico/sha256.h"
#endif
#endif

namespace jcvm {

#if JC_SUPPORT_SHA256
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

static void computeSha256(
    std::shared_ptr<JC_Array> input, const jshort_t inputOffset,
    const jshort_t inputLength, std::shared_ptr<JC_Array> output,
    const jshort_t outputOffset) {
  if (!input || !output) {
    throw Exceptions::NullPointerException;
  }
  if (input->getType() != JAVA_ARRAY_T_BYTE ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (inputOffset < 0 || inputLength < 0 || outputOffset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*input, inputOffset, inputLength);
  checkArrayRange(*output, outputOffset, 32);

#ifdef PC_VERSION
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  int result = mbedtls_sha256_starts(&context, 0);
  uint8_t block[64];
  for (jshort_t offset = 0; result == 0 && offset < inputLength;) {
    const jshort_t count = static_cast<jshort_t>(
        std::min<jshort_t>(sizeof(block), inputLength - offset));
    for (jshort_t i = 0; i < count; ++i) {
      block[i] = static_cast<uint8_t>(input->getByteEntry(
          static_cast<uint16_t>(inputOffset + offset + i)));
    }
    result = mbedtls_sha256_update(&context, block,
                                   static_cast<size_t>(count));
    offset = static_cast<jshort_t>(offset + count);
  }
  uint8_t digest[32];
  if (result == 0) {
    result = mbedtls_sha256_finish(&context, digest);
  }
  mbedtls_sha256_free(&context);
  if (result != 0) {
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  for (uint16_t i = 0; i < sizeof(digest); ++i) {
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         static_cast<jbyte_t>(digest[i]));
  }
#else
  pico_sha256_state_t state{};
  if (pico_sha256_start_blocking_until(
          &state, SHA256_BIG_ENDIAN, false, make_timeout_time_ms(100)) !=
      PICO_OK) {
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  uint8_t block[64];
  for (jshort_t offset = 0; offset < inputLength;) {
    const jshort_t count = static_cast<jshort_t>(
        std::min<jshort_t>(sizeof(block), inputLength - offset));
    for (jshort_t i = 0; i < count; ++i) {
      block[i] = static_cast<uint8_t>(input->getByteEntry(
          static_cast<uint16_t>(inputOffset + offset + i)));
    }
    pico_sha256_update_blocking(&state, block, static_cast<size_t>(count));
    offset = static_cast<jshort_t>(offset + count);
  }
  sha256_result_t digest{};
  pico_sha256_finish(&state, &digest);
  for (uint16_t i = 0; i < sizeof(digest.bytes); ++i) {
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i),
                         static_cast<jbyte_t>(digest.bytes[i]));
  }
#endif
}

void fr_gouv_ssi_nativeimpl_Sha256NativeBridge_doFinal(
    std::shared_ptr<JC_Array> state, const jshort_t inputLength,
    std::shared_ptr<JC_Array> output, const jshort_t outputOffset) {
  if (!state || !output) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (inputLength < 0 || outputOffset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*state, 0, inputLength);
  checkArrayRange(*output, outputOffset, 32);
  computeSha256(state, 0, inputLength, output, outputOffset);
}

#endif // JC_SUPPORT_SHA256

} // namespace jcvm
