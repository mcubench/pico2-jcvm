#ifndef PICO2_JCVM_GP_LIFECYCLE_RECOVERY_HPP
#define PICO2_JCVM_GP_LIFECYCLE_RECOVERY_HPP

#include <cstdint>

namespace gp {

bool recover_dynamic_lifecycle_state() noexcept;

// Backend methods must be idempotent. The CAP store and persistent filesystem
// are separate durable stores, so startup reconciles their published state:
// enabled-without-CAP is rolled back, disabled-with-CAP is an unfinished LOAD
// or DELETE, and an instance entry without its persistent root is an unfinished
// instance DELETE.
template <typename Backend>
bool recover_lifecycle(Backend &backend, const uint8_t first_package,
                       const uint8_t package_count) noexcept {
  try {
    for (uint16_t offset = 0; offset < package_count; ++offset) {
      const auto package_id = static_cast<uint8_t>(first_package + offset);
      const bool has_cap = backend.package_present(package_id);
      const bool enabled = backend.package_enabled(package_id);
      if (has_cap && !enabled) {
        if (!backend.delete_package(package_id)) return false;
        continue;
      }
      if (!has_cap && enabled) {
        if (!backend.clear_and_disable_package(package_id)) return false;
        continue;
      }
      if (!has_cap) continue;

      uint8_t index = 0;
      while (index < backend.instance_count(package_id)) {
        uint8_t applet_aid[16]{};
        uint8_t applet_aid_length = 0;
        uint8_t instance_aid[16]{};
        uint8_t instance_aid_length = 0;
        uint16_t class_index = 0xffff;
        uint8_t owner = 0;
        if (!backend.instance_at(package_id, index, applet_aid,
                                 applet_aid_length, instance_aid,
                                 instance_aid_length, class_index, owner)) {
          return false;
        }
        if (backend.instance_root_present(owner, package_id, class_index)) {
          ++index;
          continue;
        }
        if (!backend.remove_instance(package_id, instance_aid,
                                     instance_aid_length)) {
          return false;
        }
      }
    }
  } catch (...) {
    return false;
  }
  return true;
}

} // namespace gp

#endif
