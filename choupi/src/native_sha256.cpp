#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"
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
    throw Exceptions::CryptoException;
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
    throw Exceptions::CryptoException;
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

jbool_t fr_gouv_ssi_nativeimpl_Sha256NativeBridge_update(
    std::shared_ptr<JC_Array> state, std::shared_ptr<JC_Array> input,
    const jshort_t inputOffset, const jshort_t inputLength) {
  constexpr uint16_t state_capacity = 2048;
  constexpr uint16_t max_input_length = state_capacity - 2;
  if (!state || !input) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE ||
      input->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (inputOffset < 0 || inputLength < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*state, 0, state_capacity);
  checkArrayRange(*input, inputOffset, inputLength);

  const uint16_t current_length = static_cast<uint16_t>(
      (static_cast<uint8_t>(state->getByteEntry(max_input_length)) << 8) |
      static_cast<uint8_t>(state->getByteEntry(max_input_length + 1)));
  if (current_length > max_input_length ||
      static_cast<uint32_t>(current_length) +
              static_cast<uint16_t>(inputLength) >
          max_input_length) {
    return FALSE;
  }
  for (uint16_t index = 0; index < static_cast<uint16_t>(inputLength);
       ++index) {
    state->setByteEntry(
        static_cast<uint16_t>(current_length + index),
        input->getByteEntry(static_cast<uint16_t>(inputOffset + index)));
  }
  const uint16_t updated_length = static_cast<uint16_t>(
      current_length + static_cast<uint16_t>(inputLength));
  state->setByteEntry(max_input_length,
                      static_cast<jbyte_t>(updated_length >> 8));
  state->setByteEntry(max_input_length + 1,
                      static_cast<jbyte_t>(updated_length));
  return TRUE;
}

void fr_gouv_ssi_nativeimpl_Sha256NativeBridge_doFinal(
    std::shared_ptr<JC_Array> state, std::shared_ptr<JC_Array> output,
    const jshort_t outputOffset) {
  constexpr uint16_t state_capacity = 2048;
  constexpr uint16_t max_input_length = state_capacity - 2;
  if (!state || !output) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE ||
      output->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  if (outputOffset < 0) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  checkArrayRange(*state, 0, state_capacity);
  checkArrayRange(*output, outputOffset, 32);
  const uint16_t input_length = static_cast<uint16_t>(
      (static_cast<uint8_t>(state->getByteEntry(max_input_length)) << 8) |
      static_cast<uint8_t>(state->getByteEntry(max_input_length + 1)));
  if (input_length > max_input_length) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  computeSha256(
      state, 0, static_cast<jshort_t>(input_length), output, outputOffset);
  for (uint16_t index = 0; index < input_length; ++index) {
    state->setByteEntry(index, 0);
  }
  state->setByteEntry(max_input_length, 0);
  state->setByteEntry(max_input_length + 1, 0);
}

void fr_gouv_ssi_nativeimpl_Sha256NativeBridge_reset(
    std::shared_ptr<JC_Array> state) {
  constexpr uint16_t state_capacity = 2048;
  constexpr uint16_t max_input_length = state_capacity - 2;
  if (!state) {
    throw Exceptions::NullPointerException;
  }
  if (state->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }
  checkArrayRange(*state, 0, state_capacity);
  const uint16_t input_length = static_cast<uint16_t>(
      (static_cast<uint8_t>(state->getByteEntry(max_input_length)) << 8) |
      static_cast<uint8_t>(state->getByteEntry(max_input_length + 1)));
  if (input_length <= max_input_length) {
    for (uint16_t index = 0; index < input_length; ++index) {
      state->setByteEntry(index, 0);
    }
  }
  state->setByteEntry(max_input_length, 0);
  state->setByteEntry(max_input_length + 1, 0);
}

#endif // JC_SUPPORT_SHA256

} // namespace jcvm
