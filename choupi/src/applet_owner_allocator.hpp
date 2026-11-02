#pragma once

#include <cstdint>

namespace jcvm {

// Owner zero is reserved for legacy instances. Keep the finite ID search
// identical between the RP2350 and PC/VPCD CAP-store implementations.
template <typename OwnerRetained>
inline bool allocate_applet_owner_id(const bool (&used)[256],
                                     OwnerRetained owner_retained,
                                     uint8_t &owner_id) noexcept {
  for (uint16_t candidate = 1; candidate <= UINT8_MAX; ++candidate) {
    const auto id = static_cast<uint8_t>(candidate);
    if (!used[candidate] && !owner_retained(id)) {
      owner_id = id;
      return true;
    }
  }
  return false;
}

}  // namespace jcvm
