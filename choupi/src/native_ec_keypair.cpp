#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_ec_group.hpp"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#if JC_SUPPORT_KEYPAIR_EC_FP
#include "mbedtls/ecp.h"
#endif
#if JC_SUPPORT_KEYPAIR_RSA_ANY
#include "mbedtls/bignum.h"
#endif
#if JC_SUPPORT_KEYPAIR_EC_FP || JC_SUPPORT_KEYPAIR_RSA_ANY
#if !defined(PC_VERSION)
#include "pico/rand.h"
#endif
#endif

namespace jcvm {
jref_t javacard_security_KeyBuilder_buildKey(jbyte_t, jshort_t, jbool_t);

namespace {
constexpr uint8_t kAlgorithmEcFp = 5;
constexpr uint8_t kAlgorithmRsa = 1;
constexpr uint8_t kAlgorithmRsaCrt = 2;
constexpr uint8_t kTypeEcFpPublic = 11;
constexpr uint8_t kTypeEcFpPrivate = 12;
constexpr uint8_t kTypeRsaPublic = 4;
constexpr uint8_t kTypeRsaPrivate = 5;
constexpr uint8_t kTypeRsaCrtPrivate = 6;

uint16_t keyPairClassIndex() {
  const uint16_t token = Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
      .getExportedClassOffset(ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_KEYPAIR);
  return HTONS(token);
}

std::shared_ptr<JC_Instance> keyPairInstance(jref_t reference) {
  if (reference.isNullPointer() || reference.isArray())
    throw Exceptions::NullPointerException;
  auto value = Context::getActiveContext().getHeap().getInstance(reference);
  if (!value || value->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      value->getClassIndex() != keyPairClassIndex())
    throw Exceptions::SecurityException;
  return value;
}

#if JC_SUPPORT_KEYPAIR_EC_FP || JC_SUPPORT_KEYPAIR_RSA_ANY
int randomBytes(void *, unsigned char *output, size_t length) {
#ifdef PC_VERSION
  static std::random_device source;
  for (size_t i = 0; i < length; ++i)
    output[i] = static_cast<unsigned char>(source());
#else
  uint32_t word = 0;
  for (size_t i = 0; i < length; ++i) {
    if ((i & 3U) == 0) word = get_rand_32();
    output[i] = static_cast<unsigned char>(word >> ((i & 3U) * 8));
  }
#endif
  return 0;
}
#endif

#if JC_SUPPORT_KEYPAIR_RSA_ANY
bool rsaKeyPairEnabled(uint8_t algorithm, uint16_t bits) {
#define RSA_PAIR_CASE(length) \
  case length: \
    return algorithm == kAlgorithmRsa ? JC_SUPPORT_KEYPAIR_RSA_##length \
                                     : JC_SUPPORT_KEYPAIR_RSA_CRT_##length
  switch (bits) {
    RSA_PAIR_CASE(512); RSA_PAIR_CASE(736); RSA_PAIR_CASE(768);
    RSA_PAIR_CASE(896); RSA_PAIR_CASE(1024); RSA_PAIR_CASE(1280);
    RSA_PAIR_CASE(1536); RSA_PAIR_CASE(1984); RSA_PAIR_CASE(2048);
    RSA_PAIR_CASE(3072); RSA_PAIR_CASE(4096);
    default: return false;
  }
#undef RSA_PAIR_CASE
}

constexpr uint16_t kRsaComponentFields[] = {
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_MODULUS,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_EXPONENT,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_P,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_Q,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DP,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_DQ,
    ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PQ};

std::vector<unsigned char> rsaMpiBytes(const mbedtls_mpi &value, size_t width) {
  std::vector<unsigned char> bytes(width);
  if (mbedtls_mpi_write_binary(&value, bytes.data(), bytes.size()) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return bytes;
}

void putRsaComponent(const std::shared_ptr<JC_Instance> &key, uint16_t component,
                     const std::vector<unsigned char> &bytes) {
  auto &heap = Context::getActiveContext().getHeap();
  auto array = heap.getArray(key->getField_Reference(kRsaComponentFields[component]));
  auto lengths = heap.getArray(key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_LENGTHS));
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE || !lengths ||
      lengths->getType() != JAVA_ARRAY_T_SHORT || lengths->size() <= component ||
      bytes.empty() || bytes.size() > array->size())
    throw Exceptions::ArrayStoreException;
  for (uint16_t i = 0; i < array->size(); ++i) array->setByteEntry(i, 0);
  const uint16_t start = static_cast<uint16_t>(array->size() - bytes.size());
  for (size_t i = 0; i < bytes.size(); ++i)
    array->setByteEntry(static_cast<uint16_t>(start + i), bytes[i]);
  lengths->setShortEntry(component, static_cast<jshort_t>(bytes.size()));
}

struct ScopedRsaValues {
  mbedtls_mpi p, q, n, e, d, p1, q1, phi, gcd, dp, dq, pq, temp;
  ScopedRsaValues() {
    mbedtls_mpi_init(&p); mbedtls_mpi_init(&q); mbedtls_mpi_init(&n);
    mbedtls_mpi_init(&e); mbedtls_mpi_init(&d); mbedtls_mpi_init(&p1);
    mbedtls_mpi_init(&q1); mbedtls_mpi_init(&phi); mbedtls_mpi_init(&gcd);
    mbedtls_mpi_init(&dp); mbedtls_mpi_init(&dq); mbedtls_mpi_init(&pq);
    mbedtls_mpi_init(&temp);
  }
  ~ScopedRsaValues() {
    mbedtls_mpi_free(&p); mbedtls_mpi_free(&q); mbedtls_mpi_free(&n);
    mbedtls_mpi_free(&e); mbedtls_mpi_free(&d); mbedtls_mpi_free(&p1);
    mbedtls_mpi_free(&q1); mbedtls_mpi_free(&phi); mbedtls_mpi_free(&gcd);
    mbedtls_mpi_free(&dp); mbedtls_mpi_free(&dq); mbedtls_mpi_free(&pq);
    mbedtls_mpi_free(&temp);
  }
};

void generateRsaPair(jref_t publicRef, jref_t privateRef, uint16_t bits) {
  ScopedRsaValues values;
  if (mbedtls_mpi_lset(&values.e, 65537) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  bool ready = false;
  for (unsigned attempt = 0; attempt < 32 && !ready; ++attempt) {
    const size_t pBits = (bits + 1U) / 2U;
    const size_t qBits = bits - pBits;
    if (mbedtls_mpi_gen_prime(&values.p, pBits, 0, randomBytes, nullptr) != 0 ||
        mbedtls_mpi_gen_prime(&values.q, qBits, 0, randomBytes, nullptr) != 0 ||
        mbedtls_mpi_mul_mpi(&values.n, &values.p, &values.q) != 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    if (mbedtls_mpi_bitlen(&values.n) != bits ||
        mbedtls_mpi_cmp_mpi(&values.p, &values.q) == 0)
      continue;
    if (mbedtls_mpi_sub_int(&values.p1, &values.p, 1) != 0 ||
        mbedtls_mpi_sub_int(&values.q1, &values.q, 1) != 0 ||
        mbedtls_mpi_gcd(&values.gcd, &values.e, &values.p1) != 0 ||
        mbedtls_mpi_cmp_int(&values.gcd, 1) != 0 ||
        mbedtls_mpi_gcd(&values.gcd, &values.e, &values.q1) != 0 ||
        mbedtls_mpi_cmp_int(&values.gcd, 1) != 0 ||
        mbedtls_mpi_mul_mpi(&values.phi, &values.p1, &values.q1) != 0)
      continue;
    if (mbedtls_mpi_inv_mod(&values.d, &values.e, &values.phi) != 0 ||
        mbedtls_mpi_mod_mpi(&values.dp, &values.d, &values.p1) != 0 ||
        mbedtls_mpi_mod_mpi(&values.dq, &values.d, &values.q1) != 0 ||
        mbedtls_mpi_inv_mod(&values.pq, &values.q, &values.p) != 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    ready = true;
  }
  if (!ready) throwCryptoException(CryptoExceptionReason::IllegalUse);
  const size_t width = (bits + 7U) / 8U;
  auto publicKey = Context::getActiveContext().getHeap().getInstance(publicRef);
  auto privateKey = Context::getActiveContext().getHeap().getInstance(privateRef);
  if (!publicKey || !privateKey) throw Exceptions::RuntimeException;
  putRsaComponent(publicKey, 0, rsaMpiBytes(values.n, width));
  putRsaComponent(publicKey, 1, rsaMpiBytes(values.e, width));
  if (static_cast<uint8_t>(privateKey->getField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE)) == kTypeRsaPrivate) {
    putRsaComponent(privateKey, 0, rsaMpiBytes(values.n, width));
    putRsaComponent(privateKey, 1, rsaMpiBytes(values.d, width));
  } else {
    putRsaComponent(privateKey, 2, rsaMpiBytes(values.p, (width + 1U) / 2U));
    putRsaComponent(privateKey, 3, rsaMpiBytes(values.q, (width + 1U) / 2U));
    putRsaComponent(privateKey, 4, rsaMpiBytes(values.dp, (width + 1U) / 2U));
    putRsaComponent(privateKey, 5, rsaMpiBytes(values.dq, (width + 1U) / 2U));
    putRsaComponent(privateKey, 6, rsaMpiBytes(values.pq, (width + 1U) / 2U));
  }
  publicKey->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED, 1);
  privateKey->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_PERSISTENTINITIALIZED, 1);
}
#endif

#if JC_SUPPORT_KEYPAIR_EC_FP
struct ScopedGroup {
  mbedtls_ecp_group value;
  ScopedGroup() { mbedtls_ecp_group_init(&value); }
  ~ScopedGroup() { mbedtls_ecp_group_free(&value); }
};

struct ScopedKeyPair {
  mbedtls_mpi privateValue;
  mbedtls_ecp_point publicValue;
  ScopedKeyPair() {
    mbedtls_mpi_init(&privateValue);
    mbedtls_ecp_point_init(&publicValue);
  }
  ~ScopedKeyPair() {
    mbedtls_mpi_free(&privateValue);
    mbedtls_ecp_point_free(&publicValue);
  }
};

void putComponent(const std::shared_ptr<JC_Instance> &key, uint16_t index,
                  const std::vector<unsigned char> &bytes) {
  static constexpr uint16_t componentFields[] = {
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECFIELD,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECA,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECB,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECG,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECR,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECW,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECS};
  auto &heap = Context::getActiveContext().getHeap();
  const jref_t arrayRef = key->getField_Reference(componentFields[index]);
  const jref_t lengthsRef = key->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECLENGTHS);
  auto array = heap.getArray(arrayRef);
  auto lengths = heap.getArray(lengthsRef);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE || !lengths ||
      lengths->getType() != JAVA_ARRAY_T_SHORT || lengths->size() <= index ||
      bytes.size() > array->size())
    throw Exceptions::ArrayStoreException;
  for (uint16_t i = 0; i < array->size(); ++i) array->setByteEntry(i, 0);
  const uint16_t start = static_cast<uint16_t>(array->size() - bytes.size());
  for (size_t i = 0; i < bytes.size(); ++i)
    array->setByteEntry(static_cast<uint16_t>(start + i), bytes[i]);
  lengths->setShortEntry(index, static_cast<jshort_t>(bytes.size()));
}

