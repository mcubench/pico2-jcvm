// Java Card 3.2 preview post-quantum API: NamedParameterSpec (ML-KEM/ML-DSA
// names), javacardx.security.pqc keys and PQCKeyFactory, and
// javacardx.crypto.kem.KEM for ML-KEM. Cryptography comes from the vendored
// mlkem-native / mldsa-native through choupi/src/pqc (pqc_backend.h).
//
// Keys store their primary encoding (FIPS 203/204) split across up to four
// byte arrays of at most kChunkBytes, so no persistent record exceeds the
// flash journal entry limit. Every operation runs in one heap workspace
// (library arena, operand buffers and a private stack) that is zeroized and
// released before returning.

#include "native_pqc.hpp"

#include "context.hpp"
#include "exceptions.hpp"
#include "heap.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"
#include "native_secret_key.hpp"
#include "recoverable_allocation.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#if JC_SUPPORT_PQC_ANY
#include "pqc/pqc_arena.h"
#include "pqc/pqc_backend.h"
#endif

#ifdef PC_VERSION
#include <chrono>
#include <random>
#else
#include "pico/rand.h"
#include "pico/time.h"
extern "C" bool choupi_native_crypto_progress(void);
#endif

namespace jcvm {
namespace {

constexpr jshort_t kModeEncapsulation = 1;
constexpr jshort_t kModeDecapsulation = 2;
constexpr jshort_t kModeExpand = 4096;
constexpr jshort_t kAlgDhKem = 1;
constexpr jshort_t kAlgMlKem = 2;

constexpr uint16_t kAttrPrivate = 0x8000;
constexpr uint16_t kAttrSeed = 0x4000;
constexpr uint16_t kAttrMemoryMask = 0x0003;
constexpr uint8_t kMemoryPersistent = 0;
constexpr uint8_t kMemoryTransientReset = 1;
constexpr uint8_t kMemoryTransientDeselect = 2;

constexpr uint8_t kFlagMemoryMask = 0x03;
constexpr uint8_t kFlagPrivate = 0x04;
constexpr uint8_t kFlagSeed = 0x08;

constexpr uint8_t kTypeMlKem = 44; // KeyBuilder.TYPE_ML_KEM
constexpr uint8_t kTypeMlDsa = 45; // KeyBuilder.TYPE_ML_DSA

constexpr uint16_t kChunkBytes = 1632; // ML-DSA-87 sk = 3 chunks
constexpr uint16_t kSharedSecretBytes = 32;

constexpr std::array<uint16_t, 4> kDataFields = {
    ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_DATA0,
    ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_DATA1,
    ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_DATA2,
    ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_DATA3,
};

// FIPS 203 / FIPS 204 encoding sizes. The ACVP tests pin them to the backend.
const PqcParameterSet kParameterSets[] = {
    {0x0501, true, 512, 800, 1632, 64, 768, JC_SUPPORT_MLKEM_512 != 0},
    {0x0502, true, 768, 1184, 2400, 64, 1088, JC_SUPPORT_MLKEM_768 != 0},
    {0x0503, true, 1024, 1568, 3168, 64, 1568, JC_SUPPORT_MLKEM_1024 != 0},
    {0x0701, false, 44, 1312, 2560, 32, 2420, JC_SUPPORT_MLDSA_44 != 0},
    {0x0702, false, 65, 1952, 4032, 32, 3309, JC_SUPPORT_MLDSA_65 != 0},
    {0x0703, false, 87, 2592, 4896, 32, 4627, JC_SUPPORT_MLDSA_87 != 0},
};

[[noreturn]] void throwNoResource() {
  Context::getActiveContext().setPendingSystemExceptionReason(
      5); // SystemException.NO_RESOURCE
  throw Exceptions::SystemException;
}

uint16_t romClassIndex(jpackage_ID_t package, uint8_t token) {
  return HTONS(Export_Handler(Package(package)).getExportedClassOffset(token));
}

uint16_t namedParameterSpecClass() {
  return romClassIndex(
      ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
      ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_NAMEDPARAMETERSPEC);
}

uint16_t keyClass(bool kem, bool privateKey) {
  if (kem)
    return romClassIndex(
        ROM_PACKAGE_INDEX_JAVACARDX_SECURITY_PQC,
        privateKey
            ? ROM_CLASS_EXPORT_TOKEN_JAVACARDX_SECURITY_PQC_PQCKEYIMPLKEMPRIVATEKEY
            : ROM_CLASS_EXPORT_TOKEN_JAVACARDX_SECURITY_PQC_PQCKEYIMPLKEMPUBLICKEY);
  return romClassIndex(
      ROM_PACKAGE_INDEX_JAVACARDX_SECURITY_PQC,
      privateKey
          ? ROM_CLASS_EXPORT_TOKEN_JAVACARDX_SECURITY_PQC_PQCKEYIMPLDSAPRIVATEKEY
          : ROM_CLASS_EXPORT_TOKEN_JAVACARDX_SECURITY_PQC_PQCKEYIMPLDSAPUBLICKEY);
}

uint16_t kemClass(jshort_t mode) {
  const bool enc = (mode & kModeEncapsulation) != 0;
  const bool dec = (mode & kModeDecapsulation) != 0;
  return romClassIndex(
      ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO_KEM,
      enc && dec ? ROM_CLASS_EXPORT_TOKEN_JAVACARDX_CRYPTO_KEM_KEMIMPLDUAL
      : enc      ? ROM_CLASS_EXPORT_TOKEN_JAVACARDX_CRYPTO_KEM_KEMIMPLENCAPSULATOR
                 : ROM_CLASS_EXPORT_TOKEN_JAVACARDX_CRYPTO_KEM_KEMIMPLDECAPSULATOR);
}

std::shared_ptr<JC_Array> byteArray(const std::shared_ptr<JC_Array> &array) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE) throw Exceptions::ArrayStoreException;
  return array;
}

void checkRange(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                uint32_t length) {
  byteArray(array);
  if (offset < 0 || static_cast<uint32_t>(offset) > array->size() ||
      length > array->size() - static_cast<uint32_t>(offset))
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

void fillRandom(uint8_t *out, size_t length) {
#ifdef PC_VERSION
  static std::random_device source;
  for (size_t i = 0; i < length; ++i) out[i] = static_cast<uint8_t>(source());
#else
  uint32_t word = 0;
  for (size_t i = 0; i < length; ++i) {
    if ((i & 3U) == 0) word = get_rand_32();
    out[i] = static_cast<uint8_t>(word >> ((i & 3U) * 8));
  }
#endif
}

void wipe(void *p, size_t n) {
  volatile uint8_t *v = static_cast<volatile uint8_t *>(p);
  while (n--) *v++ = 0;
}

// ---------------------------------------------------------------------------
// Key objects
// ---------------------------------------------------------------------------

struct PqcKey {
  std::shared_ptr<JC_Instance> instance;
  const PqcParameterSet *set;
  uint8_t flags;
  bool isPrivate() const { return (flags & kFlagPrivate) != 0; }
  bool isSeed() const { return (flags & kFlagSeed) != 0; }
  uint16_t encodingLength() const {
    return isSeed() ? set->seed : isPrivate() ? set->priv : set->pub;
  }
};

// Returns false (not a PQC key) for any other object; throws on null.
bool loadPqcKey(jref_t reference, PqcKey &key) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  if (reference.isArray()) return false;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance ||
      instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARDX_SECURITY_PQC)
    return false;
  const uint16_t cls = instance->getClassIndex();
  if (cls != keyClass(true, true) && cls != keyClass(true, false) &&
      cls != keyClass(false, true) && cls != keyClass(false, false))
    return false;
  key.instance = instance;
  key.set = pqcParameterSet(instance->getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PARAMETERSET));
  key.flags = static_cast<uint8_t>(instance->getField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_FLAGS));
  if (key.set == nullptr) throw Exceptions::SecurityException;
  return true;
}

