#pragma once

#include <cstdint>

#include "native_progress_guard.hpp"

namespace choupi {

inline constexpr uint8_t kMaxPendingCcidSubmitFailures = 32;

inline bool record_pending_ccid_submit_failure(uint8_t &failure_count) {
  if (failure_count < kMaxPendingCcidSubmitFailures) ++failure_count;
  return failure_count >= kMaxPendingCcidSubmitFailures;
}

template <typename Pending, typename Dispatch>
void dispatch_pending_ccid_task(bool &task_active, Pending &message_pending,
                                Dispatch &&dispatch) {
  NativeProgressGuard task_guard(task_active);
  if (!task_guard || !message_pending) return;
  message_pending = false;
  dispatch();
}

template <typename Hooks>
void service_ccid_native_progress(bool &progress_active,
                                  bool &native_operation_active,
                                  bool &extension_in_flight,
                                  uint32_t &last_extension_us,
                                  uint8_t sequence, Hooks &&hooks) {
  NativeProgressGuard progress_guard(progress_active);
  if (!progress_guard || !native_operation_active || !hooks.endpoint_ready())
    return;

  const uint32_t now = hooks.now_us();
  const bool extension_due =
      static_cast<uint32_t>(now - last_extension_us) >= 1000000U;
  if (!extension_in_flight && !extension_due) return;

  hooks.service_usb();
  if (!native_operation_active || extension_in_flight || !extension_due ||
      hooks.endpoint_busy())
    return;

  if (hooks.submit_extension(sequence)) {
    extension_in_flight = true;
    last_extension_us = now;
  }
}

template <typename InFlight, typename Deadline, typename IsExpired,
          typename ServiceUsb, typename Delay>
void wait_for_ccid_extension(InFlight &extension_in_flight, Deadline deadline,
                             IsExpired &&is_expired, ServiceUsb &&service_usb,
                             Delay &&delay) {
  while (extension_in_flight && !is_expired(deadline)) {
    service_usb();
    if (extension_in_flight) delay(100);
  }
}

template <typename InFlight, typename Wait, typename Submit>
bool submit_final_after_ccid_extension(InFlight &extension_in_flight,
                                       Wait &&wait, Submit &&submit) {
  if (extension_in_flight) wait();
  if (extension_in_flight) return false;
  return submit();
}

template <typename Pending, typename InFlight, typename EndpointBusy,
          typename Submit>
bool retry_pending_ccid_transfer(Pending &pending, const InFlight in_flight,
                                EndpointBusy &&endpoint_busy,
                                Submit &&submit) {
  if (!pending || in_flight || endpoint_busy()) return false;
  if (!submit()) return false;
  pending = false;
  return true;
}

} // namespace choupi