std::vector<unsigned char> mpiBytes(const mbedtls_mpi &value, size_t width) {
  std::vector<unsigned char> output(width);
  if (mbedtls_mpi_write_binary(&value, output.data(), output.size()) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return output;
}

std::vector<unsigned char> pointBytes(const mbedtls_ecp_group &group,
                                      const mbedtls_ecp_point &point,
                                      size_t width) {
  std::vector<unsigned char> output(1 + 2 * width);
  size_t written = 0;
  if (mbedtls_ecp_point_write_binary(&group, &point,
          MBEDTLS_ECP_PF_UNCOMPRESSED, &written, output.data(), output.size()) != 0 ||
      written != output.size())
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  return output;
}

void generateEcPair(jref_t publicRef, jref_t privateRef, uint16_t bits) {
  ScopedGroup group;
  if (native_ec::loadGroupForBits(group.value, bits) != 0)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  ScopedKeyPair pair;
  if (mbedtls_ecp_gen_keypair(&group.value, &pair.privateValue,
          &pair.publicValue, randomBytes, nullptr) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);

  const size_t width = (bits + 7U) / 8U;
  const auto prime = mpiBytes(group.value.P, width);
  mbedtls_mpi curveA;
  mbedtls_mpi_init(&curveA);
  const int aResult = mbedtls_mpi_sub_int(&curveA, &group.value.P, 3);
  if (aResult != 0) {
    mbedtls_mpi_free(&curveA);
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  }
  const auto a = mpiBytes(curveA, width);
  mbedtls_mpi_free(&curveA);
  const auto b = mpiBytes(group.value.B, width);
  const auto g = pointBytes(group.value, group.value.G, width);
  const size_t orderWidth = (mbedtls_mpi_bitlen(&group.value.N) + 7U) / 8U;
  const auto order = mpiBytes(group.value.N, orderWidth);
  const auto w = pointBytes(group.value, pair.publicValue, width);
  const auto s = mpiBytes(pair.privateValue, width);

  auto &heap = Context::getActiveContext().getHeap();
  auto publicKey = heap.getInstance(publicRef);
  auto privateKey = heap.getInstance(privateRef);
  if (!publicKey || !privateKey) throw Exceptions::RuntimeException;
  putComponent(publicKey, 0, prime);
  putComponent(publicKey, 1, a);
  putComponent(publicKey, 2, b);
  putComponent(publicKey, 3, g);
  putComponent(publicKey, 4, order);
  putComponent(publicKey, 5, w);
  putComponent(privateKey, 0, prime);
  putComponent(privateKey, 1, a);
  putComponent(privateKey, 2, b);
  putComponent(privateKey, 3, g);
  putComponent(privateKey, 4, order);
  putComponent(privateKey, 6, s);
  publicKey->setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECK, 1);
  privateKey->setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECK, 1);
}
#endif
} // namespace

