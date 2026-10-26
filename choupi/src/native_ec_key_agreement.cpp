#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY
#include "mbedtls/ecp.h"
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH || JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC
#include "mbedtls/sha1.h"
#endif
#if !defined(PC_VERSION)
#include "pico/rand.h"
#endif
#endif

namespace jcvm {
namespace {
constexpr uint8_t kEcFpPrivate = 12;
constexpr uint8_t kEcFpPrivateTransientReset = 30;
constexpr uint8_t kEcFpPrivateTransientDeselect = 31;
constexpr uint8_t kAlgDh = 1;
constexpr uint8_t kAlgDhc = 2;
constexpr uint8_t kAlgDhPlain = 3;
constexpr uint8_t kAlgDhcPlain = 4;
constexpr uint8_t kAlgPaceGm = 5;
constexpr uint8_t kAlgDhPlainXy = 6;

bool supported(uint8_t algorithm) {
  switch (algorithm) {
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH
  case kAlgDh: return true;
#endif
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC
  case kAlgDhc: return true;
#endif
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN
  case kAlgDhPlain: return true;
#endif
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN
  case kAlgDhcPlain: return true;
#endif
#if JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM
  case kAlgPaceGm: return true;
#endif
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY
  case kAlgDhPlainXy: return true;
#endif
  default: return false;
  }
}

uint16_t classIndex() {
  const uint16_t token = Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
      .getExportedClassOffset(ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_KEYAGREEMENT);
  return HTONS(token);
}

std::shared_ptr<JC_Instance> instance(jref_t reference) {
  if (reference.isNullPointer() || reference.isArray())
    throw Exceptions::NullPointerException;
  auto value = Context::getActiveContext().getHeap().getInstance(reference);
  if (!value || value->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      value->getClassIndex() != classIndex())
    throw Exceptions::SecurityException;
  return value;
}

#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY
struct Group {
  mbedtls_ecp_group value;
  Group() { mbedtls_ecp_group_init(&value); }
  ~Group() { mbedtls_ecp_group_free(&value); }
};

std::vector<unsigned char> component(JC_Instance &key, uint16_t slot) {
  static constexpr uint16_t refs[] = {
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECFIELD,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECA,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECB,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECG,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECR,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECW,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECS};
  const auto keyRef = key.getField_Reference(refs[slot]);
  if (keyRef.isNullPointer())
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  auto array = Context::getActiveContext().getHeap().getArray(keyRef);
  const auto lengthsRef = key.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECLENGTHS);
  auto lengths = Context::getActiveContext().getHeap().getArray(lengthsRef);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE || !lengths ||
      lengths->getType() != JAVA_ARRAY_T_SHORT || lengths->size() <= slot)
    throw Exceptions::ArrayStoreException;
  const auto length = lengths->getShortEntry(slot);
  if (length <= 0 || static_cast<uint16_t>(length) > array->size())
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  std::vector<unsigned char> result(static_cast<size_t>(length));
  const uint16_t begin = static_cast<uint16_t>(array->size() - length);
  for (uint16_t i = 0; i < static_cast<uint16_t>(length); ++i)
    result[i] = static_cast<uint8_t>(array->getByteEntry(begin + i));
  return result;
}

mbedtls_ecp_group_id curveForBits(uint16_t bits) {
  switch (bits) {
  case 192: return MBEDTLS_ECP_DP_SECP192R1;
  case 224: return MBEDTLS_ECP_DP_SECP224R1;
  case 256: return MBEDTLS_ECP_DP_SECP256R1;
  case 384: return MBEDTLS_ECP_DP_SECP384R1;
  case 521: return MBEDTLS_ECP_DP_SECP521R1;
  default: throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
}

void loadGroup(JC_Instance &key, mbedtls_ecp_group &group, uint16_t bits,
               bool requireCofactor) {
  if (mbedtls_ecp_group_load(&group, curveForBits(bits)) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const auto p = component(key, 0);
  const auto a = component(key, 1);
  const auto b = component(key, 2);
  const auto g = component(key, 3);
  const auto n = component(key, 4);
  const uint16_t width = static_cast<uint16_t>((bits + 7) / 8);
  const size_t encodedPointLength = 1U + 2U * width;
  if (p.size() != width || a.size() != width || b.size() != width ||
      n.size() != width || g.size() != encodedPointLength || g[0] != 4)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  mbedtls_mpi supplied, expectedA, gx, gy;
  mbedtls_mpi_init(&supplied); mbedtls_mpi_init(&expectedA);
  mbedtls_mpi_init(&gx); mbedtls_mpi_init(&gy);
  int rc = mbedtls_mpi_read_binary(&supplied, p.data(), p.size());
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &group.P) != 0) rc = -1;
  if (rc == 0) rc = mbedtls_mpi_read_binary(&supplied, b.data(), b.size());
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &group.B) != 0) rc = -1;
  if (rc == 0) rc = mbedtls_mpi_read_binary(&supplied, n.data(), n.size());
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &group.N) != 0) rc = -1;
  if (rc == 0) rc = mbedtls_mpi_read_binary(&supplied, a.data(), a.size());
  if (rc == 0) rc = mbedtls_mpi_sub_int(&expectedA, &group.P, 3);
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &expectedA) != 0) rc = -1;
  if (rc == 0) rc = mbedtls_mpi_read_binary(&gx, g.data() + 1, width);
  if (rc == 0) rc = mbedtls_mpi_read_binary(&gy, g.data() + 1 + width, width);
  if (rc == 0 && (mbedtls_mpi_cmp_mpi(&gx, &group.G.MBEDTLS_PRIVATE(X)) != 0 ||
                  mbedtls_mpi_cmp_mpi(&gy, &group.G.MBEDTLS_PRIVATE(Y)) != 0)) rc = -1;
  mbedtls_mpi_free(&supplied); mbedtls_mpi_free(&expectedA);
  mbedtls_mpi_free(&gx); mbedtls_mpi_free(&gy);
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (requireCofactor && key.getField_Short(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECK) <= 0)
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
}

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

