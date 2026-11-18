// CHOUPI firmware adjustments to the Pico SDK default Mbed TLS configuration.
//
// Mbed TLS includes this file (MBEDTLS_USER_CONFIG_FILE, set for every
// firmware translation unit through pico_mbedtls_headers) after the default
// mbedtls_config.h, so it only removes features the firmware never reaches.
// Every removed feature is unreachable from native API dispatch:
// - Named curves: native_ec_group.hpp loads only secp192r1, secp224r1,
//   secp256r1, secp384r1 and secp521r1 with mbedtls_ecp_group_load(); other
//   EC field sizes use explicit domain parameters and the generic arithmetic.
//   The Koblitz, Brainpool and Montgomery curve tables were dead flash.
// - SHA-3: no Java Card algorithm in any profile maps to an Mbed TLS SHA-3
//   digest; md.c pulled it in only because the default config enables it.
// - MD5 and RIPEMD-160 are used only by HMAC_MD5 and HMAC_RIPEMD160. CMake
//   defines CHOUPI_MBEDTLS_KEEP_MD5 / CHOUPI_MBEDTLS_KEEP_RIPEMD160 when the
//   selected profile enables those signatures.
// Support reporting comes from the profile, not from this file (INV-08).

#ifndef CHOUPI_MBEDTLS_USER_CONFIG_H
#define CHOUPI_MBEDTLS_USER_CONFIG_H

#undef MBEDTLS_ECP_DP_SECP192K1_ENABLED
#undef MBEDTLS_ECP_DP_SECP224K1_ENABLED
#undef MBEDTLS_ECP_DP_SECP256K1_ENABLED
#undef MBEDTLS_ECP_DP_BP256R1_ENABLED
#undef MBEDTLS_ECP_DP_BP384R1_ENABLED
#undef MBEDTLS_ECP_DP_BP512R1_ENABLED
#undef MBEDTLS_ECP_DP_CURVE25519_ENABLED
#undef MBEDTLS_ECP_DP_CURVE448_ENABLED

#undef MBEDTLS_SHA3_C

#if !defined(CHOUPI_MBEDTLS_KEEP_MD5)
#undef MBEDTLS_MD5_C
#endif
#if !defined(CHOUPI_MBEDTLS_KEEP_RIPEMD160)
#undef MBEDTLS_RIPEMD160_C
#endif

#endif  // CHOUPI_MBEDTLS_USER_CONFIG_H