void javacard_security_KeyPair_init(jref_t receiver, jbyte_t algorithm,
                                   jshort_t keyLength) {
#if JC_SUPPORT_KEYPAIR_EC_FP || JC_SUPPORT_KEYPAIR_RSA_ANY
  if (keyLength <= 0)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint16_t bits = static_cast<uint16_t>(keyLength);
  const uint8_t alg = static_cast<uint8_t>(algorithm);
  uint8_t publicType = 0;
  uint8_t privateType = 0;
  if (alg == kAlgorithmEcFp) {
#if JC_SUPPORT_KEYPAIR_EC_FP
    if (!native_ec::supportsBits(bits))
      throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
    publicType = kTypeEcFpPublic;
    privateType = kTypeEcFpPrivate;
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  } else if (alg == kAlgorithmRsa || alg == kAlgorithmRsaCrt) {
#if JC_SUPPORT_KEYPAIR_RSA_ANY
    if (!rsaKeyPairEnabled(alg, bits))
      throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
    publicType = kTypeRsaPublic;
    privateType = alg == kAlgorithmRsa ? kTypeRsaPrivate : kTypeRsaCrtPrivate;
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  } else {
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
  const jref_t publicKey = javacard_security_KeyBuilder_buildKey(
      static_cast<jbyte_t>(publicType), keyLength, FALSE);
  const jref_t privateKey = javacard_security_KeyBuilder_buildKey(
      static_cast<jbyte_t>(privateType), keyLength, FALSE);
  auto pair = keyPairInstance(receiver);
  pair->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_ALGORITHM, algorithm);
  pair->setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_KEYLENGTH, keyLength);
  pair->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PUBLICKEY, publicKey);
  pair->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PRIVATEKEY, privateKey);
