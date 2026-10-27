#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"

#include "mbedtls/ecdsa.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#if !defined(PC_VERSION)
#include "pico/rand.h"
#endif

#if JC_SUPPORT_SIGNATURE_ECDSA_SHA || JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 || JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 || \
    JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
namespace jcvm {
namespace {
constexpr uint8_t kSign = 1;
constexpr uint8_t kVerify = 2;
constexpr uint8_t kEcFpPublic = 11;
constexpr uint8_t kEcFpPrivate = 12;

struct ScopedGroup {
  mbedtls_ecp_group value;
  ScopedGroup() { mbedtls_ecp_group_init(&value); }
  ~ScopedGroup() { mbedtls_ecp_group_free(&value); }
};

struct ScopedEcdsa {
  mbedtls_ecdsa_context value;
  ScopedEcdsa() { mbedtls_ecdsa_init(&value); }
  ~ScopedEcdsa() { mbedtls_ecdsa_free(&value); }
};

std::shared_ptr<JC_Instance> object(jref_t ref) {
  if (ref.isNullPointer() || ref.isArray()) throw Exceptions::NullPointerException;
  auto value = Context::getActiveContext().getHeap().getInstance(ref);
  if (!value || value->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  return value;
}

std::vector<unsigned char> component(JC_Instance &key, uint16_t index) {
  static constexpr uint16_t fields[] = {
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECFIELD,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECA,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECB,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECG,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECR,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECW,
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECS};
  if (index >= 7) throwCryptoException(CryptoExceptionReason::IllegalValue);
  const jref_t ref = key.getField_Reference(fields[index]);
  if (ref.isNullPointer()) throwCryptoException(CryptoExceptionReason::UninitializedKey);
  auto data = Context::getActiveContext().getHeap().getArray(ref);
  const jref_t lengthsRef = key.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECLENGTHS);
  if (!data || data->getType() != JAVA_ARRAY_T_BYTE || lengthsRef.isNullPointer())
    throw Exceptions::ArrayStoreException;
  auto lengths = Context::getActiveContext().getHeap().getArray(lengthsRef);
  if (!lengths || lengths->getType() != JAVA_ARRAY_T_SHORT || lengths->size() <= index)
    throw Exceptions::ArrayStoreException;
  const jshort_t length = lengths->getShortEntry(index);
  if (length <= 0 || static_cast<uint16_t>(length) > data->size())
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  std::vector<unsigned char> bytes(static_cast<size_t>(length));
  const uint16_t start = static_cast<uint16_t>(data->size() - length);
  for (uint16_t i = 0; i < static_cast<uint16_t>(length); ++i)
    bytes[i] = static_cast<uint8_t>(data->getByteEntry(start + i));
  return bytes;
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

int readMpi(mbedtls_mpi &out, const std::vector<unsigned char> &input) {
  return mbedtls_mpi_read_binary(&out, input.data(), input.size());
}

void loadAndValidateGroup(JC_Instance &key, mbedtls_ecp_group &group,
                          uint16_t bits) {
  if (mbedtls_ecp_group_load(&group, curveForBits(bits)) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  const auto p = component(key, 0);
  const auto a = component(key, 1);
  const auto b = component(key, 2);
  const auto g = component(key, 3);
  const auto n = component(key, 4);
  const uint16_t width = static_cast<uint16_t>((bits + 7) / 8);
  if (p.size() != width || b.size() > width || n.size() > width ||
      g.size() != static_cast<size_t>(1 + 2 * width) || g[0] != 4)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  mbedtls_mpi supplied;
  mbedtls_mpi_init(&supplied);
  int rc = readMpi(supplied, p);
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &group.P) != 0) rc = -1;
  if (rc == 0) rc = readMpi(supplied, b);
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &group.B) != 0) rc = -1;
  if (rc == 0) rc = readMpi(supplied, n);
  if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &group.N) != 0) rc = -1;
  if (rc == 0) rc = readMpi(supplied, a);
  if (rc == 0) {
    mbedtls_mpi expectedA;
    mbedtls_mpi_init(&expectedA);
    rc = mbedtls_mpi_sub_int(&expectedA, &group.P, 3);
    if (rc == 0 && mbedtls_mpi_cmp_mpi(&supplied, &expectedA) != 0) rc = -1;
    mbedtls_mpi_free(&expectedA);
  }
  mbedtls_mpi_free(&supplied);
  if (rc != 0 || key.getField_Short(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_ECK) != 1)
  {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  mbedtls_mpi gx, gy;
  mbedtls_mpi_init(&gx); mbedtls_mpi_init(&gy);
  rc = mbedtls_mpi_read_binary(&gx, g.data() + 1, width);
  if (rc == 0) rc = mbedtls_mpi_read_binary(&gy, g.data() + 1 + width, width);
  if (rc == 0 && (mbedtls_mpi_cmp_mpi(&gx, &group.G.MBEDTLS_PRIVATE(X)) != 0 ||
                  mbedtls_mpi_cmp_mpi(&gy, &group.G.MBEDTLS_PRIVATE(Y)) != 0)) rc = -1;
  mbedtls_mpi_free(&gx); mbedtls_mpi_free(&gy);
  if (rc != 0) {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
}

int randomBytes(void *, unsigned char *output, size_t length) {
#ifdef PC_VERSION
  static std::random_device source;
  for (size_t i = 0; i < length; ++i) output[i] = static_cast<unsigned char>(source());
#else
  uint32_t word = 0;
  for (size_t i = 0; i < length; ++i) {
    if ((i & 3U) == 0) word = get_rand_32();
    output[i] = static_cast<unsigned char>(word >> ((i & 3U) * 8));
  }
#endif
  return 0;
}

std::vector<unsigned char> digest(uint8_t algorithm,
                                 const std::vector<unsigned char> &message) {
  size_t length = 0;
  switch (algorithm) {
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA
  case 1: length = 20; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_224
  case 7: length = 28; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_256
  case 4: length = 32; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_384
  case 5: length = 48; break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  case 6: length = 64; break;
#endif
  default: throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
  std::vector<unsigned char> out(length);
  int rc = -1;
  switch (algorithm) {
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA
  case 1: rc = mbedtls_sha1(message.data(), message.size(), out.data()); break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_224
  case 7: rc = mbedtls_sha256(message.data(), message.size(), out.data(), 1); break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_256
  case 4: rc = mbedtls_sha256(message.data(), message.size(), out.data(), 0); break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_384
  case 5: rc = mbedtls_sha512(message.data(), message.size(), out.data(), 1); break;
#endif
#if JC_SUPPORT_SIGNATURE_ECDSA_SHA_512
  case 6: rc = mbedtls_sha512(message.data(), message.size(), out.data(), 0); break;
#endif
  }
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  return out;
}

std::vector<unsigned char> readMessage(JC_Instance &signature,
                                       const std::shared_ptr<JC_Array> &input,
                                       jshort_t offset, jshort_t length) {
  if (!input) throw Exceptions::NullPointerException;
  if (input->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 || static_cast<uint32_t>(offset) > input->size() ||
      static_cast<uint32_t>(length) > input->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
  const uint16_t bufferField = ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER;
  const uint16_t countField = ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH;
  const jref_t bufferRef = signature.getField_Reference(bufferField);
  auto buffer = Context::getActiveContext().getHeap().getArray(bufferRef);
  if (!buffer || buffer->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  const jshort_t oldLength = signature.getField_Short(countField);
  if (oldLength < 0 || static_cast<uint32_t>(oldLength) + length > 32767)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const uint16_t total = static_cast<uint16_t>(oldLength + length);
  if (total > buffer->size()) {
    uint32_t capacity = std::max<uint16_t>(64, buffer->size());
    while (capacity < total) capacity = std::min<uint32_t>(32767, capacity * 2);
    const jref_t expandedRef = Context::getActiveContext().getHeap().addArray(
        static_cast<uint16_t>(capacity), JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET);
    auto expanded = Context::getActiveContext().getHeap().getArray(expandedRef);
    for (uint16_t i = 0; i < static_cast<uint16_t>(oldLength); ++i)
      expanded->setByteEntry(i, buffer->getByteEntry(i));
    buffer = expanded;
    signature.setField_Reference(bufferField, expandedRef);
  }
  for (jshort_t i = 0; i < length; ++i)
    buffer->setByteEntry(static_cast<uint16_t>(oldLength + i),
                         input->getByteEntry(static_cast<uint16_t>(offset + i)));
  signature.setField_Short(countField, static_cast<jshort_t>(total));
  std::vector<unsigned char> message(total);
  for (uint16_t i = 0; i < total; ++i)
    message[i] = static_cast<uint8_t>(buffer->getByteEntry(i));
  for (uint16_t i = 0; i < total; ++i) buffer->setByteEntry(i, 0);
  signature.setField_Short(countField, 0);
  return message;
}

size_t derLengthBytes(size_t length) { return length < 128 ? 1 : 2; }
void appendLength(std::vector<unsigned char> &out, size_t length) {
  if (length < 128) out.push_back(static_cast<unsigned char>(length));
  else { out.push_back(0x81); out.push_back(static_cast<unsigned char>(length)); }
}
std::vector<unsigned char> encodeDer(mbedtls_mpi &r, mbedtls_mpi &s) {
  auto integer = [](mbedtls_mpi &value) {
    const size_t n = mbedtls_mpi_size(&value);
    std::vector<unsigned char> encoded(n);
    if (mbedtls_mpi_write_binary(&value, encoded.data(), n) != 0)
      throwCryptoException(CryptoExceptionReason::IllegalUse);
    if (encoded.empty()) encoded.push_back(0);
    if (encoded[0] & 0x80) encoded.insert(encoded.begin(), 0);
    std::vector<unsigned char> result{0x02};
    appendLength(result, encoded.size());
    result.insert(result.end(), encoded.begin(), encoded.end());
    return result;
  };
  auto ri = integer(r), si = integer(s);
  std::vector<unsigned char> body;
  body.insert(body.end(), ri.begin(), ri.end()); body.insert(body.end(), si.begin(), si.end());
  std::vector<unsigned char> result{0x30};
  appendLength(result, body.size()); result.insert(result.end(), body.begin(), body.end());
  return result;
}

bool decodeDer(const unsigned char *data, size_t length, mbedtls_mpi &r,
               mbedtls_mpi &s) {
  if (!data || length < 8 || data[0] != 0x30) return false;
  size_t pos = 2, sequenceLength = data[1];
  if (sequenceLength & 0x80) {
    const size_t count = sequenceLength & 0x7f;
    if (count != 1 || length < 3) return false;
    sequenceLength = data[2]; pos = 3;
  }
  if (sequenceLength != length - pos) return false;
  auto readInt = [&](mbedtls_mpi &target) {
    if (pos + 2 > length || data[pos++] != 0x02) return false;
    size_t n = data[pos++];
    if (n & 0x80) {
      const size_t count = n & 0x7f;
      if (count != 1 || pos >= length) return false;
      n = data[pos++];
    }
    if (n == 0 || n > length - pos || (data[pos] & 0x80) != 0) return false;
    if (n > 1 && data[pos] == 0 && !(data[pos + 1] & 0x80)) return false;
    const unsigned char *bytes = data + pos;
    if (bytes[0] == 0) { ++bytes; --n; }
    pos += (data[pos] == 0 && n > 0) ? n + 1 : n;
    return mbedtls_mpi_read_binary(&target, bytes, n) == 0;
  };
  return readInt(r) && readInt(s) && pos == length;
}

} // namespace

void initializeEcdsaSignature(JC_Instance &signature, jref_t keyRef,
                               uint8_t mode) {
  if (mode != kSign && mode != kVerify)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  auto key = object(keyRef);
  const uint8_t type = static_cast<uint8_t>(key->getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  const bool signing = type == kEcFpPrivate;
  if ((mode == kSign && !signing) || (mode == kVerify && type != kEcFpPublic)) {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
  ScopedGroup group;
  loadAndValidateGroup(*key, group.value, bits);
  const auto scalarOrPoint = component(*key, signing ? 6 : 5);
  const uint16_t width = static_cast<uint16_t>((bits + 7) / 8);
  int rc = 0;
  if (signing && scalarOrPoint.size() > width)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (signing) {
    mbedtls_mpi privateKey;
    mbedtls_mpi_init(&privateKey);
    rc = readMpi(privateKey, scalarOrPoint);
    if (rc == 0) rc = mbedtls_ecp_check_privkey(&group.value, &privateKey);
    mbedtls_mpi_free(&privateKey);
  } else {
    if (scalarOrPoint.size() != static_cast<size_t>(1 + 2 * width) ||
        scalarOrPoint[0] != 4) rc = -1;
    mbedtls_ecp_point publicKey;
    mbedtls_ecp_point_init(&publicKey);
    if (rc == 0) rc = mbedtls_ecp_point_read_binary(
        &group.value, &publicKey, scalarOrPoint.data(), scalarOrPoint.size());
    if (rc == 0) rc = mbedtls_ecp_check_pubkey(&group.value, &publicKey);
    mbedtls_ecp_point_free(&publicKey);
  }
  if (rc != 0) {
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  signature.setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY, keyRef);
  signature.setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGELENGTH, 0);
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE,
      static_cast<jbyte_t>(mode));
  signature.setField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 1);
}

jshort_t ecdsaSignatureLength(JC_Instance &signature) {
  auto key = object(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  const uint16_t width = static_cast<uint16_t>((key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE) + 7) / 8);
  const size_t integerLength = width + 1;
  const size_t encodedInt = 1 + derLengthBytes(integerLength) + integerLength;
  const size_t body = encodedInt * 2;
  return static_cast<jshort_t>(1 + derLengthBytes(body) + body);
}

jshort_t ecdsaSignatureSign(JC_Instance &signature,
    const std::shared_ptr<JC_Array> &input, jshort_t offset, jshort_t length,
    const std::shared_ptr<JC_Array> &output, jshort_t outputOffset) {
  auto key = object(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
  ScopedEcdsa scoped;
  auto &context = scoped.value;
  int rc = 0;
  loadAndValidateGroup(*key, context.MBEDTLS_PRIVATE(grp), bits);
  auto privateBytes = component(*key, 6);
  if (rc == 0) rc = mbedtls_mpi_read_binary(&context.MBEDTLS_PRIVATE(d), privateBytes.data(), privateBytes.size());
  if (rc == 0) rc = mbedtls_ecp_check_privkey(
      &context.MBEDTLS_PRIVATE(grp), &context.MBEDTLS_PRIVATE(d));
  const auto message = readMessage(signature, input, offset, length);
  const auto hash = digest(static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM)), message);
  mbedtls_mpi r, s;
  mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
  if (rc == 0) rc = mbedtls_ecdsa_sign(&context.MBEDTLS_PRIVATE(grp), &r, &s,
      &context.MBEDTLS_PRIVATE(d), hash.data(), hash.size(), randomBytes, nullptr);
  std::vector<unsigned char> encoded;
  if (rc == 0) encoded = encodeDer(r, s);
  mbedtls_mpi_free(&r); mbedtls_mpi_free(&s);
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (!output || output->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  if (outputOffset < 0 || static_cast<uint32_t>(outputOffset) > output->size() ||
      encoded.size() > output->size() - static_cast<uint32_t>(outputOffset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
  for (size_t i = 0; i < encoded.size(); ++i)
    output->setByteEntry(static_cast<uint16_t>(outputOffset + i), encoded[i]);
  return static_cast<jshort_t>(encoded.size());
}

jbool_t ecdsaSignatureVerify(JC_Instance &signature,
    const std::shared_ptr<JC_Array> &input, jshort_t offset, jshort_t length,
    const std::shared_ptr<JC_Array> &expected, jshort_t expectedOffset,
    jshort_t expectedLength) {
  if (!expected || expected->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  if (expectedOffset < 0 || expectedLength < 0 ||
      static_cast<uint32_t>(expectedOffset) > expected->size() ||
      static_cast<uint32_t>(expectedLength) > expected->size() - static_cast<uint32_t>(expectedOffset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
  auto key = object(signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY));
  const uint16_t bits = static_cast<uint16_t>(key->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_SIZE));
  ScopedEcdsa scoped;
  auto &context = scoped.value;
  int rc = 0;
  loadAndValidateGroup(*key, context.MBEDTLS_PRIVATE(grp), bits);
  auto point = component(*key, 5);
  if (rc == 0) rc = mbedtls_ecp_point_read_binary(&context.MBEDTLS_PRIVATE(grp), &context.MBEDTLS_PRIVATE(Q),
      point.data(), point.size());
  if (rc == 0) rc = mbedtls_ecp_check_pubkey(
      &context.MBEDTLS_PRIVATE(grp), &context.MBEDTLS_PRIVATE(Q));
  auto message = readMessage(signature, input, offset, length);
  auto hash = digest(static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEDIGESTALGORITHM)), message);
  mbedtls_mpi r, s;
  mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
  const auto sigBytes = [&]() {
    std::vector<unsigned char> v(static_cast<size_t>(expectedLength));
    for (jshort_t i = 0; i < expectedLength; ++i)
      v[static_cast<size_t>(i)] = static_cast<uint8_t>(expected->getByteEntry(
          static_cast<uint16_t>(expectedOffset + i)));
    return v;
  }();
  const bool der = decodeDer(sigBytes.data(), sigBytes.size(), r, s);
  if (rc == 0 && der) rc = mbedtls_ecdsa_verify(&context.MBEDTLS_PRIVATE(grp), hash.data(), hash.size(),
                                                &context.MBEDTLS_PRIVATE(Q), &r, &s);
  mbedtls_mpi_free(&r); mbedtls_mpi_free(&s);
  return rc == 0 && der ? TRUE : FALSE;
}

} // namespace jcvm
#endif
