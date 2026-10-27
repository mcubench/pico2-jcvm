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
#if !defined(PC_VERSION)
#include "pico/rand.h"
#endif
#endif

namespace jcvm {
jref_t javacard_security_KeyBuilder_buildKey(jbyte_t, jshort_t, jbool_t);

namespace {
constexpr uint8_t kAlgorithmEcFp = 5;
constexpr uint8_t kTypeEcFpPublic = 11;
constexpr uint8_t kTypeEcFpPrivate = 12;

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
#if JC_SUPPORT_KEYPAIR_EC_FP
  if (static_cast<uint8_t>(algorithm) != kAlgorithmEcFp || keyLength <= 0)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint16_t bits = static_cast<uint16_t>(keyLength);
  if (!native_ec::supportsBits(bits))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const jref_t publicKey = javacard_security_KeyBuilder_buildKey(
      static_cast<jbyte_t>(kTypeEcFpPublic), keyLength, FALSE);
  const jref_t privateKey = javacard_security_KeyBuilder_buildKey(
      static_cast<jbyte_t>(kTypeEcFpPrivate), keyLength, FALSE);
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
#if JC_SUPPORT_KEYPAIR_EC_FP
  auto pair = keyPairInstance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(pair->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_ALGORITHM));
  if (algorithm != kAlgorithmEcFp)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const uint16_t bits = static_cast<uint16_t>(pair->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_KEYLENGTH));
  const jref_t publicRef = pair->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PUBLICKEY);
  const jref_t privateRef = pair->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYPAIR_KEYPAIR_PRIVATEKEY);
  if (publicRef.isNullPointer() || privateRef.isNullPointer())
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  generateEcPair(publicRef, privateRef, bits);
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
