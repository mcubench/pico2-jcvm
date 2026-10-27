#pragma once

#include "mbedtls/ecp.h"

#include <cstdint>

namespace jcvm {
namespace native_ec {

inline int readHex(mbedtls_mpi &target, const char *value) {
  return mbedtls_mpi_read_string(&target, 16, value);
}

inline int loadSmallCurve(mbedtls_ecp_group &group, uint16_t bits) {
  const char *p = nullptr;
  const char *a = nullptr;
  const char *b = nullptr;
  const char *gx = nullptr;
  const char *gy = nullptr;
  const char *n = nullptr;
  switch (bits) {
  case 128:
    p = "FFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFF";
    a = "FFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFC";
    b = "E87579C11079F43DD824993C2CEE5ED3";
    gx = "161FF7528B899B2D0C28607CA52C5B86";
    gy = "CF5AC8395BAFEB13C02DA292DDED7A83";
    n = "FFFFFFFE0000000075A30D1B9038A115";
    break;
  case 160:
    p = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF";
    a = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC";
    b = "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45";
    gx = "4A96B5688EF573284664698968C38BB913CBFC82";
    gy = "23A628553168947D59DCC912042351377AC5FB32";
    n = "100000000000000000001F4C8F927AED3CA752257";
    break;
  default:
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
  }

  int rc = readHex(group.P, p);
  if (rc == 0) rc = readHex(group.A, a);
  if (rc == 0) rc = readHex(group.B, b);
  if (rc == 0) rc = readHex(group.G.MBEDTLS_PRIVATE(X), gx);
  if (rc == 0) rc = readHex(group.G.MBEDTLS_PRIVATE(Y), gy);
  if (rc == 0) rc = mbedtls_mpi_lset(&group.G.MBEDTLS_PRIVATE(Z), 1);
  if (rc == 0) rc = readHex(group.N, n);
  if (rc != 0) return rc;

  group.id = MBEDTLS_ECP_DP_NONE;
  group.pbits = mbedtls_mpi_bitlen(&group.P);
  group.nbits = mbedtls_mpi_bitlen(&group.N);
  group.MBEDTLS_PRIVATE(h) = 1;
  return 0;
}

inline int loadGroupForBits(mbedtls_ecp_group &group, uint16_t bits) {
  switch (bits) {
  case 128:
  case 160:
    return loadSmallCurve(group, bits);
  case 192:
    return mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP192R1);
  case 224:
    return mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP224R1);
  case 256:
    return mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1);
  case 384:
    return mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP384R1);
  case 521:
    return mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP521R1);
  default:
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
  }
}

inline bool supportsBits(uint16_t bits) {
  return bits == 128 || bits == 160 || bits == 192 || bits == 224 ||
         bits == 256 || bits == 384 || bits == 521;
}

} // namespace native_ec
} // namespace jcvm
