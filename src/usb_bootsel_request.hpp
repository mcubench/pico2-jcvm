#pragma once

namespace choupi {

inline void request_bootsel_reset(volatile bool &pending) noexcept {
  pending = true;
}

template <typename Reset>
bool service_pending_bootsel_reset(volatile bool &pending, Reset &&reset) {
  if (!pending) return false;
  pending = false;
  reset();
  return true;
}

template <typename ServiceUsb, typename Reset>
bool service_usb_then_bootsel_reset(volatile bool &pending,
                                    ServiceUsb &&service_usb,
                                    Reset &&reset) {
  service_usb();
  return service_pending_bootsel_reset(pending, reset);
}

} // namespace choupi
