#include "cap_load.hpp"
#include "cap_store.hpp"
#include "jc_handlers/flashmemory.hpp"

#include <cstdio>
#include <cstring>

extern "C" uint8_t choupi_store_cap_package(uint8_t package_id,
                                             const uint8_t *data,
                                             uint32_t length);
extern "C" uint8_t choupi_store_cap_instance(const uint8_t *aid,
                                              uint8_t aid_length);
extern "C" uint16_t runtime_install_method(uint8_t package_id,
                                            uint16_t method_offset,
                                           uint8_t *exception_id,
                                           uint8_t *exception_opcode,
                                           uint32_t *exception_pc,
                                           uint32_t *exception_prefix,
                                           uint16_t *exception_operand,
                                           uint8_t *exception_stack_depth,
                                           uint32_t *exception_opcode_history,
                                           uint32_t *exception_depth_history);

extern "C" uint8_t choupi_vm_find_applet(const uint8_t *aid,
                                           uint8_t aid_length,
                                           uint8_t *package_id,
                                           uint16_t *install_offset) {
  if (package_id == nullptr || install_offset == nullptr) return 1;
  try {
    jcvm::jpackage_ID_t resolved_package = 0;
    uint16_t resolved_offset = 0;
    if (!jcvm::FlashMemory_Handler::findAppletByAID(
            aid, aid_length, resolved_package, resolved_offset) ||
        resolved_package != jcvm::FlashMemory_Handler::DYNAMIC_PACKAGE_ID) {
      return 1;
    }
    *package_id = resolved_package;
    *install_offset = resolved_offset;
  } catch (...) {
    return 1;
  }
  return 0;
}

namespace {
constexpr uint8_t kPackageAid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                   0x54, 0x65, 0x73, 0x74};
constexpr uint8_t kIsdAid[] = {0xa0, 0x00, 0x00, 0x01,
                               0x51, 0x00, 0x00, 0x00};
constexpr uint8_t kPackageId = 20;
constexpr size_t kMaxLoadTlvLength = 28 * 1024;

bool read_aid(const uint8_t *data, size_t length, size_t &offset,
              uint8_t aid[16], uint8_t &aid_length) noexcept {
  if (offset >= length) return false;
  const uint8_t size = data[offset++];
  if (size < 5 || size > 16 || size > length - offset) return false;
  std::memcpy(aid, data + offset, size);
  aid_length = size;
  offset += size;
  return true;
}

bool read_lv(const uint8_t *data, size_t length, size_t &offset,
             const uint8_t *&value, size_t &value_length) noexcept {
  if (offset >= length) return false;
  value_length = data[offset++];
  if (value_length > length - offset) return false;
  value = data + offset;
  offset += value_length;
  return true;
}

bool read_ber_length(const uint8_t *data, size_t length, size_t &offset,
                     size_t &value_length) noexcept {
  if (offset >= length) return false;
  const uint8_t first = data[offset++];
  if ((first & 0x80) == 0) {
    value_length = first;
    return true;
  }
  const uint8_t count = first & 0x7f;
  if (count == 0 || count > 2 || count > length - offset) return false;
  value_length = 0;
  for (uint8_t index = 0; index < count; ++index) {
    value_length = (value_length << 8) | data[offset++];
  }
  return value_length <= kMaxLoadTlvLength;
}

bool aid_matches(const uint8_t *left, uint8_t left_length,
                 const uint8_t *right, size_t right_length) noexcept {
  return left_length == right_length &&
         std::memcmp(left, right, right_length) == 0;
}

bool validate_cap_code(const uint8_t *code, size_t length,
                        uint8_t package_aid[16],
                        uint8_t &package_aid_length,
                        uint8_t applet_aid[16],
                        uint8_t &applet_aid_length) noexcept {
  bool found_header = false;
  bool found_applet = false;
  size_t offset = 0;
  while (offset < length) {
    if (length - offset < 3) return false;
    const uint8_t tag = code[offset];
    const size_t component_length =
        (static_cast<size_t>(code[offset + 1]) << 8) | code[offset + 2];
    if (component_length > length - offset - 3) return false;
    const uint8_t *payload = code + offset + 3;
    size_t cursor = 0;

    if (tag == 1) {
      if (found_header || component_length < 13 ||
          payload[0] != 0xde || payload[1] != 0xca ||
          payload[2] != 0xff || payload[3] != 0xed) {
        return false;
      }
      const uint8_t aid_length = payload[9];
      if (aid_length < 5 || aid_length > 16 ||
          component_length < static_cast<size_t>(10 + aid_length)) {
        return false;
      }
      std::memcpy(package_aid, payload + 10, aid_length);
      package_aid_length = aid_length;
      found_header = true;
    } else if (tag == 3) {
      if (found_applet || component_length < 1) return false;
      const uint8_t count = payload[cursor++];
      for (uint8_t index = 0; index < count; ++index) {
        if (cursor >= component_length) return false;
        const uint8_t aid_length = payload[cursor++];
        if (aid_length < 5 || aid_length > 16 ||
            aid_length > component_length - cursor ||
            component_length - cursor - aid_length < 2) {
          return false;
        }
        if (index == 0) {
          std::memcpy(applet_aid, payload + cursor, aid_length);
          applet_aid_length = aid_length;
        }
        cursor += aid_length + 2; // AID plus install-method offset
      }
      if (cursor != component_length || count == 0) return false;
      found_applet = true;
    }
    offset += 3 + component_length;
  }
  return found_header && found_applet && offset == length &&
         aid_matches(package_aid, package_aid_length, kPackageAid,
                     sizeof(kPackageAid));
}
} // namespace

