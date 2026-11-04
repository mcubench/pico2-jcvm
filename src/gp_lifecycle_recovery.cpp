#include "gp_lifecycle_recovery.hpp"
#include "ffi.h"

#include "cap_store.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "vm_filesystem.h"

namespace {
constexpr uint8_t kFirstDynamicPackageId = 56;
constexpr uint8_t kDynamicPackageCount = 8;

struct LifecycleRecoveryBackend {
  bool package_present(uint8_t package_id) const noexcept {
    uint32_t length = 0;
    return cap_store_data_for_package(package_id, &length) != nullptr &&
           length != 0;
  }

  bool package_enabled(uint8_t package_id) const {
    const uint8_t *package_bits =
        jcvm::FlashMemory_Handler::getPackagesArray();
    return (package_bits[package_id / 8] & (1u << (package_id % 8))) != 0;
  }

  bool delete_package(uint8_t package_id) const noexcept {
    return cap_store_delete_package(package_id);
  }

  bool clear_and_disable_package(uint8_t package_id) const noexcept {
    if (!jcvm::FlashMemory_Handler::beginPersistentWriteTransaction())
      return false;
    try {
      if (!jcvm::FlashMemory_Handler::erasePersistentPackageState(package_id)) {
        (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
        return false;
      }
      jcvm::FlashMemory_Handler::disablePackage(package_id);
      return jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    } catch (...) {
      (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
      return false;
    }
  }

  uint8_t instance_count(uint8_t package_id) const noexcept {
    return cap_store_package_instance_count(package_id);
  }

  bool instance_at(uint8_t package_id, uint8_t index, uint8_t *applet_aid,
                   uint8_t &applet_aid_length, uint8_t *instance_aid,
                   uint8_t &instance_aid_length, uint16_t &class_index,
                   uint8_t &owner) const noexcept {
    return cap_store_package_instance_at(
               package_id, index, applet_aid, applet_aid_length, instance_aid,
               instance_aid_length, class_index) &&
           cap_store_applet_owner(instance_aid, instance_aid_length, owner);
  }

  bool instance_root_present(uint8_t owner, uint8_t package_id,
                             uint16_t class_index) const noexcept {
    const auto tag = jcvm::FlashMemory_Handler::getAppletInstanceTag(
        owner, package_id, class_index);
    return fs_exists(tag.value, tag.len) != 0;
  }

  bool remove_instance(uint8_t package_id, const uint8_t *instance_aid,
                       uint8_t instance_aid_length) const noexcept {
    return cap_store_remove_package_instance(package_id, instance_aid,
                                             instance_aid_length);
  }
};
} // namespace

bool gp::recover_dynamic_lifecycle_state() noexcept {
  LifecycleRecoveryBackend backend;
  return recover_lifecycle(backend, kFirstDynamicPackageId,
                           kDynamicPackageCount);
}
