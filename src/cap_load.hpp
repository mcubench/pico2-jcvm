#ifndef PICO2_JCVM_CAP_LOAD_HPP
#define PICO2_JCVM_CAP_LOAD_HPP

#include <cstddef>
#include <cstdint>

class CapLoadSession {
public:
  void reset() noexcept;
  void abortTransfer() noexcept;

  bool beginInstallForLoad(const uint8_t *data, size_t length) noexcept;
  bool addLoadBlock(uint8_t p1, uint8_t p2, const uint8_t *data,
                    size_t length) noexcept;
  bool installAndMakeSelectable(const uint8_t *data,
                                size_t length) noexcept;
  void restoreInstalled() noexcept;

  bool installed() const noexcept { return installed_; }
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
    case 9:
      return 0x6f00; // unexpected internal error
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
  bool appletClassIndex(uint16_t &class_index) const noexcept;

private:
  static constexpr size_t kMaxLoadTlvLength = 28 * 1024;
  uint8_t load_data_[kMaxLoadTlvLength]{};
  uint8_t load_package_aid_[16]{};
  uint8_t load_package_aid_length_ = 0;
  uint8_t applet_aid_[16]{};
  uint8_t applet_aid_length_ = 0;
  uint8_t instance_aid_[16]{};
  uint8_t instance_aid_length_ = 0;
  uint16_t applet_class_index_ = 0xffff;
  size_t load_data_length_ = 0;
  uint16_t next_block_ = 0;
  bool loading_ = false;
  bool loaded_ = false;
  bool installed_ = false;
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