std::shared_ptr<JC_Array> inputArray(const std::shared_ptr<JC_Array> &array,
                                     jshort_t offset, jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 || static_cast<uint32_t>(offset) > array->size() ||
      static_cast<uint32_t>(length) > array->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
  return array;
}
#endif
} // namespace

jref_t javacard_security_KeyAgreement_getInstance(jbyte_t algorithm,
                                                   jbool_t externalAccess) {
  const uint8_t selected = static_cast<uint8_t>(algorithm);
  if (externalAccess || !supported(selected))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  Heap &heap = Context::getActiveContext().getHeap();
  const jref_t result = heap.addInstance(ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
                                        classIndex());
  auto agreement = heap.getInstance(result);
  if (!agreement) throw Exceptions::RuntimeException;
  agreement->setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_ALGORITHM,
      algorithm);
  agreement->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_PRIVATEKEY,
      jref_t());
  return result;
}

void javacard_security_KeyAgreement_init(jref_t receiver, jref_t privateKeyRef) {
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY
  auto agreement = instance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(agreement->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_ALGORITHM));
  if (!supported(algorithm)) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  if (privateKeyRef.isNullPointer() || privateKeyRef.isArray())
    throw Exceptions::NullPointerException;
  auto key = Context::getActiveContext().getHeap().getInstance(privateKeyRef);
  if (!key || key->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint8_t keyType = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  if (keyType != kEcFpPrivate && keyType != kEcFpPrivateTransientReset &&
      keyType != kEcFpPrivateTransientDeselect)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
  Group group;
  const bool needsCofactor = algorithm == kAlgDhc || algorithm == kAlgDhcPlain;
  loadGroup(*key, group.value, bits, needsCofactor);
  const auto scalar = component(*key, 6);
  mbedtls_mpi d;
  mbedtls_mpi_init(&d);
  int rc = mbedtls_mpi_read_binary(&d, scalar.data(), scalar.size());
  if (rc == 0) rc = mbedtls_ecp_check_privkey(&group.value, &d);
  mbedtls_mpi_free(&d);
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  agreement->setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_PRIVATEKEY,
      privateKeyRef);
#else
  (void)receiver; (void)privateKeyRef;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

jbyte_t javacard_security_KeyAgreement_getAlgorithm(jref_t receiver) {
  return instance(receiver)->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_ALGORITHM);
}

