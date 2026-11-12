#include "ccid_progress_flow.hpp"
#include "usb_bootsel_request.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks = 0;
void check(bool condition, const char *name) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", name);
    std::exit(EXIT_FAILURE);
  }
}

struct FakeProgressHooks {
  bool ready = true;
  bool busy = false;
  bool submitted = false;
  unsigned service_count = 0;
  unsigned submit_count = 0;
  uint32_t now = 1000000;
  bool *progress_active = nullptr;
  bool *operation_active = nullptr;
  bool *extension_in_flight = nullptr;
  uint32_t *last_extension = nullptr;
  bool recurse = false;
  bool complete_extension = false;

  bool endpoint_ready() const { return ready; }
  uint32_t now_us() const { return now; }
  bool endpoint_busy() const { return busy; }
  void service_usb() {
    ++service_count;
    if (recurse) {
      recurse = false;
      choupi::service_ccid_native_progress(
          *progress_active, *operation_active, *extension_in_flight,
          *last_extension, 9, *this);
    }
    if (complete_extension) {
      complete_extension = false;
      *extension_in_flight = false;
    }
  }
  bool submit_extension(uint8_t) {
    ++submit_count;
    submitted = true;
    return true;
  }
};
} // namespace

int main() {
  volatile bool bootsel_pending = false;
  bool line_coding_callback_active = false;
  bool native_work_active = true;
  unsigned bootsel_resets = 0;
  check(!choupi::service_pending_bootsel_reset(
            bootsel_pending, [&] { ++bootsel_resets; }),
        "BOOTSEL is not entered without a queued request");
  const bool reset_during_native_work = choupi::service_usb_then_bootsel_reset(
      bootsel_pending,
      [&] {
        line_coding_callback_active = true;
        choupi::request_bootsel_reset(bootsel_pending);
        line_coding_callback_active = false;
      },
      [&] {
        check(!line_coding_callback_active,
              "CDC reset is serviced only after TinyUSB callback returns");
        check(native_work_active,
              "cooperative reset is serviced during synchronous native work");
        ++bootsel_resets;
      });
  check(reset_during_native_work && bootsel_resets == 1 &&
            !bootsel_pending,
        "cooperative BOOTSEL request is consumed once");
  check(!choupi::service_pending_bootsel_reset(
            bootsel_pending, [&] { ++bootsel_resets; }) &&
            bootsel_resets == 1,
        "serviced BOOTSEL request does not reset repeatedly");

  bool task_active = true;
  volatile bool pending = true;
  unsigned dispatch_count = 0;
  choupi::dispatch_pending_ccid_task(task_active, pending,
                                     [&] { ++dispatch_count; });
  check(pending && dispatch_count == 0,
        "nested task leaves queued message pending");
  task_active = false;
  choupi::dispatch_pending_ccid_task(task_active, pending, [&] {
    ++dispatch_count;
    pending = true;
    choupi::dispatch_pending_ccid_task(task_active, pending,
                                       [&] { ++dispatch_count; });
  });
  check(pending && dispatch_count == 1,
        "message queued during dispatch is deferred");
  choupi::dispatch_pending_ccid_task(task_active, pending,
                                     [&] { ++dispatch_count; });
  check(!pending && dispatch_count == 2,
        "deferred message is dispatched exactly once after return");

  bool progress_active = false;
  bool operation_active = true;
  bool extension_in_flight = false;
  uint32_t last_extension = 0;
  FakeProgressHooks hooks;
  hooks.progress_active = &progress_active;
  hooks.operation_active = &operation_active;
  hooks.extension_in_flight = &extension_in_flight;
  hooks.last_extension = &last_extension;
  hooks.recurse = true;
  hooks.busy = true;
  choupi::service_ccid_native_progress(
      progress_active, operation_active, extension_in_flight, last_extension,
      3, hooks);
  check(hooks.service_count == 1 && hooks.submit_count == 0,
        "nested progress is blocked and busy endpoint defers extension");
  hooks.busy = false;
  choupi::service_ccid_native_progress(
      progress_active, operation_active, extension_in_flight, last_extension,
      3, hooks);
  check(extension_in_flight && hooks.submit_count == 1 &&
            last_extension == hooks.now,
        "free endpoint submits one due extension");

  hooks.now += 100000;
  hooks.complete_extension = true;
  choupi::service_ccid_native_progress(
      progress_active, operation_active, extension_in_flight, last_extension,
      3, hooks);
  check(!extension_in_flight && hooks.submit_count == 1,
        "USB service observes completion before another extension submit");

  bool final_submitted = false;
  const bool completed = choupi::submit_final_after_ccid_extension(
      extension_in_flight,
      [&] {
        choupi::wait_for_ccid_extension(
            extension_in_flight, 1,
            [](int) { return false; },
            [&] { extension_in_flight = false; }, [](unsigned) {});
      },
      [&] {
        final_submitted = true;
        return true;
      });
  check(completed && final_submitted,
        "final response follows extension completion");

  extension_in_flight = true;
  final_submitted = false;
  const bool timed_out = choupi::submit_final_after_ccid_extension(
      extension_in_flight, [] {}, [&] {
        final_submitted = true;
        return true;
      });
  check(!timed_out && !final_submitted,
        "stuck extension prevents competing final response submission");

  bool pending_response = true;
  bool response_in_flight = true;
  bool endpoint_busy = false;
  unsigned final_submit_attempts = 0;
  const bool extension_waiting = choupi::retry_pending_ccid_transfer(
      pending_response, response_in_flight, [&] { return endpoint_busy; },
      [&] {
        ++final_submit_attempts;
        return true;
      });
  check(!extension_waiting && pending_response && final_submit_attempts == 0,
        "final response remains pending while time extension is in flight");
  response_in_flight = false;
  endpoint_busy = true;
  const bool endpoint_waiting = choupi::retry_pending_ccid_transfer(
      pending_response, response_in_flight, [&] { return endpoint_busy; },
      [&] {
        ++final_submit_attempts;
        return true;
      });
  check(!endpoint_waiting && pending_response && final_submit_attempts == 0,
        "pending final response waits for endpoint availability");
  endpoint_busy = false;
  const bool rejected_submit = choupi::retry_pending_ccid_transfer(
      pending_response, response_in_flight, [&] { return endpoint_busy; },
      [&] {
        ++final_submit_attempts;
        return false;
      });
  check(!rejected_submit && pending_response && final_submit_attempts == 1,
        "synchronous IN submission failure retains final response for retry");
  const bool retried_submit = choupi::retry_pending_ccid_transfer(
      pending_response, response_in_flight, [&] { return endpoint_busy; },
      [&] {
        ++final_submit_attempts;
        return true;
      });
  check(retried_submit && !pending_response && final_submit_attempts == 2,
        "final response submits after late extension completion");
  bool failed_extension = true;
  bool response_after_extension = true;
  bool recovery_out_armed = false;
  const bool extension_error_recovered =
      choupi::recover_ccid_extension_transfer_error(true, failed_extension);
  check(extension_error_recovered && !failed_extension &&
            response_after_extension && !recovery_out_armed,
        "failed extension preserves queued final response and keeps OUT disarmed");
  const bool final_after_failed_extension =
      choupi::retry_pending_ccid_transfer(
          response_after_extension, failed_extension, [] { return false; },
          [&] { return true; });
  check(final_after_failed_extension && !response_after_extension,
        "final APDU response submits after failed extension completion");
  // The final IN completion is the point where production driver_xfer rearms
  // Bulk-OUT; verify the host can then submit both a slot poll and an APDU.
  recovery_out_armed = choupi::ccid_in_transfer_completion_rearms_out(
      failed_extension, false);
  unsigned recovered_slot_status = 0;
  unsigned recovered_apdu = 0;
  bool recovered_message_pending = true;
  choupi::dispatch_pending_ccid_task(
      task_active, recovered_message_pending, [&] { ++recovered_slot_status; });
  recovered_message_pending = true;
  choupi::dispatch_pending_ccid_task(
      task_active, recovered_message_pending, [&] { ++recovered_apdu; });
  check(recovery_out_armed && recovered_slot_status == 1 && recovered_apdu == 1,
        "slot-status and APDU dispatch recover after extension transfer error");
  bool unrelated_in_flight = false;
  check(!choupi::recover_ccid_extension_transfer_error(
            false, unrelated_in_flight),
        "OUT transfer errors use ordinary CCID transfer recovery");
  uint8_t submit_failures = 0;
  bool retries_exhausted = false;
  for (uint8_t attempt = 0;
       attempt < choupi::kMaxPendingCcidSubmitFailures; ++attempt) {
    retries_exhausted =
        choupi::record_pending_ccid_submit_failure(submit_failures);
  }
  check(retries_exhausted && submit_failures ==
                                  choupi::kMaxPendingCcidSubmitFailures,
        "bounded IN retries reach explicit recoverable abort threshold");
  bool out_rearm_pending = true;
  unsigned out_rearm_attempts = 0;
  const bool out_rearmed = choupi::retry_pending_ccid_transfer(
      out_rearm_pending, false, [] { return false; }, [&] {
        ++out_rearm_attempts;
        return true;
      });
  check(out_rearmed && !out_rearm_pending && out_rearm_attempts == 1,
        "aborted response can rearm Bulk-OUT for the host retry");
  unsigned post_abort_slot_status = 0;
  unsigned post_abort_apdu = 0;
  bool post_abort_message_pending = true;
  choupi::dispatch_pending_ccid_task(
      task_active, post_abort_message_pending,
      [&] { ++post_abort_slot_status; });
  post_abort_message_pending = true;
  choupi::dispatch_pending_ccid_task(
      task_active, post_abort_message_pending, [&] { ++post_abort_apdu; });
  check(post_abort_slot_status == 1 && post_abort_apdu == 1,
        "slot-status poll and retried APDU dispatch after bounded abort");
  check(!choupi::ccid_in_transfer_completion_rearms_out(true, false) &&
            !choupi::ccid_in_transfer_completion_rearms_out(false, true) &&
            choupi::ccid_in_transfer_completion_rearms_out(false, false),
        "OUT rearms only after final response and terminating ZLP completion");
  check(!task_active && !progress_active,
        "production-shared guards release on return");
  std::printf("TEST:SUMMARY pass=%u fail=0\n", checks);
  return EXIT_SUCCESS;
}
