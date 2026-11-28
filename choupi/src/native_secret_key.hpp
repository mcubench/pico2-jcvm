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
class JC_Instance;
class JC_Array;
// Logical key length of the KEYDATA array `data` (shorter than it for HMAC).
uint16_t nativeKeyDataLength(JC_Instance &key, const JC_Array &data);

} // namespace jcvm
