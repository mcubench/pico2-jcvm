#include "cap_load.hpp"
#include "cap_store.hpp"
#include "gp_delete.hpp"
#include "ffi.h"
#include "vm_lifecycle.h"
#include "vm_filesystem.h"
#include "vm_cap_initialization.h"

#ifdef PC_VERSION
#include <cstdio>
#include <cstdlib>
// The host filesystem backend writes each record synchronously, so it has no
// separate snapshot flush phase.
namespace {
bool test_fail_install_journal_clear_sync_armed = false;
bool test_fail_install_journal_clear_sync_injected = false;
} // namespace

extern "C" uint8_t fs_sync(void) {
  if (test_fail_install_journal_clear_sync_armed) {
    test_fail_install_journal_clear_sync_armed = false;
    test_fail_install_journal_clear_sync_injected = true;
    std::fprintf(stderr,
                 "VPCD:TEST_FAILED_INSTALL_JOURNAL_CLEAR_SYNC\n");
    return 1;
  }
  return 0;
}
extern "C" uint8_t fs_erase_prefix(const uint8_t *prefix,
                                    uint8_t prefix_length) {
  return fs_erase(prefix, prefix_length);
}
extern "C" uint8_t fs_erase_package_state(uint8_t) { return 0; }
#endif
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_handlers/package.hpp"

#include <cstring>
#include <cstddef>
#include <cstdio>

namespace {
bool find_cap_applet_install_offset(const uint8_t *code, size_t length,
                                    const uint8_t *aid, uint8_t aid_length,
                                    uint16_t &install_offset) noexcept;
}

extern "C" uint8_t choupi_vm_find_applet(const uint8_t *aid,
                                           uint8_t aid_length,
                                           uint8_t *package_id,
                                           uint16_t *install_offset) {
  if (aid == nullptr || package_id == nullptr || install_offset == nullptr)
    return 1;
  try {
    uint8_t resolved_package = 0;
    uint8_t applet_aid[16]{};
    uint8_t applet_aid_length = 0;
    uint16_t class_index = 0xffff;
    if (!cap_store_find_instance(aid, aid_length, resolved_package,
                                 applet_aid, applet_aid_length,
                                 class_index) ||
        !jcvm::FlashMemory_Handler::isDynamicPackageID(resolved_package))
      return 1;

    uint32_t cap_length = 0;
    const uint8_t *cap_code =
        cap_store_data_for_package(resolved_package, &cap_length);
    uint16_t resolved_offset = 0;
    if (cap_code == nullptr || cap_length == 0 ||
        !find_cap_applet_install_offset(cap_code, cap_length, applet_aid,
                                        applet_aid_length,
                                        resolved_offset)) {
      return 1;
    }
    *package_id = resolved_package;
    *install_offset = resolved_offset;
  } catch (...) {
    return 1;
  }
  return 0;
}

extern "C" uint8_t choupi_vm_is_dynamic_package(uint8_t package_id) {
  return jcvm::FlashMemory_Handler::isDynamicPackageID(package_id) ? 1 : 0;
}

extern "C" uint8_t choupi_cap_package_applet_aid(
    uint8_t package_id, uint8_t applet_index, uint8_t *aid,
    uint8_t *aid_length) {
  if (aid == nullptr || aid_length == nullptr) return 0;
  uint32_t code_length = 0;
  const uint8_t *code = cap_store_data_for_package(package_id, &code_length);
  if (code == nullptr || code_length == 0) return 0;
  size_t offset = 0;
  while (offset + 3 <= code_length) {
    const uint8_t tag = code[offset];
    const size_t component_length =
        (static_cast<size_t>(code[offset + 1]) << 8) | code[offset + 2];
    if (component_length > code_length - offset - 3) return 0;
    if (tag == 3) {
      const uint8_t *payload = code + offset + 3;
      if (component_length < 1) return 0;
      const uint8_t count = payload[0];
      size_t cursor = 1;
      for (uint8_t index = 0; index < count; ++index) {
        if (cursor >= component_length) return 0;
        const uint8_t current_length = payload[cursor++];
        if (current_length < 5 || current_length > 16 ||
            current_length > component_length - cursor ||
            component_length - cursor - current_length < 2) return 0;
        if (index == applet_index) {
          std::memcpy(aid, payload + cursor, current_length);
          *aid_length = current_length;
          return 1;
        }
        cursor += current_length + 2;
      }
      return 0;
    }
    offset += 3 + component_length;
  }
  return 0;
}