PqcKey receiverKey(jref_t receiver) {
  PqcKey key;
  if (!loadPqcKey(receiver, key)) throw Exceptions::SecurityException;
  return key;
}

std::shared_ptr<JC_Array> transientFlag(const PqcKey &key) {
  const jref_t reference = key.instance->getField_Reference(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_TRANSIENTINITIALIZED);
  if (reference.isNullPointer()) return nullptr;
  auto flag = Context::getActiveContext().getHeap().getArray(reference);
  if (!flag || flag->getType() != JAVA_ARRAY_T_BYTE || flag->size() < 1)
    throw Exceptions::ArrayStoreException;
  return flag;
}

bool initialized(const PqcKey &key) {
  if (auto flag = transientFlag(key)) return flag->getByteEntry(0) != 0;
  return key.instance->getField_Byte(
             ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PERSISTENTINITIALIZED) != 0;
}

void setInitialized(const PqcKey &key, bool value) {
  if (auto flag = transientFlag(key)) {
    flag->setByteEntry(0, value ? 1 : 0);
    return;
  }
  key.instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PERSISTENTINITIALIZED,
      value ? 1 : 0);
}

std::shared_ptr<JC_Array> chunk(const PqcKey &key, size_t index) {
  const jref_t reference = key.instance->getField_Reference(kDataFields[index]);
  if (reference.isNullPointer()) throw Exceptions::SecurityException;
  auto array = Context::getActiveContext().getHeap().getArray(reference);
  if (!array || array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  return array;
}

template <class F> void forEachChunk(const PqcKey &key, F f) {
  const uint16_t total = key.encodingLength();
  for (uint16_t offset = 0, index = 0; offset < total;
       offset = static_cast<uint16_t>(offset + kChunkBytes), ++index) {
    const uint16_t n = static_cast<uint16_t>(
        total - offset < kChunkBytes ? total - offset : kChunkBytes);
    auto array = chunk(key, index);
    if (array->size() != n) throw Exceptions::SecurityException;
    f(*array, offset, n);
  }
}

void readKey(const PqcKey &key, uint8_t *out) {
  forEachChunk(key, [&](JC_Array &array, uint16_t offset, uint16_t n) {
    array.copyPrimitiveEntriesTo(0, n, out + offset);
  });
}

void writeKey(const PqcKey &key, const uint8_t *in) {
  // One range write per chunk: a single journal record each.
  forEachChunk(key, [&](JC_Array &array, uint16_t offset, uint16_t n) {
    array.copyPrimitiveEntriesFrom(0, n, in + offset, true);
  });
}

void clearKeyValue(const PqcKey &key) {
  // Spec order: the key becomes uninitialized before its value changes.
  setInitialized(key, false);
  static const uint8_t zeros[64] = {0};
  forEachChunk(key, [&](JC_Array &array, uint16_t, uint16_t n) {
    for (uint16_t at = 0; at < n; at = static_cast<uint16_t>(at + 64)) {
      const uint16_t len = static_cast<uint16_t>(n - at < 64 ? n - at : 64);
      array.copyPrimitiveEntriesFrom(at, len, zeros, true);
    }
  });
}

void storeKey(const PqcKey &key, const uint8_t *encoding) {
  setInitialized(key, false);
  writeKey(key, encoding);
  setInitialized(key, true);
}

jref_t newNamedParameterSpec(jshort_t name) {
  Heap &heap = Context::getActiveContext().getHeap();
  const jref_t reference = heap.addInstance(ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
                                            namedParameterSpecClass());
  auto instance = heap.getInstance(reference);
  if (!instance) throw Exceptions::RuntimeException;
  instance->setField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NAMEDPARAMETERSPEC_NAMEDPARAMETERSPEC_NAME,
      name);
  return reference;
}

const PqcParameterSet *namedParameter(jref_t reference) {
  if (reference.isNullPointer() || reference.isArray()) return nullptr;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance ||
      instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != namedParameterSpecClass())
    return nullptr;
  return pqcParameterSet(instance->getField_Short(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NAMEDPARAMETERSPEC_NAMEDPARAMETERSPEC_NAME));
}

#if JC_SUPPORT_PQC_ANY
// ---------------------------------------------------------------------------
// Workspace: [operand buffers | library arena | private stack]. A stack
// overrun therefore lands in this block's own arena, never in foreign heap,
// and is detected from the painted stack afterwards.
// ---------------------------------------------------------------------------

constexpr size_t kPqcStackBytes = 8192;
constexpr size_t kAlign = 32;

size_t alignUp(size_t n) { return (n + kAlign - 1) & ~(kAlign - 1); }

uint32_t nowUs() {
#ifdef PC_VERSION
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
#else
  return time_us_32();
#endif
}

class Workspace {
public:
  Workspace(const char *op, const PqcParameterSet &set, size_t buffers)
      : op_(op), set_(set), buffers_(alignUp(buffers)),
        arena_(alignUp(set.kem ? pqc_mlkem_scratch(set.number)
                               : pqc_mldsa_scratch(set.number))),
        size_(buffers_ + arena_ + kPqcStackBytes + kAlign) {
    try {
      RecoverableAllocationScope scope;
      raw_ = new uint8_t[size_];
    } catch (const std::bad_alloc &) {
      std::printf("CHOUPI:PQC op=%s set=%u alloc_fail=%u\r\n", op_,
                  static_cast<unsigned>(set_.number),
                  static_cast<unsigned>(size_));
      throwNoResource();
    }
    base_ = reinterpret_cast<uint8_t *>(
        (reinterpret_cast<uintptr_t>(raw_) + kAlign - 1) & ~(kAlign - 1));
    std::memset(base_, 0, buffers_);
  }
  ~Workspace() {
    wipe(raw_, size_);
    delete[] raw_;
  }
  Workspace(const Workspace &) = delete;
  Workspace &operator=(const Workspace &) = delete;