void CapLoadSession::reset() noexcept {
  load_package_aid_length_ = 0;
  applet_aid_length_ = 0;
  instance_aid_length_ = 0;
  load_data_length_ = 0;
  next_block_ = 0;
  loading_ = false;
  loaded_ = false;
  installed_ = false;
  install_failure_stage_ = 0;
  install_vm_status_ = 0;
  install_vm_exception_id_ = 0;
  install_vm_exception_opcode_ = 0;
  install_vm_exception_pc_ = 0;
  install_vm_exception_prefix_ = 0;
  install_vm_exception_operand_ = 0;
  install_vm_exception_stack_depth_ = 0;
  install_vm_exception_opcode_history_ = 0;
  install_vm_exception_depth_history_ = 0;
}

void CapLoadSession::abortTransfer() noexcept {
  load_data_length_ = 0;
  next_block_ = 0;
  loading_ = false;
}

bool CapLoadSession::beginInstallForLoad(const uint8_t *data,
                                         size_t length) noexcept {
  reset();
  if (data == nullptr) return false;
  size_t offset = 0;
  uint8_t domain_aid[16]{};
  uint8_t domain_aid_length = 0;
  const uint8_t *value = nullptr;
  size_t value_length = 0;
  if (!read_aid(data, length, offset, load_package_aid_,
                load_package_aid_length_) ||
      !aid_matches(load_package_aid_, load_package_aid_length_, kPackageAid,
                   sizeof(kPackageAid)) ||
      !read_aid(data, length, offset, domain_aid, domain_aid_length) ||
      !aid_matches(domain_aid, domain_aid_length, kIsdAid, sizeof(kIsdAid)) ||
      !read_lv(data, length, offset, value, value_length) ||
      !read_ber_length(data, length, offset, value_length) ||
      value_length > length - offset) {
    reset();
    return false;
  }
  offset += value_length;
  // GlobalPlatformPro includes an empty load-token LV after the parameters.
  if (offset < length &&
      (!read_lv(data, length, offset, value, value_length) ||
       value_length != 0)) {
    reset();
    return false;
  }
  if (offset != length) {
    reset();
    return false;
  }
  loading_ = true;
  return true;
}

bool CapLoadSession::addLoadBlock(uint8_t p1, uint8_t p2,
                                  const uint8_t *data,
                                  size_t length) noexcept {
  if (!loading_ || data == nullptr || p2 != static_cast<uint8_t>(next_block_) ||
      (p1 != 0x00 && p1 != 0x80) ||
      length > sizeof(load_data_) - load_data_length_) {
    reset();
    return false;
  }
  std::memcpy(load_data_ + load_data_length_, data, length);
  load_data_length_ += length;
  ++next_block_;
  if (p1 == 0x00) return true;
  loading_ = false;
  return finishLoad();
}

bool CapLoadSession::finishLoad() noexcept {
  size_t offset = 0;
  if (load_data_length_ < 2 || load_data_[offset++] != 0xc4) return false;
  size_t code_length = 0;
  if (!read_ber_length(load_data_, load_data_length_, offset, code_length) ||
      code_length != load_data_length_ - offset) {
    return false;
  }
  if (!validate_cap_code(load_data_ + offset, code_length, load_package_aid_,
                         load_package_aid_length_, applet_aid_,
                         applet_aid_length_) ||
      choupi_store_cap_package(kPackageId, load_data_ + offset,
                               static_cast<uint32_t>(code_length)) != 0) {
    return false;
  }
  loaded_ = true;
  load_data_length_ = 0;
  return true;
}

