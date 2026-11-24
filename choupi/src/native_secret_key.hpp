#pragma once

#include "jc_types/jref_t.hpp"

#include <cstdint>

namespace jcvm {

// Byte length a native secret key (AES, DES, HMAC) accepts from a derived
// secret, or 0 if the reference is not such a key. HMAC keys accept any length
// up to their capacity; `length` is checked against that.
bool nativeSecretKeyAccepts(jref_t key, uint16_t length);
// Sets the key value from raw bytes and marks it initialized. Call only after
// nativeSecretKeyAccepts(key, length) returned true.
void nativeSecretKeySet(jref_t key, const uint8_t *bytes, uint16_t length);

} // namespace jcvm
