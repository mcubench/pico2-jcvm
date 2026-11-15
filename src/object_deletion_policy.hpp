#pragma once

#include <cstdint>

namespace pico2_jcvm {

// Leave enough headroom for the largest expected persistent-write burst in a
// single APDU; the collector only runs after the APDU reaches a safe point.
inline constexpr uint32_t kObjectDeletionLowWatermarkBytes = 20u * 1024u;
inline constexpr uint8_t kObjectDeletionRetryCooldownApdus = 8;

constexpr bool may_service_object_deletion(const bool request_pending,
                                           const bool stack_empty,
                                           const bool transaction_active) {
  return request_pending && stack_empty && !transaction_active;
}

constexpr bool should_request_automatic_object_deletion(
    const uint32_t available_bytes, const bool request_pending,
    const bool stack_empty, const bool transaction_active,
    const bool retry_cooldown_elapsed) {
  return !request_pending && stack_empty && !transaction_active &&
         retry_cooldown_elapsed &&
         available_bytes <= kObjectDeletionLowWatermarkBytes;
}

struct AutomaticObjectDeletionThrottle {
  uint8_t cooldown_apdus = 0;

  void on_apdu_boundary() {
    if (cooldown_apdus != 0) --cooldown_apdus;
  }

  void completed(const uint32_t erased_records) {
    cooldown_apdus = erased_records == 0
                         ? kObjectDeletionRetryCooldownApdus
                         : 0;
  }
};

} // namespace pico2_jcvm
