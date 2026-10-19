#include "cap_load.hpp"
#include "cap_store.hpp"
#include "gp_delete.hpp"

#ifdef PC_VERSION
// The host filesystem backend writes each record synchronously, so it has no
// separate snapshot flush phase.
extern "C" uint8_t fs_sync(void) { return 0; }
#else
extern "C" uint8_t fs_sync(void);
#endif
#include "heap.hpp"
#include "jc_cap/jc_cap_static_field.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_static.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"
#include "ffi.h"

#include <cstring>

extern "C" uint8_t choupi_store_cap_package(uint8_t package_id,
                                             const uint8_t *data,
                                             uint32_t length);
extern "C" uint8_t choupi_store_cap_instance(const uint8_t *aid,
                                              uint8_t aid_length,
                                              uint16_t applet_class_index);
extern "C" uint16_t runtime_install_method(uint8_t package_id,
                                            uint16_t method_offset,
                                            const uint8_t *install_data,
                                            uint8_t install_length,
                                            const uint8_t *instance_aid,
                                            uint8_t instance_aid_length,
                                            uint8_t *applet_registered,
                                            uint16_t *applet_class_index,
                                           uint8_t *exception_id,
                                           uint8_t *exception_opcode,
                                           uint32_t *exception_pc,
                                           uint32_t *exception_prefix,
                                           uint16_t *exception_operand,
                                           uint8_t *exception_stack_depth,
                                           uint32_t *exception_opcode_history,
                                           uint32_t *exception_depth_history,
                                           uint8_t *install_debug_stage,
                                           uint32_t *install_debug_context);

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
// File type 0x02 is the OS-approved static-field namespace. Package 0xff is
// outside the VM package range and reserved here for card-manager metadata.
constexpr uint8_t kLifecycleTagType = 0x02;
constexpr uint8_t kLifecyclePackageId = 0xff;
constexpr uint8_t kAppletDeletedState = 1;
constexpr uint8_t kPackageDeletedState = 2;

jcvm::fs::Tag lifecycle_tag(uint8_t kind) noexcept {
  jcvm::fs::Tag tag;
  tag.len = 3;
  tag.value[0] = kLifecycleTagType;
  tag.value[1] = kLifecyclePackageId;
  tag.value[2] = kind;
  return tag;
}

bool read_lifecycle_state(const jcvm::fs::Tag &tag,
                          uint8_t &state) noexcept {
  state = 0;
  if (fs_exists(tag.value, tag.len) == 0) return true;
  try {
    state = static_cast<uint8_t>(
        jcvm::FlashMemory_Handler::getPersistentField_Byte(tag));
    return true;
  } catch (...) {
    return false;
  }
}

bool clear_applet_persistent_state(uint16_t class_index) noexcept {
  if (class_index > UINT8_MAX) return false;
  // Root object and instance fields use the same five-byte tag namespace. Any
  // child array/object records become unreachable when these owning fields are
  // erased and are overwritten if the applet is installed again.
  for (uint16_t field = 0; field <= 0xff; ++field) {
    const auto tag = jcvm::FlashMemory_Handler::getPersistentFieldTag(
        0, jcvm::FlashMemory_Handler::DYNAMIC_PACKAGE_ID, class_index,
        static_cast<uint8_t>(field));
    if (!jcvm::FlashMemory_Handler::erasePersistentTag(tag)) return false;
  }
  return true;
}

bool clear_package_static_state() noexcept {
  for (uint16_t field = 0; field <= 0xff; ++field) {
    const auto tag = jcvm::FlashMemory_Handler::getStaticFieldTag(
        jcvm::FlashMemory_Handler::DYNAMIC_PACKAGE_ID,
        static_cast<uint8_t>(field));
    if (!jcvm::FlashMemory_Handler::erasePersistentTag(tag)) return false;
  }
  return true;
}

