#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"
#include "native_crypto_exception.hpp"
#include <cstdint>
#include <memory>
#ifdef JC_SUPPORT_SECURE_RANDOM
#include <random>
#ifndef PC_VERSION
#include "pico/rand.h"
#endif
#endif

namespace jcvm {

#if JC_SUPPORT_SECURE_RANDOM
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

void fr_gouv_ssi_nativeimpl_RandomDataNativeBridge_generateRandomData(
    std::shared_ptr<JC_Array> buffer, const jshort_t offset,
    const jshort_t length) {
  if (buffer == nullptr) {
    throw Exceptions::NullPointerException;
  }
  checkArrayRange(*buffer, offset, length);
  if (length == 0) {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  if (buffer->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::ArrayStoreException;
  }

#ifdef PC_VERSION
  std::random_device random_source;
#endif
  uint32_t random_word = 0;
  for (jshort_t index = 0; index < length; ++index) {
    if ((index & 3) == 0) {
#ifdef PC_VERSION
      random_word = static_cast<uint32_t>(random_source());
#else
      random_word = get_rand_32();
#endif
    }
    const uint8_t random_byte = static_cast<uint8_t>(
        random_word >> ((index & 3) * 8));
    buffer->setByteEntry(static_cast<uint16_t>(offset + index),
                         static_cast<jbyte_t>(random_byte));
  }
}

#endif // JC_SUPPORT_SECURE_RANDOM

} // namespace jcvm
