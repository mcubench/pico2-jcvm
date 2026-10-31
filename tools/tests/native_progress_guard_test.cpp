#include "native_progress_guard.hpp"

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

void dispatch_apdu(bool &task_active, unsigned &dispatch_count,
                   unsigned &queued_messages) {
  choupi::NativeProgressGuard task_guard(task_active);
  if (!task_guard) return;
  if (queued_messages == 0) return;
  --queued_messages;
  ++dispatch_count;
}

void service_usb(bool &progress_active, bool &task_active,
                 unsigned &usb_service_count, unsigned &dispatch_count,
                 unsigned &queued_messages) {
  choupi::NativeProgressGuard progress_guard(progress_active);
  if (!progress_guard) return;
  ++usb_service_count;

  // Simulate TinyUSB receiving another CCID message, then an accidental
  // recursive attempt to pump USB and dispatch that message from this hook.
  ++queued_messages;
  service_usb(progress_active, task_active, usb_service_count,
              dispatch_count, queued_messages);
  dispatch_apdu(task_active, dispatch_count, queued_messages);
}
} // namespace

int main() {
  bool progress_active = false;
  bool task_active = false;
  unsigned usb_service_count = 0;
  unsigned dispatch_count = 0;
  unsigned queued_messages = 0;

  {
    choupi::NativeProgressGuard outer_task(task_active);
    check(static_cast<bool>(outer_task), "outer APDU task enters");
    ++dispatch_count;
    service_usb(progress_active, task_active, usb_service_count,
                dispatch_count, queued_messages);
    check(dispatch_count == 1,
          "queued APDU is not recursively dispatched in active task");
    check(queued_messages == 1,
          "nested USB message remains queued for normal task iteration");
  }

  dispatch_apdu(task_active, dispatch_count, queued_messages);
  check(dispatch_count == 2 && queued_messages == 0,
        "queued APDU dispatches after outer task returns");
  check(usb_service_count == 1,
        "recursive native progress does not pump USB twice");
  check(!task_active && !progress_active,
        "guards release after callback completion");

  std::printf("TEST:SUMMARY pass=%u fail=0\n", checks);
  return EXIT_SUCCESS;
}