bool clear_lifecycle_markers() {
  jcvm::FlashMemory_Handler::setPersistentField_Byte(lifecycle_tag(1), 0);
  jcvm::FlashMemory_Handler::setPersistentField_Byte(lifecycle_tag(2), 0);
  return true;
}

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

uint16_t read_u16_be(const uint8_t *data) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               data[1]);
}

bool initialize_cap_static_arrays(uint8_t package_id) {
  const jcvm::JC_Cap cap = jcvm::Package(package_id).getCap();
  const auto *component = cap.getStaticField();
  if (component == nullptr) return true;

  const auto *component_bytes = reinterpret_cast<const uint8_t *>(component);
  const size_t component_size = read_u16_be(component_bytes + 1);
  // The StaticField payload starts with image_size, reference_count, and
  // array_init_count. Each array initializer then has a 3-byte header.
  if (component_size < 6) return false;
  const uint16_t array_count = read_u16_be(component_bytes + 7);
  size_t remaining = component_size - 6;
  const uint8_t *cursor = component_bytes + 9;
  const uint16_t image_size = read_u16_be(component_bytes + 3);
  jcvm::Static_Handler statics{jcvm::Package(package_id)};

  for (uint16_t index = 0; index < array_count; ++index) {
    if (remaining < 3) return false;
    const uint8_t cap_type = cursor[0];
    const uint16_t byte_count = read_u16_be(cursor + 1);
    cursor += 3;
    remaining -= 3;
    if (byte_count > remaining) return false;

    jcvm::jc_array_type array_type;
    uint16_t entry_size = 0;
    switch (cap_type) {
    case 2:
      array_type = jcvm::JAVA_ARRAY_T_BOOLEAN;
      entry_size = 1;
      break;
    case 3:
      array_type = jcvm::JAVA_ARRAY_T_BYTE;
      entry_size = 1;
      break;
    case 4:
      array_type = jcvm::JAVA_ARRAY_T_SHORT;
      entry_size = 2;
      break;
#ifdef JCVM_INT_SUPPORTED
    case 5:
      array_type = jcvm::JAVA_ARRAY_T_INT;
      entry_size = 4;
      break;
#endif
    default:
      return false;
    }
    if (byte_count % entry_size != 0) return false;

    // CAP StaticField array initializers correspond to the 16-bit static
    // image slot at the same initializer index. CHOUPI's static-field API
    // stores that byte offset in an 8-bit tag.
    const size_t static_offset = static_cast<size_t>(index) * 2;
    if (static_offset + sizeof(jcvm::jref_t) > image_size ||
        static_offset > UINT8_MAX) {
      return false;
    }
    const auto tag = jcvm::FlashMemory_Handler::getStaticFieldTag(
        package_id, static_cast<uint8_t>(static_offset));
    if (fs_exists(tag.value, tag.len) != 0 &&
        jcvm::FlashMemory_Handler::getPersistentField_Type(tag) !=
            jcvm::FieldType::FIELD_TYPE_UNINITIALIZED) {
      cursor += byte_count;
      remaining -= byte_count;
      continue;
    }

    const uint16_t entry_count = byte_count / entry_size;
    jcvm::Heap heap(0);
    const jcvm::jref_t reference = heap.addArray(entry_count, array_type);
    const auto array = heap.getArray(reference);
    if (array == nullptr) return false;
    for (uint16_t entry = 0; entry < entry_count; ++entry) {
      const uint8_t *value = cursor + entry * entry_size;
      if (entry_size == 1) {
        array->setByteEntry(entry, static_cast<jcvm::jbyte_t>(value[0]));
      } else if (entry_size == 2) {
        array->setShortEntry(entry,
                             static_cast<jcvm::jshort_t>(read_u16_be(value)));
#ifdef JCVM_INT_SUPPORTED
      } else {
        const uint32_t int_value =
            (static_cast<uint32_t>(value[0]) << 24) |
            (static_cast<uint32_t>(value[1]) << 16) |
            (static_cast<uint32_t>(value[2]) << 8) | value[3];
        array->setIntEntry(entry, static_cast<jcvm::jint_t>(int_value));
#endif
      }
    }
    statics.setPersistentRef(static_cast<uint16_t>(static_offset), reference,
                             heap);
    cursor += byte_count;
    remaining -= byte_count;
  }

  // The two trailing counts are followed only by the non-default bytes.
  if (remaining < 4) return false;
  const uint16_t default_value_count = read_u16_be(cursor);
  const uint16_t non_default_value_count = read_u16_be(cursor + 2);
  (void)default_value_count;
  return non_default_value_count == remaining - 4;
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
  applet_class_index_ = 0xffff;
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
  install_vm_debug_stage_ = 0;
  install_vm_debug_context_ = 0;
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
  if (!jcvm::FlashMemory_Handler::beginPersistentWriteTransaction()) {
    return false;
  }
  bool transaction_active = true;
  try {
    clear_lifecycle_markers();
    if (fs_sync() != 0 ||
        !jcvm::FlashMemory_Handler::commitPersistentWriteTransaction()) {
      (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
      transaction_active = false;
      (void)fs_sync();
      return false;
    }
    transaction_active = false;
  } catch (...) {
    if (transaction_active) {
      (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
      (void)fs_sync();
    }
    return false;
  }
  loaded_ = true;
  load_data_length_ = 0;
  return true;
}

bool CapLoadSession::installAndMakeSelectable(const uint8_t *data,
                                              size_t length) noexcept {
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
  const uint8_t *install_parameters = nullptr;
  size_t install_parameters_length = 0;
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
      !read_ber_length(data, length, offset, install_parameters_length) ||
      install_parameters_length > length - offset ||
      install_parameters_length > 0xff) {
    install_failure_stage_ = 2;
    return false;
  }
  install_parameters = data + offset;
  offset += install_parameters_length;
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
  uint8_t resolved_package_id = 0;
  uint16_t install_method_offset = 0;
  if (choupi_vm_find_applet(applet_aid, applet_aid_length,
                            &resolved_package_id, &install_method_offset) != 0 ||
      resolved_package_id != jcvm::FlashMemory_Handler::DYNAMIC_PACKAGE_ID ||
      install_method_offset == 0) {
    install_failure_stage_ = 4;
    return false;
  }

  if (!jcvm::FlashMemory_Handler::beginPersistentWriteTransaction()) {
    install_failure_stage_ = 7;
    return false;
  }
  bool transaction_active = true;
  const auto rollback = [&transaction_active]() noexcept {
    if (!transaction_active) {
      return true;
    }
    transaction_active = false;
    const bool rolled_back =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    return fs_sync() == 0 && rolled_back;
  };

  uint8_t applet_registered = 0;
  uint16_t applet_class_index = 0xffff;
  try {
    clear_lifecycle_markers();
    if (!initialize_cap_static_arrays(resolved_package_id)) {
      install_failure_stage_ = rollback() ? 5 : 8;
      return false;
    }
    install_vm_status_ = runtime_install_method(
        resolved_package_id, install_method_offset, install_parameters,
        static_cast<uint8_t>(install_parameters_length), instance_aid,
        instance_aid_length, &applet_registered,
        &applet_class_index, &install_vm_exception_id_,
        &install_vm_exception_opcode_, &install_vm_exception_pc_,
        &install_vm_exception_prefix_, &install_vm_exception_operand_,
        &install_vm_exception_stack_depth_, &install_vm_exception_opcode_history_,
        &install_vm_exception_depth_history_, &install_vm_debug_stage_,
        &install_vm_debug_context_);
    if (install_vm_status_ != 0x9000 || applet_registered == 0 ||
        applet_class_index == 0xffff) {
      install_failure_stage_ = rollback() ? 5 : 8;
      return false;
    }
    if (fs_sync() != 0) {
      install_failure_stage_ = rollback() ? 6 : 8;
      return false;
    }
    if (choupi_store_cap_instance(instance_aid, instance_aid_length,
                                  applet_class_index) != 0) {
      install_failure_stage_ = rollback() ? 6 : 8;
      return false;
    }
    if (!jcvm::FlashMemory_Handler::commitPersistentWriteTransaction()) {
      transaction_active = false;
      install_failure_stage_ = 8;
      return false;
    }
    transaction_active = false;
  } catch (...) {
    install_failure_stage_ = rollback() ? 9 : 8;
    return false;
  }
  std::memcpy(instance_aid_, instance_aid, instance_aid_length);
  instance_aid_length_ = instance_aid_length;
  applet_class_index_ = applet_class_index;
  installed_ = true;
  return true;
}

uint16_t CapLoadSession::deleteObject(const uint8_t *aid, uint8_t aid_length,
                                     bool delete_related_objects) noexcept {
  gp::DeleteTarget target;
  const uint16_t target_status = gp::resolve_delete_target(
      loaded_, installed_, aid, aid_length, load_package_aid_,
      load_package_aid_length_, instance_aid_, instance_aid_length_,
      delete_related_objects, target);
  if (target_status != 0x9000) return target_status;
  const bool delete_package = target.package;

  struct DeleteBackend {
    bool begin() noexcept {
      return jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    }
    bool clear_applet(uint16_t class_index) noexcept {
      return clear_applet_persistent_state(class_index);
    }
    bool clear_package_static() noexcept {
      return clear_package_static_state();
    }
    void mark_deleted(bool package) {
      jcvm::FlashMemory_Handler::setPersistentField_Byte(
          lifecycle_tag(package ? 2 : 1),
          package ? kPackageDeletedState : kAppletDeletedState);
    }
    uint8_t sync() noexcept { return fs_sync(); }
    bool rollback() noexcept {
      return jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    }
    bool commit() noexcept {
      return jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    }
  } backend;

  const uint16_t transaction_status = gp::commit_delete_state(
      backend, delete_package, installed_, applet_class_index_);
  if (transaction_status != 0x9000) return transaction_status;

  if (delete_package) {
    reset();
    return cap_store_delete() ? 0x9000 : 0x6581;
  } else {
    installed_ = false;
    instance_aid_length_ = 0;
    applet_class_index_ = 0xffff;
  }
  return 0x9000;
}

void CapLoadSession::restoreInstalled() noexcept {
  reset();
  uint32_t code_length = 0;
  const uint8_t *code = cap_store_data(&code_length);
  if (code == nullptr ||
      !validate_cap_code(code, code_length, load_package_aid_,
                         load_package_aid_length_, applet_aid_,
                         applet_aid_length_)) {
    reset();
    return;
  }
  uint8_t package_state = 0;
  uint8_t applet_state = 0;
  if (!read_lifecycle_state(lifecycle_tag(2), package_state) ||
      !read_lifecycle_state(lifecycle_tag(1), applet_state)) {
    reset();
    return;
  }
  if ((package_state != 0 && package_state != kPackageDeletedState) ||
      (applet_state != 0 && applet_state != kAppletDeletedState)) {
    reset();
    return;
  }
  if (package_state == kPackageDeletedState) {
    reset();
    return;
  }
  loaded_ = true;
  if (applet_state == kAppletDeletedState) return;

  uint8_t instance_aid[16]{};
  uint8_t instance_aid_length = 0;
  if (!cap_store_instance_aid(instance_aid, instance_aid_length)) return;
  // INSTALL for install-and-make-selectable records the registered instance
  // AID separately from the Applet component AID. They are allowed to differ.
  std::memcpy(instance_aid_, instance_aid, instance_aid_length);
  instance_aid_length_ = instance_aid_length;
  (void)cap_store_applet_class_index(applet_class_index_);
  installed_ = true;
}

bool CapLoadSession::appletClassIndex(uint16_t &class_index) const noexcept {
  if (!installed_ || applet_class_index_ == 0xffff) {
    return false;
  }
  class_index = applet_class_index_;
  return true;
}
