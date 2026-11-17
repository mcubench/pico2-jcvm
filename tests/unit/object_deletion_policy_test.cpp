#include "object_deletion_policy.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks = 0;

void check(const bool condition, const char *name) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", name);
    std::exit(EXIT_FAILURE);
  }
}
} // namespace

int main() {
  check(!pico2_jcvm::may_service_object_deletion(false, true, false),
        "no_request");
  check(!pico2_jcvm::may_service_object_deletion(true, false, false),
        "active_java_frames");
  check(!pico2_jcvm::may_service_object_deletion(true, true, true),
        "active_persistent_transaction");
  check(pico2_jcvm::may_service_object_deletion(true, true, false),
        "safe_apdu_boundary");
  check(!pico2_jcvm::should_request_automatic_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes + 1, false, true,
            false, true),
        "automatic_request_above_watermark");
  check(pico2_jcvm::should_request_automatic_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes, false, true, false,
            true),
        "automatic_request_at_watermark");
  check(!pico2_jcvm::should_request_automatic_object_deletion(
            0, true, true, false, true),
        "automatic_request_does_not_replace_pending_request");
  check(!pico2_jcvm::should_request_automatic_object_deletion(
            0, false, false, false, true),
        "automatic_request_requires_empty_stack");
  check(!pico2_jcvm::should_request_automatic_object_deletion(
            0, false, true, true, true),
        "automatic_request_defers_during_transaction");
  check(!pico2_jcvm::should_request_automatic_object_deletion(
            0, false, true, false, false),
        "automatic_request_honors_retry_cooldown");
  check(!pico2_jcvm::should_compact_after_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes + 1, 8, 0),
        "deletion_does_not_compact_above_low_watermark");
  check(!pico2_jcvm::should_compact_after_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes + 1, 8,
            pico2_jcvm::kObjectDeletionTombstoneCompactThreshold - 1),
        "deletion_does_not_compact_below_tombstone_threshold");
  check(pico2_jcvm::should_compact_after_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes + 1, 8,
            pico2_jcvm::kObjectDeletionTombstoneCompactThreshold),
        "deletion_compacts_at_tombstone_threshold_above_watermark");
  check(!pico2_jcvm::should_compact_after_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes + 1, 0,
            pico2_jcvm::kObjectDeletionTombstoneCompactThreshold),
        "tombstone_threshold_requires_erased_records");
  check(!pico2_jcvm::should_compact_after_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes, 0, 0),
        "deletion_does_not_compact_without_erased_records");
  check(pico2_jcvm::should_compact_after_object_deletion(
            pico2_jcvm::kObjectDeletionLowWatermarkBytes, 1, 0),
        "applet_or_automatic_deletion_compacts_at_low_watermark");
  check(pico2_jcvm::should_compact_after_object_deletion(0, 8, 0),
        "deletion_compacts_below_low_watermark");
  pico2_jcvm::AutomaticObjectDeletionThrottle throttle;
  throttle.completed(0);
  check(throttle.cooldown_apdus ==
            pico2_jcvm::kObjectDeletionRetryCooldownApdus,
        "empty_reclaim_starts_retry_cooldown");
  for (uint8_t index = 0;
       index < pico2_jcvm::kObjectDeletionRetryCooldownApdus; ++index) {
    throttle.on_apdu_boundary();
  }
  check(throttle.cooldown_apdus == 0,
        "retry_cooldown_expires_after_bounded_apdus");
  throttle.completed(1);
  check(throttle.cooldown_apdus == 0,
        "successful_reclaim_does_not_delay_followup");
  std::printf("TEST:PASS object_deletion_policy checks=%u\n", checks);
  return EXIT_SUCCESS;
}
