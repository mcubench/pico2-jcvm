#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-support.h"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_random.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#if JC_SUPPORT_SECURE_RANDOM || JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
#ifdef PC_VERSION
#include <random>
#else
#include "pico/rand.h"
#endif
#endif

#if JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
#include "mbedtls/ctr_drbg.h"
#endif

namespace jcvm {
namespace {

constexpr jbyte_t kPseudoRandom = 1;
constexpr jbyte_t kSecureRandom = 2;
constexpr jbyte_t kPreseededDrbg = 4;
constexpr uint16_t kAlgorithmField = 0;
constexpr uint16_t kStateField = 1;

void checkRange(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + length > array->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

std::shared_ptr<JC_Instance> randomInstance(jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  if (reference.isArray()) throw Exceptions::SecurityException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != nativeRandomClass())
    throw Exceptions::SecurityException;
  return instance;
}

#if JC_SUPPORT_SECURE_RANDOM || JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
uint32_t entropyWord() {
#ifdef PC_VERSION
  static std::random_device randomSource;
  return randomSource();
#else
  return get_rand_32();
#endif
}
#endif

#if JC_SUPPORT_SECURE_RANDOM
void generateSecure(JC_Array &output, uint16_t offset, uint16_t length) {
  uint32_t word = 0;
  for (uint16_t i = 0; i < length; ++i) {
    if ((i & 3) == 0) word = entropyWord();
    output.setByteEntry(offset + i, static_cast<jbyte_t>(word >> (8 * (i & 3))));
  }
}
#endif

#if JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
#if defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO) || defined(MBEDTLS_THREADING_C)
#error Native RandomData requires the pointer-free software CTR_DRBG state
#endif
constexpr size_t kDrbgStateBytes =
    offsetof(mbedtls_ctr_drbg_context, MBEDTLS_PRIVATE(f_entropy));

int entropy(void *, unsigned char *output, size_t length) {
  uint32_t word = 0;
  for (size_t i = 0; i < length; ++i) {
    if ((i & 3) == 0) word = entropyWord();
    output[i] = static_cast<unsigned char>(word >> (8 * (i & 3)));
  }
  return 0;
}

class Drbg {
public:
  mbedtls_ctr_drbg_context value;
  Drbg() { mbedtls_ctr_drbg_init(&value); }
  ~Drbg() { mbedtls_ctr_drbg_free(&value); }
  Drbg(const Drbg &) = delete;
  Drbg &operator=(const Drbg &) = delete;
};

void checkDrbg(int status) {
  if (status != 0) throwCryptoException(CryptoExceptionReason::IllegalUse);
}

std::shared_ptr<JC_Array> drbgState(JC_Instance &instance) {
  auto state = Context::getActiveContext().getHeap().getArray(
      instance.getField_Reference(kStateField));
  if (!state || !state->isTransientArray() || state->getType() != JAVA_ARRAY_T_BYTE ||
      state->size() != kDrbgStateBytes + 1)
    throw Exceptions::SecurityException;
  return state;
}

void loadDrbg(JC_Array &state, Drbg &drbg) {
  if (state.getByteEntry(0) == 0) {
    checkDrbg(mbedtls_ctr_drbg_seed(&drbg.value, entropy, nullptr, nullptr, 0));
    return;
  }
  // The pinned software context stores an inline AES schedule. The entropy
  // callback is rebound on load so no function or data pointers are stored.
  state.copyByteEntriesTo(1, kDrbgStateBytes,
                          reinterpret_cast<uint8_t *>(&drbg.value));
  drbg.value.MBEDTLS_PRIVATE(f_entropy) = entropy;
}

void saveDrbg(JC_Array &state, const Drbg &drbg) {
  state.copyByteEntriesFrom(1, kDrbgStateBytes,
                            reinterpret_cast<const uint8_t *>(&drbg.value));
  state.setByteEntry(0, 1);
}

void generateDrbg(JC_Instance &instance, JC_Array &output, uint16_t offset,
                   uint16_t length) {
  auto state = drbgState(instance);
  Drbg drbg;
  loadDrbg(*state, drbg);
  uint8_t chunk[64];
  while (length != 0) {
    const uint16_t count = std::min<uint16_t>(length, sizeof(chunk));
    checkDrbg(mbedtls_ctr_drbg_random(&drbg.value, chunk, count));
    // Advance before the caller can observe bytes, including a failed write.
    saveDrbg(*state, drbg);
    for (uint16_t i = 0; i < count; ++i)
      output.setByteEntry(offset + i, static_cast<jbyte_t>(chunk[i]));
    offset += count;
    length -= count;
  }
}
#endif

} // namespace

#if JC_SUPPORT_SECURE_RANDOM
void fr_gouv_ssi_nativeimpl_RandomDataNativeBridge_generateRandomData(
    std::shared_ptr<JC_Array> output, jshort_t offset, jshort_t length) {
  checkRange(output, offset, length);
  if (length == 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  generateSecure(*output, offset, length);
}
#endif

jref_t javacard_security_RandomData_getInstance(jbyte_t algorithm) {
  if (!(algorithm == kSecureRandom && JC_SUPPORT_SECURE_RANDOM) &&
      !(algorithm == kPreseededDrbg && JC_SUPPORT_RANDOM_PRESEEDED_DRBG) &&
      !(algorithm == kPseudoRandom && JC_SUPPORT_RANDOM_PSEUDO))
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  auto &heap = Context::getActiveContext().getHeap();
  const auto reference = heap.addInstance(ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
                                          nativeRandomClass());
  auto instance = randomInstance(reference);
  instance->setField_Byte(kAlgorithmField, algorithm);
#if JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
  if (algorithm != kSecureRandom) {
    instance->setField_Reference(kStateField, heap.addTransientArray(
        kDrbgStateBytes + 1, JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET));
    auto state = drbgState(*instance);
    Drbg drbg;
    loadDrbg(*state, drbg);
    saveDrbg(*state, drbg);
  }
#endif
  return reference;
}

jbyte_t javacard_security_RandomData_getAlgorithm(jref_t receiver) {
  const jbyte_t algorithm = randomInstance(receiver)->getField_Byte(kAlgorithmField);
  Context::getActiveContext().setSensitiveResultShort(algorithm);
  return algorithm;
}

jshort_t javacard_security_RandomData_nextBytes(jref_t receiver,
    std::shared_ptr<JC_Array> output, jshort_t offset, jshort_t length) {
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto instance = randomInstance(receiver);
  checkRange(output, offset, length);
  if (length == 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  switch (instance->getField_Byte(kAlgorithmField)) {
#if JC_SUPPORT_SECURE_RANDOM
  case kSecureRandom:
    generateSecure(*output, offset, length);
    break;
#endif
#if JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
  case kPreseededDrbg:
  case kPseudoRandom:
    generateDrbg(*instance, *output, offset, length);
    break;
#endif
  default:
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  }
  const auto result = signedShortFromBits(static_cast<uint16_t>(offset + length));
  context.setSensitiveResultShort(result);
  return result;
}

void javacard_security_RandomData_generateData(jref_t receiver,
    std::shared_ptr<JC_Array> output, jshort_t offset, jshort_t length) {
  (void)javacard_security_RandomData_nextBytes(receiver, std::move(output), offset, length);
}

void javacard_security_RandomData_setSeed(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t length) {
  auto instance = randomInstance(receiver);
  checkRange(input, offset, length);
#if JC_SUPPORT_RANDOM_PRESEEDED_DRBG || JC_SUPPORT_RANDOM_PSEUDO
  if (instance->getField_Byte(kAlgorithmField) != kSecureRandom) {
    auto state = drbgState(*instance);
    Drbg drbg;
    loadDrbg(*state, drbg);
    const auto *bytes = length == 0 ? nullptr : input->getByteDataView() + offset;
    // Keep the existing state and mix in the applet input, then reseed from
    // live platform entropy. Split input to stay within the library bound.
    while (length != 0) {
      const auto count = std::min<jshort_t>(length, MBEDTLS_CTR_DRBG_MAX_INPUT);
      checkDrbg(mbedtls_ctr_drbg_update(&drbg.value, bytes, count));
      bytes += count;
      length -= count;
    }
    checkDrbg(mbedtls_ctr_drbg_reseed(&drbg.value, nullptr, 0));
    saveDrbg(*state, drbg);
  }
#endif
}

} // namespace jcvm