  uint8_t *buffer(size_t offset) { return base_ + offset; }

  // Runs f() (noexcept, C calls only) on the private stack with the arena.
  template <class F> int run(F &f) {
#ifndef PC_VERSION
    (void)choupi_native_crypto_progress();
#endif
    uint8_t *arena = base_ + buffers_;
    uint8_t *stack = arena + arena_;
    const uint32_t start = nowUs();
    pqc_arena_begin(arena, arena_);
    const int rc = pqc_run_on_stack(
        stack, kPqcStackBytes,
        [](void *p) -> int { return (*static_cast<F *>(p))(); }, &f);
    const size_t peak = pqc_arena_end();
    const size_t stackUsed = pqc_stack_used(stack, kPqcStackBytes);
    const uint32_t elapsed = nowUs() - start;
#ifndef PC_VERSION
    (void)choupi_native_crypto_progress();
#endif
    std::printf("CHOUPI:PQC op=%s set=%u rc=%d us=%lu arena=%u/%u stack=%u/%u "
                "block=%u\r\n",
                op_, static_cast<unsigned>(set_.number), rc,
                static_cast<unsigned long>(elapsed),
                static_cast<unsigned>(peak), static_cast<unsigned>(arena_),
                static_cast<unsigned>(stackUsed),
                static_cast<unsigned>(kPqcStackBytes),
                static_cast<unsigned>(size_));
    if (stackUsed >= kPqcStackBytes) {
      // The private stack overflowed into the arena: results are untrusted.
      throw Exceptions::SecurityException;
    }
    return rc;
  }

private:
  const char *op_;
  const PqcParameterSet &set_;
  size_t buffers_;
  size_t arena_;
  size_t size_;
  uint8_t *raw_ = nullptr;
  uint8_t *base_ = nullptr;
};

// Expanded private encoding from a key in either representation. `out` must
// hold set->priv bytes; `scratch` must hold set->pub bytes (unused for
// expanded keys). Returns the library status.
int expandPrivate(Workspace &ws, const PqcKey &key, uint8_t *out,
                  uint8_t *scratch) {
  if (!key.isSeed()) {
    readKey(key, out);
    return 0;
  }
  uint8_t seed[64];
  readKey(key, seed);
  const int set = key.set->number;
  int rc;
  if (key.set->kem) {
    auto f = [&]() noexcept { return pqc_mlkem_keypair(set, scratch, out, seed); };
    rc = ws.run(f);
  } else {
    auto f = [&]() noexcept { return pqc_mldsa_keypair(set, scratch, out, seed); };
    rc = ws.run(f);
  }
  wipe(seed, sizeof(seed));
  return rc;
}
#endif // JC_SUPPORT_PQC_ANY

void requireEnabled(const PqcParameterSet *set) {
  if (set == nullptr || !set->enabled)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}

// ---------------------------------------------------------------------------
// KEM objects
// ---------------------------------------------------------------------------

struct KemState {
  const PqcParameterSet *set;
  jshort_t mode;
};

KemState receiverKem(jref_t receiver) {
  if (receiver.isNullPointer()) throw Exceptions::NullPointerException;
  if (receiver.isArray()) throw Exceptions::SecurityException;
  auto instance = Context::getActiveContext().getHeap().getInstance(receiver);
  if (!instance ||
      instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO_KEM)
    throw Exceptions::SecurityException;
  const uint16_t cls = instance->getClassIndex();
  if (cls != kemClass(kModeEncapsulation) && cls != kemClass(kModeDecapsulation) &&
      cls != kemClass(kModeEncapsulation | kModeDecapsulation))
    throw Exceptions::SecurityException;
  KemState state;
  state.set = pqcParameterSet(instance->getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_KEM_KEMIMPL_KEMIMPL_PARAMETERSET));
  state.mode = instance->getField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_KEM_KEMIMPL_KEMIMPL_MODE);
  if (state.set == nullptr || !state.set->kem) throw Exceptions::SecurityException;
  requireEnabled(state.set);
  return state;
}

void checkSecretRange(jshort_t from, jshort_t to) {
  if (from < 0 || from > to || to > static_cast<jshort_t>(kSharedSecretBytes))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
}

enum class SecretSink { Bytes, Key };

// Encapsulation core: ek bytes in, ct + ss out (ss into `ssOut`).
#if JC_SUPPORT_MLKEM_ANY
void encapsulateRaw(const PqcParameterSet &set, const uint8_t *ek, uint8_t *ct,
                    uint8_t *ss) {
  Workspace ws("kem_encaps", set, 0);
  uint8_t m[32];
  fillRandom(m, sizeof(m));
  auto f = [&]() noexcept {
    return pqc_mlkem_encaps(set.number, ct, ss, ek, m);
  };
  const int rc = ws.run(f);
  wipe(m, sizeof(m));
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
}
#endif