jshort_t javacard_security_KeyAgreement_generateSecret(
    jref_t receiver, std::shared_ptr<JC_Array> publicData, jshort_t publicOffset,
    jshort_t publicLength, std::shared_ptr<JC_Array> secret, jshort_t secretOffset) {
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN || \
    JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM || \
    JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY
  auto agreement = instance(receiver);
  const uint8_t algorithm = static_cast<uint8_t>(agreement->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_ALGORITHM));
  if (!supported(algorithm)) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const jref_t keyRef = agreement->getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_KEYAGREEMENT_KEYAGREEMENT_PRIVATEKEY);
  if (keyRef.isNullPointer()) throwCryptoException(CryptoExceptionReason::InvalidInit);
  auto key = Context::getActiveContext().getHeap().getInstance(keyRef);
  if (!key) throwCryptoException(CryptoExceptionReason::InvalidInit);
  inputArray(publicData, publicOffset, publicLength);
  if (!secret) throw Exceptions::NullPointerException;
  if (secret->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  if (secretOffset < 0 || static_cast<uint32_t>(secretOffset) > secret->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
  const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
  const uint16_t width = static_cast<uint16_t>((bits + 7) / 8);
  Group group;
  const bool needsCofactor = algorithm == kAlgDhc || algorithm == kAlgDhcPlain;
  loadGroup(*key, group.value, bits, needsCofactor);
  const auto scalarBytes = component(*key, 6);
  mbedtls_mpi scalar;
  mbedtls_mpi_init(&scalar);
  int rc = mbedtls_mpi_read_binary(&scalar, scalarBytes.data(), scalarBytes.size());
  if (rc != 0) { mbedtls_mpi_free(&scalar); throwCryptoException(CryptoExceptionReason::IllegalValue); }
  mbedtls_ecp_point peer, result;
  mbedtls_ecp_point_init(&peer); mbedtls_ecp_point_init(&result);
  auto publicBytes = std::vector<unsigned char>(static_cast<size_t>(publicLength));
  for (jshort_t i = 0; i < publicLength; ++i)
    publicBytes[static_cast<size_t>(i)] = static_cast<uint8_t>(
        publicData->getByteEntry(static_cast<uint16_t>(publicOffset + i)));
  if (publicLength != static_cast<jshort_t>(1 + 2 * width) || publicBytes[0] != 4)
    rc = -1;
  if (rc == 0) rc = mbedtls_ecp_point_read_binary(
      &group.value, &peer, publicBytes.data(), publicBytes.size());
  if (rc == 0) rc = mbedtls_ecp_check_pubkey(&group.value, &peer);
  if (rc == 0 && algorithm == kAlgPaceGm) {
    mbedtls_mpi one;
    mbedtls_mpi_init(&one);
    rc = mbedtls_mpi_lset(&one, 1);
    if (rc == 0)
      rc = mbedtls_ecp_muladd(&group.value, &result, &scalar,
                              &group.value.G, &one, &peer);
    mbedtls_mpi_free(&one);
  } else if (rc == 0) {
    if (algorithm == kAlgDhc || algorithm == kAlgDhcPlain) {
      const auto cofactor = key->getField_Short(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECK);
      if (cofactor <= 0) rc = -1;
      else {
        mbedtls_mpi product;
        mbedtls_mpi_init(&product);
        rc = mbedtls_mpi_mul_int(&product, &scalar, static_cast<uint32_t>(cofactor));
        if (rc == 0) rc = mbedtls_mpi_mod_mpi(&scalar, &product, &group.value.N);
        mbedtls_mpi_free(&product);
      }
    }
    if (rc == 0) rc = mbedtls_ecp_mul(&group.value, &result, &scalar, &peer,
                                       randomBytes, nullptr);
  }
  mbedtls_mpi_free(&scalar);
  mbedtls_ecp_point_free(&peer);
  if (rc != 0) { mbedtls_ecp_point_free(&result); throwCryptoException(CryptoExceptionReason::IllegalValue); }
  std::vector<unsigned char> output;
  if (algorithm == kAlgDh || algorithm == kAlgDhc) {
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH || JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC
    output.resize(20);
    std::vector<unsigned char> x(width);
    rc = mbedtls_mpi_write_binary(&result.MBEDTLS_PRIVATE(X), x.data(), width);
    if (rc == 0) rc = mbedtls_sha1(x.data(), x.size(), output.data());
#else
    rc = -1;
#endif
  } else if (algorithm == kAlgDhPlain || algorithm == kAlgDhcPlain) {
#if JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN
    if (algorithm == kAlgDhcPlain) {
#if !JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN
      rc = -1;
#endif
    }
    output.resize(width);
    if (rc == 0)
      rc = mbedtls_mpi_write_binary(&result.MBEDTLS_PRIVATE(X), output.data(), width);
#else
    rc = -1;
#endif
  } else {
    size_t written = 0;
    const size_t capacity = algorithm == kAlgDhPlainXy ? 1 + 2 * width : 1 + 2 * width;
    output.resize(capacity);
    rc = mbedtls_ecp_point_write_binary(&group.value, &result,
        MBEDTLS_ECP_PF_UNCOMPRESSED, &written, output.data(), output.size());
    output.resize(written);
  }
  mbedtls_ecp_point_free(&result);
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (output.size() > secret->size() - static_cast<uint32_t>(secretOffset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
  for (size_t i = 0; i < output.size(); ++i)
    secret->setByteEntry(static_cast<uint16_t>(secretOffset + i), output[i]);
  return static_cast<jshort_t>(output.size());
#else
  (void)receiver; (void)publicData; (void)publicOffset; (void)publicLength;
  (void)secret; (void)secretOffset;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

} // namespace jcvm
