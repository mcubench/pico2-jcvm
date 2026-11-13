#ifndef PICO2_JCVM_CAP_LOAD_HPP
#define PICO2_JCVM_CAP_LOAD_HPP

#include <cstddef>
#include <cstdint>

extern "C" uint8_t choupi_cap_package_applet_aid(
    uint8_t package_id, uint8_t applet_index, uint8_t *aid,
    uint8_t *aid_length);

class CapLoadSession {
public:
  void reset() noexcept;
  void abortTransfer() noexcept;

  bool beginInstallForLoad(const uint8_t *data, size_t length) noexcept;
  bool addLoadBlock(uint8_t p1, uint8_t p2, const uint8_t *data,
                    size_t length) noexcept;
  bool installAndMakeSelectable(const uint8_t *data,
                                size_t length) noexcept;
  uint16_t deleteObject(const uint8_t *aid, uint8_t aid_length,
                        bool delete_related_objects) noexcept;
  bool deletePersistentStateCommitted() const noexcept {
    return delete_persistent_state_committed_;
  }
  bool recoverPendingInstalls() noexcept;
  void restoreInstalled() noexcept;
  bool restorePackageForAid(const uint8_t *package_aid,
                            uint8_t package_aid_length) noexcept;
  bool restoreInstalledForAid(const uint8_t *instance_aid,
                              uint8_t instance_aid_length) noexcept;

  bool installed() const noexcept { return installed_; }
  bool loaded() const noexcept { return loaded_; }
  uint8_t installFailureStage() const noexcept { return install_failure_stage_; }
  static constexpr uint16_t failureStatusWordForStage(uint8_t stage) noexcept {
    switch (stage) {
    case 1:
    case 2:
    case 3:
    case 5:
      return 0x6a80; // incorrect data / applet installation failed
    case 4:
      return 0x6a82; // applet package not found
    case 6:
    case 8:
      return 0x6581; // nonvolatile memory failure
    case 7:
      return 0x6985; // install transaction unavailable
    case 13:
      return 0x6982; // CAP imports a private ROM implementation package
    case 9:
      return 0x6f00; // unexpected internal error
    case 10:
      return 0x6a88; // an imported package is not resident
    case 12:
      return 0x6a80; // duplicate instance AID or full applet registry
    default:
      return 0x6a80;
    }
  }
  uint16_t installFailureStatusWord() const noexcept {
    return failureStatusWordForStage(install_failure_stage_);
  }
  uint16_t installVmStatus() const noexcept { return install_vm_status_; }
  uint8_t installVmExceptionId() const noexcept {
    return install_vm_exception_id_;
  }
  uint8_t installVmExceptionOpcode() const noexcept {
    return install_vm_exception_opcode_;
  }
  uint32_t installVmExceptionPc() const noexcept {
    return install_vm_exception_pc_;
  }
  uint32_t installVmExceptionPrefix() const noexcept {
    return install_vm_exception_prefix_;
  }
  uint16_t installVmExceptionOperand() const noexcept {
    return install_vm_exception_operand_;
  }
  uint8_t installVmExceptionStackDepth() const noexcept {
    return install_vm_exception_stack_depth_;
  }
  uint32_t installVmExceptionOpcodeHistory() const noexcept {
    return install_vm_exception_opcode_history_;
  }
  uint32_t installVmExceptionDepthHistory() const noexcept {
    return install_vm_exception_depth_history_;
  }
  uint8_t installVmDebugStage() const noexcept { return install_vm_debug_stage_; }
  uint32_t installVmDebugContext() const noexcept {
    return install_vm_debug_context_;
  }
  const uint8_t *instanceAid() const noexcept { return instance_aid_; }
  uint8_t instanceAidLength() const noexcept { return instance_aid_length_; }
  const uint8_t *packageAid() const noexcept { return load_package_aid_; }
  uint8_t packageAidLength() const noexcept {
    return load_package_aid_length_;
  }
  const uint8_t *appletAid() const noexcept { return applet_aid_; }
  uint8_t appletAidLength() const noexcept { return applet_aid_length_; }
  bool appletClassIndex(uint16_t &class_index) const noexcept;
  uint8_t packageId() const noexcept { return package_id_; }

private:
  static constexpr size_t kMaxLoadTlvLength = 64 * 1024 + 1024;
  uint8_t load_data_[kMaxLoadTlvLength]{};
  uint8_t load_package_aid_[16]{};
  uint8_t load_package_aid_length_ = 0;
  uint8_t applet_aid_[16]{};
  uint8_t applet_aid_length_ = 0;
  uint8_t instance_aid_[16]{};
  uint8_t instance_aid_length_ = 0;
  uint16_t applet_class_index_ = 0xffff;
  uint8_t package_id_ = 0xff;
  size_t load_data_length_ = 0;
  uint16_t next_block_ = 0;
  bool loading_ = false;
  bool loaded_ = false;
  bool installed_ = false;
  bool delete_persistent_state_committed_ = false;
  uint8_t install_failure_stage_ = 0;
  uint16_t install_vm_status_ = 0;
  uint8_t install_vm_exception_id_ = 0;
  uint8_t install_vm_exception_opcode_ = 0;
  uint32_t install_vm_exception_pc_ = 0;
  uint32_t install_vm_exception_prefix_ = 0;
  uint16_t install_vm_exception_operand_ = 0;
  uint8_t install_vm_exception_stack_depth_ = 0;
  uint32_t install_vm_exception_opcode_history_ = 0;
  uint32_t install_vm_exception_depth_history_ = 0;
  uint8_t install_vm_debug_stage_ = 0;
  uint32_t install_vm_debug_context_ = 0;

  bool finishLoad() noexcept;
};

#endif
