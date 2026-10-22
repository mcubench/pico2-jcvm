/* Use the Pico SDK's Mbed TLS settings, but disable its optional AES-NI
 * module for the portable host emulator build. */
#include "mbedtls/mbedtls_config.h"
#undef MBEDTLS_AESNI_C