jshort_t encapsulate(jref_t receiver, const uint8_t *ekBytes, jref_t publicKey,
                     const std::shared_ptr<JC_Array> &cipherText,
                     jshort_t cipherTextOffset, SecretSink sink,
                     const std::shared_ptr<JC_Array> &secretBytes,
                     jshort_t secretOffset, jref_t secretKey, jshort_t from,
                     jshort_t to) {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  const KemState state = receiverKem(receiver);
  if ((state.mode & kModeEncapsulation) == 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const PqcParameterSet &set = *state.set;
  checkRange(cipherText, cipherTextOffset, set.extra);
  if (sink == SecretSink::Bytes) {
    checkRange(secretBytes, secretOffset, kSharedSecretBytes);
  } else {
    checkSecretRange(from, to);
    if (!nativeSecretKeyAccepts(secretKey, static_cast<uint16_t>(to - from)))
      throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
#if JC_SUPPORT_MLKEM_ANY
  std::unique_ptr<uint8_t[]> buffers;
  try {
    RecoverableAllocationScope scope;
    buffers.reset(new uint8_t[set.pub + set.extra]);
  } catch (const std::bad_alloc &) {
    throwNoResource();
  }
  uint8_t *ek = buffers.get();
  uint8_t *ct = ek + set.pub;
  if (ekBytes != nullptr) {
    std::memcpy(ek, ekBytes, set.pub);
  } else {
    PqcKey key;
    if (!loadPqcKey(publicKey, key) || !key.set->kem || key.isPrivate() ||
        key.set != state.set)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    if (!initialized(key))
      throwCryptoException(CryptoExceptionReason::UninitializedKey);
    readKey(key, ek);
  }
  uint8_t ss[kSharedSecretBytes];
  encapsulateRaw(set, ek, ct, ss);
  cipherText->copyPrimitiveEntriesFrom(static_cast<uint16_t>(cipherTextOffset),
                                       set.extra, ct, false);
  if (sink == SecretSink::Bytes)
    secretBytes->copyPrimitiveEntriesFrom(static_cast<uint16_t>(secretOffset),
                                          kSharedSecretBytes, ss, false);
  else
    nativeSecretKeySet(secretKey, ss + from, static_cast<uint16_t>(to - from));
  wipe(ss, sizeof(ss));
  context.setSensitiveResultShort(static_cast<jshort_t>(set.extra));
  return static_cast<jshort_t>(set.extra);
#else
  (void)ekBytes; (void)publicKey; (void)secretKey;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

jshort_t decapsulate(jref_t receiver, jref_t privateKey,
                     const std::shared_ptr<JC_Array> &cipherText,
                     jshort_t cipherTextOffset, SecretSink sink,
                     const std::shared_ptr<JC_Array> &secretBytes,
                     jshort_t secretOffset, jref_t secretKey, jshort_t from,
                     jshort_t to) {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  const KemState state = receiverKem(receiver);
  if ((state.mode & kModeDecapsulation) == 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  const PqcParameterSet &set = *state.set;
  PqcKey key;
  if (!loadPqcKey(privateKey, key) || !key.set->kem || !key.isPrivate() ||
      key.set != state.set)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (!initialized(key))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  if (key.isSeed() && (state.mode & kModeExpand) == 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  byteArray(cipherText);
  if (cipherTextOffset < 0 ||
      static_cast<int32_t>(cipherText->size()) - cipherTextOffset <
          static_cast<int32_t>(set.extra))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (sink == SecretSink::Bytes) {
    checkRange(secretBytes, secretOffset, kSharedSecretBytes);
  } else {
    checkSecretRange(from, to);
    if (!nativeSecretKeyAccepts(secretKey, static_cast<uint16_t>(to - from)))
      throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
#if JC_SUPPORT_MLKEM_ANY
  // Buffers: dk | ct | ek scratch (seed expansion).
  Workspace ws("kem_decaps", set, set.priv + set.extra + set.pub);
  uint8_t *dk = ws.buffer(0);
  uint8_t *ct = dk + set.priv;
  uint8_t *scratch = ct + set.extra;
  if (expandPrivate(ws, key, dk, scratch) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  cipherText->copyPrimitiveEntriesTo(static_cast<uint16_t>(cipherTextOffset),
                                     set.extra, ct);
  uint8_t ss[kSharedSecretBytes];
  auto f = [&]() noexcept { return pqc_mlkem_decaps(set.number, ss, ct, dk); };
  if (ws.run(f) != 0) {
    wipe(ss, sizeof(ss));
    // The only decapsulation failure is the FIPS 203 dk hash check.
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  jshort_t written;
  if (sink == SecretSink::Bytes) {
    secretBytes->copyPrimitiveEntriesFrom(static_cast<uint16_t>(secretOffset),
                                          kSharedSecretBytes, ss, false);
    written = static_cast<jshort_t>(kSharedSecretBytes);
  } else {
    nativeSecretKeySet(secretKey, ss + from, static_cast<uint16_t>(to - from));
    written = static_cast<jshort_t>(to - from);
  }
  wipe(ss, sizeof(ss));
  context.setSensitiveResultShort(written);
  return written;
#else
  (void)secretKey;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

} // namespace

const PqcParameterSet *pqcParameterSet(jshort_t name) {
  for (const auto &set : kParameterSets)
    if (set.name == name) return &set;
  return nullptr;
}

// ---------------------------------------------------------------------------
// NamedParameterSpec
// ---------------------------------------------------------------------------

jref_t javacard_security_NamedParameterSpec_getInstance(jshort_t stdName) {
  // Only names whose algorithm this build implements: other curves and
  // schemes in the 3.2 list (X25519, Ed25519, ...) are not available.
  requireEnabled(pqcParameterSet(stdName));
  return newNamedParameterSpec(stdName);
}

// ---------------------------------------------------------------------------
// PQCKeyFactory
// ---------------------------------------------------------------------------

jref_t javacardx_security_pqc_PQCKeyFactory_buildKey(jshort_t algName,
                                                     jshort_t attributes) {
  Context &context = Context::getActiveContext();
  context.resetSensitiveResult();
  const PqcParameterSet *set = pqcParameterSet(algName);
  requireEnabled(set);
  const uint16_t attrs = static_cast<uint16_t>(attributes);
  const uint8_t memory = static_cast<uint8_t>(attrs & kAttrMemoryMask);
  const bool privateKey = (attrs & kAttrPrivate) != 0;
  const bool seed = (attrs & kAttrSeed) != 0;
  if ((attrs & ~(kAttrPrivate | kAttrSeed | kAttrMemoryMask)) != 0 ||
      memory > kMemoryTransientDeselect || (seed && !privateKey))
    throwCryptoException(CryptoExceptionReason::IllegalValue);

  Heap &heap = context.getHeap();
  const jref_t reference = heap.addInstance(
      ROM_PACKAGE_INDEX_JAVACARDX_SECURITY_PQC, keyClass(set->kem, privateKey));
  auto instance = heap.getInstance(reference);
  if (!instance) throw Exceptions::RuntimeException;
  const uint8_t flags = static_cast<uint8_t>(
      memory | (privateKey ? kFlagPrivate : 0) | (seed ? kFlagSeed : 0));
  instance->setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PARAMETERSET,
      algName);
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_FLAGS, flags);
  instance->setField_Byte(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PERSISTENTINITIALIZED,
      0);
  // getParams() returns this instance; allocating it here keeps getParams()
  // free of per-call allocation (INV-16).
  instance->setField_Reference(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PARAMS,
      newNamedParameterSpec(algName));
  const bool transient = memory != kMemoryPersistent;
  const ClearEvent event = memory == kMemoryTransientDeselect
                               ? ClearEvent::CLEAR_ON_DESELECT
                               : ClearEvent::CLEAR_ON_RESET;
  PqcKey key{instance, set, flags};
  const uint16_t total = key.encodingLength();
  for (uint16_t offset = 0, index = 0; offset < total;
       offset = static_cast<uint16_t>(offset + kChunkBytes), ++index) {
    const uint16_t n = static_cast<uint16_t>(
        total - offset < kChunkBytes ? total - offset : kChunkBytes);
    instance->setField_Reference(
        kDataFields[index],
        transient ? heap.addArray(JC_Array(heap, n, JAVA_ARRAY_T_BYTE, true, event))
                  : heap.addArray(n, JAVA_ARRAY_T_BYTE));
  }
  if (transient) {
    instance->setField_Reference(
        ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_TRANSIENTINITIALIZED,
        heap.addArray(JC_Array(heap, 1, JAVA_ARRAY_T_BYTE, true, event)));
  }
  context.setSensitiveResultObject(reference);
  return reference;
}

namespace {
void requireSameSet(const PqcKey &a, const PqcKey &b) {
  if (a.set != b.set) throwCryptoException(CryptoExceptionReason::IllegalValue);
}
} // namespace

void pqcGenerateKeyPair(jref_t publicKey, jref_t privateKey) {
  PqcKey pub, priv;
  if (publicKey.isNullPointer() || privateKey.isNullPointer())
    throw Exceptions::NullPointerException;
  if (!loadPqcKey(publicKey, pub) || !loadPqcKey(privateKey, priv) ||
      pub.isPrivate() || !priv.isPrivate())
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  requireSameSet(pub, priv);
  requireEnabled(pub.set);
#if JC_SUPPORT_PQC_ANY
  const PqcParameterSet &set = *pub.set;
  // Spec order: clear public, clear private, generate, set private, set public.
  clearKeyValue(pub);
  clearKeyValue(priv);
  Workspace ws(set.kem ? "kem_keygen" : "dsa_keygen", set, set.pub + set.priv);
  uint8_t *pk = ws.buffer(0);
  uint8_t *sk = pk + set.pub;
  uint8_t seed[64];
  fillRandom(seed, set.seed);
  auto f = [&]() noexcept {
    return set.kem ? pqc_mlkem_keypair(set.number, pk, sk, seed)
                   : pqc_mldsa_keypair(set.number, pk, sk, seed);
  };
  const int rc = ws.run(f);
  if (rc != 0) {
    wipe(seed, sizeof(seed));
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  }
  storeKey(priv, priv.isSeed() ? seed : sk);
  wipe(seed, sizeof(seed));
  storeKey(pub, pk);
#endif
}

bool pqcCheckKeyPair(jref_t publicKey, jref_t privateKey) {
  if (publicKey.isNullPointer() || privateKey.isNullPointer())
    throw Exceptions::NullPointerException;
  PqcKey pub, priv;
  const bool pubPqc = loadPqcKey(publicKey, pub);
  const bool privPqc = loadPqcKey(privateKey, priv);
  if (!pubPqc && !privPqc) return false;
  if (!pubPqc || !privPqc || pub.isPrivate() || !priv.isPrivate() ||
      pub.set != priv.set)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  requireEnabled(pub.set);
  return true;
}

void javacardx_security_pqc_PQCKeyFactory_generateKeyPair(jref_t publicKey,
                                                          jref_t privateKey) {
  pqcGenerateKeyPair(publicKey, privateKey);
}

void javacardx_security_pqc_PQCKeyFactory_generateKeyPair(
    jref_t privateKeySeed, jref_t publicKeyExpanded, jref_t privateKeyExpanded) {
  if (privateKeySeed.isNullPointer()) throw Exceptions::NullPointerException;
  PqcKey seed, pub, priv;
  const bool hasPub = !publicKeyExpanded.isNullPointer();
  const bool hasPriv = !privateKeyExpanded.isNullPointer();
  if (!loadPqcKey(privateKeySeed, seed) || !seed.isPrivate() || !seed.isSeed() ||
      (!hasPub && !hasPriv))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (hasPub && (!loadPqcKey(publicKeyExpanded, pub) || pub.isPrivate()))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (hasPriv && (!loadPqcKey(privateKeyExpanded, priv) || !priv.isPrivate() ||
                  priv.isSeed()))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (hasPub) requireSameSet(seed, pub);
  if (hasPriv) requireSameSet(seed, priv);
  requireEnabled(seed.set);
  if (!initialized(seed))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
#if JC_SUPPORT_PQC_ANY
  const PqcParameterSet &set = *seed.set;
  if (hasPub) clearKeyValue(pub);
  if (hasPriv) clearKeyValue(priv);
  Workspace ws(set.kem ? "kem_expand" : "dsa_expand", set, set.pub + set.priv);
  uint8_t *pk = ws.buffer(0);
  uint8_t *sk = pk + set.pub;
  if (expandPrivate(ws, seed, sk, pk) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  if (hasPriv) storeKey(priv, sk);
  if (hasPub) storeKey(pub, pk);
#endif
}

// ---------------------------------------------------------------------------
// PQCKeyImpl (EncodableKey and the ML-KEM/ML-DSA key interfaces)
// ---------------------------------------------------------------------------

jbool_t javacardx_security_pqc_PQCKeyImpl_isInitialized(jref_t receiver) {
  return initialized(receiverKey(receiver)) ? TRUE : FALSE;
}

void javacardx_security_pqc_PQCKeyImpl_clearKey(jref_t receiver) {
  clearKeyValue(receiverKey(receiver));
}

jbyte_t javacardx_security_pqc_PQCKeyImpl_getType(jref_t receiver) {
  return static_cast<jbyte_t>(receiverKey(receiver).set->kem ? kTypeMlKem
                                                             : kTypeMlDsa);
}

jshort_t javacardx_security_pqc_PQCKeyImpl_getSize(jref_t receiver) {
  // The specification leaves the "size in bits" of named-parameter keys
  // open; report the FIPS parameter-set number (512/768/1024, 44/65/87).
  return static_cast<jshort_t>(receiverKey(receiver).set->number);
}

jref_t javacardx_security_pqc_PQCKeyImpl_getParams(jref_t receiver) {
  return receiverKey(receiver).instance->getField_Reference(
      ROM_FIELD_INDEX_JAVACARDX_SECURITY_PQC_PQCKEYIMPL_PQCKEYIMPL_PARAMS);
}

jshort_t javacardx_security_pqc_PQCKeyImpl_getEncodingLength(jref_t receiver) {
  return static_cast<jshort_t>(receiverKey(receiver).encodingLength());
}

jshort_t javacardx_security_pqc_PQCKeyImpl_getEncoded(
    jref_t receiver, std::shared_ptr<JC_Array> value, jshort_t offset) {
  const PqcKey key = receiverKey(receiver);
  if (!initialized(key))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  const uint16_t length = key.encodingLength();
  checkRange(value, offset, length);
  forEachChunk(key, [&](JC_Array &array, uint16_t at, uint16_t n) {
    uint8_t bytes[kChunkBytes];
    array.copyPrimitiveEntriesTo(0, n, bytes);
    value->copyPrimitiveEntriesFrom(static_cast<uint16_t>(offset + at), n,
                                    bytes, false);
    wipe(bytes, n);
  });
  return static_cast<jshort_t>(offset + length);
}

void javacardx_security_pqc_PQCKeyImpl_setEncoded(
    jref_t receiver, std::shared_ptr<JC_Array> value, jshort_t offset,
    jshort_t length) {
  const PqcKey key = receiverKey(receiver);
  requireEnabled(key.set);
  byteArray(value);
  if (length != static_cast<jshort_t>(key.encodingLength()))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkRange(value, offset, static_cast<uint32_t>(length));
#if JC_SUPPORT_PQC_ANY
  const PqcParameterSet &set = *key.set;
  // Buffers: encoding | derived public key (ML-DSA sk validation).
  const bool validateDsaSk = !set.kem && key.isPrivate() && !key.isSeed();
  Workspace ws(set.kem ? "kem_import" : "dsa_import", set,
               static_cast<size_t>(length) + (validateDsaSk ? set.pub : 0));
  uint8_t *encoding = ws.buffer(0);
  value->copyPrimitiveEntriesTo(static_cast<uint16_t>(offset),
                                static_cast<uint16_t>(length), encoding);
  // Key consistency checks (FIPS 203 7.2/7.3 modulus and hash checks; ML-DSA
  // secret keys are validated by re-deriving their public key).
  int rc = 0;
  if (set.kem && !key.isSeed()) {
    auto f = [&]() noexcept {
      return key.isPrivate() ? pqc_mlkem_check_dk(set.number, encoding)
                             : pqc_mlkem_check_ek(set.number, encoding);
    };
    rc = ws.run(f);
  } else if (validateDsaSk) {
    uint8_t *pk = encoding + length;
    auto f = [&]() noexcept {
      return pqc_mldsa_pk_from_sk(set.number, pk, encoding);
    };
    rc = ws.run(f);
  }
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  storeKey(key, encoding);
#else
  (void)offset;
#endif
}

jshort_t javacardx_security_pqc_PQCKeyImpl_getPublicKey(
    jref_t receiver, std::shared_ptr<JC_Array> value, jshort_t offset) {
  const PqcKey key = receiverKey(receiver);
  if (!key.set->kem || !key.isPrivate()) throw Exceptions::SecurityException;
  if (!initialized(key))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  const PqcParameterSet &set = *key.set;
  checkRange(value, offset, set.pub);
  requireEnabled(key.set);
#if JC_SUPPORT_MLKEM_ANY
  Workspace ws("kem_pubkey", set, set.priv + set.pub);
  uint8_t *dk = ws.buffer(0);
  uint8_t *ek = dk + set.priv;
  if (key.isSeed()) {
    if (expandPrivate(ws, key, dk, ek) != 0)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
  } else {
    // FIPS 203: dk = dk_PKE (384k bytes) || ek || H(ek) || z.
    readKey(key, dk);
    std::memcpy(ek, dk + (set.priv - set.pub - 64), set.pub);
  }
  value->copyPrimitiveEntriesFrom(static_cast<uint16_t>(offset), set.pub, ek,
                                  false);
  return static_cast<jshort_t>(offset + set.pub);
#else
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
}

// ---------------------------------------------------------------------------
// KEM
// ---------------------------------------------------------------------------

jref_t javacardx_crypto_kem_KEM_getInstance(jshort_t algorithm, jref_t params,
                                            jshort_t mode) {
  if (algorithm == kAlgDhKem || algorithm != kAlgMlKem)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  const PqcParameterSet *set = namedParameter(params);
  if (set == nullptr || !set->kem) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  requireEnabled(set);
  const jshort_t known = kModeEncapsulation | kModeDecapsulation | kModeExpand;
  if ((mode & ~known) != 0 ||
      (mode & (kModeEncapsulation | kModeDecapsulation)) == 0 ||
      ((mode & kModeExpand) != 0 && (mode & kModeDecapsulation) == 0))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  Heap &heap = Context::getActiveContext().getHeap();
  const jref_t reference =
      heap.addInstance(ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO_KEM, kemClass(mode));
  auto instance = heap.getInstance(reference);
  if (!instance) throw Exceptions::RuntimeException;
  instance->setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_KEM_KEMIMPL_KEMIMPL_ALGORITHM, algorithm);
  instance->setField_Short(
      ROM_FIELD_INDEX_JAVACARDX_CRYPTO_KEM_KEMIMPL_KEMIMPL_PARAMETERSET,
      set->name);
  instance->setField_Short(ROM_FIELD_INDEX_JAVACARDX_CRYPTO_KEM_KEMIMPL_KEMIMPL_MODE,
                           mode);
  return reference;
}

jshort_t javacardx_crypto_kem_KEMImpl_decapsulate(
    jref_t receiver, jref_t privateKey, std::shared_ptr<JC_Array> cipherText,
    jshort_t cipherTextOffset, std::shared_ptr<JC_Array> sharedSecret,
    jshort_t sharedSecretOffset) {
  return decapsulate(receiver, privateKey, cipherText, cipherTextOffset,
                     SecretSink::Bytes, sharedSecret, sharedSecretOffset,
                     jref_t(), 0, 0);
}

jshort_t javacardx_crypto_kem_KEMImpl_decapsulate(
    jref_t receiver, jref_t privateKey, std::shared_ptr<JC_Array> cipherText,
    jshort_t cipherTextOffset, jref_t sharedSecret, jshort_t from, jshort_t to) {
  return decapsulate(receiver, privateKey, cipherText, cipherTextOffset,
                     SecretSink::Key, nullptr, 0, sharedSecret, from, to);
}

namespace {
std::unique_ptr<uint8_t[]> readEncKey(jref_t receiver,
                                      const std::shared_ptr<JC_Array> &encKey,
                                      jshort_t offset, jshort_t length) {
  const KemState state = receiverKem(receiver);
  byteArray(encKey);
  if (length != static_cast<jshort_t>(state.set->pub))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkRange(encKey, offset, static_cast<uint32_t>(length));
  std::unique_ptr<uint8_t[]> ek;
  try {
    RecoverableAllocationScope scope;
    ek.reset(new uint8_t[state.set->pub]);
  } catch (const std::bad_alloc &) {
    throwNoResource();
  }
  encKey->copyPrimitiveEntriesTo(static_cast<uint16_t>(offset),
                                 static_cast<uint16_t>(length), ek.get());
  return ek;
}
} // namespace

jshort_t javacardx_crypto_kem_KEMImpl_encapsulate(
    jref_t receiver, std::shared_ptr<JC_Array> encKey, jshort_t encKeyOffset,
    jshort_t encKeyLength, std::shared_ptr<JC_Array> cipherText,
    jshort_t cipherTextOffset, std::shared_ptr<JC_Array> sharedSecret,
    jshort_t sharedSecretOffset) {
  auto ek = readEncKey(receiver, encKey, encKeyOffset, encKeyLength);
  return encapsulate(receiver, ek.get(), jref_t(), cipherText, cipherTextOffset,
                     SecretSink::Bytes, sharedSecret, sharedSecretOffset,
                     jref_t(), 0, 0);
}

jshort_t javacardx_crypto_kem_KEMImpl_encapsulate(
    jref_t receiver, std::shared_ptr<JC_Array> encKey, jshort_t encKeyOffset,
    jshort_t encKeyLength, std::shared_ptr<JC_Array> cipherText,
    jshort_t cipherTextOffset, jref_t sharedSecret, jshort_t from, jshort_t to) {
  auto ek = readEncKey(receiver, encKey, encKeyOffset, encKeyLength);
  return encapsulate(receiver, ek.get(), jref_t(), cipherText, cipherTextOffset,
                     SecretSink::Key, nullptr, 0, sharedSecret, from, to);
}

jshort_t javacardx_crypto_kem_KEMImpl_encapsulate(
    jref_t receiver, jref_t publicKey, std::shared_ptr<JC_Array> cipherText,
    jshort_t cipherTextOffset, std::shared_ptr<JC_Array> sharedSecret,
    jshort_t sharedSecretOffset) {
  return encapsulate(receiver, nullptr, publicKey, cipherText, cipherTextOffset,
                     SecretSink::Bytes, sharedSecret, sharedSecretOffset,
                     jref_t(), 0, 0);
}

jshort_t javacardx_crypto_kem_KEMImpl_encapsulate(
    jref_t receiver, jref_t publicKey, std::shared_ptr<JC_Array> cipherText,
    jshort_t cipherTextOffset, jref_t sharedSecret, jshort_t from, jshort_t to) {
  return encapsulate(receiver, nullptr, publicKey, cipherText, cipherTextOffset,
                     SecretSink::Key, nullptr, 0, sharedSecret, from, to);
}

jshort_t javacardx_crypto_kem_KEMImpl_sharedSecretSize(jref_t receiver) {
  (void)receiverKem(receiver);
  return static_cast<jshort_t>(kSharedSecretBytes);
}

jshort_t javacardx_crypto_kem_KEMImpl_cipherTextSize(jref_t receiver) {
  return static_cast<jshort_t>(receiverKem(receiver).set->extra);
}

// ---------------------------------------------------------------------------
// ML-DSA in javacard.security.Signature
// ---------------------------------------------------------------------------

#if JC_SUPPORT_MLDSA_ANY
namespace {
constexpr uint8_t kSigModeSign = 1;
constexpr uint8_t kSigModeVerify = 2;
constexpr uint16_t kMuStateBytes = sizeof(pqc_shake256_t);
constexpr uint16_t kMuBytes = 64;

uint8_t sigCipher(JC_Instance &signature) {
  return static_cast<uint8_t>(signature.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_CIPHERALGORITHM));
}
bool externalMu(uint8_t cipher) {
  return cipher == kSigCipherMlDsaExternalMu ||
         cipher == kSigCipherMlDsaExternalMuExpandSeed;
}
bool expandSeedAllowed(uint8_t cipher) {
  return cipher == kSigCipherMlDsaExpandSeed ||
         cipher == kSigCipherMlDsaExternalMuExpandSeed;
}

// [state after the mu prefix | running state], CLEAR_ON_RESET, allocated once
// per Signature object and reused by every later init (INV-16).
std::shared_ptr<JC_Array> muStates(JC_Instance &signature) {
  Heap &heap = Context::getActiveContext().getHeap();
  jref_t reference = signature.getField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER);
  if (!reference.isNullPointer()) {
    auto array = heap.getArray(reference);
    if (array && array->getType() == JAVA_ARRAY_T_BYTE &&
        array->size() == 2 * kMuStateBytes)
      return array;
  }
  reference = heap.addTransientArray(2 * kMuStateBytes, JAVA_ARRAY_T_BYTE,
                                     ClearEvent::CLEAR_ON_RESET);
  signature.setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MESSAGEBUFFER,
      reference);
  auto array = heap.getArray(reference);
  if (!array) throw Exceptions::RuntimeException;
  return array;
}

void loadMuState(JC_Array &states, uint16_t slot, pqc_shake256_t &state) {
  states.copyPrimitiveEntriesTo(static_cast<uint16_t>(slot * kMuStateBytes),
                                kMuStateBytes,
                                reinterpret_cast<uint8_t *>(&state));
}
void saveMuState(JC_Array &states, uint16_t slot, const pqc_shake256_t &state) {
  states.copyPrimitiveEntriesFrom(static_cast<uint16_t>(slot * kMuStateBytes),
                                  kMuStateBytes,
                                  reinterpret_cast<const uint8_t *>(&state),
                                  false);
}

void absorbArray(pqc_shake256_t &state, JC_Array &input, jshort_t offset,
                 jshort_t length) {
  uint8_t block[128];
  for (uint16_t at = 0; at < static_cast<uint16_t>(length);) {
    const uint16_t n = static_cast<uint16_t>(
        length - at < static_cast<jshort_t>(sizeof(block)) ? length - at
                                                           : sizeof(block));
    input.copyPrimitiveEntriesTo(static_cast<uint16_t>(offset + at), n, block);
    pqc_shake256_absorb(&state, block, n);
    at = static_cast<uint16_t>(at + n);
  }
}

PqcKey signatureKey(JC_Instance &signature, uint8_t mode) {
  if (!signature.getNativeSessionField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED))
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  if (mode != 0 &&
      signature.getNativeSessionField_Byte(
          ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE) != mode)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  PqcKey key;
  if (!loadPqcKey(signature.getField_Reference(
                      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY),
                  key) ||
      key.set->kem)
    throwCryptoException(CryptoExceptionReason::InvalidInit);
  if (!initialized(key))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  return key;
}

// mu for the current message, then rewinds the running state to the state
// after init so the object can process the next message (Signature reset).
void finishMu(JC_Instance &signature, const std::shared_ptr<JC_Array> &input,
              jshort_t offset, jshort_t length, uint8_t *mu) {
  if (externalMu(sigCipher(signature))) {
    if (length != static_cast<jshort_t>(kMuBytes))
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    input->copyPrimitiveEntriesTo(static_cast<uint16_t>(offset), kMuBytes, mu);
    return;
  }
  auto states = muStates(signature);
  pqc_shake256_t state;
  loadMuState(*states, 1, state);
  absorbArray(state, *input, offset, length);
  pqc_shake256_final(&state, mu, kMuBytes);
  loadMuState(*states, 0, state);
  saveMuState(*states, 1, state);
  wipe(&state, sizeof(state));
}
} // namespace

void mldsaSignatureInit(JC_Instance &signature, jref_t keyReference,
                        uint8_t mode, const std::shared_ptr<JC_Array> &ctx,
                        jshort_t ctxOffset, jshort_t ctxLength) {
  if (mode != kSigModeSign && mode != kSigModeVerify)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  PqcKey key;
  if (keyReference.isNullPointer()) throw Exceptions::NullPointerException;
  if (!loadPqcKey(keyReference, key) || key.set->kem ||
      key.isPrivate() != (mode == kSigModeSign))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  requireEnabled(key.set);
  if (!initialized(key))
    throwCryptoException(CryptoExceptionReason::UninitializedKey);
  const uint8_t cipher = sigCipher(signature);
  if (mode == kSigModeSign && key.isSeed() && !expandSeedAllowed(cipher))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  uint8_t context[255];
  uint8_t contextLength = 0;
  if (ctx) {
    if (externalMu(cipher) || ctxLength < 0 || ctxLength > 255)
      throwCryptoException(CryptoExceptionReason::IllegalValue);
    checkRange(ctx, ctxOffset, static_cast<uint32_t>(ctxLength));
    contextLength = static_cast<uint8_t>(ctxLength);
    ctx->copyPrimitiveEntriesTo(static_cast<uint16_t>(ctxOffset),
                                contextLength, context);
  }
  // Uninitialize first so a failed init leaves no usable stale state.
  signature.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 0);
  if (!externalMu(cipher)) {
    const PqcParameterSet &set = *key.set;
    uint8_t tr[64];
    if (mode == kSigModeSign && !key.isSeed()) {
      // FIPS 204 skEncode: rho (32) || K (32) || tr (64) || ...
      chunk(key, 0)->copyPrimitiveEntriesTo(64, 64, tr);
    } else {
      // tr = H(pk, 64); a seed key is expanded to obtain pk.
      Workspace ws("dsa_tr", set, set.pub + set.priv);
      uint8_t *pk = ws.buffer(0);
      if (mode == kSigModeVerify) {
        readKey(key, pk);
      } else if (expandPrivate(ws, key, pk + set.pub, pk) != 0) {
        throwCryptoException(CryptoExceptionReason::IllegalValue);
      }
      pqc_mldsa_tr_from_pk(pk, set.pub, tr);
    }
    pqc_shake256_t state;
    pqc_mldsa_mu_init(&state, tr, context, contextLength);
    auto states = muStates(signature);
    saveMuState(*states, 0, state);
    saveMuState(*states, 1, state);
    wipe(&state, sizeof(state));
  }
  signature.setField_Reference(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_KEY, keyReference);
  signature.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_MODE,
      static_cast<jbyte_t>(mode));
  signature.setNativeSessionField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_SIGNATURE_SIGNATURE_INITIALIZED, 1);
}