bool CapLoadSession::installAndMakeSelectable(const uint8_t *data,
                                              size_t length) noexcept {
  std::printf("CHOUPI:INSTALL_ENTRY loaded=%u installed=%u data_len=%lu\r\n",
              static_cast<unsigned>(loaded_),
              static_cast<unsigned>(installed_),
              static_cast<unsigned long>(length));
  std::fflush(stdout);
  install_failure_stage_ = 0;
  install_vm_status_ = 0;
  install_vm_exception_id_ = 0;
  if (!loaded_ || installed_ || data == nullptr) {
    install_failure_stage_ = 1;
    return false;
  }
  size_t offset = 0;
  uint8_t package_aid[16]{};
  uint8_t package_aid_length = 0;
  uint8_t applet_aid[16]{};
  uint8_t applet_aid_length = 0;
  const uint8_t *value = nullptr;
  size_t value_length = 0;
  if (!read_aid(data, length, offset, package_aid, package_aid_length) ||
      !aid_matches(package_aid, package_aid_length, load_package_aid_,
                   load_package_aid_length_) ||
      !read_aid(data, length, offset, applet_aid, applet_aid_length) ||
      !aid_matches(applet_aid, applet_aid_length, applet_aid_,
                   applet_aid_length_)) {
    install_failure_stage_ = 2;
    return false;
  }
  uint8_t instance_aid[16]{};
  uint8_t instance_aid_length = 0;
  if (!read_aid(data, length, offset, instance_aid, instance_aid_length) ||
      !read_lv(data, length, offset, value, value_length) ||
      !read_ber_length(data, length, offset, value_length) ||
      value_length > length - offset) {
    install_failure_stage_ = 2;
    return false;
  }
  offset += value_length;
  if (offset < length &&
      (!read_lv(data, length, offset, value, value_length) ||
       value_length != 0)) {
    install_failure_stage_ = 3;
    return false;
  }
  if (offset != length) {
    install_failure_stage_ = 3;
    return false;
  }
  std::printf("CHOUPI:INSTALL_PARSE_OK applet_aid_len=%u instance_aid_len=%u\r\n",
              static_cast<unsigned>(applet_aid_length),
              static_cast<unsigned>(instance_aid_length));
  std::fflush(stdout);
  uint8_t resolved_package_id = 0;
  uint16_t install_method_offset = 0;
  if (choupi_vm_find_applet(applet_aid, applet_aid_length,
                            &resolved_package_id, &install_method_offset) != 0 ||
      resolved_package_id != jcvm::FlashMemory_Handler::DYNAMIC_PACKAGE_ID ||
      install_method_offset == 0) {
    install_failure_stage_ = 4;
    std::printf("CHOUPI:INSTALL_RESOLVE_FAIL package=%u offset=%u\r\n",
                static_cast<unsigned>(resolved_package_id),
                static_cast<unsigned>(install_method_offset));
    std::fflush(stdout);
    return false;
  }
  std::printf("CHOUPI:INSTALL_INVOKE package=%u offset=%u\r\n",
              static_cast<unsigned>(resolved_package_id),
              static_cast<unsigned>(install_method_offset));
  std::fflush(stdout);
  install_vm_status_ = runtime_install_method(
      resolved_package_id, install_method_offset, &install_vm_exception_id_,
      &install_vm_exception_opcode_, &install_vm_exception_pc_,
      &install_vm_exception_prefix_,
      &install_vm_exception_operand_, &install_vm_exception_stack_depth_,
      &install_vm_exception_opcode_history_,
      &install_vm_exception_depth_history_);
  if (install_vm_status_ != 0x9000) {
    install_failure_stage_ = 5;
    return false;
  }
  if (choupi_store_cap_instance(instance_aid, instance_aid_length) != 0) {
    install_failure_stage_ = 6;
    return false;
  }
  std::memcpy(instance_aid_, instance_aid, instance_aid_length);
  instance_aid_length_ = instance_aid_length;
  installed_ = true;
  return true;
}

void CapLoadSession::restoreInstalled() noexcept {
  reset();
  uint32_t code_length = 0;
  const uint8_t *code = cap_store_data(&code_length);
  uint8_t instance_aid[16]{};
  uint8_t instance_aid_length = 0;
  if (code == nullptr ||
      !cap_store_instance_aid(instance_aid, instance_aid_length) ||
      !validate_cap_code(code, code_length, load_package_aid_,
                         load_package_aid_length_, applet_aid_,
                         applet_aid_length_) ||
      !aid_matches(instance_aid, instance_aid_length, applet_aid_,
                   applet_aid_length_)) {
    reset();
    return;
  }
  std::memcpy(instance_aid_, instance_aid, instance_aid_length);
  instance_aid_length_ = instance_aid_length;
  loaded_ = true;
  installed_ = true;
}