#else
  (void)receiver; (void)algorithm; (void)keyLength;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

void javacard_security_KeyPair_genKeyPair(jref_t receiver) {
#if JC_SUPPORT_KEYPAIR_EC_FP || JC_SUPPORT_KEYPAIR_RSA_ANY
  auto pair = keyPairInstance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(pair->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_ALGORITHM));
  const uint16_t bits = static_cast<uint16_t>(pair->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_KEYLENGTH));
  const jref_t publicRef = pair->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PUBLICKEY);
  const jref_t privateRef = pair->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PRIVATEKEY);
  if (publicRef.isNullPointer() || privateRef.isNullPointer())
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  if (algorithm == kAlgorithmEcFp) {
#if JC_SUPPORT_KEYPAIR_EC_FP
    generateEcPair(publicRef, privateRef, bits);
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  } else if (algorithm == kAlgorithmRsa || algorithm == kAlgorithmRsaCrt) {
#if JC_SUPPORT_KEYPAIR_RSA_ANY
    if (!rsaKeyPairEnabled(algorithm, bits))
      throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
    generateRsaPair(publicRef, privateRef, bits);
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  } else {
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
#else
  (void)receiver;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

jref_t javacard_security_KeyPair_getPublic(jref_t receiver) {
  // The imported Java Card 3.0.5 KeyPair method-token table maps getPublic
  // and getPrivate to the opposite generated native selectors. Keep their
  // public API behavior aligned with the CAP method tokens here.
  return keyPairInstance(receiver)->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PRIVATEKEY);
}

jref_t javacard_security_KeyPair_getPrivate(jref_t receiver) {
  return keyPairInstance(receiver)->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PUBLICKEY);
}

} // namespace jcvm
