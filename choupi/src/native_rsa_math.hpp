#pragma once

#include "native_crypto_exception.hpp"

#include "mbedtls/bignum.h"

#include <array>
#include <cstddef>
#include <vector>

namespace jcvm {

inline std::vector<unsigned char> rsaCrtModulus(
    const std::vector<unsigned char> &pBytes,
    const std::vector<unsigned char> &qBytes, size_t width) {
  mbedtls_mpi p, q, n;
  mbedtls_mpi_init(&p);
  mbedtls_mpi_init(&q);
  mbedtls_mpi_init(&n);
  int result = pBytes.empty() || qBytes.empty()
      ? -1
      : mbedtls_mpi_read_binary(&p, pBytes.data(), pBytes.size());
  if (result == 0)
    result = mbedtls_mpi_read_binary(&q, qBytes.data(), qBytes.size());
  if (result == 0) result = mbedtls_mpi_mul_mpi(&n, &p, &q);
  std::vector<unsigned char> modulus(width);
  const int writeResult = result == 0
      ? mbedtls_mpi_write_binary(&n, modulus.data(), modulus.size())
      : result;
  mbedtls_mpi_free(&n);
  mbedtls_mpi_free(&q);
  mbedtls_mpi_free(&p);
  if (writeResult != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return modulus;
}

inline std::vector<unsigned char> rsaCrtModExp(
    const std::vector<unsigned char> &modulusBytes,
    const std::array<std::vector<unsigned char>, 5> &components,
    const std::vector<unsigned char> &inputBytes) {
  // Java Card's PQ parameter is q^-1 mod p, as required by the PKCS#1 CRT
  // recombination formula m = m2 + q * (((m1 - m2) * qInv) mod p).
  mbedtls_mpi n, p, q, dp, dq, qInv, input, product, m1, m2, h, result;
  mbedtls_mpi_init(&n);
  mbedtls_mpi_init(&p);
  mbedtls_mpi_init(&q);
  mbedtls_mpi_init(&dp);
  mbedtls_mpi_init(&dq);
  mbedtls_mpi_init(&qInv);
  mbedtls_mpi_init(&input);
  mbedtls_mpi_init(&product);
  mbedtls_mpi_init(&m1);
  mbedtls_mpi_init(&m2);
  mbedtls_mpi_init(&h);
  mbedtls_mpi_init(&result);

  int rc = modulusBytes.empty() || inputBytes.empty() ? -1 : 0;
  mbedtls_mpi *destinations[] = {&p, &q, &dp, &dq, &qInv};
  for (size_t i = 0; rc == 0 && i < components.size(); ++i) {
    if (components[i].empty()) {
      rc = -1;
      break;
    }
    rc = mbedtls_mpi_read_binary(destinations[i], components[i].data(),
                                 components[i].size());
  }
  if (rc == 0) {
    rc |= mbedtls_mpi_read_binary(&n, modulusBytes.data(), modulusBytes.size());
    rc |= mbedtls_mpi_read_binary(&input, inputBytes.data(), inputBytes.size());
  }
  if (rc == 0 && (mbedtls_mpi_cmp_int(&p, 1) <= 0 ||
                  mbedtls_mpi_cmp_int(&q, 1) <= 0 ||
                  mbedtls_mpi_cmp_mpi(&input, &n) >= 0))
    rc = -1;
  if (rc == 0) rc |= mbedtls_mpi_mul_mpi(&product, &p, &q);
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&product, &n) != 0) rc = -1;
  if (rc == 0) rc |= mbedtls_mpi_exp_mod(&m1, &input, &dp, &p, nullptr);
  if (rc == 0) rc |= mbedtls_mpi_exp_mod(&m2, &input, &dq, &q, nullptr);
  if (rc == 0) rc |= mbedtls_mpi_sub_mpi(&h, &m1, &m2);
  if (rc == 0) rc |= mbedtls_mpi_mod_mpi(&h, &h, &p);
  if (rc == 0) rc |= mbedtls_mpi_mul_mpi(&h, &h, &qInv);
  if (rc == 0) rc |= mbedtls_mpi_mod_mpi(&h, &h, &p);
  if (rc == 0) rc |= mbedtls_mpi_mul_mpi(&h, &h, &q);
  if (rc == 0) rc |= mbedtls_mpi_add_mpi(&result, &m2, &h);

  std::vector<unsigned char> output(modulusBytes.size());
  const int writeResult = rc == 0
      ? mbedtls_mpi_write_binary(&result, output.data(), output.size())
      : rc;
  mbedtls_mpi_free(&result);
  mbedtls_mpi_free(&h);
  mbedtls_mpi_free(&m2);
  mbedtls_mpi_free(&m1);
  mbedtls_mpi_free(&product);
  mbedtls_mpi_free(&input);
  mbedtls_mpi_free(&qInv);
  mbedtls_mpi_free(&dq);
  mbedtls_mpi_free(&dp);
  mbedtls_mpi_free(&q);
  mbedtls_mpi_free(&p);
  mbedtls_mpi_free(&n);
  if (writeResult != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return output;
}

} // namespace jcvm