void mldsaSignatureUpdate(JC_Instance &signature,
                          const std::shared_ptr<JC_Array> &input,
                          jshort_t offset, jshort_t length) {
  if (externalMu(sigCipher(signature)))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  (void)signatureKey(signature, 0);
  checkRange(input, offset, static_cast<uint32_t>(length < 0 ? 0 : length));
  if (length <= 0) return;
  auto states = muStates(signature);
  pqc_shake256_t state;
  loadMuState(*states, 1, state);
  absorbArray(state, *input, offset, length);
  saveMuState(*states, 1, state);
  wipe(&state, sizeof(state));
}

jshort_t mldsaSignatureLength(JC_Instance &signature) {
  return static_cast<jshort_t>(signatureKey(signature, 0).set->extra);
}

jshort_t mldsaSignatureSign(JC_Instance &signature,
                            const std::shared_ptr<JC_Array> &input,
                            jshort_t offset, jshort_t length,
                            const std::shared_ptr<JC_Array> &output,
                            jshort_t outputOffset) {
  const PqcKey key = signatureKey(signature, kSigModeSign);
  const PqcParameterSet &set = *key.set;
  checkRange(input, offset, static_cast<uint32_t>(length < 0 ? 0 : length));
  if (length < 0) throw Exceptions::ArrayIndexOutOfBoundsException;
  checkRange(output, outputOffset, set.extra);
  uint8_t mu[kMuBytes];
  finishMu(signature, input, offset, length, mu);
  // Buffers: sk | signature | pk scratch (seed expansion only).
  Workspace ws("dsa_sign", set,
               set.priv + set.extra + (key.isSeed() ? set.pub : 0));
  uint8_t *sk = ws.buffer(0);
  uint8_t *sig = sk + set.priv;
  if (expandPrivate(ws, key, sk, sig + set.extra) != 0)
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  uint8_t rnd[32];
  fillRandom(rnd, sizeof(rnd)); // hedged variant (FIPS 204 Algorithm 2)
  auto f = [&]() noexcept {
    return pqc_mldsa_sign_mu(set.number, sig, mu, rnd, sk);
  };
  const int rc = ws.run(f);
  wipe(rnd, sizeof(rnd));
  wipe(mu, sizeof(mu));
  if (rc != 0) throwCryptoException(CryptoExceptionReason::IllegalValue);
  output->copyPrimitiveEntriesFrom(static_cast<uint16_t>(outputOffset),
                                   set.extra, sig, false);
  return static_cast<jshort_t>(set.extra);
}