namespace {
constexpr uint8_t kIsdAid[] = {0xa0, 0x00, 0x00, 0x01,
                               0x51, 0x00, 0x00, 0x00};
constexpr uint8_t kFirstDynamicPackageId = 56;
constexpr uint8_t kDynamicPackageCount = 8;
constexpr size_t kMaxLoadTlvLength = 64 * 1024;
// File type 0x02 is the OS-approved static-field namespace. Package 0xff is
// outside the VM package range and reserved here for card-manager metadata.
constexpr uint8_t kLifecycleTagType = 0x02;
constexpr uint8_t kLifecyclePackageId = 0xff;
constexpr uint8_t kAppletDeletedState = 1;
constexpr uint8_t kPackageDeletedState = 2;
constexpr uint8_t kInstallJournalKind = 3;
constexpr uint32_t kInstallJournalMagic = 0x32534e49; // "INS2"
constexpr uint32_t kLegacyInstallJournalMagic = 0x31534e49; // "INS1"

bool aid_matches(const uint8_t *left, uint8_t left_length,
                 const uint8_t *right, size_t right_length) noexcept;

#pragma pack(push, 1)
struct InstallJournalRecord {
  uint32_t magic;
  uint8_t package_id;
  uint8_t package_aid_length;
  uint8_t applet_aid_length;
  uint8_t instance_aid_length;
  uint8_t applet_owner;
  uint16_t applet_class_index;
  uint8_t package_aid[16];
  uint8_t applet_aid[16];
  uint8_t instance_aid[16];
  uint32_t crc32;
};

struct LegacyInstallJournalRecord {
  uint32_t magic;
  uint8_t package_id;
  uint8_t package_aid_length;
  uint8_t applet_aid_length;
  uint8_t instance_aid_length;
  uint16_t applet_class_index;
  uint8_t package_aid[16];
  uint8_t applet_aid[16];
  uint8_t instance_aid[16];
  uint32_t crc32;
};
#pragma pack(pop)

jcvm::fs::Tag lifecycle_tag(uint8_t package_id, uint8_t kind) noexcept {
  jcvm::fs::Tag tag;
  tag.len = 4;
  tag.value[0] = kLifecycleTagType;
  tag.value[1] = kLifecyclePackageId;
  tag.value[2] = package_id;
  tag.value[3] = kind;
  return tag;
}

uint32_t install_journal_crc32(const uint8_t *data, size_t length) noexcept {
  uint32_t crc = 0xffffffffu;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (unsigned int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

bool valid_install_journal(const InstallJournalRecord &record,
                           uint8_t package_id) noexcept {
  return record.magic == kInstallJournalMagic &&
         record.package_id == package_id &&
         record.package_aid_length >= 5 && record.package_aid_length <= 16 &&
         record.applet_aid_length >= 5 && record.applet_aid_length <= 16 &&
         record.instance_aid_length >= 5 &&
         record.instance_aid_length <= 16 &&
         record.applet_class_index != 0xffff &&
         record.crc32 == install_journal_crc32(
                             reinterpret_cast<const uint8_t *>(&record),
                             offsetof(InstallJournalRecord, crc32));
}

bool read_install_journal(uint8_t package_id,
                          InstallJournalRecord &record) noexcept {
  const auto tag = lifecycle_tag(package_id, kInstallJournalKind);
  if (fs_exists(tag.value, tag.len) == 0) return false;
  try {
    auto [length, data] =
        jcvm::FlashMemory_Handler::readPersistentRecord(tag);
    if (data == nullptr) return false;
    if (length == sizeof(record)) {
      std::memcpy(&record, data.get(), sizeof(record));
      return valid_install_journal(record, package_id);
    }
    if (length != sizeof(LegacyInstallJournalRecord)) return false;
    LegacyInstallJournalRecord legacy{};
    std::memcpy(&legacy, data.get(), sizeof(legacy));
    if (legacy.magic != kLegacyInstallJournalMagic ||
        legacy.package_id != package_id ||
        legacy.package_aid_length < 5 || legacy.package_aid_length > 16 ||
        legacy.applet_aid_length < 5 || legacy.applet_aid_length > 16 ||
        legacy.instance_aid_length < 5 || legacy.instance_aid_length > 16 ||
        legacy.applet_class_index == 0xffff ||
        legacy.crc32 != install_journal_crc32(
                            reinterpret_cast<const uint8_t *>(&legacy),
                            offsetof(LegacyInstallJournalRecord, crc32)))
      return false;
    record = {};
    record.magic = kInstallJournalMagic;
    record.package_id = legacy.package_id;
    record.package_aid_length = legacy.package_aid_length;
    record.applet_aid_length = legacy.applet_aid_length;
    record.instance_aid_length = legacy.instance_aid_length;
    record.applet_owner = 0; // v1 install roots were persisted as owner 0.
    record.applet_class_index = legacy.applet_class_index;
    std::memcpy(record.package_aid, legacy.package_aid, sizeof(record.package_aid));
    std::memcpy(record.applet_aid, legacy.applet_aid, sizeof(record.applet_aid));
    std::memcpy(record.instance_aid, legacy.instance_aid,
                sizeof(record.instance_aid));
    record.crc32 = install_journal_crc32(
        reinterpret_cast<const uint8_t *>(&record),
        offsetof(InstallJournalRecord, crc32));
    return true;
  } catch (...) {
    return false;
  }
}

bool write_install_journal(const InstallJournalRecord &record) noexcept {
  const auto tag = lifecycle_tag(record.package_id, kInstallJournalKind);
  return jcvm::FlashMemory_Handler::writePersistentRecord(
      tag, reinterpret_cast<const uint8_t *>(&record), sizeof(record));
}

bool clear_install_journal(uint8_t package_id) noexcept {
#ifdef PC_VERSION
  static bool failure_injected = false;
  const char *leave_journal =
      std::getenv("CHOUPI_TEST_LEAVE_INSTALL_JOURNAL_ONCE");
  if (leave_journal != nullptr && leave_journal[0] == '1' &&
      !failure_injected) {
    failure_injected = true;
    std::fprintf(stderr, "VPCD:TEST_LEFT_INSTALL_JOURNAL package=%u\n",
                 static_cast<unsigned>(package_id));
    return true;
  }
  const char *fail_clear_sync =
      std::getenv("CHOUPI_TEST_FAIL_INSTALL_JOURNAL_CLEAR_SYNC_ONCE");
  if (fail_clear_sync != nullptr && fail_clear_sync[0] == '1' &&
      !test_fail_install_journal_clear_sync_injected) {
    test_fail_install_journal_clear_sync_armed = true;
    std::fprintf(stderr,
                 "VPCD:TEST_ARMED_INSTALL_JOURNAL_CLEAR_SYNC package=%u\n",
                 static_cast<unsigned>(package_id));
  }
#endif
  const auto tag = lifecycle_tag(package_id, kInstallJournalKind);
  if (!jcvm::FlashMemory_Handler::beginPersistentWriteTransaction())
    return false;
  if (!jcvm::FlashMemory_Handler::erasePersistentTag(tag)) {
    (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    return false;
  }
  return jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
}

bool recover_pending_install_journals() noexcept {
  bool recovered_all = true;
  for (uint8_t package_id = kFirstDynamicPackageId;
       package_id < kFirstDynamicPackageId + kDynamicPackageCount;
       ++package_id) {
    InstallJournalRecord record{};
    if (!read_install_journal(package_id, record)) {
      const auto tag = lifecycle_tag(package_id, kInstallJournalKind);
      if (fs_exists(tag.value, tag.len) != 0) recovered_all = false;
      continue;
    }

    uint8_t stored_package_aid[16]{};
    uint8_t stored_package_aid_length = 0;
    uint32_t cap_length = 0;
    if (!cap_store_package_aid(package_id, stored_package_aid,
                               stored_package_aid_length) ||
        cap_store_data_for_package(package_id, &cap_length) == nullptr ||
        cap_length == 0 ||
        !aid_matches(record.package_aid, record.package_aid_length,
                     stored_package_aid, stored_package_aid_length)) {
      recovered_all = false;
      continue;
    }

    uint8_t existing_package_id = 0xff;
    uint8_t existing_applet_aid[16]{};
    uint8_t existing_applet_aid_length = 0;
    uint16_t existing_class_index = 0xffff;
    if (cap_store_find_instance(record.instance_aid,
                                record.instance_aid_length,
                                existing_package_id, existing_applet_aid,
                                existing_applet_aid_length,
                                existing_class_index)) {
      if (existing_package_id != package_id ||
          !aid_matches(record.applet_aid, record.applet_aid_length,
                       existing_applet_aid,
                       existing_applet_aid_length) ||
          existing_class_index != record.applet_class_index) {
        recovered_all = false;
        continue;
      }
    } else if (choupi_store_cap_instance(
                   package_id, record.applet_aid,
                   record.applet_aid_length, record.instance_aid,
                   record.instance_aid_length,
                   record.applet_class_index,
                   record.applet_owner) != 0) {
      recovered_all = false;
      continue;
    }

    if (!clear_install_journal(package_id)) {
      recovered_all = false;
    } else {
#ifdef PC_VERSION
      std::fprintf(stderr, "VPCD:INSTALL_JOURNAL_RECOVERED package=%u\n",
                   static_cast<unsigned>(package_id));
#endif
    }
  }
  return recovered_all;
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

bool clear_applet_persistent_state(uint8_t applet_owner,
                                   uint8_t package_id,
                                   uint16_t class_index) noexcept {
  // Root object and instance fields use the same tag namespace. Any child
  // array/object records become unreachable when these owning fields are
  // erased and are overwritten if the applet is installed again.
  for (uint16_t field = 0; field <= 0xff; ++field) {
    const auto tag = jcvm::FlashMemory_Handler::getPersistentFieldTag(
        applet_owner, package_id, class_index, static_cast<uint8_t>(field));
    if (!jcvm::FlashMemory_Handler::erasePersistentPrefix(tag)) return false;
    if (class_index > UINT8_MAX) {
      const auto legacy_tag =
          jcvm::FlashMemory_Handler::getLegacyPersistentFieldTag(
              applet_owner, package_id, class_index,
              static_cast<uint8_t>(field));
      if (!jcvm::FlashMemory_Handler::erasePersistentPrefix(legacy_tag))
        return false;
    }
  }
  return true;
}

bool clear_package_static_state(uint8_t package_id) noexcept {
#ifdef PC_VERSION
  for (uint16_t field = 0; field <= 0xff; ++field) {
    const auto tag = jcvm::FlashMemory_Handler::getStaticFieldTag(
        package_id, static_cast<uint8_t>(field));
    if (!jcvm::FlashMemory_Handler::erasePersistentPrefix(tag)) return false;
  }
  return true;
#else
  return jcvm::FlashMemory_Handler::erasePersistentPackageState(package_id);
#endif
}

bool clear_orphaned_dynamic_persistent_state() noexcept {
  bool has_orphans = false;
  for (uint8_t package_id = kFirstDynamicPackageId;
       package_id < kFirstDynamicPackageId + kDynamicPackageCount;
       ++package_id) {
    uint32_t cap_length = 0;
    if (!jcvm::FlashMemory_Handler::isPackageExist(package_id) &&
        cap_store_data_for_package(package_id, &cap_length) == nullptr) {
      has_orphans = true;
      break;
    }
  }
  if (!has_orphans) return true;
  if (!jcvm::FlashMemory_Handler::beginPersistentWriteTransaction())
    return false;
  bool success = true;
  for (uint8_t package_id = kFirstDynamicPackageId;
       package_id < kFirstDynamicPackageId + kDynamicPackageCount;
       ++package_id) {
    uint32_t cap_length = 0;
    if (jcvm::FlashMemory_Handler::isPackageExist(package_id) ||
        cap_store_data_for_package(package_id, &cap_length) != nullptr) {
      continue;
    }
    if (!clear_package_static_state(package_id)) {
      success = false;
      break;
    }
  }
  if (!success) {
    (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    (void)fs_sync();
    return false;
  }
  if (fs_sync() != 0) {
    (void)jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    (void)fs_sync();
    return false;
  }
  if (!jcvm::FlashMemory_Handler::commitPersistentWriteTransaction())
    return false;
  return true;
}

bool clear_lifecycle_markers(uint8_t package_id) {
  jcvm::FlashMemory_Handler::setPersistentField_Byte(
      lifecycle_tag(package_id, 1), 0);
  jcvm::FlashMemory_Handler::setPersistentField_Byte(
      lifecycle_tag(package_id, 2), 0);
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

enum class CapImportValidation : uint8_t {
  valid,
  missing_package,
  internal_error,
};

CapImportValidation validate_cap_imports(uint8_t package_id) noexcept {
  try {
    const jcvm::Package package(package_id);
    const jcvm::JC_Cap cap = package.getCap();
    const jcvm::jc_cap_import_component *component = cap.getImport();
    if (component == nullptr) {
      return CapImportValidation::valid;
    }

    jcvm::Import_Handler imports(package);
    for (uint16_t index = 0; index < component->count; ++index) {
      const auto *imported_package =
          imports.getPackageAID(static_cast<uint8_t>(index));
      try {
        (void)imports.getPackageIndex(imported_package);
      } catch (jcvm::Exceptions exception) {
        if (exception == jcvm::RuntimeException) {
          return CapImportValidation::missing_package;
        }
        return CapImportValidation::internal_error;
      }
    }
    imports.validateRemoteInterfaceReferences();
  } catch (...) {
    return CapImportValidation::internal_error;
  }
  return CapImportValidation::valid;
}

bool allocate_dynamic_package_id(uint8_t &package_id) noexcept {
  for (uint8_t offset = 0; offset < kDynamicPackageCount; ++offset) {
    const uint8_t candidate =
        static_cast<uint8_t>(kFirstDynamicPackageId + offset);
    uint32_t existing_length = 0;
    if (!jcvm::FlashMemory_Handler::isPackageExist(candidate) &&
        cap_store_data_for_package(candidate, &existing_length) == nullptr) {
      package_id = candidate;
      return true;
    }
  }
  return false;
}

uint16_t read_u16_be(const uint8_t *data) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               data[1]);
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
  return found_header && offset == length;
}

bool find_cap_applet_install_offset(const uint8_t *code, size_t length,
                                    const uint8_t *aid, uint8_t aid_length,
                                    uint16_t &install_offset) noexcept {
  if (code == nullptr || aid == nullptr || aid_length < 5 || aid_length > 16)
    return false;
  size_t offset = 0;
  while (offset + 3 <= length) {
    const uint8_t tag = code[offset];
    const size_t component_length = read_u16_be(code + offset + 1);
    if (component_length > length - offset - 3) return false;
    if (tag == 3) {
      if (component_length < 1) return false;
      const uint8_t *payload = code + offset + 3;
      const uint8_t count = payload[0];
      size_t cursor = 1;
      bool found = false;
      uint16_t resolved_offset = 0;
      for (uint8_t index = 0; index < count; ++index) {
        if (cursor >= component_length) return false;
        const uint8_t current_length = payload[cursor++];
        if (current_length < 5 || current_length > 16 ||
            current_length > component_length - cursor ||
            component_length - cursor - current_length < 2) {
          return false;
        }
        const bool matches = current_length == aid_length &&
                             std::memcmp(payload + cursor, aid,
                                         aid_length) == 0;
        const size_t offset_index = cursor + current_length;
        if (matches) {
          if (found) return false;
          found = true;
          resolved_offset = read_u16_be(payload + offset_index);
        }
        cursor = offset_index + 2;
      }
      if (cursor != component_length || !found || resolved_offset == 0)
        return false;
      install_offset = resolved_offset;
      return true;
    }
    offset += 3 + component_length;
  }
  return false;
}
} // namespace

void CapLoadSession::reset() noexcept {
  load_package_aid_length_ = 0;
  applet_aid_length_ = 0;
  instance_aid_length_ = 0;
  applet_class_index_ = 0xffff;
  package_id_ = 0xff;
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
      cap_store_find_package(load_package_aid_, load_package_aid_length_,
                             package_id_) ||
      !clear_orphaned_dynamic_persistent_state() ||
      !allocate_dynamic_package_id(package_id_) ||
      choupi_store_cap_package(package_id_, load_package_aid_,
                               load_package_aid_length_, load_data_ + offset,
                               static_cast<uint32_t>(code_length)) != 0) {
    return false;
  }
  if (!jcvm::FlashMemory_Handler::beginPersistentWriteTransaction()) {
    return false;
  }
  bool transaction_active = true;
  try {
    clear_lifecycle_markers(package_id_);
    jcvm::FlashMemory_Handler::enablePackage(package_id_);
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
  if (data == nullptr) {
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
  // INSTALL is a separate GlobalPlatform command and commonly arrives after
  // another PC/SC power-on. In that case restoreInstalled() may have changed
  // the session's current package to a different installed applet. Resolve the
  // package from this command's package AID every time; do not rely on the
  // transient LOAD/session fields.
  if (!read_aid(data, length, offset, package_aid, package_aid_length) ||
      !cap_store_find_package(package_aid, package_aid_length, package_id_)) {
    install_failure_stage_ = 4;
    return false;
  }
  uint32_t requested_cap_length = 0;
  const uint8_t *requested_cap =
      cap_store_data_for_package(package_id_, &requested_cap_length);
  uint8_t stored_package_aid[16]{};
  uint8_t stored_package_aid_length = 0;
  uint8_t first_applet_aid[16]{};
  uint8_t first_applet_aid_length = 0;
  if (requested_cap == nullptr || requested_cap_length == 0 ||
      !validate_cap_code(requested_cap, requested_cap_length,
                         stored_package_aid, stored_package_aid_length,
                         first_applet_aid, first_applet_aid_length) ||
      !aid_matches(package_aid, package_aid_length, stored_package_aid,
                   stored_package_aid_length) ||
      !read_aid(data, length, offset, applet_aid, applet_aid_length)) {
    install_failure_stage_ = 2;
    return false;
  }
  std::memcpy(load_package_aid_, stored_package_aid, stored_package_aid_length);
  load_package_aid_length_ = stored_package_aid_length;
  loaded_ = true;
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
  uint32_t cap_length = 0;
  const uint8_t *cap_code =
      cap_store_data_for_package(package_id_, &cap_length);
  if (!jcvm::FlashMemory_Handler::isDynamicPackageID(package_id_) ||
      cap_code == nullptr || cap_length == 0 ||
      !find_cap_applet_install_offset(cap_code, cap_length, applet_aid,
                                       applet_aid_length,
                                       install_method_offset)) {
    install_failure_stage_ = 4;
    return false;
  }
  resolved_package_id = package_id_;

  const CapImportValidation import_validation =
      validate_cap_imports(resolved_package_id);
  if (import_validation == CapImportValidation::missing_package) {
    install_failure_stage_ = 10;
    return false;
  }
  if (import_validation != CapImportValidation::valid) {
    install_failure_stage_ = 9;
    return false;
  }

  if (!recover_pending_install_journals()) {
    install_failure_stage_ = 11;
    return false;
  }
  uint8_t applet_owner = 0;
  if (!cap_store_allocate_applet_owner(applet_owner)) {
    install_failure_stage_ = 11;
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
    clear_lifecycle_markers(resolved_package_id);
    if (!jcvm::initializeCapStaticArrays(resolved_package_id)) {
      install_failure_stage_ = rollback() ? 5 : 8;
      return false;
    }
    install_vm_status_ = runtime_install_method(
        resolved_package_id, applet_owner, install_method_offset,
        install_parameters,
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
    InstallJournalRecord journal{};
    journal.magic = kInstallJournalMagic;
    journal.package_id = resolved_package_id;
    journal.package_aid_length = load_package_aid_length_;
    journal.applet_aid_length = applet_aid_length;
    journal.instance_aid_length = instance_aid_length;
    journal.applet_owner = applet_owner;
    journal.applet_class_index = applet_class_index;
    std::memcpy(journal.package_aid, load_package_aid_,
                journal.package_aid_length);
    std::memcpy(journal.applet_aid, applet_aid, applet_aid_length);
    std::memcpy(journal.instance_aid, instance_aid, instance_aid_length);
    journal.crc32 = install_journal_crc32(
        reinterpret_cast<const uint8_t *>(&journal),
        offsetof(InstallJournalRecord, crc32));
    if (!write_install_journal(journal)) {
      install_failure_stage_ = rollback() ? 6 : 8;
      return false;
    }
    if (fs_sync() != 0) {
      install_failure_stage_ = rollback() ? 6 : 8;
      return false;
    }
    if (!jcvm::FlashMemory_Handler::commitPersistentWriteTransaction()) {
      transaction_active = false;
      install_failure_stage_ = 8;
      return false;
    }
    transaction_active = false;
    // The committed filesystem journal is the recovery point. If this
    // separate CAP-header update is interrupted, restoreInstalled() replays
    // the journal and publishes the registry entry idempotently.
    if (choupi_store_cap_instance(resolved_package_id, applet_aid,
                                  applet_aid_length, instance_aid,
                                  instance_aid_length,
                                  applet_class_index, applet_owner) != 0) {
      install_failure_stage_ = 8;
      return false;
    }
    if (!clear_install_journal(resolved_package_id)) {
      install_failure_stage_ = 8;
      return false;
    }
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
  // Card-manager DELETE may target any previously installed package or
  // instance. Restore its metadata before resolving the requested AID so the
  // result does not depend on the applet most recently selected on the card.
  if (!restoreInstalledForAid(aid, aid_length))
    (void)restorePackageForAid(aid, aid_length);
  const bool target_is_loaded_package =
      loaded_ && aid_matches(aid, aid_length, load_package_aid_,
                            load_package_aid_length_);
  if (target_is_loaded_package && !delete_related_objects &&
      cap_store_package_instance_count(package_id_) != 0) {
    return 0x6985;
  }
  gp::DeleteTarget target;
  const uint16_t target_status = gp::resolve_delete_target(
      loaded_, installed_, aid, aid_length, load_package_aid_,
      load_package_aid_length_, instance_aid_, instance_aid_length_,
      delete_related_objects, target);
  if (target_status != 0x9000) return target_status;
  const bool delete_package = target.package;
  uint8_t applet_owner = 0;
  if (!delete_package &&
      !cap_store_applet_owner(instance_aid_, instance_aid_length_,
                              applet_owner)) return 0x6581;

  struct DeleteBackend {
    bool begin() noexcept {
      stage = 1;
      return jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    }
    uint8_t package_id;
    uint8_t applet_owner;
    bool clear_applet(uint16_t class_index) noexcept {
      stage = 2;
      if (delete_package) {
        const uint8_t count = cap_store_package_instance_count(package_id);
        for (uint8_t index = 0; index < count; ++index) {
          uint8_t applet_aid[16]{};
          uint8_t applet_aid_length = 0;
          uint8_t instance_aid[16]{};
          uint8_t instance_aid_length = 0;
          uint16_t related_class_index = 0xffff;
          uint8_t related_owner = 0;
          if (!cap_store_package_instance_at(
                  package_id, index, applet_aid, applet_aid_length,
                  instance_aid, instance_aid_length, related_class_index) ||
              !cap_store_applet_owner(instance_aid, instance_aid_length,
                                      related_owner) ||
              !clear_applet_persistent_state(related_owner, package_id,
                                             related_class_index))
            return false;
        }
        return true;
      }
      return clear_applet_persistent_state(applet_owner, package_id,
                                           class_index);
    }
    bool clear_package_static() noexcept {
      stage = 3;
      return clear_package_static_state(package_id);
    }
    void mark_deleted(bool package) {
      stage = 4;
      if (!package) return;
      jcvm::FlashMemory_Handler::setPersistentField_Byte(
          lifecycle_tag(package_id, 2), kPackageDeletedState);
      jcvm::FlashMemory_Handler::disablePackage(package_id);
    }
    uint8_t sync() noexcept {
      stage = 5;
      return fs_sync();
    }
    bool rollback() noexcept {
      if (failure_stage == 0) failure_stage = stage;
      return jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    }
    bool commit() noexcept {
      stage = 6;
      return jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    }
    bool delete_package = false;
    uint8_t stage = 0;
    uint8_t failure_stage = 0;
  } backend{package_id_, applet_owner};

  backend.delete_package = delete_package;

  const uint16_t transaction_status = gp::commit_delete_state(
      backend, delete_package,
      installed_ ||
          (delete_package && cap_store_package_instance_count(package_id_) != 0),
      applet_class_index_);
  if (transaction_status != 0x9000) {
#ifdef PC_VERSION
    std::fprintf(stderr, "VPCD:DELETE_FAIL stage=%u sw=%04X\n",
                 static_cast<unsigned>(backend.failure_stage != 0
                                           ? backend.failure_stage
                                           : backend.stage),
                 transaction_status);
#else
    std::printf("CHOUPI:DELETE_FAIL stage=%u sw=%04X\r\n",
                static_cast<unsigned>(backend.failure_stage != 0
                                          ? backend.failure_stage
                                          : backend.stage),
                transaction_status);
    std::fflush(stdout);
#endif
    return transaction_status;
  }

  if (delete_package) {
    const uint8_t deleted_package_id = package_id_;
    reset();
    if (!cap_store_delete_package(deleted_package_id)) {
#ifdef PC_VERSION
      std::fprintf(stderr, "VPCD:DELETE_FAIL stage=7 sw=6581\n");
#else
      std::printf("CHOUPI:DELETE_FAIL stage=7 sw=6581\r\n");
      std::fflush(stdout);
#endif
      return 0x6581;
    }
    return 0x9000;
  } else {
    if (!cap_store_remove_package_instance(package_id_, aid, aid_length))
      return 0x6581;
    installed_ = false;
    instance_aid_length_ = 0;
    applet_class_index_ = 0xffff;
  }
  return 0x9000;
}

bool CapLoadSession::recoverPendingInstalls() noexcept {
  return recover_pending_install_journals();
}

void CapLoadSession::restoreInstalled() noexcept {
  reset();
  (void)recoverPendingInstalls();
  for (uint8_t id = kFirstDynamicPackageId;
       id < kFirstDynamicPackageId + kDynamicPackageCount; ++id) {
    const uint8_t count = cap_store_package_instance_count(id);
    for (uint8_t index = 0; index < count; ++index) {
      uint8_t applet_aid[16]{};
      uint8_t applet_aid_length = 0;
      uint8_t instance_aid[16]{};
      uint8_t instance_aid_length = 0;
      uint16_t class_index = 0xffff;
      if (cap_store_package_instance_at(id, index, applet_aid,
                                        applet_aid_length, instance_aid,
                                        instance_aid_length, class_index) &&
          restoreInstalledForAid(instance_aid, instance_aid_length)) return;
    }
  }
}

bool CapLoadSession::restorePackageForAid(
    const uint8_t *requested_package_aid,
    uint8_t requested_package_aid_length) noexcept {
  reset();
  if (requested_package_aid == nullptr || requested_package_aid_length < 5 ||
      requested_package_aid_length > 16) return false;
  uint8_t package_id = 0xff;
  if (!cap_store_find_package(requested_package_aid,
                              requested_package_aid_length, package_id))
    return false;
  uint8_t package_state = 0;
  if (!read_lifecycle_state(lifecycle_tag(package_id, 2), package_state) ||
      package_state == kPackageDeletedState) return false;
  uint32_t code_length = 0;
  const uint8_t *code = cap_store_data_for_package(package_id, &code_length);
  if (code == nullptr ||
      !validate_cap_code(code, code_length, load_package_aid_,
                         load_package_aid_length_, applet_aid_,
                         applet_aid_length_)) return false;
  package_id_ = package_id;
  std::memcpy(load_package_aid_, requested_package_aid,
              requested_package_aid_length);
  load_package_aid_length_ = requested_package_aid_length;
  loaded_ = true;
  return true;
}

bool CapLoadSession::restoreInstalledForAid(
    const uint8_t *requested_instance_aid,
    uint8_t requested_instance_aid_length) noexcept {
  reset();
  if (requested_instance_aid == nullptr ||
      requested_instance_aid_length < 5 ||
      requested_instance_aid_length > 16)
    return false;
  uint8_t package_id = 0xff;
  uint8_t applet_aid[16]{};
  uint8_t applet_aid_length = 0;
  uint16_t applet_class_index = 0xffff;
  if (!cap_store_find_instance(requested_instance_aid,
                               requested_instance_aid_length, package_id,
                               applet_aid, applet_aid_length,
                               applet_class_index)) return false;
  {
    uint8_t instance_aid[16]{};
    std::memcpy(instance_aid, requested_instance_aid,
                requested_instance_aid_length);
    const uint8_t instance_aid_length = requested_instance_aid_length;
    uint32_t code_length = 0;
    const uint8_t *code =
        cap_store_data_for_package(package_id, &code_length);
    if (code == nullptr ||
        !validate_cap_code(code, code_length, load_package_aid_,
                           load_package_aid_length_, applet_aid_,
                           applet_aid_length_)) { reset(); return false; }
    package_id_ = package_id;
    uint8_t package_state = 0;
    if (!read_lifecycle_state(lifecycle_tag(package_id, 2), package_state) ||
        (package_state != 0 && package_state != kPackageDeletedState) ||
        package_state == kPackageDeletedState) {
      reset();
      return false;
    }
    std::memcpy(applet_aid_, applet_aid, applet_aid_length);
    applet_aid_length_ = applet_aid_length;
    std::memcpy(instance_aid_, instance_aid, instance_aid_length);
    instance_aid_length_ = instance_aid_length;
    applet_class_index_ = applet_class_index;
    installed_ = true;
    loaded_ = true;
    return true;
  }
  reset();
  return false;
}

bool CapLoadSession::appletClassIndex(uint16_t &class_index) const noexcept {
  if (!installed_ || applet_class_index_ == 0xffff) {
    return false;
  }
  class_index = applet_class_index_;
  return true;
}
