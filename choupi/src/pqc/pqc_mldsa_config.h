/* Project configuration for the vendored mldsa-native (third_party/mldsa-native).
 * Upstream defaults first, then the Pico 2 JCVM integration choices. */
#ifndef CHOUPI_PQC_MLDSA_CONFIG_H
#define CHOUPI_PQC_MLDSA_CONFIG_H
#include "mldsa_native_config.h"

/* Smallest working set (about 22 KiB for ML-DSA-87) at some speed cost. */
#define MLD_CONFIG_REDUCE_RAM

/* Randomness (keygen seed, hedged-signing rnd) is supplied by the caller. */
#define MLD_CONFIG_NO_RANDOMIZED_API

#define MLD_CONFIG_CUSTOM_ALLOC_FREE
#if !defined(__ASSEMBLER__)
#include "pqc_arena.h"
#define MLD_CUSTOM_ALLOC(v, T, N) \
  T *(v) = (T *)choupi_pqc_alloc(MLD_ALIGN_UP(sizeof(T) * (N)))
#define MLD_CUSTOM_FREE(v, T, N) choupi_pqc_free((v), MLD_ALIGN_UP(sizeof(T) * (N)))
#endif

/* Service the watchdog/CCID between signing attempts; a non-zero return
 * aborts signing (MLD_ERR_SIGNING_PAUSED without resume). */
#define MLD_CONFIG_SIGN_HOOK_ATTEMPT
#if !defined(__ASSEMBLER__)
#include <stdint.h>
static inline int mld_sign_hook_attempt(uint16_t attempt)
{
  (void)attempt;
  return choupi_pqc_progress() ? 0 : 1;
}
#endif
#endif
