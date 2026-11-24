/* Single compilation unit for the three ML-DSA levels plus the flat wrapper. */
#include "pqc_backend.h"

#define MLD_CONFIG_FILE "pqc_mldsa_config.h"
#define MLD_CONFIG_MULTILEVEL_WITH_SHARED
#define MLD_CONFIG_MONOBUILD_KEEP_SHARED_HEADERS
#define MLD_CONFIG_PARAMETER_SET 44
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET
#undef MLD_CONFIG_MULTILEVEL_WITH_SHARED
#define MLD_CONFIG_MULTILEVEL_NO_SHARED
#define MLD_CONFIG_PARAMETER_SET 65
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET
/* Shared headers stay defined (KEEP_SHARED_HEADERS) for the SHAKE256 wrapper
 * below; the MLD_NAMESPACE macro is gone, so it uses the expanded names. */
#define MLD_CONFIG_PARAMETER_SET 87
#include "mldsa_native.c"
#undef MLD_CONFIG_PARAMETER_SET

#include "mldsa_native_all.h"

_Static_assert(sizeof(pqc_shake256_t) >= sizeof(mld_shake256ctx),
               "pqc_shake256_t too small");

int pqc_mldsa_sizes(int set, pqc_mldsa_sizes_t *out)
{
  switch (set)
  {
    case 44:
      *out = (pqc_mldsa_sizes_t){MLDSA44_PUBLICKEYBYTES, MLDSA44_SECRETKEYBYTES,
                                 MLDSA44_BYTES};
      return 0;
    case 65:
      *out = (pqc_mldsa_sizes_t){MLDSA65_PUBLICKEYBYTES, MLDSA65_SECRETKEYBYTES,
                                 MLDSA65_BYTES};
      return 0;
    case 87:
      *out = (pqc_mldsa_sizes_t){MLDSA87_PUBLICKEYBYTES, MLDSA87_SECRETKEYBYTES,
                                 MLDSA87_BYTES};
      return 0;
  }
  return PQC_ERR_PARAMETER_SET;
}

size_t pqc_mldsa_scratch(int set)
{
  switch (set)
  {
    case 44:
      return MLD_TOTAL_ALLOC_44;
    case 65:
      return MLD_TOTAL_ALLOC_65;
    case 87:
      return MLD_TOTAL_ALLOC_87;
  }
  return 0;
}

#define PQC_MLDSA_DISPATCH(call) \
  switch (set)                   \
  {                              \
    case 44:                     \
      return mldsa44_##call;     \
    case 65:                     \
      return mldsa65_##call;     \
    case 87:                     \
      return mldsa87_##call;     \
  }                              \
  return PQC_ERR_PARAMETER_SET

int pqc_mldsa_keypair(int set, uint8_t *pk, uint8_t *sk, const uint8_t xi[32])
{
  PQC_MLDSA_DISPATCH(keypair_internal(pk, sk, xi));
}

int pqc_mldsa_pk_from_sk(int set, uint8_t *pk, const uint8_t *sk)
{
  PQC_MLDSA_DISPATCH(pk_from_sk(pk, sk));
}

int pqc_mldsa_sign_mu(int set, uint8_t *sig, const uint8_t mu[64],
                      const uint8_t rnd[32], const uint8_t *sk)
{
  PQC_MLDSA_DISPATCH(signature_internal(sig, mu, 64, 0, 0, rnd, sk, 1));
}

int pqc_mldsa_verify_mu(int set, const uint8_t *sig, const uint8_t mu[64],
                        const uint8_t *pk)
{
  PQC_MLDSA_DISPATCH(verify_internal(sig, mu, 64, 0, 0, pk, 1));
}

void pqc_shake256_init(pqc_shake256_t *state)
{
  mldsa_shake256_init((mld_shake256ctx *)state);
}

void pqc_shake256_absorb(pqc_shake256_t *state, const uint8_t *in, size_t len)
{
  mldsa_shake256_absorb((mld_shake256ctx *)state, in, len);
}

void pqc_shake256_final(pqc_shake256_t *state, uint8_t *out, size_t len)
{
  mldsa_shake256_finalize((mld_shake256ctx *)state);
  mldsa_shake256_squeeze(out, len, (mld_shake256ctx *)state);
  mldsa_shake256_release((mld_shake256ctx *)state);
}

void pqc_mldsa_tr_from_pk(const uint8_t *pk, size_t pk_len, uint8_t tr[64])
{
  pqc_shake256_t state;
  pqc_shake256_init(&state);
  pqc_shake256_absorb(&state, pk, pk_len);
  pqc_shake256_final(&state, tr, 64);
}

void pqc_mldsa_mu_init(pqc_shake256_t *state, const uint8_t tr[64],
                       const uint8_t *ctx, uint8_t ctx_len)
{
  /* FIPS 204 Algorithm 2: M' = 0 || |ctx| || ctx || M (pure ML-DSA). */
  const uint8_t prefix[2] = {0, ctx_len};
  pqc_shake256_init(state);
  pqc_shake256_absorb(state, tr, 64);
  pqc_shake256_absorb(state, prefix, 2);
  if (ctx_len != 0) pqc_shake256_absorb(state, ctx, ctx_len);
}
