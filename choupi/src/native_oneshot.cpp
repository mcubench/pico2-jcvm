// JCRE-owned temporary OneShot instances (Java Card 3.0.5, javacardx.crypto
// Cipher.OneShot and javacard.security Signature/MessageDigest/RandomData
// .OneShot).
//
// Specification points implemented here:
//   - open() returns a JCRE-owned temporary Entry Point Object; references to
//     it cannot be stored in fields, statics or array components (enforced by
//     Heap::isTemporaryGlobalInstance at every store).
//   - The instance is bound to the context that opened it; calls from another
//     context throw SecurityException.
//   - close(), the return of any applet entry point (end of the APDU), a card
//     reset or a tear release it back to the JCRE; afterwards its methods
//     throw CryptoException.ILLEGAL_USE.
//   - Nothing is ever written to persistent memory: the OneShot objects live
//     in the runtime-wide RAM heap, the per-open delegate (an ordinary native
//     Cipher/Signature/MessageDigest object holding the algorithm state) is a
//     volatile object referenced only from this table, and its transient work
//     buffers are zeroed and freed on release. A power loss therefore loses
//     all OneShot state, as required.
// Each kind has kSlotsPerKind instances (the platform must support at least
// one); opening more at the same time raises SystemException.NO_RESOURCE.

#include "native_oneshot.hpp"

#include "context.hpp"
#include "exceptions.hpp"
#include "heap.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "jc_handlers/jc_export.hpp"
#include "native_crypto_exception.hpp"

#include <array>
#include <cstdint>

