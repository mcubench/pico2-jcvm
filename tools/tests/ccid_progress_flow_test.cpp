#include "ccid_progress_flow.hpp"

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
  check(!task_active && !progress_active,
        "production-shared guards release on return");
  std::printf("TEST:SUMMARY pass=%u fail=0\n", checks);
  return EXIT_SUCCESS;
}
