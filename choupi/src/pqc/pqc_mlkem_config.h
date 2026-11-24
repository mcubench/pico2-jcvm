/* Project configuration for the vendored mlkem-native (third_party/mlkem-native).
 * Upstream defaults first, then the Pico 2 JCVM integration choices. */
#ifndef CHOUPI_PQC_MLKEM_CONFIG_H
#define CHOUPI_PQC_MLKEM_CONFIG_H
#include "mlkem_native_config.h"

/* Randomness is drawn by the native Java Card layer and passed to the
 * derandomized APIs, so the library never calls an RNG itself. */
#define MLK_CONFIG_NO_RANDOMIZED_API

/* The RP2350 main stack is 4 KiB: large buffers come from a per-operation
 * arena (pqc_arena.c) instead of the stack. */
#define MLK_CONFIG_CUSTOM_ALLOC_FREE
#if !defined(__ASSEMBLER__)
#include "pqc_arena.h"
#define MLK_CUSTOM_ALLOC(v, T, N) \
  T *(v) = (T *)choupi_pqc_alloc(MLK_ALIGN_UP(sizeof(T) * (N)))
#define MLK_CUSTOM_FREE(v, T, N) choupi_pqc_free((v), MLK_ALIGN_UP(sizeof(T) * (N)))
#endif
#endif
