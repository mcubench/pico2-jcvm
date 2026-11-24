/* Single compilation unit for the three ML-KEM levels plus the flat wrapper. */
#include "pqc_backend.h"

#define MLK_CONFIG_FILE "pqc_mlkem_config.h"
#define MLK_CONFIG_MULTILEVEL_WITH_SHARED
#define MLK_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS
#define MLK_CONFIG_PARAMETER_SET 512
#include "mlkem_native.c"
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_CONFIG_MULTILEVEL_WITH_SHARED
#define MLK_CONFIG_MULTILEVEL_NO_SHARED
#define MLK_CONFIG_PARAMETER_SET 768
#include "mlkem_native.c"
#undef MLK_CONFIG_PARAMETER_SET
#undef MLK_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS
#define MLK_CONFIG_PARAMETER_SET 1024
#include "mlkem_native.c"
#undef MLK_CONFIG_PARAMETER_SET

#include "mlkem_native_all.h"

int pqc_mlkem_sizes(int set, pqc_mlkem_sizes_t *out)
{
  switch (set)
  {
    case 512:
      *out = (pqc_mlkem_sizes_t){MLKEM512_PUBLICKEYBYTES,
                                 MLKEM512_SECRETKEYBYTES,
                                 MLKEM512_CIPHERTEXTBYTES};
      return 0;
    case 768:
      *out = (pqc_mlkem_sizes_t){MLKEM768_PUBLICKEYBYTES,
                                 MLKEM768_SECRETKEYBYTES,
                                 MLKEM768_CIPHERTEXTBYTES};
      return 0;
    case 1024:
      *out = (pqc_mlkem_sizes_t){MLKEM1024_PUBLICKEYBYTES,
                                 MLKEM1024_SECRETKEYBYTES,
                                 MLKEM1024_CIPHERTEXTBYTES};
      return 0;
  }
  return PQC_ERR_PARAMETER_SET;
}

size_t pqc_mlkem_scratch(int set)
{
  switch (set)
  {
    case 512:
      return MLK_TOTAL_ALLOC_512;
    case 768:
      return MLK_TOTAL_ALLOC_768;
    case 1024:
      return MLK_TOTAL_ALLOC_1024;
  }
  return 0;
}

#define PQC_MLKEM_DISPATCH(call)                       \
  switch (set)                                         \
  {                                                    \
    case 512:                                          \
      return mlkem512_##call;                          \
    case 768:                                          \
      return mlkem768_##call;                          \
    case 1024:                                         \
      return mlkem1024_##call;                         \
  }                                                    \
  return PQC_ERR_PARAMETER_SET

int pqc_mlkem_keypair(int set, uint8_t *ek, uint8_t *dk, const uint8_t dz[64])
{
  PQC_MLKEM_DISPATCH(keypair_derand(ek, dk, dz));
}

int pqc_mlkem_encaps(int set, uint8_t *ct, uint8_t ss[32], const uint8_t *ek,
                     const uint8_t m[32])
{
  PQC_MLKEM_DISPATCH(enc_derand(ct, ss, ek, m));
}

int pqc_mlkem_decaps(int set, uint8_t ss[32], const uint8_t *ct,
                     const uint8_t *dk)
{
  PQC_MLKEM_DISPATCH(dec(ss, ct, dk));
}

int pqc_mlkem_check_ek(int set, const uint8_t *ek)
{
  PQC_MLKEM_DISPATCH(check_pk(ek));
}

int pqc_mlkem_check_dk(int set, const uint8_t *dk)
{
  PQC_MLKEM_DISPATCH(check_sk(dk));
}
