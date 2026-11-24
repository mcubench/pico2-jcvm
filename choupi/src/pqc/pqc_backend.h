/* Flat C interface over mlkem-native and mldsa-native for the native Java Card
 * layer. Every call needs an active arena (pqc_arena_begin). Parameter sets are
 * identified by their FIPS number: 512/768/1024 for ML-KEM, 44/65/87 for ML-DSA.
 * Functions return 0 on success and a negative library error otherwise. */
#ifndef CHOUPI_PQC_BACKEND_H
#define CHOUPI_PQC_BACKEND_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define PQC_ERR_PARAMETER_SET (-100)

typedef struct {
  uint16_t ek, dk, ct; /* encoded sizes in bytes; shared secret is 32 */
} pqc_mlkem_sizes_t;
int pqc_mlkem_sizes(int set, pqc_mlkem_sizes_t *out);
size_t pqc_mlkem_scratch(int set);
/* dz = d || z (64 bytes), FIPS 203 Algorithm 16. */
int pqc_mlkem_keypair(int set, uint8_t *ek, uint8_t *dk, const uint8_t dz[64]);
/* m: 32 random bytes, FIPS 203 Algorithm 17; checks ek (modulus check). */
int pqc_mlkem_encaps(int set, uint8_t *ct, uint8_t ss[32], const uint8_t *ek,
                     const uint8_t m[32]);
/* FIPS 203 Algorithm 18; checks dk (hash check). */
int pqc_mlkem_decaps(int set, uint8_t ss[32], const uint8_t *ct,
                     const uint8_t *dk);
int pqc_mlkem_check_ek(int set, const uint8_t *ek);
int pqc_mlkem_check_dk(int set, const uint8_t *dk);

typedef struct {
  uint16_t pk, sk, sig;
} pqc_mldsa_sizes_t;
int pqc_mldsa_sizes(int set, pqc_mldsa_sizes_t *out);
size_t pqc_mldsa_scratch(int set);
/* FIPS 204 Algorithm 6 from the 32-byte seed xi. */
int pqc_mldsa_keypair(int set, uint8_t *pk, uint8_t *sk, const uint8_t xi[32]);
/* Derives pk from an expanded sk and validates sk. */
int pqc_mldsa_pk_from_sk(int set, uint8_t *pk, const uint8_t *sk);
/* FIPS 204 Algorithm 7 on an external mu; rnd is 32 bytes (hedged). */
int pqc_mldsa_sign_mu(int set, uint8_t *sig, const uint8_t mu[64],
                      const uint8_t rnd[32], const uint8_t *sk);
/* FIPS 204 Algorithm 8 on an external mu; 0 = valid signature. */
int pqc_mldsa_verify_mu(int set, const uint8_t *sig, const uint8_t mu[64],
                        const uint8_t *pk);

/* Incremental SHAKE256, used to stream mu = H(tr || 0 || |ctx| || ctx || M). */
typedef struct {
  uint64_t opaque[26];
} pqc_shake256_t;
void pqc_shake256_init(pqc_shake256_t *state);
void pqc_shake256_absorb(pqc_shake256_t *state, const uint8_t *in, size_t len);
/* Finalizes and squeezes len bytes; the state must be re-initialized after. */
void pqc_shake256_final(pqc_shake256_t *state, uint8_t *out, size_t len);

/* tr = H(pk, 64) for verification; signing reads tr from sk bytes 64..127. */
void pqc_mldsa_tr_from_pk(const uint8_t *pk, size_t pk_len, uint8_t tr[64]);
/* Starts mu = H(tr || 0 || ctx_len || ctx || M); absorb M, then final(64). */
void pqc_mldsa_mu_init(pqc_shake256_t *state, const uint8_t tr[64],
                       const uint8_t *ctx, uint8_t ctx_len);

#ifdef __cplusplus
}
#endif
#endif