namespace jcvm {

// Native factories of the ordinary API classes (generated JNI bindings).
jref_t javacardx_crypto_Cipher_getInstance(jbyte_t cipherAlgorithm,
                                            jbyte_t paddingAlgorithm,
                                            jbool_t externalAccess);
jref_t javacard_security_Signature_getInstance(jbyte_t digest, jbyte_t cipher,
                                               jbyte_t padding,
                                               jbool_t externalAccess);
jref_t javacard_security_MessageDigest_getInstance(jbyte_t algorithm,
                                                   jbool_t externalAccess);
#if JC_SUPPORT_SECURE_RANDOM
void fr_gouv_ssi_nativeimpl_RandomDataNativeBridge_generateRandomData(
    std::shared_ptr<JC_Array> buffer, jshort_t offset, jshort_t length);
#endif

namespace {

enum Kind : uint8_t {
  kCipher = 1,
  kSignature = 2,
  kMessageDigest = 3,
  kRandomData = 4,
};
constexpr uint8_t kKinds = 4;
constexpr size_t kSlotsPerKind = 2;
constexpr uint8_t kAlgSecureRandom = 2; // RandomData.ALG_SECURE_RANDOM

struct Slot {
  jref_t instance;  // runtime-wide temporary global instance, created once
  jref_t delegate;  // volatile object in the owner's heap, null when closed
  Context *owner = nullptr;
  bool open = false;
};

std::array<Slot, kKinds * kSlotsPerKind> slots{};

uint16_t classOffset(const jpackage_ID_t package, const uint8_t token) {
  const uint16_t offset =
      Export_Handler(Package(package)).getExportedClassOffset(token);
  return HTONS(offset);
}

jref_t newOneShotInstance(const uint8_t kind) {
  jpackage_ID_t package = 0;
  uint16_t class_index = 0;
  switch (kind) {
  case kCipher:
    package = ROM_PACKAGE_INDEX_JAVACARDX_CRYPTO;
    class_index = classOffset(
        package, ROM_CLASS_EXPORT_TOKEN_JAVACARDX_CRYPTO_CIPHER_ONESHOT);
    break;
  case kSignature:
    package = ROM_PACKAGE_INDEX_JAVACARD_SECURITY;
    class_index = classOffset(
        package, ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_SIGNATURE_ONESHOT);
    break;
  case kMessageDigest:
    package = ROM_PACKAGE_INDEX_JAVACARD_SECURITY;
    class_index = classOffset(
        package,
        ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_MESSAGEDIGEST_ONESHOT);
    break;
  default:
    package = ROM_PACKAGE_INDEX_JAVACARD_SECURITY;
    class_index = classOffset(
        package, ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_RANDOMDATA_ONESHOT);
    break;
  }
  const jref_t reference = Heap::addGlobalInstance(package, class_index);
  Heap::markTemporaryGlobalInstance(reference);
  return reference;
}

void releaseSlot(Slot &slot) noexcept {
  if (slot.owner != nullptr && !slot.delegate.isNullPointer()) {
    slot.owner->getHeap().releaseRuntimeDelegate(slot.delegate);
  }
  slot.delegate = jref_t();
  slot.owner = nullptr;
  slot.open = false;
}

Slot &slotFor(const jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  for (auto &slot : slots) {
    if (!slot.instance.isNullPointer() &&
        slot.instance.compact() == reference.compact()) {
      return slot;
    }
  }
  // Not a OneShot created by open(): no usable state.
  throwCryptoException(CryptoExceptionReason::IllegalUse);
}

Slot &checkedOpenSlot(const jref_t reference) {
  Slot &slot = slotFor(reference);
  if (!slot.open) throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (slot.owner != &Context::getActiveContext()) {
    throw Exceptions::SecurityException;
  }
  return slot;
}

[[noreturn]] void throwNoResource() {
  Context::getActiveContext().setPendingSystemExceptionReason(
      5); // SystemException.NO_RESOURCE
  throw Exceptions::SystemException;
}

} // namespace

void OneShotNative::releaseAll() noexcept {
  for (auto &slot : slots) {
    if (slot.open) releaseSlot(slot);
  }
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotOpen(
    const jbyte_t kind_value, const jbyte_t first, const jbyte_t second,
    const jbyte_t third) {
  const uint8_t kind = static_cast<uint8_t>(kind_value);
  if (kind < kCipher || kind > kRandomData) {
    throw Exceptions::SecurityException;
  }
  Context &context = Context::getActiveContext();
  // The algorithm check comes first: an unsupported algorithm is reported as
  // NO_SUCH_ALGORITHM even when every instance is in use. The delegate is
  // created by the same factory as getInstance(), so OneShot support is
  // exactly the getInstance() support of the build profile (INV-08).
  jref_t delegate;
  switch (kind) {
  case kCipher:
    delegate = javacardx_crypto_Cipher_getInstance(first, second, FALSE);
    break;
  case kSignature:
    delegate =
        javacard_security_Signature_getInstance(first, second, third, FALSE);
    break;
  case kMessageDigest:
    delegate = javacard_security_MessageDigest_getInstance(first, FALSE);
    break;
  default:
#if JC_SUPPORT_SECURE_RANDOM
    if (static_cast<uint8_t>(first) != kAlgSecureRandom) {
      throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
    }
#else
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
    break;
  }

  Slot *free_slot = nullptr;
  const size_t base = static_cast<size_t>(kind - 1) * kSlotsPerKind;
  for (size_t index = base; index < base + kSlotsPerKind; ++index) {
    if (!slots[index].open) {
      free_slot = &slots[index];
      break;
    }
  }
  try {
    if (free_slot == nullptr) throwNoResource();
    if (free_slot->instance.isNullPointer()) {
      free_slot->instance = newOneShotInstance(kind);
    }
  } catch (...) {
    context.getHeap().releaseRuntimeDelegate(delegate);
    throw;
  }
  free_slot->delegate = delegate;
  free_slot->owner = &context;
  free_slot->open = true;
  return free_slot->instance;
}

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotDelegate(
    const jref_t oneShot) {
  return checkedOpenSlot(oneShot).delegate;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotClose(
    const jref_t oneShot) {
  Slot &slot = slotFor(oneShot);
  if (!slot.open) return; // closing again does nothing
  if (slot.owner != &Context::getActiveContext()) {
    throw Exceptions::SecurityException;
  }
  releaseSlot(slot);
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotRandom(
    const jref_t oneShot, std::shared_ptr<JC_Array> buffer,
    const jshort_t offset, const jshort_t length) {
  (void)checkedOpenSlot(oneShot);
#if JC_SUPPORT_SECURE_RANDOM
  fr_gouv_ssi_nativeimpl_RandomDataNativeBridge_generateRandomData(
      std::move(buffer), offset, length);
#else
  (void)buffer;
  (void)offset;
  (void)length;
  throwCryptoException(CryptoExceptionReason::IllegalUse);
#endif
}

} // namespace jcvm
