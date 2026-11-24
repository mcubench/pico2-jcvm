#pragma once

#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "jc_types/jref_t.hpp"

#include <cstdint>
#include <memory>

namespace jcvm {

// One ML-KEM or ML-DSA parameter set (FIPS 203 / FIPS 204 sizes in bytes).
struct PqcParameterSet {
  jshort_t name;      // NamedParameterSpec constant
  bool kem;           // ML-KEM (true) or ML-DSA (false)
  int number;         // 512/768/1024 or 44/65/87
  uint16_t pub;       // ek / pk
  uint16_t priv;      // dk / sk (expanded)
  uint16_t seed;      // d||z (64) / xi (32)
  uint16_t extra;     // ciphertext (ML-KEM) or signature (ML-DSA)
  bool enabled;       // selected by the build profile
};

// nullptr for names other than the six ML-KEM / ML-DSA parameter sets.
const PqcParameterSet *pqcParameterSet(jshort_t name);

// KeyPair(PublicKey, PrivateKey).genKeyPair() and PQCKeyFactory share this.
void pqcGenerateKeyPair(jref_t publicKey, jref_t privateKey);
// KeyPair(PublicKey, PrivateKey): false unless both are PQC keys; throws
// CryptoException.ILLEGAL_VALUE for PQC keys that do not form a pair.
bool pqcCheckKeyPair(jref_t publicKey, jref_t privateKey);

// ML-DSA inside javacard.security.Signature (cipher algorithms
// SIG_CIPHER_ML_DSA* = 21..24, digest ALG_NULL, padding PAD_NULL).
constexpr uint8_t kSigCipherMlDsa = 21;
constexpr uint8_t kSigCipherMlDsaExternalMu = 22;
constexpr uint8_t kSigCipherMlDsaExpandSeed = 23;
constexpr uint8_t kSigCipherMlDsaExternalMuExpandSeed = 24;
inline bool isMlDsaCipher(uint8_t cipher) {
  return cipher >= kSigCipherMlDsa &&
         cipher <= kSigCipherMlDsaExternalMuExpandSeed;
}
// ctx == nullptr: init(Key, byte) (empty context).
void mldsaSignatureInit(JC_Instance &signature, jref_t key, uint8_t mode,
                        const std::shared_ptr<JC_Array> &ctx,
                        jshort_t ctxOffset, jshort_t ctxLength);
void mldsaSignatureUpdate(JC_Instance &signature,
                          const std::shared_ptr<JC_Array> &input,
                          jshort_t offset, jshort_t length);
jshort_t mldsaSignatureLength(JC_Instance &signature);
jshort_t mldsaSignatureSign(JC_Instance &signature,
                            const std::shared_ptr<JC_Array> &input,
                            jshort_t offset, jshort_t length,
                            const std::shared_ptr<JC_Array> &output,
                            jshort_t outputOffset);
jbool_t mldsaSignatureVerify(JC_Instance &signature,
                             const std::shared_ptr<JC_Array> &input,
                             jshort_t offset, jshort_t length,
                             const std::shared_ptr<JC_Array> &sig,
                             jshort_t sigOffset, jshort_t sigLength);

} // namespace jcvm
