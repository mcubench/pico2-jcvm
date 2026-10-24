#ifndef CHOUPI_NATIVE_CRYPTO_EXCEPTION_HPP
#define CHOUPI_NATIVE_CRYPTO_EXCEPTION_HPP

#include "context.hpp"
#include "exceptions.hpp"

namespace jcvm {

enum class CryptoExceptionReason : jshort_t {
  IllegalValue = 1,
  UninitializedKey = 2,
  NoSuchAlgorithm = 3,
  InvalidInit = 4,
  IllegalUse = 5,
};

[[noreturn]] inline void throwCryptoException(
    const CryptoExceptionReason reason) {
  Context::getActiveContext().setPendingCryptoExceptionReason(
      static_cast<uint16_t>(reason));
  throw Exceptions::CryptoException;
}

} // namespace jcvm

#endif // CHOUPI_NATIVE_CRYPTO_EXCEPTION_HPP
