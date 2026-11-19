// Native OwnerPIN state (RV-20261106-04, FLASH-03).
//
// Java Card keeps OwnerPIN's try counter, validated flag and blocking state
// out of transactions, and the try counter must be decremented durably before
// the PIN comparison result can be observed. Ordinary persistent field writes
// reach flash only when the APDU completes and are rolled back by
// abortTransaction, so a timed power cut after the comparison would restore
// the counter. These natives:
//   - write the try counter outside the transaction and commit that single
//     record to flash before returning;
//   - keep the validated flag of persistent PINs in RAM, so it is never
//     written to flash, is not transactional and clears on card reset;
//   - compare PIN bytes in constant time.

#include "context.hpp"
#include "exceptions.hpp"
#include "heap.hpp"
#include "javacard-rom-metadata.h"
#include "jc_handlers/flashmemory.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_owner_pin.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>

namespace jcvm {
namespace {

// Validated persistent PINs, identified by their persistent tag. A full
// table forgets the oldest entry, which only makes a PIN read as not
// validated (the applet asks for the PIN again).
constexpr size_t kValidatedPinCapacity = 8;
std::array<fs::Tag, kValidatedPinCapacity> validated_pins{};
size_t validated_pin_count = 0;
size_t validated_pin_next = 0;

bool same_tag(const fs::Tag &left, const fs::Tag &right) noexcept {
  return left.len == right.len &&
         std::memcmp(left.value, right.value, left.len) == 0;
}

std::shared_ptr<JC_Instance> owner_pin_instance(const jref_t reference) {
  if (reference.isNullPointer() || reference.isArray()) {
    throw Exceptions::NullPointerException;
  }
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance) throw Exceptions::NullPointerException;
  return instance;
}

int find_validated(const fs::Tag &tag) noexcept {
  for (size_t index = 0; index < validated_pin_count; ++index) {
    if (same_tag(validated_pins[index], tag)) return static_cast<int>(index);
  }
  return -1;
}

} // namespace

void OwnerPinNative::clearValidated() noexcept {
  validated_pin_count = 0;
  validated_pin_next = 0;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_ownerPinSetTries(
    const jref_t pin, const jbyte_t tries) {
  owner_pin_instance(pin)->setField_ByteDurable(
      ROM_FIELD_INDEX_JAVACARD_FRAMEWORK_OWNERPIN_OWNERPIN_TRIESLEFT, tries);
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_ownerPinGetValidated(
    const jref_t pin) {
  const auto instance = owner_pin_instance(pin);
  if (!instance->isPersistent()) {
    return byte2bool(instance->getField_Byte(
        ROM_FIELD_INDEX_JAVACARD_FRAMEWORK_OWNERPIN_OWNERPIN_FLAG));
  }
  return find_validated(instance->getPersistentTag()) >= 0 ? TRUE : FALSE;
}

void fr_gouv_ssi_nativeimpl_NativeImplementation_ownerPinSetValidated(
    const jref_t pin, const jbool_t value) {
  const auto instance = owner_pin_instance(pin);
  if (!instance->isPersistent()) {
    // A RAM-only object: its field is never written to flash.
    instance->setField_Byte(
        ROM_FIELD_INDEX_JAVACARD_FRAMEWORK_OWNERPIN_OWNERPIN_FLAG,
        value == FALSE ? 0 : 1);
    return;
  }
  const fs::Tag tag = instance->getPersistentTag();
  const int index = find_validated(tag);
  if (value == FALSE) {
    if (index >= 0) {
      validated_pins[static_cast<size_t>(index)] =
          validated_pins[validated_pin_count - 1];
      --validated_pin_count;
      if (validated_pin_next > validated_pin_count) validated_pin_next = 0;
    }
    return;
  }
  if (index >= 0) return;
  if (validated_pin_count < kValidatedPinCapacity) {
    validated_pins[validated_pin_count++] = tag;
    return;
  }
  validated_pins[validated_pin_next] = tag;
  validated_pin_next = (validated_pin_next + 1) % kValidatedPinCapacity;
}

jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_ownerPinCompare(
    const std::shared_ptr<JC_Array> pin, const jshort_t offset,
    const std::shared_ptr<JC_Array> value, const jbyte_t length) {
  if (!pin || !value) throw Exceptions::NullPointerException;
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + static_cast<uint8_t>(length) >
          pin->size() ||
      static_cast<uint8_t>(length) > value->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  // Constant time in the PIN contents: always visit every byte.
  uint8_t difference = 0;
  for (uint16_t index = 0; index < static_cast<uint8_t>(length); ++index) {
    difference |= static_cast<uint8_t>(
        pin->getByteEntry(static_cast<uint16_t>(offset + index)) ^
        value->getByteEntry(index));
  }
  return difference == 0 ? TRUE : FALSE;
}

} // namespace jcvm
