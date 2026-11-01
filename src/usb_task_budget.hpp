#pragma once

#include <cstdint>

#include "device/usbd.h"

namespace choupi {

// Keep each cooperative USB servicing point finite. TinyUSB retains events
// beyond this count in its FIFO for the next main-loop/progress invocation.
constexpr uint32_t kUsbTaskEventBudget = 8;

inline void service_usb_task() {
  (void)tud_task_budgeted(kUsbTaskEventBudget);
}

} // namespace choupi
