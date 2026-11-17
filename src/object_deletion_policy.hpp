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

// Erased records leave tombstones that keep their hash slot and key bytes
// until compaction. Compact after a deletion batch that erased records when
// persistent space is low, or when enough tombstones have accumulated since
// the last compaction. Without the second trigger, a long-running applet that
// creates and drops objects (JCAlgTest re-assigning its cached fields) builds
// up thousands of tombstones, inflating the file count and shrinking usable
// space, until the low watermark is reached.
inline constexpr uint32_t kObjectDeletionTombstoneCompactThreshold = 256;

constexpr bool should_compact_after_object_deletion(
    const uint32_t available_bytes, const uint32_t erased_records,
    const uint32_t tombstones_since_compaction) {
  return erased_records != 0 &&
         (available_bytes <= kObjectDeletionLowWatermarkBytes ||
          tombstones_since_compaction >=
              kObjectDeletionTombstoneCompactThreshold);
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