jbool_t mldsaSignatureVerify(JC_Instance &signature,
                             const std::shared_ptr<JC_Array> &input,
                             jshort_t offset, jshort_t length,
                             const std::shared_ptr<JC_Array> &sigArray,
                             jshort_t sigOffset, jshort_t sigLength) {
  const PqcKey key = signatureKey(signature, kSigModeVerify);
  const PqcParameterSet &set = *key.set;
  checkRange(input, offset, static_cast<uint32_t>(length < 0 ? 0 : length));
  if (length < 0) throw Exceptions::ArrayIndexOutOfBoundsException;
  checkRange(sigArray, sigOffset,
             static_cast<uint32_t>(sigLength < 0 ? 0 : sigLength));
  if (sigLength < 0) throw Exceptions::ArrayIndexOutOfBoundsException;
  uint8_t mu[kMuBytes];
  finishMu(signature, input, offset, length, mu);
  if (sigLength != static_cast<jshort_t>(set.extra)) return FALSE;
  Workspace ws("dsa_verify", set, set.pub + set.extra);
  uint8_t *pk = ws.buffer(0);
  uint8_t *sig = pk + set.pub;
  readKey(key, pk);
  sigArray->copyPrimitiveEntriesTo(static_cast<uint16_t>(sigOffset), set.extra,
                                   sig);
  auto f = [&]() noexcept {
    return pqc_mldsa_verify_mu(set.number, sig, mu, pk);
  };
  return ws.run(f) == 0 ? TRUE : FALSE;
}
#else
void mldsaSignatureInit(JC_Instance &, jref_t, uint8_t,
                        const std::shared_ptr<JC_Array> &, jshort_t, jshort_t) {
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}
void mldsaSignatureUpdate(JC_Instance &, const std::shared_ptr<JC_Array> &,
                          jshort_t, jshort_t) {
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}
jshort_t mldsaSignatureLength(JC_Instance &) {
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}
jshort_t mldsaSignatureSign(JC_Instance &, const std::shared_ptr<JC_Array> &,
                            jshort_t, jshort_t,
                            const std::shared_ptr<JC_Array> &, jshort_t) {
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}
jbool_t mldsaSignatureVerify(JC_Instance &, const std::shared_ptr<JC_Array> &,
                             jshort_t, jshort_t,
                             const std::shared_ptr<JC_Array> &, jshort_t,
                             jshort_t) {
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}
#endif // JC_SUPPORT_MLDSA_ANY

} // namespace jcvm

#if JC_SUPPORT_PQC_ANY
extern "C" int choupi_pqc_progress(void) {
#ifndef PC_VERSION
  // The VM deadline is enforced when the native call returns; signing has a
  // static attempt bound, so the hook never pauses it.
  (void)choupi_native_crypto_progress();
#endif
  return 1;
}
#endif
