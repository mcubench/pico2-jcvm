#pragma once

namespace choupi {

// Prevent a callback from re-entering a service path while it is pumping
// other event callbacks. The owning path remains responsible for consuming
// work queued by nested callbacks on its next normal iteration.
class NativeProgressGuard {
public:
  explicit NativeProgressGuard(bool &active) noexcept
      : active_(active), entered_(!active) {
    if (entered_) {
      active_ = true;
    }
  }

  ~NativeProgressGuard() noexcept {
    if (entered_) {
      active_ = false;
    }
  }

  NativeProgressGuard(const NativeProgressGuard &) = delete;
  NativeProgressGuard &operator=(const NativeProgressGuard &) = delete;

  explicit operator bool() const noexcept { return entered_; }

private:
  bool &active_;
  bool entered_;
};

} // namespace choupi
