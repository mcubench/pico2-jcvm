#include "ffi.h"
#include "cap_store.hpp"
#include "flash_layout.hpp"
#include "vm_filesystem.h"
#include "object_deletion_policy.hpp"
#include "gp_lifecycle_recovery.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "persistent_inventory.hpp"
#include "heap.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_handlers/package.hpp"
#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <cstring>
#include <limits>
#include <vector>

extern "C" const uint8_t choupi_rommask_start[] = {0};
extern "C" const uint8_t choupi_rommask_end[] = {0};

extern "C" uint8_t rp2350_fs_test_write_parent_encoded(
    const uint8_t *tag, uint8_t tag_length, const uint8_t *data,
    uint32_t length);
extern "C" uint32_t rp2350_fs_test_overlay_file_count(void);
extern "C" uint32_t rp2350_fs_test_live_record_count(void);
extern "C" uint32_t rp2350_fs_test_overlay_bytes_used(void);
extern "C" uint32_t choupi_available_persistent_memory_bytes(void);
extern "C" uint32_t choupi_persistent_tombstones_since_compaction(void);
extern "C" void choupi_persistent_memory_usage(uint32_t *files_used,
                                                  uint32_t *bytes_used,
                                                  uint32_t *byte_capacity);
extern "C" uint8_t rp2350_fs_test_corrupt_record_header(
    const uint8_t *tag, uint8_t tag_length, uint8_t header);
extern "C" uint8_t rp2350_fs_test_corrupt_record_hash(
    const uint8_t *tag, uint8_t tag_length);
extern "C" uint8_t rp2350_fs_test_corrupt_record_tag_length(
    const uint8_t *tag, uint8_t tag_length, uint8_t replacement_length);
extern "C" uint8_t rp2350_fs_test_integrity_valid(void);
extern "C" uint8_t rp2350_fs_test_share_record_offset(
    const uint8_t *source_tag, uint8_t source_tag_length,
    const uint8_t *target_tag, uint8_t target_tag_length);
extern "C" uint8_t rp2350_fs_test_overlap_record_offset(
    const uint8_t *source_tag, uint8_t source_tag_length,
    const uint8_t *target_tag, uint8_t target_tag_length);
extern "C" uint8_t rp2350_fs_test_move_slot_beyond_probe_gap(
    const uint8_t *tag, uint8_t tag_length);
extern "C" uint8_t rp2350_fs_test_repoint_parent_encoded(
    const uint8_t *tag, uint8_t tag_length, const uint8_t *parent_tag,
    uint8_t parent_tag_length);
extern "C" uint8_t rp2350_fs_test_invalidate_parent_reference(
    const uint8_t *tag, uint8_t tag_length);
extern "C" uint8_t rp2350_fs_test_compact(void);
extern "C" void rp2350_fs_test_fail_bulk_erase_after(uint32_t count);
extern "C" void rp2350_fs_test_fail_next_sync(void);
extern "C" void rp2350_fs_test_hide_exists_tag(const uint8_t *tag,
                                                uint8_t tag_length);
extern "C" void rp2350_fs_test_reset_persistent_transaction_state(void);
namespace {
std::array<uint8_t, 2 * 1024 * 1024> fake_flash;
std::array<uint8_t, 2 * 1024 * 1024> committed_flash;
std::array<uint8_t, 2 * 1024 * 1024> cross_store_empty_journal_flash;
std::array<uint8_t, 2 * 1024 * 1024> cross_store_committed_flash;
std::array<uint8_t, 0x27000> filesystem_snapshot_scratch;
size_t flash_mutation_count = 0;
size_t fail_after_mutations = static_cast<size_t>(-1);
bool persistent_transaction_active = false;

struct ReclaimerRootContext {
  const std::vector<jcvm::jref_t> *references;
};

bool enumerate_reclaimer_roots(jcvm::Heap::ReferenceVisitor visitor,
                               void *visitor_context, void *root_context) {
  if (visitor == nullptr || root_context == nullptr) return false;
  const auto &references =
      *static_cast<ReclaimerRootContext *>(root_context)->references;
  for (const auto &reference : references) {
    visitor(reference, visitor_context);
  }
  return true;
}

bool persistent_root_graph_matches(
    jcvm::Heap &heap, const jcvm::jref_t root_reference,
    const std::array<uint8_t, 3> &expected_bytes) {
  const auto root = heap.getArray(root_reference);
  if (!root || root->getType() != jcvm::JAVA_ARRAY_T_REFERENCE ||
      root->size() != 1) {
    return false;
  }
  const auto child = heap.getArray(root->getReferenceEntry(0));
  if (!child || child->getType() != jcvm::JAVA_ARRAY_T_BYTE ||
      child->size() != expected_bytes.size()) {
    return false;
  }
  for (uint16_t index = 0; index < expected_bytes.size(); ++index) {
    if (child->getByteEntry(index) != expected_bytes[index]) return false;
  }
  return true;
}

bool collect_reclaimer_candidate(
    const jcvm::PersistentInventory::Tag &tag, void *candidate_context) {
  if (candidate_context == nullptr) return false;
  static_cast<std::vector<jcvm::PersistentInventory::Tag> *>(
      candidate_context)->push_back(tag);
  return true;
}

} // namespace

// The overlay host test links selected production Heap/JC_Instance code but
// does not construct CAP classes or run VM diagnostics. Keep those unrelated
// firmware services inert in this test executable.
extern "C" void choupi_install_stage(uint8_t) {}
extern "C" void choupi_install_origin(uint32_t) {}
extern "C" void choupi_vm_record_array_write_failure(
    uint8_t, uint16_t, uint16_t, uint16_t, uint8_t, uint8_t) {}
extern "C" void choupi_vm_record_field_write(
    uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint8_t,
    uint8_t) {}
extern "C" void choupi_vm_record_field_bounds(
    uint16_t, uint16_t, uint16_t, uint16_t, uint16_t) {}
extern "C" void choupi_vm_record_persistent_restore_failure(
    uint16_t, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t) {}

namespace jcvm {
Package::Package(jpackage_ID_t package_id) noexcept : ID(package_id) {}
const uint16_t
Class_Handler::getInstanceFieldsSize(const jclass_index_t) const {
  return 0;
}
const jc_cap_class_ref
ConstantPool_Handler::getClassRef(const jc_cp_offset_t) {
  return {};
}
std::pair<Package, const uint8_t *>
ConstantPool_Handler::resolveClassref(const jc_cap_class_ref, const bool) {
  return {Package(0), nullptr};
}
const jc_cap_class_info *
ConstantPool_Handler::getClassFromClassIndex(const jclass_index_t) {
  return nullptr;
}
std::pair<Package, const jc_cap_class_info *>
ConstantPool_Handler::classref2class(const jc_cap_class_ref) {
  return {Package(0), nullptr};
}
jbool_t Class_Handler::docheckcast(
    const std::pair<Package, const uint8_t *>,
    const std::pair<Package, const uint8_t *>) {
  return TRUE;
}
} // namespace jcvm

namespace {

#pragma pack(push, 1)
struct FilesystemHeaderTest {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t generation;
  uint32_t file_count;
  uint32_t bytes_used;
  uint32_t payload_length;
  uint32_t payload_crc32;
  uint32_t header_crc32;
};

struct LegacyAppletInstanceRecord {
  uint8_t applet_aid_length;
  uint8_t instance_aid_length;
  uint16_t class_index;
  uint8_t applet_aid[16];
  uint8_t instance_aid[16];
};

struct LegacyCapSlotHeaderV3 {
  uint32_t magic;
  uint16_t version;
  uint8_t package_id;
  uint8_t package_aid_length;
  uint32_t code_length;
  uint32_t code_crc32;
  uint8_t package_aid[16];
  uint8_t instance_count;
  uint16_t reserved;
  LegacyAppletInstanceRecord instances[40];
  uint32_t header_crc32;
};
#pragma pack(pop)

struct SimulatedPowerCut {};

void reset_flash_failure(size_t fail_after = static_cast<size_t>(-1)) {
  flash_mutation_count = 0;
  fail_after_mutations = fail_after;
}

void before_flash_mutation() {
  if (flash_mutation_count == fail_after_mutations) {
    throw SimulatedPowerCut{};
  }
  ++flash_mutation_count;
}

uint32_t test_crc32(const uint8_t *data, size_t length) {
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

void convert_v4_snapshot_to_legacy(uint32_t offset, uint16_t version) {
  auto *header = reinterpret_cast<FilesystemHeaderTest *>(
      fake_flash.data() + offset);
  if (header->version != 5 || version < 2 || version > 3) std::abort();
  const uint8_t *source = fake_flash.data() + offset + 4096;
  size_t output_length = 0;
  uint32_t restored_count = 0;
  for (uint16_t slot = 0; slot < 8192; ++slot) {
    if ((source[slot / 8] & (1u << (slot % 8))) == 0) continue;
    const uint8_t *record = source + 1024 + restored_count * 8;
    std::memcpy(filesystem_snapshot_scratch.data() + output_length, &slot,
                sizeof(slot));
    output_length += sizeof(slot);
    std::memcpy(filesystem_snapshot_scratch.data() + output_length, record, 8);
    output_length += 8;
    ++restored_count;
  }
  if (restored_count != header->file_count) std::abort();
  const uint8_t *source_bytes =
      source + 1024 + restored_count * 8;
  std::memcpy(filesystem_snapshot_scratch.data() + output_length,
              source_bytes, header->bytes_used);
  output_length += header->bytes_used;
  std::memcpy(fake_flash.data() + offset + 4096,
              filesystem_snapshot_scratch.data(), output_length);
  header->version = version;
  header->payload_length = static_cast<uint32_t>(output_length);
  header->payload_crc32 = test_crc32(filesystem_snapshot_scratch.data(),
                                     output_length);
  header->header_crc32 = test_crc32(
      reinterpret_cast<const uint8_t *>(header),
      offsetof(FilesystemHeaderTest, header_crc32));
}

bool make_v4_snapshot_record_offsets_overlap(const uint32_t offset) {
  auto *header = reinterpret_cast<FilesystemHeaderTest *>(
      fake_flash.data() + offset);
  if ((header->version != 4 && header->version != 5) ||
      header->file_count < 2 ||
      header->payload_length > filesystem_snapshot_scratch.size()) {
    return false;
  }
  uint8_t *payload = fake_flash.data() + offset + 4096;
  uint8_t *records = payload + 1024;
  uint8_t *first = nullptr;
  uint8_t *second = nullptr;
  uint32_t record_rank = 0;
  for (uint16_t slot = 0; slot < 8192; ++slot) {
    if ((payload[slot / 8] & (1u << (slot % 8))) == 0) continue;
    uint8_t *record = records + record_rank * 8;
    if (first == nullptr) {
      first = record;
    } else if (second == nullptr) {
      second = record;
      break;
    }
    ++record_rank;
  }
  if (first == nullptr || second == nullptr) return false;
  second[4] = first[4];
  second[5] = first[5];
  header->payload_crc32 = test_crc32(payload, header->payload_length);
  header->header_crc32 = test_crc32(
      reinterpret_cast<const uint8_t *>(header),
      offsetof(FilesystemHeaderTest, header_crc32));
  return true;
}

bool v4_snapshot_checksums_valid(const uint32_t offset) {
  const auto *header = reinterpret_cast<const FilesystemHeaderTest *>(
      fake_flash.data() + offset);
  if (header->magic != 0x3253464a ||
      (header->version != 4 && header->version != 5) ||
      header->payload_length > flash_layout::filesystem_slot_size - 4096) {
    return false;
  }
  const uint8_t *payload = fake_flash.data() + offset + 4096;
  return header->payload_crc32 ==
             test_crc32(payload, header->payload_length) &&
         header->header_crc32 == test_crc32(
             reinterpret_cast<const uint8_t *>(header),
             offsetof(FilesystemHeaderTest, header_crc32));
}
}

extern "C" const uint8_t *rp2350_fs_test_flash_base(void) {
  return fake_flash.data();
}

extern "C" uint8_t rp2350_fs_test_manual_transaction_active(void) {
  return persistent_transaction_active ? 1u : 0u;
}
extern "C" void choupi_record_transaction_buffer_full(void) {}

void flash_range_erase(uint32_t offset, size_t count) {
  if (offset > fake_flash.size() || count > fake_flash.size() - offset ||
      offset % 4096 != 0 || count % 4096 != 0) {
    std::abort();
  }
  for (size_t index = 0; index < count; ++index) {
    before_flash_mutation();
    fake_flash[offset + index] = 0xff;
  }
}

void flash_range_program(uint32_t offset, const uint8_t *data, size_t count) {
  if (offset > fake_flash.size() || count > fake_flash.size() - offset ||
      offset % 256 != 0 || count % 256 != 0 ||
      reinterpret_cast<uintptr_t>(data) % 4 != 0) {
    std::abort();
  }
  for (size_t index = 0; index < count; ++index) {
    before_flash_mutation();
    if ((fake_flash[offset + index] & data[index]) != data[index]) {
      std::abort();
    }
    fake_flash[offset + index] &= data[index];
  }
}

int flash_safe_execute(void (*func)(void *), void *param, uint32_t) {
  try {
    func(param);
    return 0;
  } catch (const SimulatedPowerCut &) {
    return -1;
  }
}

namespace {
int failures = 0;

void expect(bool condition, const char *name) {
  if (condition) {
    std::printf("TEST:PASS %s\n", name);
  } else {
    std::printf("TEST:FAIL %s\n", name);
    ++failures;
  }
}

uint16_t hash_bucket(const std::array<uint8_t, 3> &tag) {
  uint32_t hash = 2166136261u;
  for (const uint8_t byte : tag) {
    hash = (hash ^ byte) * 16777619u;
  }
  uint16_t compact_hash = static_cast<uint16_t>(hash ^ (hash >> 16));
  if (compact_hash == 0) compact_hash = 1;
  return compact_hash % 8192;
}

std::pair<std::array<uint8_t, 3>, std::array<uint8_t, 3>> colliding_tags() {
  std::unordered_map<uint16_t, std::array<uint8_t, 3>> first_by_bucket;
  for (uint32_t value = 0; value <= 0xffff; ++value) {
    const std::array<uint8_t, 3> tag = {
        0xa5, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    const auto bucket = hash_bucket(tag);
    const auto inserted = first_by_bucket.emplace(bucket, tag);
    if (!inserted.second && inserted.first->second != tag) {
      return {inserted.first->second, tag};
    }
  }
  std::abort();
}
} // namespace

int main(const int argc, char **argv) {
  if (argc == 3 && std::strcmp(argv[1], "--inspect-flash") == 0) {
    FILE *input = std::fopen(argv[2], "rb");
    if (input == nullptr) {
      std::fprintf(stderr, "OVERLAY_INSPECT:FAIL open=%s\n", argv[2]);
      return 2;
    }
    const size_t bytes_read = std::fread(fake_flash.data(), 1,
                                         fake_flash.size(), input);
    const bool exact_size = bytes_read == fake_flash.size() &&
                            std::fgetc(input) == EOF;
    const bool closed = std::fclose(input) == 0;
    if (!exact_size || !closed) {
      std::fprintf(stderr,
                   "OVERLAY_INSPECT:FAIL size expected=%zu actual=%zu\n",
                   fake_flash.size(), bytes_read);
      return 2;
    }
    if (fs_init() != FS_STATUS_OK || rp2350_fs_test_integrity_valid() != 1) {
      std::fprintf(stderr, "OVERLAY_INSPECT:FAIL snapshot_init_or_integrity\n");
      return 1;
    }

    uint32_t file_count = 0;
    uint32_t bytes_used = 0;
    uint32_t capacity = 0;
    choupi_persistent_memory_usage(&file_count, &bytes_used, &capacity);
    const uint32_t live_record_count = rp2350_fs_test_live_record_count();
    std::printf("OVERLAY_INSPECT:PASS slots=%u live_records=%u bytes=%u "
                "capacity=%u available=%u\n",
                file_count, live_record_count, bytes_used, capacity,
                choupi_available_persistent_memory_bytes());

    fs_tag_next_request_t cursor{};
    uint8_t empty_prefix = 0;
    uint32_t records = 0;
    uint64_t payload_bytes = 0;
    while (true) {
      const uint8_t status = fs_next_tag(&empty_prefix, 0, &cursor);
      if (status == 1) break;
      if (status != 0) {
        std::fprintf(stderr,
                     "OVERLAY_INSPECT:FAIL enumeration status=%u after=%u\n",
                     static_cast<unsigned>(status), records);
        return 1;
      }
      const uint8_t tag_length = cursor.tag[0];
      const uint8_t *tag = cursor.tag + 1;
      const uint8_t *data = nullptr;
      uint32_t length = 0;
      if (fs_read_inplace(tag, tag_length, &data, &length) != FS_STATUS_OK ||
          (length != 0 && data == nullptr)) {
        std::fprintf(stderr,
                     "OVERLAY_INSPECT:FAIL read tag_length=%u\n",
                     static_cast<unsigned>(tag_length));
        return 1;
      }
      std::printf("OVERLAY_RECORD tag=");
      for (uint8_t index = 0; index < tag_length; ++index) {
        std::printf("%02X", tag[index]);
      }
      std::printf(" length=%u\n", length);
      ++records;
      payload_bytes += length;
    }
    if (records != live_record_count) {
      std::fprintf(stderr,
                   "OVERLAY_INSPECT:FAIL enumerated=%u expected=%u\n",
                   records, live_record_count);
      return 1;
    }
    std::printf("OVERLAY_INSPECT:SUMMARY records=%u payload_bytes=%llu\n",
                records, static_cast<unsigned long long>(payload_bytes));
    return 0;
  }
  if (argc != 1) {
    std::fprintf(stderr, "usage: rp2350_fs_overlay_test [--inspect-flash FILE]\n");
    return 2;
  }
  fake_flash.fill(0xff);
  expect(fs_init() == 0, "filesystem_initializes");

  constexpr uint8_t reusable_tag[] = {0xa0, 0x01, 0x02};
  constexpr uint8_t missing_tag[] = {0xa0, 0x01, 0xfe};
  constexpr uint8_t first_value[] = {0x10, 0x20, 0x30};
  uint8_t actual[3]{};
  uint32_t missing_length = 0;
  const uint8_t *missing_data = nullptr;
  expect(fs_read(missing_tag, sizeof(missing_tag), actual, sizeof(actual)) ==
                 FS_STATUS_NO_SUCH_TAG &&
             fs_length(missing_tag, sizeof(missing_tag), &missing_length) ==
                 FS_STATUS_NO_SUCH_TAG &&
             fs_read_inplace(missing_tag, sizeof(missing_tag), &missing_data,
                             &missing_length) == FS_STATUS_NO_SUCH_TAG &&
             fs_erase(missing_tag, sizeof(missing_tag)) ==
                 FS_STATUS_NO_SUCH_TAG,
         "missing_records_use_shared_status");
  expect(fs_read(reusable_tag, 0, actual, sizeof(actual)) ==
                 FS_STATUS_INVALID_TAG_LENGTH &&
             fs_write(reusable_tag, 0, first_value, sizeof(first_value)) ==
                 FS_STATUS_INVALID_TAG_LENGTH &&
             fs_erase(reusable_tag, 0) == FS_STATUS_INVALID_TAG_LENGTH &&
             fs_erase_prefix(reusable_tag, 0) ==
                 FS_STATUS_INVALID_TAG_LENGTH,
         "invalid_tag_lengths_use_shared_status");
  std::array<uint8_t, 129> oversized_tag{};
  expect(fs_read(oversized_tag.data(), oversized_tag.size(), actual,
                 sizeof(actual)) == FS_STATUS_INVALID_TAG_LENGTH &&
             fs_write(oversized_tag.data(), oversized_tag.size(), first_value,
                      sizeof(first_value)) == FS_STATUS_INVALID_TAG_LENGTH &&
             fs_erase(oversized_tag.data(), oversized_tag.size()) ==
                 FS_STATUS_INVALID_TAG_LENGTH,
         "oversized_tags_use_shared_status");
  expect(fs_write(reusable_tag, sizeof(reusable_tag), first_value,
                  sizeof(first_value)) == FS_STATUS_OK,
         "write_before_erase");
  expect(fs_read_1b_at(reusable_tag, sizeof(reusable_tag),
                       sizeof(first_value), actual) ==
                 FS_STATUS_IO_OUT_OF_BOUNDS &&
             fs_write_1b_at(reusable_tag, sizeof(reusable_tag),
                            sizeof(first_value), first_value[0]) ==
                 FS_STATUS_IO_OUT_OF_BOUNDS &&
             fs_read_2b_at(reusable_tag, sizeof(reusable_tag), UINT32_MAX,
                           reinterpret_cast<uint16_t *>(actual)) ==
                 FS_STATUS_IO_OUT_OF_BOUNDS &&
             fs_write_4b_at(reusable_tag, sizeof(reusable_tag), UINT32_MAX,
                            0) == FS_STATUS_IO_OUT_OF_BOUNDS,
         "typed_access_uses_shared_out_of_bounds_status");
  const uint8_t *borrowed_data = nullptr;
  uint32_t borrowed_length = 0;
  expect(fs_read_inplace(reusable_tag, sizeof(reusable_tag), &borrowed_data,
                         &borrowed_length) == 0 &&
             borrowed_data != nullptr && borrowed_length == sizeof(first_value) &&
             std::memcmp(borrowed_data, first_value, sizeof(first_value)) == 0,
         "inplace_read_borrows_record_bytes");
  expect(fs_sync() == 0 && borrowed_data != nullptr &&
             std::memcmp(borrowed_data, first_value, sizeof(first_value)) == 0,
         "sync_keeps_overlay_borrow_address_valid");
  uint8_t oversized_read[4] = {0, 0, 0, 0xa5};
  expect(fs_read(reusable_tag, sizeof(reusable_tag), oversized_read,
                 sizeof(oversized_read)) == 0 &&
             std::memcmp(oversized_read, first_value, sizeof(first_value)) == 0 &&
             oversized_read[3] == 0xa5,
         "pico_read_copies_available_bytes_for_oversized_buffer");
  constexpr uint8_t empty_tag[] = {0xa0, 0x01, 0x03};
  expect(fs_write(empty_tag, sizeof(empty_tag), nullptr, 0) == 0,
         "write_empty_record");
  borrowed_data = nullptr;
  borrowed_length = 1;
  expect(fs_read_inplace(empty_tag, sizeof(empty_tag), &borrowed_data,
                         &borrowed_length) == 0 &&
             borrowed_data != nullptr && borrowed_length == 0,
         "inplace_read_accepts_empty_record");
  expect(fs_erase(empty_tag, sizeof(empty_tag)) == 0,
         "erase_empty_record_after_borrow");
  expect(fs_erase(reusable_tag, sizeof(reusable_tag)) == 0,
         "erase_overlay_record");
  expect(fs_exists(reusable_tag, sizeof(reusable_tag)) == 0,
         "erased_record_is_hidden");
  expect(fs_read(reusable_tag, sizeof(reusable_tag), actual,
                 sizeof(actual)) != 0,
         "erased_record_cannot_be_read");

  constexpr uint8_t second_value[] = {0x42, 0x43, 0x44};
  expect(fs_write(reusable_tag, sizeof(reusable_tag), second_value,
                  sizeof(second_value)) == 0,
         "write_reuses_erased_tag");
  expect(fs_read(reusable_tag, sizeof(reusable_tag), actual,
                 sizeof(actual)) == 0 &&
             actual[0] == second_value[0] && actual[1] == second_value[1] &&
             actual[2] == second_value[2],
         "reused_tag_has_new_value");

  constexpr uint8_t tag_prefix[] = {0x03, 0x2a};
  constexpr uint8_t short_tag[] = {0x03, 0x2a, 0x38, 0x01, 0x00};
  uint8_t long_tag[80]{};
  std::memcpy(long_tag, "\x03\x2a\x38\x01\x00\x01", 6);
  for (size_t index = 6; index < sizeof(long_tag); index += 2) {
    long_tag[index] = static_cast<uint8_t>(index);
    long_tag[index + 1] = static_cast<uint8_t>(index);
  }
  constexpr uint8_t other_owner_tag[] = {0x03, 0x2b, 0x38, 0x01, 0x00};
  expect(fs_write(short_tag, sizeof(short_tag), first_value,
                  sizeof(first_value)) == 0 &&
             fs_write(long_tag, sizeof(long_tag), first_value,
                      sizeof(first_value)) == 0 &&
             fs_write(other_owner_tag, sizeof(other_owner_tag), first_value,
                      sizeof(first_value)) == 0,
         "write_tags_for_cursor_scan");
  fs_tag_next_request_t cursor{};
  bool found_short_tag = false;
  bool found_long_tag = false;
  unsigned int matching_tag_count = 0;
  uint8_t cursor_status = 0;
  while ((cursor_status =
              fs_next_tag(tag_prefix, sizeof(tag_prefix), &cursor)) == 0) {
    ++matching_tag_count;
    found_short_tag |= cursor.tag[0] == sizeof(short_tag) &&
                       std::memcmp(cursor.tag + 1, short_tag,
                                   sizeof(short_tag)) == 0;
    found_long_tag |= cursor.tag[0] == sizeof(long_tag) &&
                      std::memcmp(cursor.tag + 1, long_tag,
                                  sizeof(long_tag)) == 0;
  }
  expect(cursor_status == 1 && matching_tag_count == 2 && found_short_tag &&
             found_long_tag,
         "cursor_returns_matching_short_and_long_tags_only");
  cursor = {};
  cursor.offset = 1;
  expect(fs_next_tag(tag_prefix, sizeof(tag_prefix), &cursor) == 2,
         "cursor_rejects_invalid_backend_offset");
  cursor = {};
  expect(fs_erase(long_tag, sizeof(long_tag)) == 0,
         "erase_long_tag_before_rescan");
  found_short_tag = false;
  found_long_tag = false;
  matching_tag_count = 0;
  while ((cursor_status =
              fs_next_tag(tag_prefix, sizeof(tag_prefix), &cursor)) == 0) {
    ++matching_tag_count;
    found_short_tag |= cursor.tag[0] == sizeof(short_tag) &&
                       std::memcmp(cursor.tag + 1, short_tag,
                                   sizeof(short_tag)) == 0;
    found_long_tag |= cursor.tag[0] == sizeof(long_tag) &&
                      std::memcmp(cursor.tag + 1, long_tag,
                                  sizeof(long_tag)) == 0;
  }
  expect(cursor_status == 1 && matching_tag_count == 1 && found_short_tag &&
             !found_long_tag,
         "cursor_skips_erased_tags");

  const auto collision = colliding_tags();
  constexpr uint8_t collision_value[] = {0x55};
  uint8_t collision_actual = 0;
  expect(fs_write(collision.first.data(), collision.first.size(),
                  collision_value, sizeof(collision_value)) == 0 &&
             fs_write(collision.second.data(), collision.second.size(),
                      collision_value, sizeof(collision_value)) == 0,
         "write_colliding_tags");
  expect(fs_erase(collision.first.data(), collision.first.size()) == 0,
         "erase_first_colliding_tag");
  expect(fs_read(collision.second.data(), collision.second.size(),
                 &collision_actual, sizeof(collision_actual)) == 0 &&
             collision_actual == collision_value[0],
         "collision_probe_survives_erase");

  constexpr uint8_t parent_tag[] = {0x02, 0x14, 0x03};
  constexpr uint8_t child_tag[] = {0x02, 0x14, 0x03, 0x77, 0x77};
  expect(fs_write(parent_tag, sizeof(parent_tag), first_value,
                  sizeof(first_value)) == 0 &&
             fs_write(child_tag, sizeof(child_tag), second_value,
                      sizeof(second_value)) == 0,
         "write_parent_and_compressed_child");
  expect(fs_erase(child_tag, sizeof(child_tag)) == 0 &&
             fs_exists(child_tag, sizeof(child_tag)) == 0 &&
             fs_exists(parent_tag, sizeof(parent_tag)) == 1,
         "erase_compressed_child_preserves_parent");
  expect(fs_write(child_tag, sizeof(child_tag), second_value,
                  sizeof(second_value)) == 0 &&
             fs_read(child_tag, sizeof(child_tag), actual, sizeof(actual)) == 0 &&
             actual[0] == second_value[0],
         "reuse_compressed_child_entry");

  constexpr uint8_t deleted_parent_tag[] = {
      0x02, 0x38, 0x00, 0x05, 0x05};
  constexpr uint8_t child_of_deleted_parent_tag[] = {
      0x02, 0x38, 0x00, 0x05, 0x05, 0xff, 0xff};
  expect(fs_write(deleted_parent_tag, sizeof(deleted_parent_tag), first_value,
                  sizeof(first_value)) == 0 &&
             fs_erase(deleted_parent_tag, sizeof(deleted_parent_tag)) == 0 &&
             fs_exists(deleted_parent_tag, sizeof(deleted_parent_tag)) == 0,
         "delete_compressed_tag_parent");
  expect(fs_write(child_of_deleted_parent_tag,
                   sizeof(child_of_deleted_parent_tag), second_value,
                   sizeof(second_value)) == 0 &&
             fs_read(child_of_deleted_parent_tag,
                     sizeof(child_of_deleted_parent_tag), actual,
                     sizeof(actual)) == 0 &&
             actual[0] == second_value[0],
         "write_child_under_deleted_parent");

  constexpr uint8_t legacy_parent_tag[] = {
      0x02, 0x38, 0x00, 0x06, 0x06};
  constexpr uint8_t legacy_child_tag[] = {
      0x02, 0x38, 0x00, 0x06, 0x06, 0xff, 0xff};
  constexpr uint8_t legacy_value[] = {0x31, 0x32, 0x33};
  expect(fs_write(legacy_parent_tag, sizeof(legacy_parent_tag), first_value,
                  sizeof(first_value)) == 0 &&
             rp2350_fs_test_write_parent_encoded(
                 legacy_child_tag, sizeof(legacy_child_tag), legacy_value,
                 sizeof(legacy_value)) == 0 &&
             fs_erase(legacy_child_tag, sizeof(legacy_child_tag)) == 0 &&
             fs_erase(legacy_parent_tag, sizeof(legacy_parent_tag)) == 0,
         "create_legacy_parent_encoded_tombstone");
  expect(fs_write(legacy_child_tag, sizeof(legacy_child_tag), second_value,
                  sizeof(second_value)) == 0 &&
             fs_write(legacy_parent_tag, sizeof(legacy_parent_tag), first_value,
                      sizeof(first_value)) == 0 &&
             fs_read(legacy_child_tag, sizeof(legacy_child_tag), actual,
                     sizeof(actual)) == 0 &&
             actual[0] == second_value[0] && actual[1] == second_value[1] &&
             actual[2] == second_value[2],
         "live_record_wins_over_resurrected_legacy_tombstone");

  constexpr uint8_t namespace_prefix[] = {0x02, 0x14};
  constexpr uint8_t namespace_parent[] = {0x02, 0x14, 0x08};
  constexpr uint8_t namespace_child[] = {0x02, 0x14, 0x08, 0x31, 0x31};
  constexpr uint8_t namespace_grandchild[] = {
      0x02, 0x14, 0x08, 0x31, 0x31, 0x42, 0x42};
  expect(fs_write(namespace_parent, sizeof(namespace_parent), first_value,
                  sizeof(first_value)) == 0 &&
             fs_write(namespace_child, sizeof(namespace_child), second_value,
                      sizeof(second_value)) == 0 &&
             fs_write(namespace_grandchild, sizeof(namespace_grandchild),
                      first_value, sizeof(first_value)) == 0,
         "write_nested_namespace_records");
  expect(fs_erase_prefix(namespace_prefix, sizeof(namespace_prefix)) == 0,
         "erase_nested_namespace_prefix");
  expect(fs_exists(namespace_parent, sizeof(namespace_parent)) == 0,
         "erase_nested_namespace_parent");
  expect(fs_exists(namespace_child, sizeof(namespace_child)) == 0,
         "erase_nested_namespace_child");
  expect(fs_exists(namespace_grandchild,
                   sizeof(namespace_grandchild)) == 0,
         "erase_nested_namespace_grandchild");

  {
    constexpr uint8_t transaction_prefix[] = {0xa4, 0x31};
    constexpr uint8_t transaction_first[] = {0xa4, 0x31, 0x01};
    constexpr uint8_t transaction_second[] = {0xa4, 0x31, 0x02};
    constexpr uint8_t transaction_first_value[] = {0x41, 0x42, 0x43};
    constexpr uint8_t transaction_second_value[] = {0x51, 0x52, 0x53, 0x54};
    const bool seeded =
        fs_write(transaction_first, sizeof(transaction_first),
                 transaction_first_value, sizeof(transaction_first_value)) == 0 &&
        fs_write(transaction_second, sizeof(transaction_second),
                 transaction_second_value, sizeof(transaction_second_value)) == 0 &&
        fs_sync() == 0;
    const bool began = seeded &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    jcvm::fs::Tag prefix{};
    prefix.len = sizeof(transaction_prefix);
    std::memcpy(prefix.value, transaction_prefix, sizeof(transaction_prefix));
    rp2350_fs_test_fail_bulk_erase_after(1);
    const bool erase_failed_after_mutation = began &&
        !jcvm::FlashMemory_Handler::erasePersistentPrefix(prefix) &&
        fs_exists(transaction_first, sizeof(transaction_first)) == 0 &&
        fs_exists(transaction_second, sizeof(transaction_second)) != 0;
    const bool rolled_back =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool cold_restored = rolled_back && fs_init() == 0 &&
        fs_exists(transaction_first, sizeof(transaction_first)) != 0 &&
        fs_exists(transaction_second, sizeof(transaction_second)) != 0 &&
        fs_read(transaction_first, sizeof(transaction_first), actual,
                sizeof(transaction_first_value)) == 0 &&
        std::memcmp(actual, transaction_first_value,
                    sizeof(transaction_first_value)) == 0;
    expect(erase_failed_after_mutation && cold_restored,
           "transactional_prefix_delete_recovers_real_overlay_erase_failure");
  }

  {
    constexpr uint8_t rollback_sync_tag[] = {0xa4, 0x32, 0x01};
    constexpr uint8_t rollback_original[] = {0x61, 0x62, 0x63};
    constexpr uint8_t rollback_tentative[] = {0x71, 0x72, 0x73};
    const bool seeded =
        fs_write(rollback_sync_tag, sizeof(rollback_sync_tag),
                 rollback_original, sizeof(rollback_original)) == 0 &&
        fs_sync() == 0;
    if (seeded) committed_flash = fake_flash;
    jcvm::fs::Tag tag{};
    tag.len = sizeof(rollback_sync_tag);
    std::memcpy(tag.value, rollback_sync_tag, sizeof(rollback_sync_tag));
    const bool began = seeded &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    const bool wrote = began &&
        jcvm::FlashMemory_Handler::writePersistentRecord(
            tag, rollback_tentative, sizeof(rollback_tentative));
    rp2350_fs_test_fail_next_sync();
    const bool failed_rollback_sync = wrote &&
        !jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
        jcvm::FlashMemory_Handler::persistentWriteTransactionActive() &&
        jcvm::FlashMemory_Handler::persistentWriteRecoveryRequired();
    bool quarantined_read_blocked = false;
    try {
      (void)jcvm::FlashMemory_Handler::readPersistentRecord(tag);
    } catch (...) {
      quarantined_read_blocked = true;
    }

    // Recreate volatile process state and flash contents as on a cold reset.
    fake_flash = committed_flash;
    rp2350_fs_test_reset_persistent_transaction_state();
    const bool restarted = fs_init() == 0 &&
        !jcvm::FlashMemory_Handler::persistentWriteTransactionActive() &&
        !jcvm::FlashMemory_Handler::persistentWriteRecoveryRequired();
    uint8_t recovered[sizeof(rollback_original)]{};
    const bool restored_old_state = restarted &&
        fs_read(rollback_sync_tag, sizeof(rollback_sync_tag), recovered,
                sizeof(recovered)) == 0 &&
        std::memcmp(recovered, rollback_original, sizeof(recovered)) == 0;
    const bool usable_after_restart = restored_old_state &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction() &&
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    expect(seeded && wrote && failed_rollback_sync &&
               quarantined_read_blocked && restored_old_state &&
               usable_after_restart,
           "failed_rollback_sync_quarantines_until_reset_restores_committed_state");
  }

  constexpr uint8_t orphan_prefix[] = {0x02, 0x19};
  constexpr uint8_t orphan_parent[] = {0x02, 0x19, 0x09};
  constexpr uint8_t orphan_child[] = {0x02, 0x19, 0x09, 0x11, 0x11};
  expect(fs_write(orphan_parent, sizeof(orphan_parent), first_value,
                  sizeof(first_value)) == 0 &&
             fs_write(orphan_child, sizeof(orphan_child), second_value,
                      sizeof(second_value)) == 0 &&
             fs_erase(orphan_parent, sizeof(orphan_parent)) == 0,
         "create_orphaned_overlay_child");
  expect(fs_erase_prefix(orphan_prefix, sizeof(orphan_prefix)) == 0 &&
             fs_write(orphan_parent, sizeof(orphan_parent), first_value,
                      sizeof(first_value)) == 0 &&
             fs_exists(orphan_child, sizeof(orphan_child)) == 0,
         "prefix_cleanup_removes_child_of_deleted_parent");

  constexpr uint8_t class_one_tag[] = {0x03, 0x00, 0x38, 0x01, 0xff};
  constexpr uint8_t class_257_tag[] = {0x03, 0x00, 0x38, 0x01, 0x01, 0xff};
  constexpr uint8_t class_one_value[] = {0x11};
  constexpr uint8_t class_257_value[] = {0x22};
  uint8_t class_value = 0;
  expect(fs_write(class_one_tag, sizeof(class_one_tag), class_one_value,
                  sizeof(class_one_value)) == 0 &&
             fs_write(class_257_tag, sizeof(class_257_tag), class_257_value,
                      sizeof(class_257_value)) == 0,
         "write_legacy_and_16_bit_class_roots");
  expect(fs_read(class_one_tag, sizeof(class_one_tag), &class_value,
                 sizeof(class_value)) == 0 &&
             class_value == class_one_value[0] &&
             fs_read(class_257_tag, sizeof(class_257_tag), &class_value,
                     sizeof(class_value)) == 0 &&
             class_value == class_257_value[0],
         "16_bit_class_root_does_not_alias_legacy_class");
  constexpr uint8_t class_257_array_element[] = {
      0x03, 0x00, 0x38, 0x01, 0x01, 0x07, 0x00, 0x00};
  expect(fs_write(class_257_array_element,
                  sizeof(class_257_array_element), second_value,
                  sizeof(second_value)) == 0 &&
             fs_read(class_257_array_element,
                     sizeof(class_257_array_element), actual,
                     sizeof(actual)) == 0 &&
             actual[0] == second_value[0],
         "compress_16_bit_class_array_element_tag");

  // New V5 parent-prefix keys store arbitrary suffix bytes. The child tag is
  // much longer than its existing parent and is not eligible for equal-pair
  // compression, so this checks the production fs_write path and its byte
  // savings rather than only the test-only legacy encoder.
  std::array<uint8_t, 20> prefix_parent{};
  prefix_parent[0] = 0x02;
  prefix_parent[1] = 0xe7;
  prefix_parent[2] = 0xa1;
  for (size_t index = 3; index < prefix_parent.size(); ++index) {
    prefix_parent[index] = static_cast<uint8_t>(0x31u + index * 7u);
  }
  std::array<uint8_t, 35> prefix_child{};
  std::copy(prefix_parent.begin(), prefix_parent.end(), prefix_child.begin());
  for (size_t index = prefix_parent.size(); index < prefix_child.size();
       ++index) {
    prefix_child[index] = static_cast<uint8_t>(0x91u + index * 11u);
  }
  constexpr uint8_t prefix_parent_value[] = {0x51};
  constexpr uint8_t prefix_child_value[] = {0xa1, 0xb2, 0xc3, 0xd4};
  std::array<uint8_t, sizeof(prefix_child_value)> prefix_child_actual{};
  const uint32_t prefix_bytes_before = rp2350_fs_test_overlay_bytes_used();
  const bool prefix_parent_written =
      fs_write(prefix_parent.data(), prefix_parent.size(),
               prefix_parent_value, sizeof(prefix_parent_value)) ==
      FS_STATUS_OK;
  const uint32_t prefix_bytes_after_parent =
      rp2350_fs_test_overlay_bytes_used();
  const bool prefix_child_written =
      fs_write(prefix_child.data(), prefix_child.size(), prefix_child_value,
               sizeof(prefix_child_value)) == FS_STATUS_OK;
  const uint32_t prefix_child_bytes = rp2350_fs_test_overlay_bytes_used() -
                                      prefix_bytes_after_parent;
  const uint8_t prefix_child_read_status =
      fs_read(prefix_child.data(), prefix_child.size(),
              prefix_child_actual.data(), prefix_child_actual.size());
  std::printf("TEST:INFO arbitrary_prefix written=%u parent_bytes=%lu "
              "child_delta=%lu expected_delta=%lu read_status=%u "
              "value_match=%u\n",
              static_cast<unsigned>(prefix_child_written),
              static_cast<unsigned long>(prefix_bytes_after_parent -
                                         prefix_bytes_before),
              static_cast<unsigned long>(prefix_child_bytes),
              static_cast<unsigned long>(1u + 3u +
                  (prefix_child.size() - prefix_parent.size()) +
                  sizeof(prefix_child_value)),
              static_cast<unsigned>(prefix_child_read_status),
              static_cast<unsigned>(std::memcmp(
                  prefix_child_actual.data(), prefix_child_value,
                  sizeof(prefix_child_value)) == 0));
  expect(prefix_parent_written && prefix_child_written &&
             prefix_bytes_after_parent >= prefix_bytes_before &&
             prefix_child_bytes ==
                 1u + 3u + (prefix_child.size() - prefix_parent.size()) +
                     sizeof(prefix_child_value) &&
             prefix_child_read_status == FS_STATUS_OK &&
             std::memcmp(prefix_child_actual.data(), prefix_child_value,
                         sizeof(prefix_child_value)) == 0,
         "arbitrary_prefix_encoding_saves_tag_bytes_and_reads_child");
  expect(fs_sync() == FS_STATUS_OK &&
             fs_erase(prefix_parent.data(), prefix_parent.size()) ==
                 FS_STATUS_OK &&
             rp2350_fs_test_compact() == FS_STATUS_OK &&
             fs_read(prefix_child.data(), prefix_child.size(),
                     prefix_child_actual.data(),
                     prefix_child_actual.size()) == FS_STATUS_OK &&
             std::memcmp(prefix_child_actual.data(), prefix_child_value,
                         sizeof(prefix_child_value)) == 0 &&
             fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
             fs_read(prefix_child.data(), prefix_child.size(),
                     prefix_child_actual.data(),
                     prefix_child_actual.size()) == FS_STATUS_OK &&
             std::memcmp(prefix_child_actual.data(), prefix_child_value,
                         sizeof(prefix_child_value)) == 0,
         "arbitrary_prefix_child_survives_parent_erase_compaction_and_restore");

  // Start the snapshot-slot fallback test from a known generation. Earlier
  // contract checks intentionally committed a snapshot to cover sync.
  fake_flash.fill(0xff);
  expect(fs_init() == 0, "reset_before_snapshot_fallback_test");
  constexpr uint8_t persistent_tag[] = {0xa1, 0x22, 0x33};
  constexpr uint8_t persisted_first[] = {0x11, 0x22, 0x33, 0x44};
  constexpr uint8_t persisted_second[] = {0xaa, 0xbb, 0xcc, 0xdd};
  uint8_t persisted_actual[sizeof(persisted_second)]{};
  expect(fs_write(persistent_tag, sizeof(persistent_tag), persisted_first,
                  sizeof(persisted_first)) == 0 &&
             fs_sync() == 0,
         "filesystem_snapshot_commits");
  expect(fs_write(persistent_tag, sizeof(persistent_tag), persisted_second,
                  sizeof(persisted_second)) == 0 &&
             fs_sync() == 0,
         "filesystem_second_snapshot_commits");
  expect(fs_init() == 0 &&
             fs_read(persistent_tag, sizeof(persistent_tag), persisted_actual,
                     sizeof(persisted_actual)) == 0 &&
             std::memcmp(persisted_actual, persisted_second,
                         sizeof(persisted_actual)) == 0,
         "filesystem_snapshot_restores_latest_generation");

  // Corrupt the newest slot and require recovery from the previous snapshot.
  fake_flash[flash_layout::filesystem_slot(1) + 4096 + 17] ^= 0x01;
  expect(fs_init() == 0 &&
             fs_read(persistent_tag, sizeof(persistent_tag), persisted_actual,
                     sizeof(persisted_actual)) == 0 &&
             std::memcmp(persisted_actual, persisted_first,
                         sizeof(persisted_actual)) == 0,
         "filesystem_snapshot_falls_back_after_crc_failure");

  constexpr uint8_t transaction_tag_a[] = {0xa2, 0x10, 0x01};
  constexpr uint8_t transaction_tag_b[] = {0xa2, 0x10, 0x02};
  constexpr uint8_t transaction_old_a[] = {0x10, 0x11};
  constexpr uint8_t transaction_old_b[] = {0x20, 0x21};
  constexpr uint8_t transaction_new_a[] = {0xa0, 0xa1};
  constexpr uint8_t transaction_new_b[] = {0xb0, 0xb1};
  expect(fs_init() == 0 &&
             fs_write(transaction_tag_a, sizeof(transaction_tag_a),
                      transaction_old_a, sizeof(transaction_old_a)) == 0 &&
             fs_write(transaction_tag_b, sizeof(transaction_tag_b),
                      transaction_old_b, sizeof(transaction_old_b)) == 0 &&
             fs_sync() == 0,
         "multi_record_baseline_commits");
  committed_flash = fake_flash;

  {
    constexpr uint8_t delete_prefix[] = {0xa4, 0x31};
    constexpr uint8_t delete_first[] = {0xa4, 0x31, 0x01};
    constexpr uint8_t delete_second[] = {0xa4, 0x31, 0x02};
    constexpr uint8_t delete_first_value[] = {0x41, 0x42, 0x43};
    constexpr uint8_t delete_second_value[] = {0x51, 0x52, 0x53, 0x54};
    const bool seeded =
        fs_write(delete_first, sizeof(delete_first), delete_first_value,
                 sizeof(delete_first_value)) == 0 &&
        fs_write(delete_second, sizeof(delete_second), delete_second_value,
                 sizeof(delete_second_value)) == 0 &&
        fs_sync() == 0;
    jcvm::fs::Tag prefix{};
    prefix.len = sizeof(delete_prefix);
    std::memcpy(prefix.value, delete_prefix, sizeof(delete_prefix));
    const bool began = seeded &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    rp2350_fs_test_fail_bulk_erase_after(1);
    const bool partial_erase_observed = began &&
        !jcvm::FlashMemory_Handler::erasePersistentPrefix(prefix) &&
        fs_exists(delete_first, sizeof(delete_first)) == 0 &&
        fs_exists(delete_second, sizeof(delete_second)) != 0;
    const bool rollback =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool restored = rollback && fs_init() == 0 &&
        fs_exists(delete_first, sizeof(delete_first)) != 0 &&
        fs_exists(delete_second, sizeof(delete_second)) != 0;
    uint8_t restored_first[sizeof(delete_first_value)]{};
    uint8_t restored_second[sizeof(delete_second_value)]{};
    const bool values_restored = restored &&
        fs_read(delete_first, sizeof(delete_first), restored_first,
                sizeof(restored_first)) == 0 &&
        fs_read(delete_second, sizeof(delete_second), restored_second,
                sizeof(restored_second)) == 0 &&
        std::memcmp(restored_first, delete_first_value,
                    sizeof(restored_first)) == 0 &&
        std::memcmp(restored_second, delete_second_value,
                    sizeof(restored_second)) == 0;
    expect(partial_erase_observed && values_restored,
           "transactional_prefix_delete_restores_real_overlay_after_partial_erase");
  }

  {
    constexpr uint8_t package_id = 56;
    constexpr uint8_t package_static[] = {0x02, package_id, 0x01};
    constexpr uint8_t package_applet[] = {0x03, 0x09, package_id, 0x02};
    constexpr uint8_t package_lifecycle[] = {0x02, 0xff, package_id, 0x01};
    constexpr uint8_t unrelated_static[] = {0x02, 57, 0x01};
    constexpr uint8_t package_static_value[] = {0x61, 0x62};
    constexpr uint8_t package_applet_value[] = {0x71, 0x72, 0x73};
    constexpr uint8_t package_lifecycle_value[] = {0x81};
    constexpr uint8_t unrelated_value[] = {0x91, 0x92};
    constexpr uint8_t package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x56};
    constexpr uint8_t applet_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x57};
    constexpr uint8_t instance_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x58};
    constexpr uint8_t cap_code[] = {0xc4, 0x01, 0x00};
    constexpr uint8_t applet_owner = 200;
    const bool package_seeded =
        cap_store_write_package(package_id, package_aid, sizeof(package_aid),
                                cap_code, sizeof(cap_code)) &&
        cap_store_mark_package_installed(
            package_id, applet_aid, sizeof(applet_aid), instance_aid,
            sizeof(instance_aid), 3, applet_owner) &&
        fs_write(package_static, sizeof(package_static), package_static_value,
                 sizeof(package_static_value)) == 0 &&
        fs_write(package_applet, sizeof(package_applet), package_applet_value,
                 sizeof(package_applet_value)) == 0 &&
        fs_write(package_lifecycle, sizeof(package_lifecycle),
                 package_lifecycle_value, sizeof(package_lifecycle_value)) == 0 &&
        fs_write(unrelated_static, sizeof(unrelated_static), unrelated_value,
                 sizeof(unrelated_value)) == 0 &&
        fs_sync() == 0;
    const bool package_began = package_seeded &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    rp2350_fs_test_fail_bulk_erase_after(1);
    const bool package_partial_failure = package_began &&
        !jcvm::FlashMemory_Handler::erasePersistentPackageState(package_id) &&
        (fs_exists(package_static, sizeof(package_static)) == 0 ||
         fs_exists(package_applet, sizeof(package_applet)) == 0 ||
         fs_exists(package_lifecycle, sizeof(package_lifecycle)) == 0);
    const bool package_rolled_back =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    uint8_t found_package_id = 0;
    uint8_t found_owner = 0;
    const bool package_restored = package_rolled_back && fs_init() == 0 &&
        fs_exists(package_static, sizeof(package_static)) != 0 &&
        fs_exists(package_applet, sizeof(package_applet)) != 0 &&
        fs_exists(package_lifecycle, sizeof(package_lifecycle)) != 0 &&
        fs_exists(unrelated_static, sizeof(unrelated_static)) != 0 &&
        cap_store_find_package(package_aid, sizeof(package_aid),
                               found_package_id) &&
        found_package_id == package_id &&
        cap_store_package_instance_count(package_id) == 1 &&
        cap_store_applet_owner(instance_aid, sizeof(instance_aid),
                               found_owner) &&
        found_owner == applet_owner;
    expect(package_partial_failure && package_restored,
           "transactional_package_delete_restores_static_applet_and_lifecycle_records");
    (void)cap_store_delete_package(package_id);
    (void)fs_erase_package_state(package_id);
    (void)fs_erase(unrelated_static, sizeof(unrelated_static));
    (void)fs_sync();
  }

  {
    constexpr uint8_t package_list_tag[] = {0x00};
    uint8_t package_bits[JCVM_MAX_PACKAGES / 8]{};
    expect(fs_write(package_list_tag, sizeof(package_list_tag), package_bits,
                    sizeof(package_bits)) == 0 && fs_sync() == 0,
           "lifecycle_recovery_package_bitmap_baseline");

    constexpr uint8_t staged_package_id = 62;
    constexpr uint8_t staged_package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x62};
    constexpr uint8_t cap_code[] = {0xc4, 0x01, 0x00};
    const bool staged_cap = cap_store_write_package(
        staged_package_id, staged_package_aid, sizeof(staged_package_aid),
        cap_code, sizeof(cap_code));
    const bool staged_recovered = staged_cap &&
        gp::recover_dynamic_lifecycle_state();
    const bool staged_recovered_again = staged_recovered &&
        gp::recover_dynamic_lifecycle_state();
    uint8_t staged_package_found = 0;
    expect(staged_recovered_again &&
               !cap_store_find_package(staged_package_aid,
                                       sizeof(staged_package_aid),
                                       staged_package_found),
           "lifecycle_recovery_removes_unenabled_staged_load");

    constexpr uint8_t deleted_package_id = 63;
    constexpr uint8_t deleted_package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x63};
    constexpr uint8_t deleted_marker_tag[] = {0x02, 0xff, deleted_package_id,
                                              0x02};
    constexpr uint8_t deleted_marker = 2;
    const bool deleted_state_seeded =
        cap_store_write_package(deleted_package_id, deleted_package_aid,
                                sizeof(deleted_package_aid), cap_code,
                                sizeof(cap_code)) &&
        fs_write(deleted_marker_tag, sizeof(deleted_marker_tag),
                 &deleted_marker, sizeof(deleted_marker)) == 0 &&
        fs_sync() == 0;
    const bool deleted_state_recovered = deleted_state_seeded &&
        gp::recover_dynamic_lifecycle_state();
    uint8_t deleted_package_found = 0;
    expect(deleted_state_recovered &&
               !cap_store_find_package(deleted_package_aid,
                                       sizeof(deleted_package_aid),
                                       deleted_package_found),
           "lifecycle_recovery_finishes_package_delete_after_filesystem_commit");

    constexpr uint8_t enabled_orphan_id = 60;
    constexpr uint8_t enabled_orphan_static[] = {0x02, enabled_orphan_id, 0x01};
    constexpr uint8_t enabled_orphan_value[] = {0x61, 0x62};
    const bool orphan_seeded =
        fs_write(enabled_orphan_static, sizeof(enabled_orphan_static),
                 enabled_orphan_value, sizeof(enabled_orphan_value)) == 0 &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    if (orphan_seeded) {
      jcvm::FlashMemory_Handler::enablePackage(enabled_orphan_id);
    }
    const bool orphan_enabled = orphan_seeded &&
        jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    const bool orphan_recovered = orphan_enabled &&
        gp::recover_dynamic_lifecycle_state();
    const bool orphan_recovered_again = orphan_recovered &&
        gp::recover_dynamic_lifecycle_state();
    const uint8_t *recovered_package_bits =
        jcvm::FlashMemory_Handler::getPackagesArray();
    expect(orphan_recovered_again &&
               (recovered_package_bits[enabled_orphan_id / 8] &
                (1u << (enabled_orphan_id % 8))) == 0 &&
               fs_exists(enabled_orphan_static, sizeof(enabled_orphan_static)) == 0,
           "lifecycle_recovery_disables_package_without_cap_and_clears_state");

    constexpr uint8_t instance_package_id = 61;
    constexpr uint8_t instance_package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x61};
    constexpr uint8_t instance_applet_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x62};
    constexpr uint8_t instance_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x63};
    constexpr uint8_t owner_id = 180;
    const auto stale_instance_root =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(
            owner_id, instance_package_id, 4);
    bool instance_root_cleared =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    if (instance_root_cleared) {
      instance_root_cleared =
          jcvm::FlashMemory_Handler::erasePersistentTag(stale_instance_root) &&
          jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    }
    bool instance_seeded = cap_store_write_package(
        instance_package_id, instance_package_aid, sizeof(instance_package_aid),
        cap_code, sizeof(cap_code));
    instance_seeded = instance_root_cleared && instance_seeded &&
        cap_store_mark_package_installed(
        instance_package_id, instance_applet_aid, sizeof(instance_applet_aid),
        instance_aid, sizeof(instance_aid), 4, owner_id);
    if (instance_seeded &&
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction()) {
      jcvm::FlashMemory_Handler::enablePackage(instance_package_id);
      instance_seeded =
          jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    } else {
      instance_seeded = false;
    }
    const bool instance_recovered = instance_seeded &&
        gp::recover_dynamic_lifecycle_state();
    const bool instance_recovered_again = instance_recovered &&
        gp::recover_dynamic_lifecycle_state();
    uint32_t retained_cap_length = 0;
    const bool retained_cap =
        cap_store_data_for_package(instance_package_id,
                                   &retained_cap_length) != nullptr;
    const bool instance_registry_cleared =
        cap_store_package_instance_count(instance_package_id) == 0;
    expect(instance_recovered_again && instance_registry_cleared &&
               retained_cap && retained_cap_length == sizeof(cap_code),
           "lifecycle_recovery_removes_registry_entry_with_missing_applet_root");
    (void)cap_store_delete_package(instance_package_id);
    if (jcvm::FlashMemory_Handler::beginPersistentWriteTransaction()) {
      (void)jcvm::FlashMemory_Handler::erasePersistentPackageState(
          instance_package_id);
      jcvm::FlashMemory_Handler::disablePackage(instance_package_id);
      (void)jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    }
    (void)fs_sync();
  }

  {
    constexpr uint8_t cap_code[] = {0xc4, 0x01, 0x00};
    constexpr uint8_t package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x59};
    constexpr uint8_t applet_aid[] = {0xa0, 0x00, 0x00, 0x01, 0x59};
    constexpr uint8_t first_instance_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x59};
    const bool package_seeded = cap_store_write_package(
        59, package_aid, sizeof(package_aid), cap_code, sizeof(cap_code));
    const auto empty_preflight = cap_store_preflight_package_instance(
        59, first_instance_aid, sizeof(first_instance_aid));
    const bool first_instance_stored = package_seeded &&
        empty_preflight == CapInstancePreflightResult::available &&
        cap_store_mark_package_installed(
            59, applet_aid, sizeof(applet_aid), first_instance_aid,
            sizeof(first_instance_aid), 1, 0);
    const auto duplicate_same_package = cap_store_preflight_package_instance(
        59, first_instance_aid, sizeof(first_instance_aid));
    constexpr uint8_t second_instance_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x5a};
    const auto distinct_preflight = cap_store_preflight_package_instance(
        59, second_instance_aid, sizeof(second_instance_aid));
    expect(first_instance_stored &&
               duplicate_same_package ==
                   CapInstancePreflightResult::duplicate_instance_aid &&
               cap_store_package_instance_count(59) == 1 &&
               distinct_preflight == CapInstancePreflightResult::available,
           "install_preflight_rejects_same_package_duplicate_without_mutation");

    constexpr uint8_t other_package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x58};
    const bool other_package_seeded = cap_store_write_package(
        58, other_package_aid, sizeof(other_package_aid), cap_code,
        sizeof(cap_code));
    const auto duplicate_other_package = cap_store_preflight_package_instance(
        58, first_instance_aid, sizeof(first_instance_aid));
    expect(other_package_seeded &&
               duplicate_other_package ==
                   CapInstancePreflightResult::duplicate_instance_aid &&
               cap_store_package_instance_count(58) == 0,
           "install_preflight_rejects_cross_package_duplicate");

    constexpr uint8_t full_package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x57};
    constexpr uint8_t full_applet_aid[] = {0xa0, 0x00, 0x00, 0x01, 0x57};
    bool full_package_seeded = cap_store_write_package(
        57, full_package_aid, sizeof(full_package_aid), cap_code,
        sizeof(cap_code));
    for (uint8_t index = 0; index < CAP_STORE_MAX_PACKAGE_INSTANCES; ++index) {
      const uint8_t full_instance_aid[] = {0xa0, 0x00, 0x57, 0x00, index};
      full_package_seeded = full_package_seeded &&
          cap_store_mark_package_installed(
              57, full_applet_aid, sizeof(full_applet_aid),
              full_instance_aid, sizeof(full_instance_aid), index, 0);
    }
    constexpr uint8_t overflow_instance_aid[] = {0xa0, 0x00, 0x57, 0xff,
                                                 0xff};
    const auto full_preflight = cap_store_preflight_package_instance(
        57, overflow_instance_aid, sizeof(overflow_instance_aid));
    expect(full_package_seeded &&
               cap_store_package_instance_count(57) ==
                   CAP_STORE_MAX_PACKAGE_INSTANCES &&
               full_preflight == CapInstancePreflightResult::package_full,
           "install_preflight_rejects_full_instance_registry");
    (void)cap_store_delete_package(57);
    (void)cap_store_delete_package(58);
    (void)cap_store_delete_package(59);
  }

  persistent_transaction_active = true;
  expect(fs_write(transaction_tag_a, sizeof(transaction_tag_a),
                  transaction_new_a, sizeof(transaction_new_a)) == 0 &&
             fs_write(transaction_tag_b, sizeof(transaction_tag_b),
                      transaction_new_b, sizeof(transaction_new_b)) == 0,
         "multi_record_update_stages");
  reset_flash_failure();
  expect(fs_sync() == 0 && flash_mutation_count == 0,
         "active_transaction_sync_defers_snapshot");
  persistent_transaction_active = false;
  reset_flash_failure();
  expect(fs_sync() == 0, "multi_record_update_snapshot_commits");
  const size_t snapshot_mutations = flash_mutation_count;
  expect(snapshot_mutations > 0, "snapshot_has_flash_mutations");

  fake_flash = committed_flash;
  persistent_transaction_active = true;
  expect(fs_init() == 0 &&
             fs_write(transaction_tag_a, sizeof(transaction_tag_a),
                      transaction_new_a, sizeof(transaction_new_a)) == 0 &&
             fs_sync() == 0,
         "uncommitted_transaction_stages_without_flash_error");
  persistent_transaction_active = false;
  expect(fs_init() == 0,
         "reset_discards_volatile_transaction_state");
  uint8_t reset_actual_a[sizeof(transaction_old_a)]{};
  uint8_t reset_actual_b[sizeof(transaction_old_b)]{};
  expect(fs_read(transaction_tag_a, sizeof(transaction_tag_a), reset_actual_a,
                 sizeof(reset_actual_a)) == 0 &&
             fs_read(transaction_tag_b, sizeof(transaction_tag_b), reset_actual_b,
                     sizeof(reset_actual_b)) == 0 &&
             std::memcmp(reset_actual_a, transaction_old_a,
                         sizeof(reset_actual_a)) == 0 &&
             std::memcmp(reset_actual_b, transaction_old_b,
                         sizeof(reset_actual_b)) == 0,
         "reset_restores_last_committed_transaction_state");

  bool every_cut_recovers_atomically = snapshot_mutations > 0;
  uint32_t random_state = 0x2350cafe;
  for (size_t trial = 0; trial < 1000 && snapshot_mutations != 0; ++trial) {
    fake_flash = committed_flash;
    persistent_transaction_active = true;
    if (fs_init() != 0 ||
        fs_write(transaction_tag_a, sizeof(transaction_tag_a),
                 transaction_new_a, sizeof(transaction_new_a)) != 0 ||
        fs_write(transaction_tag_b, sizeof(transaction_tag_b),
                 transaction_new_b, sizeof(transaction_new_b)) != 0) {
      every_cut_recovers_atomically = false;
      persistent_transaction_active = false;
      break;
    }
    reset_flash_failure();
    if (fs_sync() != 0 || flash_mutation_count != 0) {
      every_cut_recovers_atomically = false;
      persistent_transaction_active = false;
      break;
    }
    persistent_transaction_active = false;
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    reset_flash_failure(random_state % snapshot_mutations);
    if (fs_sync() == 0) {
      every_cut_recovers_atomically = false;
      break;
    }
    reset_flash_failure();

    if (fs_init() != 0) {
      every_cut_recovers_atomically = false;
      break;
    }
    uint8_t actual_a[sizeof(transaction_new_a)]{};
    uint8_t actual_b[sizeof(transaction_new_b)]{};
    const bool old_state =
        fs_read(transaction_tag_a, sizeof(transaction_tag_a), actual_a,
                sizeof(actual_a)) == 0 &&
        fs_read(transaction_tag_b, sizeof(transaction_tag_b), actual_b,
                sizeof(actual_b)) == 0 &&
        std::memcmp(actual_a, transaction_old_a, sizeof(actual_a)) == 0 &&
        std::memcmp(actual_b, transaction_old_b, sizeof(actual_b)) == 0;
    const bool new_state =
        fs_read(transaction_tag_a, sizeof(transaction_tag_a), actual_a,
                sizeof(actual_a)) == 0 &&
        fs_read(transaction_tag_b, sizeof(transaction_tag_b), actual_b,
                sizeof(actual_b)) == 0 &&
        std::memcmp(actual_a, transaction_new_a, sizeof(actual_a)) == 0 &&
        std::memcmp(actual_b, transaction_new_b, sizeof(actual_b)) == 0;
    if (!old_state && !new_state) {
      every_cut_recovers_atomically = false;
      break;
    }
  }
  expect(every_cut_recovers_atomically,
         "filesystem_1000_randomized_multi_record_power_cuts_are_atomic");

  // The install journal uses this lifecycle metadata tag. Exercise removal as
  // a transaction: a cut while writing the cleared snapshot may recover
  // either the old journal record or its committed tombstone, never a torn
  // record that cannot be interpreted after boot.
  constexpr uint8_t install_journal_tag[] = {0x02, 0xff, 56, 3};
  constexpr uint8_t install_journal_value[] = {
      'I', 'N', 'S', '1', 56, 5, 5, 5, 0x01, 0x23};
  uint32_t cleared_snapshot_mutations = 0;
  bool journal_baseline_ready =
      fs_init() == 0 &&
      fs_write(install_journal_tag, sizeof(install_journal_tag),
               install_journal_value, sizeof(install_journal_value)) == 0 &&
      fs_sync() == 0;
  cross_store_empty_journal_flash = fake_flash;
  if (journal_baseline_ready) {
    persistent_transaction_active = true;
    journal_baseline_ready =
        fs_erase(install_journal_tag, sizeof(install_journal_tag)) == 0 &&
        fs_sync() == 0;
    persistent_transaction_active = false;
    reset_flash_failure();
    journal_baseline_ready = journal_baseline_ready && fs_sync() == 0;
    cleared_snapshot_mutations =
        static_cast<uint32_t>(flash_mutation_count);
  }
  expect(journal_baseline_ready && cleared_snapshot_mutations > 0,
         "install_journal_clear_transaction_commits_tombstone");

  bool journal_clear_recovers_atomically =
      journal_baseline_ready && cleared_snapshot_mutations > 0;
  uint32_t journal_cut_state = 0x1a57c0de;
  for (size_t trial = 0; trial < 1000 && journal_clear_recovers_atomically;
       ++trial) {
    fake_flash = cross_store_empty_journal_flash;
    if (fs_init() != 0) {
      journal_clear_recovers_atomically = false;
      break;
    }
    persistent_transaction_active = true;
    const bool erase_staged =
        fs_erase(install_journal_tag, sizeof(install_journal_tag)) == 0 &&
        fs_sync() == 0;
    persistent_transaction_active = false;
    if (!erase_staged) {
      journal_clear_recovers_atomically = false;
      break;
    }
    journal_cut_state ^= journal_cut_state << 13;
    journal_cut_state ^= journal_cut_state >> 17;
    journal_cut_state ^= journal_cut_state << 5;
    const size_t cut = journal_cut_state % cleared_snapshot_mutations;
    reset_flash_failure(cut);
    const bool interrupted = fs_sync() == FS_STATUS_IO_UNKNOWN;
    reset_flash_failure();
    if (!interrupted || fs_init() != 0) {
      journal_clear_recovers_atomically = false;
      break;
    }
    const bool journal_absent =
        fs_exists(install_journal_tag, sizeof(install_journal_tag)) == 0;
    uint8_t recovered_journal[sizeof(install_journal_value)]{};
    uint32_t existing_length = 0;
    const bool journal_intact =
        fs_length(install_journal_tag, sizeof(install_journal_tag),
                  &existing_length) == 0 &&
        existing_length == sizeof(install_journal_value) &&
        fs_read(install_journal_tag, sizeof(install_journal_tag),
                recovered_journal, sizeof(recovered_journal)) == 0 &&
        std::memcmp(recovered_journal, install_journal_value,
                    sizeof(install_journal_value)) == 0;
    if (!journal_absent && !journal_intact) {
      journal_clear_recovers_atomically = false;
    }
  }
  expect(journal_clear_recovers_atomically,
         "install_journal_clear_1000_power_cuts_recover_old_or_cleared_state");

  // Persistent reclamation removes confirmed-unreachable tags in bounded
  // batches, then the CCID APDU boundary commits one COW snapshot. A reset
  // during that commit must recover every candidate or none of them, preserve
  // the live applet root, and allow retry when the old snapshot wins.
  constexpr uint8_t reclamation_owner = 0x45;
  constexpr uint8_t reclamation_root_tag[] = {
      0x03, reclamation_owner, 0x01, 0x23, 0xff};
  constexpr std::array<uint8_t, 4> reclamation_other_owner_tag{
      0x03, 0x46, 0x30, 0x01};
  constexpr uint8_t reclamation_other_owner_value[] = {0xa1, 0xb2};
  constexpr std::array<uint8_t, 3> reclamation_root_value{
      0x51, 0x52, 0x53};
  std::array<std::array<uint8_t, 4>, 10> reclamation_candidates{};
  std::array<uint8_t, 2> reclamation_candidate_value{};
  std::vector<jcvm::PersistentInventory::Tag> reclamation_candidate_records;
  size_t reclamation_snapshot_mutations = 0;
  fake_flash.fill(0xff);
  reset_flash_failure();
  bool reclamation_baseline = fs_init() == 0;
  jcvm::jref_t reclamation_root_reference;
  try {
    jcvm::Context reclamation_context(reclamation_owner, reclamation_owner);
    const auto previous_context =
        jcvm::Context::setActiveContext(&reclamation_context);
    auto &reclamation_baseline_heap = reclamation_context.getHeap();
    reclamation_root_reference = reclamation_baseline_heap.addArray(
        1, jcvm::JAVA_ARRAY_T_REFERENCE, 0xffff);
    const auto child_reference = reclamation_baseline_heap.addArray(
        reclamation_root_value.size(), jcvm::JAVA_ARRAY_T_BYTE);
    auto root_array =
        reclamation_baseline_heap.getArray(reclamation_root_reference);
    root_array->setReferenceEntry(0, child_reference, reclamation_context,
                                  reclamation_owner);
    auto child_array = reclamation_baseline_heap.getArray(child_reference);
    for (uint16_t index = 0; index < reclamation_root_value.size(); ++index) {
      child_array->setByteEntry(index, reclamation_root_value[index]);
    }
    jcvm::fs::Tag root_tag{};
    root_tag.len = sizeof(reclamation_root_tag);
    std::memcpy(root_tag.value, reclamation_root_tag, root_tag.len);
    jcvm::FlashMemory_Handler::setPersistentField_Array(
        root_tag, *root_array, reclamation_baseline_heap);
    reclamation_baseline = reclamation_baseline &&
        persistent_root_graph_matches(reclamation_baseline_heap,
                                      reclamation_root_reference,
                                      reclamation_root_value);

    for (uint8_t index = 0; index < reclamation_candidates.size(); ++index) {
      reclamation_candidates[index] = {
          0x03, reclamation_owner, 0x30, index};
      auto orphan_reference = reclamation_baseline_heap.addArray(
          reclamation_candidate_value.size(), jcvm::JAVA_ARRAY_T_BYTE);
      auto orphan = reclamation_baseline_heap.getArray(orphan_reference);
      reclamation_candidate_value = {index, static_cast<uint8_t>(~index)};
      orphan->setByteEntry(0, reclamation_candidate_value[0]);
      orphan->setByteEntry(1, reclamation_candidate_value[1]);
      jcvm::fs::Tag orphan_tag{};
      orphan_tag.len = reclamation_candidates[index].size();
      std::memcpy(orphan_tag.value, reclamation_candidates[index].data(),
                  orphan_tag.len);
      jcvm::FlashMemory_Handler::setPersistentField_Array(
          orphan_tag, *orphan, reclamation_baseline_heap);
    }
    reclamation_baseline = reclamation_baseline &&
        fs_write(reclamation_other_owner_tag.data(),
                 reclamation_other_owner_tag.size(),
                 reclamation_other_owner_value,
                 sizeof(reclamation_other_owner_value)) == 0;
    reclamation_baseline = reclamation_baseline && fs_sync() == 0;
    committed_flash = fake_flash;
    reset_flash_failure();

    const std::vector<jcvm::jref_t> baseline_roots{
        reclamation_root_reference};
    ReclaimerRootContext baseline_root_context{&baseline_roots};
    const auto inventory_result =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            reclamation_owner, reclamation_baseline_heap, baseline_roots,
            collect_reclaimer_candidate, &reclamation_candidate_records);
    uint32_t staged_reclamation_erased = 0;
    bool reclamation_stage = reclamation_baseline &&
        inventory_result ==
            jcvm::PersistentInventory::VisitResult::Complete &&
        reclamation_candidate_records.size() >= reclamation_candidates.size() &&
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            reclamation_owner, reclamation_baseline_heap,
            enumerate_reclaimer_roots, &baseline_root_context,
            &staged_reclamation_erased) ==
            jcvm::PersistentInventory::VisitResult::Complete &&
        staged_reclamation_erased == reclamation_candidate_records.size() &&
        persistent_root_graph_matches(reclamation_baseline_heap,
                                      reclamation_root_reference,
                                      reclamation_root_value) &&
        fs_sync() == 0;
    reclamation_snapshot_mutations = flash_mutation_count;
    reclamation_baseline = reclamation_baseline && reclamation_stage;
    // Keep committed_flash at the pre-reclamation snapshot so every trial
    // replays the full owner inventory before cutting the reclaimer commit.
    jcvm::Context::setActiveContext(previous_context);
  } catch (...) {
    reclamation_baseline = false;
  }

  bool reclamation_cut_recovery = reclamation_baseline &&
                                  reclamation_snapshot_mutations > 0;
  uint32_t reclamation_cut_state = 0x45a23550;
  for (size_t trial = 0; trial < 1000 && reclamation_cut_recovery; ++trial) {
    fake_flash = committed_flash;
    reset_flash_failure();
    if (fs_init() != 0) {
      reclamation_cut_recovery = false;
      break;
    }
    jcvm::Heap attempt_heap(reclamation_owner);
    jcvm::jref_t attempt_root;
    bool restored_root = false;
    try {
      jcvm::fs::Tag root_tag{};
      root_tag.len = sizeof(reclamation_root_tag);
      std::memcpy(root_tag.value, reclamation_root_tag, root_tag.len);
      attempt_root = jcvm::FlashMemory_Handler::
          getPersistentField_ReferenceStrict(root_tag, attempt_heap);
      restored_root = !attempt_root.isNullPointer() && attempt_root.isArray() &&
          persistent_root_graph_matches(attempt_heap, attempt_root,
                                        reclamation_root_value);
    } catch (...) {
      restored_root = false;
    }
    const std::vector<jcvm::jref_t> attempt_roots{attempt_root};
    ReclaimerRootContext attempt_root_context{&attempt_roots};
    uint32_t attempt_erased = 0;
    const auto attempt_result = restored_root
        ? jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
              reclamation_owner, attempt_heap, enumerate_reclaimer_roots,
              &attempt_root_context, &attempt_erased)
        : jcvm::PersistentInventory::VisitResult::IncompleteReachability;
    if (attempt_result != jcvm::PersistentInventory::VisitResult::Complete ||
        attempt_erased != reclamation_candidate_records.size()) {
      reclamation_cut_recovery = false;
      break;
    }
    reclamation_cut_state ^= reclamation_cut_state << 13;
    reclamation_cut_state ^= reclamation_cut_state >> 17;
    reclamation_cut_state ^= reclamation_cut_state << 5;
    reset_flash_failure(reclamation_cut_state %
                        reclamation_snapshot_mutations);
    if (fs_sync() == 0) {
      reclamation_cut_recovery = false;
      break;
    }
    reset_flash_failure();
    if (fs_init() != 0) {
      reclamation_cut_recovery = false;
      break;
    }
    bool old_candidates = true;
    bool no_candidates = true;
    for (const auto &tag : reclamation_candidate_records) {
      const bool exists =
          fs_exists(tag.value, tag.len) != 0;
      old_candidates = old_candidates && exists;
      no_candidates = no_candidates && !exists;
    }
    bool live_root_intact = false;
    try {
      jcvm::Heap recovered_heap(reclamation_owner);
      jcvm::fs::Tag root_tag{};
      root_tag.len = sizeof(reclamation_root_tag);
      std::memcpy(root_tag.value, reclamation_root_tag, root_tag.len);
      const auto recovered_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root_tag, recovered_heap);
      live_root_intact = persistent_root_graph_matches(
          recovered_heap, recovered_reference, reclamation_root_value);
    } catch (...) {
      live_root_intact = false;
    }
    uint8_t recovered_other_owner[sizeof(reclamation_other_owner_value)]{};
    const bool other_owner_intact =
        fs_read(reclamation_other_owner_tag.data(),
                reclamation_other_owner_tag.size(), recovered_other_owner,
                sizeof(recovered_other_owner)) == 0 &&
        std::memcmp(recovered_other_owner, reclamation_other_owner_value,
                    sizeof(recovered_other_owner)) == 0;
    if ((!old_candidates && !no_candidates) || !live_root_intact ||
        !other_owner_intact) {
      reclamation_cut_recovery = false;
      break;
    }

    // If reboot selected the old snapshot, retry the same bounded deletion
    // work and confirm that the next committed snapshot frees every candidate.
    if (old_candidates) {
      jcvm::Heap retry_heap(reclamation_owner);
      jcvm::jref_t retry_root;
      bool retry_root_restored = false;
      try {
        jcvm::fs::Tag root_tag{};
        root_tag.len = sizeof(reclamation_root_tag);
        std::memcpy(root_tag.value, reclamation_root_tag, root_tag.len);
        retry_root = jcvm::FlashMemory_Handler::
            getPersistentField_ReferenceStrict(root_tag, retry_heap);
        retry_root_restored = !retry_root.isNullPointer() &&
            retry_root.isArray() &&
            persistent_root_graph_matches(retry_heap, retry_root,
                                          reclamation_root_value);
      } catch (...) {
        retry_root_restored = false;
      }
      const std::vector<jcvm::jref_t> retry_roots{retry_root};
      ReclaimerRootContext retry_root_context{&retry_roots};
      uint32_t retry_erased = 0;
      const auto retry_result = retry_root_restored
          ? jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
                reclamation_owner, retry_heap, enumerate_reclaimer_roots,
                &retry_root_context, &retry_erased)
          : jcvm::PersistentInventory::VisitResult::IncompleteReachability;
      bool retry_staged =
          retry_result == jcvm::PersistentInventory::VisitResult::Complete &&
          retry_erased == reclamation_candidate_records.size() &&
          fs_sync() == 0 &&
          fs_init() == 0;
      if (!retry_staged) {
        reclamation_cut_recovery = false;
        break;
      }
      for (const auto &tag : reclamation_candidate_records) {
        reclamation_cut_recovery = reclamation_cut_recovery &&
            fs_exists(tag.value, tag.len) == 0;
      }
      uint8_t retry_other_owner_bytes[sizeof(reclamation_other_owner_value)]{};
      uint32_t retry_root_length = 0;
      bool retry_root_intact = false;
      try {
        jcvm::Heap final_heap(reclamation_owner);
        jcvm::fs::Tag root_tag{};
        root_tag.len = sizeof(reclamation_root_tag);
        std::memcpy(root_tag.value, reclamation_root_tag, root_tag.len);
        const auto final_root =
            jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
                root_tag, final_heap);
        retry_root_intact = persistent_root_graph_matches(
            final_heap, final_root, reclamation_root_value);
      } catch (...) {
        retry_root_intact = false;
      }
      const bool retry_other_owner_intact =
          fs_length(reclamation_other_owner_tag.data(),
                    reclamation_other_owner_tag.size(), &retry_root_length) ==
              0 &&
          retry_root_length == sizeof(reclamation_other_owner_value) &&
          fs_read(reclamation_other_owner_tag.data(),
                  reclamation_other_owner_tag.size(), retry_other_owner_bytes,
                  sizeof(reclamation_other_owner_value)) == 0 &&
          std::memcmp(retry_other_owner_bytes, reclamation_other_owner_value,
                      sizeof(reclamation_other_owner_value)) == 0;
      reclamation_cut_recovery = reclamation_cut_recovery &&
          retry_root_intact && retry_other_owner_intact;
    }
  }
  expect(reclamation_cut_recovery,
         "persistent_reclamation_1000_power_cuts_recover_old_or_clean_state_and_retry");
  std::printf("TEST:METRIC persistent_reclamation_orphan_tags=%zu cuts=1000\n",
              reclamation_candidate_records.size());

  // H3: tombstones left by erased records must trigger compaction well
  // above the low watermark, and the counter must restart after compaction.
  fake_flash.fill(0xff);
  {
    bool tombstone_ok = fs_init() == FS_STATUS_OK &&
                        choupi_persistent_tombstones_since_compaction() == 0;
    constexpr uint16_t tombstone_records = 300;
    const uint8_t tombstone_value[] = {0x5a};
    const auto tombstone_tag = [](const uint16_t id) {
      return std::array<uint8_t, 6>{0x03, 0x4b, 0x38, 0x01,
                                    static_cast<uint8_t>(id >> 8),
                                    static_cast<uint8_t>(id)};
    };
    for (uint16_t id = 0; id < tombstone_records && tombstone_ok; ++id) {
      const auto tag = tombstone_tag(id);
      tombstone_ok = fs_write(tag.data(), tag.size(), tombstone_value,
                              sizeof(tombstone_value)) == FS_STATUS_OK;
    }
    const uint32_t files_with_records = rp2350_fs_test_overlay_file_count();
    for (uint16_t id = 0; id < tombstone_records && tombstone_ok; ++id) {
      const auto tag = tombstone_tag(id);
      tombstone_ok = fs_erase(tag.data(), tag.size()) == FS_STATUS_OK;
    }
    const uint32_t tombstones = choupi_persistent_tombstones_since_compaction();
    const uint32_t available = choupi_available_persistent_memory_bytes();
    tombstone_ok = tombstone_ok && tombstones == tombstone_records &&
                   rp2350_fs_test_overlay_file_count() == files_with_records &&
                   available > pico2_jcvm::kObjectDeletionLowWatermarkBytes &&
                   pico2_jcvm::should_compact_after_object_deletion(
                       available, 8, tombstones) &&
                   !pico2_jcvm::should_compact_after_object_deletion(
                       available, 8,
                       pico2_jcvm::kObjectDeletionTombstoneCompactThreshold -
                           1) &&
                   choupi_compact_persistent_memory() == FS_STATUS_OK &&
                   choupi_persistent_tombstones_since_compaction() == 0 &&
                   rp2350_fs_test_overlay_file_count() ==
                       files_with_records - tombstone_records &&
                   fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
                   rp2350_fs_test_live_record_count() ==
                       files_with_records - tombstone_records;
    std::printf("TEST:METRIC rp2350_overlay_tombstone_compaction "
                "tombstones=%u files_before=%u files_after=%u\n",
                static_cast<unsigned>(tombstones),
                static_cast<unsigned>(files_with_records),
                static_cast<unsigned>(rp2350_fs_test_overlay_file_count()));
    expect(tombstone_ok,
           "rp2350_overlay_tombstones_trigger_compaction_above_watermark");
  }

  // Simulate automatic low-watermark object deletion on the RP2350 overlay.
  // The producer never calls requestObjectDeletion(): it adds persistent
  // garbage once per APDU, while the runtime policy requests a bounded batch
  // only after the available arena falls below its low-watermark.
  fake_flash.fill(0xff);
  constexpr uint8_t overlay_churn_owner = 0x4a;
  constexpr uint8_t overlay_churn_root_tag[] = {
      0x03, overlay_churn_owner, 0x01, 0x23, 0xff};
  constexpr std::array<uint8_t, 3> overlay_churn_root_value{0x41, 0x42, 0x43};
  constexpr uint32_t overlay_churn_cycles = 1030;
  constexpr uint8_t overlay_churn_objects_per_cycle = 8;
  bool overlay_churn_ok = fs_init() == FS_STATUS_OK;
  size_t overlay_churn_expected_live_records = 0;
  size_t overlay_churn_final_index_slots = 0;
  size_t overlay_churn_min_available =
      std::numeric_limits<size_t>::max();
  size_t overlay_churn_peak_bytes = 0;
  size_t overlay_churn_peak_files = 0;
  uint32_t overlay_churn_auto_requests = 0;
  uint32_t overlay_churn_erased = 0;
  bool overlay_churn_deletion_pending = false;
  pico2_jcvm::AutomaticObjectDeletionThrottle overlay_churn_throttle;
  for (uint32_t cycle = 0; cycle < overlay_churn_cycles && overlay_churn_ok;
       ++cycle) {
    size_t bytes_before_reclaim = 0;
    try {
      overlay_churn_throttle.on_apdu_boundary();
      jcvm::Context context(overlay_churn_owner, overlay_churn_owner);
      const auto previous_context = jcvm::Context::setActiveContext(&context);
      auto &heap = context.getHeap();
      jcvm::jref_t root_reference;
      if (cycle == 0) {
        root_reference = heap.addArray(overlay_churn_root_value.size(),
                                       jcvm::JAVA_ARRAY_T_BYTE);
        auto root = heap.getArray(root_reference);
        for (uint16_t index = 0; index < overlay_churn_root_value.size();
             ++index) {
          root->setByteEntry(index, overlay_churn_root_value[index]);
        }
        jcvm::fs::Tag root_tag{};
        root_tag.len = sizeof(overlay_churn_root_tag);
        std::memcpy(root_tag.value, overlay_churn_root_tag, root_tag.len);
        jcvm::FlashMemory_Handler::setPersistentField_Array(
            root_tag, *root, heap);
      } else {
        jcvm::fs::Tag root_tag{};
        root_tag.len = sizeof(overlay_churn_root_tag);
        std::memcpy(root_tag.value, overlay_churn_root_tag, root_tag.len);
        root_reference = jcvm::FlashMemory_Handler::
            getPersistentField_ReferenceStrict(root_tag, heap);
      }

      for (uint8_t index = 0; index < overlay_churn_objects_per_cycle;
           ++index) {
        const auto garbage_reference =
            heap.addArray(4, jcvm::JAVA_ARRAY_T_BYTE);
        auto garbage = heap.getArray(garbage_reference);
        garbage->setByteEntry(0, static_cast<uint8_t>(cycle));
        garbage->setByteEntry(1, index);
        garbage->setByteEntry(2, static_cast<uint8_t>(~cycle));
        garbage->setByteEntry(3, static_cast<uint8_t>(~index));
        const std::array<uint8_t, 6> garbage_tag{
            0x03, overlay_churn_owner, 0x30,
            static_cast<uint8_t>(cycle >> 8), static_cast<uint8_t>(cycle),
            index};
        jcvm::fs::Tag tag{};
        tag.len = garbage_tag.size();
        std::memcpy(tag.value, garbage_tag.data(), tag.len);
        jcvm::FlashMemory_Handler::setPersistentField_Array(tag, *garbage,
                                                            heap);
      }

      bytes_before_reclaim = rp2350_fs_test_overlay_bytes_used();
      const size_t files_before_reclaim =
          rp2350_fs_test_overlay_file_count();
      overlay_churn_peak_bytes =
          std::max(overlay_churn_peak_bytes, bytes_before_reclaim);
      overlay_churn_peak_files =
          std::max(overlay_churn_peak_files, files_before_reclaim);
      const uint32_t available_before_reclaim =
          choupi_available_persistent_memory_bytes();
      if (pico2_jcvm::should_request_automatic_object_deletion(
              available_before_reclaim, overlay_churn_deletion_pending, true,
              false, overlay_churn_throttle.cooldown_apdus == 0)) {
        overlay_churn_deletion_pending = true;
        ++overlay_churn_auto_requests;
      }

      const std::vector<jcvm::jref_t> roots{root_reference};
      ReclaimerRootContext root_context{&roots};
      uint32_t erased_this_cycle = 0;
      if (overlay_churn_deletion_pending) {
        const auto batch =
            jcvm::PersistentReclaimer::reclaimUnreachableObjectDeletionBatch(
                overlay_churn_owner, overlay_churn_owner, heap,
                enumerate_reclaimer_roots, &root_context,
                jcvm::PersistentReclaimer::kObjectDeletionMaxRecordsPerApdu);
        erased_this_cycle = batch.erasedTotal();
        overlay_churn_erased += erased_this_cycle;
        if (pico2_jcvm::should_compact_after_object_deletion(
                available_before_reclaim, erased_this_cycle,
                choupi_persistent_tombstones_since_compaction()) &&
            choupi_compact_persistent_memory() != FS_STATUS_OK) {
          overlay_churn_ok = false;
        }
        if (batch.complete()) {
          overlay_churn_deletion_pending = false;
          overlay_churn_throttle.completed(erased_this_cycle);
        } else if (batch.erased_applet == 0 && batch.erased_package == 0 &&
                   batch.applet_result !=
                       jcvm::PersistentInventory::VisitResult::Stopped &&
                   batch.package_result !=
                       jcvm::PersistentInventory::VisitResult::Stopped) {
          overlay_churn_ok = false;
          std::printf("TEST:INFO rp2350_overlay_object_deletion "
                      "stage=batch_failure cycle=%u applet=%u package=%u\n",
                      static_cast<unsigned>(cycle),
                      static_cast<unsigned>(batch.applet_result),
                      static_cast<unsigned>(batch.package_result));
        }
      }
      overlay_churn_ok = overlay_churn_ok && fs_sync() == FS_STATUS_OK;
      if (overlay_churn_ok) {
        auto root = heap.getArray(root_reference);
        overlay_churn_ok = root != nullptr &&
            root->size() == overlay_churn_root_value.size();
        for (uint16_t index = 0;
             index < overlay_churn_root_value.size() && overlay_churn_ok;
             ++index) {
          overlay_churn_ok =
              root->getByteEntry(index) == overlay_churn_root_value[index];
        }
      }
      jcvm::Context::setActiveContext(previous_context);
    } catch (...) {
      std::printf("TEST:INFO rp2350_overlay_object_deletion stage=exception cycle=%u\n",
                  static_cast<unsigned>(cycle));
      overlay_churn_ok = false;
    }
    if (overlay_churn_ok) {
      const size_t live_records = rp2350_fs_test_live_record_count();
      const size_t available = choupi_available_persistent_memory_bytes();
      overlay_churn_min_available =
          std::min(overlay_churn_min_available, available);
      overlay_churn_final_index_slots =
          rp2350_fs_test_overlay_file_count();
      overlay_churn_expected_live_records =
          1 + (static_cast<size_t>(cycle) + 1) *
                  overlay_churn_objects_per_cycle -
          overlay_churn_erased;
      overlay_churn_ok =
          live_records == overlay_churn_expected_live_records &&
          available != 0 &&
          bytes_before_reclaim <= (96u * 1024u * 9u) / 10u;
      if (!overlay_churn_ok) {
        std::printf("TEST:INFO rp2350_overlay_object_deletion "
                    "stage=watermark cycle=%u expected_live=%zu "
                    "actual_live=%zu index_slots=%zu available=%zu "
                    "bytes=%zu\n",
                    static_cast<unsigned>(cycle),
                    overlay_churn_expected_live_records, live_records,
                    overlay_churn_final_index_slots, available,
                    bytes_before_reclaim);
      }
    }
  }
  // With the per-pass budget above the per-cycle garbage, the automatic pass
  // catches up and completes during churn; garbage created afterwards stays
  // until the next low-watermark trigger. Drain that residue with one final
  // request so the end state (only the live root) is still verified.
  const uint32_t overlay_churn_erased_during_churn = overlay_churn_erased;
  overlay_churn_deletion_pending = true;
  uint32_t overlay_churn_drain_cycles = 0;
  for (; overlay_churn_deletion_pending && overlay_churn_ok &&
         overlay_churn_drain_cycles < 2048;
       ++overlay_churn_drain_cycles) {
    try {
      overlay_churn_throttle.on_apdu_boundary();
      jcvm::Context context(overlay_churn_owner, overlay_churn_owner);
      const auto previous_context = jcvm::Context::setActiveContext(&context);
      jcvm::fs::Tag root_tag{};
      root_tag.len = sizeof(overlay_churn_root_tag);
      std::memcpy(root_tag.value, overlay_churn_root_tag, root_tag.len);
      const auto root_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root_tag, context.getHeap());
      const std::vector<jcvm::jref_t> roots{root_reference};
      ReclaimerRootContext root_context{&roots};
      const auto batch =
          jcvm::PersistentReclaimer::reclaimUnreachableObjectDeletionBatch(
              overlay_churn_owner, overlay_churn_owner, context.getHeap(),
              enumerate_reclaimer_roots, &root_context,
              jcvm::PersistentReclaimer::kObjectDeletionMaxRecordsPerApdu);
      overlay_churn_erased += batch.erasedTotal();
      if (pico2_jcvm::should_compact_after_object_deletion(
              choupi_available_persistent_memory_bytes(),
              batch.erasedTotal(),
              choupi_persistent_tombstones_since_compaction())) {
        overlay_churn_ok =
            choupi_compact_persistent_memory() == FS_STATUS_OK;
      }
      if (batch.complete()) {
        overlay_churn_deletion_pending = false;
        overlay_churn_throttle.completed(batch.erasedTotal());
      } else if (batch.erasedTotal() == 0 &&
                 batch.applet_result !=
                     jcvm::PersistentInventory::VisitResult::Stopped &&
                 batch.package_result !=
                     jcvm::PersistentInventory::VisitResult::Stopped) {
        overlay_churn_ok = false;
      }
      overlay_churn_ok = overlay_churn_ok && fs_sync() == FS_STATUS_OK;
      jcvm::Context::setActiveContext(previous_context);
    } catch (...) {
      overlay_churn_ok = false;
    }
  }
  overlay_churn_expected_live_records = 1;
  overlay_churn_ok = overlay_churn_ok && overlay_churn_auto_requests != 0 &&
      overlay_churn_erased_during_churn != 0 &&
      !overlay_churn_deletion_pending &&
      fs_init() == FS_STATUS_OK &&
      rp2350_fs_test_live_record_count() ==
          overlay_churn_expected_live_records;
  expect(overlay_churn_ok,
         "rp2350_overlay_low_watermark_reclaims_without_applet_request");
  std::printf("TEST:METRIC rp2350_overlay_auto_reclaim cycles=%u "
              "objects_per_cycle=%u live_records=%zu erased=%u "
              "triggers=%u min_available_bytes=%zu peak_bytes=%zu "
              "peak_files=%zu\n",
              static_cast<unsigned>(overlay_churn_cycles),
              static_cast<unsigned>(overlay_churn_objects_per_cycle),
              overlay_churn_expected_live_records,
              static_cast<unsigned>(overlay_churn_erased),
              static_cast<unsigned>(overlay_churn_auto_requests),
              overlay_churn_min_available, overlay_churn_peak_bytes,
              overlay_churn_peak_files);
  std::printf("TEST:METRIC rp2350_overlay_auto_reclaim_drain cycles=%u "
              "erased_during_churn=%u\n",
              static_cast<unsigned>(overlay_churn_drain_cycles),
              static_cast<unsigned>(overlay_churn_erased_during_churn));
  std::printf("TEST:METRIC rp2350_overlay_object_deletion index_slots=%zu\n",
              overlay_churn_final_index_slots);

  // Exercise the overlay near a realistic mixed live occupancy instead of
  // keeping a single small root. The parent-encoded children model persistent
  // object fields; independent values and variable payload sizes keep records
  // interleaved in the arena. Repeated compaction checks integrity cost and a
  // steady-state capacity floor.
  fake_flash.fill(0xff);
  constexpr uint8_t mixed_live_owner = 0x4b;
  constexpr uint8_t mixed_live_parent_count = 20;
  constexpr uint8_t mixed_live_children_per_parent = 14;
  constexpr uint8_t mixed_live_independent_count = 20;
  constexpr uint32_t mixed_live_cycles = 12;
  constexpr uint8_t mixed_live_garbage_per_cycle = 12;
  struct MixedLiveRecord {
    std::vector<uint8_t> tag;
    std::vector<uint8_t> value;
  };
  std::vector<MixedLiveRecord> mixed_live_records;
  mixed_live_records.reserve(mixed_live_parent_count *
                             mixed_live_children_per_parent +
                             mixed_live_independent_count);
  bool mixed_live_ok = fs_init() == FS_STATUS_OK;
  for (uint8_t parent_id = 0;
       parent_id < mixed_live_parent_count && mixed_live_ok; ++parent_id) {
    const std::array<uint8_t, 5> parent_tag = {
        0x03, mixed_live_owner, parent_id, 0x42, 0x24};
    mixed_live_ok = fs_write(parent_tag.data(), parent_tag.size(), nullptr, 0) ==
                    FS_STATUS_OK;
    for (uint8_t child_index = 0;
         child_index < mixed_live_children_per_parent && mixed_live_ok;
         ++child_index) {
      const uint8_t suffix = static_cast<uint8_t>(
          parent_id * mixed_live_children_per_parent + child_index);
      std::array<uint8_t, 7> child_tag{};
      std::copy(parent_tag.begin(), parent_tag.end(), child_tag.begin());
      child_tag[5] = suffix;
      child_tag[6] = suffix;
      const size_t value_size = 180 + ((suffix * 73u + parent_id * 19u) % 101u);
      std::vector<uint8_t> value(value_size);
      for (size_t index = 0; index < value.size(); ++index) {
        value[index] = static_cast<uint8_t>(suffix * 31u + index * 17u);
      }
      const uint8_t child_status = rp2350_fs_test_write_parent_encoded(
          child_tag.data(), child_tag.size(), value.data(), value.size());
      if (child_status != FS_STATUS_OK) {
        std::printf("TEST:INFO mixed_live_child_write parent=%u child=%u "
                    "status=%u files=%u bytes=%lu\n",
                    static_cast<unsigned>(parent_id),
                    static_cast<unsigned>(child_index),
                    static_cast<unsigned>(child_status),
                    static_cast<unsigned>(rp2350_fs_test_overlay_file_count()),
                    static_cast<unsigned long>(rp2350_fs_test_overlay_bytes_used()));
      }
      mixed_live_ok = child_status == FS_STATUS_OK;
      if (mixed_live_ok) {
        MixedLiveRecord record;
        record.tag.assign(child_tag.begin(), child_tag.end());
        record.value = std::move(value);
        mixed_live_records.push_back(std::move(record));
      }
    }
  }
  for (uint8_t index = 0;
       index < mixed_live_independent_count && mixed_live_ok; ++index) {
    const std::array<uint8_t, 5> tag = {
        0xa1, 0x55, index, static_cast<uint8_t>(index ^ 0x5a), 0x01};
    std::vector<uint8_t> value(250 + (index * 11u) % 51u);
    for (size_t byte = 0; byte < value.size(); ++byte) {
      value[byte] = static_cast<uint8_t>(index * 23u + byte * 7u);
    }
    mixed_live_ok = fs_write(tag.data(), tag.size(), value.data(),
                             value.size()) == FS_STATUS_OK;
    if (mixed_live_ok) {
      MixedLiveRecord record;
      record.tag.assign(tag.begin(), tag.end());
      record.value = std::move(value);
      mixed_live_records.push_back(std::move(record));
    }
  }
  const size_t mixed_live_initial_slots = rp2350_fs_test_overlay_file_count();
  const size_t mixed_live_initial_bytes =
      rp2350_fs_test_overlay_bytes_used();
  const size_t mixed_live_initial_available =
      choupi_available_persistent_memory_bytes();
  mixed_live_ok = mixed_live_ok &&
      mixed_live_initial_slots == mixed_live_parent_count *
              (mixed_live_children_per_parent + 1) +
          mixed_live_independent_count &&
      mixed_live_initial_bytes >= 60 * 1024 &&
      mixed_live_initial_bytes <= 75 * 1024 &&
      mixed_live_initial_available > 8000 &&
      rp2350_fs_test_integrity_valid() == 1 && fs_sync() == FS_STATUS_OK;
  std::printf("TEST:INFO mixed_live_setup ok=%u records=%zu slots=%zu "
              "bytes=%zu available=%zu\n",
              static_cast<unsigned>(mixed_live_ok), mixed_live_records.size(),
              mixed_live_initial_slots, mixed_live_initial_bytes,
              mixed_live_initial_available);
  uint64_t integrity_check_total_us = 0;
  uint64_t integrity_check_max_us = 0;
  size_t mixed_live_min_available = mixed_live_initial_available;
  uint32_t mixed_live_compactions = 0;
  auto verify_mixed_live_records = [&]() {
    for (const auto &record : mixed_live_records) {
      std::vector<uint8_t> actual(record.value.size());
      if (fs_read(record.tag.data(), static_cast<uint8_t>(record.tag.size()),
                  actual.data(), actual.size()) != FS_STATUS_OK ||
          actual != record.value) {
        return false;
      }
    }
    return true;
  };
  for (uint32_t cycle = 0; cycle < mixed_live_cycles && mixed_live_ok;
       ++cycle) {
    for (uint8_t index = 0; index < mixed_live_garbage_per_cycle &&
                             mixed_live_ok;
         ++index) {
      const std::array<uint8_t, 6> tag = {
          0x03, mixed_live_owner, 0x70, static_cast<uint8_t>(cycle >> 8),
          static_cast<uint8_t>(cycle), index};
      std::vector<uint8_t> value(80 + ((cycle * 37u + index * 29u) % 221u));
      for (size_t byte = 0; byte < value.size(); ++byte) {
        value[byte] = static_cast<uint8_t>(cycle * 13u + index * 41u + byte);
      }
      mixed_live_ok = fs_write(tag.data(), tag.size(), value.data(),
                               value.size()) == FS_STATUS_OK &&
          fs_erase(tag.data(), tag.size()) == FS_STATUS_OK;
    }
    if (mixed_live_ok) {
      mixed_live_ok = rp2350_fs_test_compact() == FS_STATUS_OK;
      if (mixed_live_ok) ++mixed_live_compactions;
    }
    if (mixed_live_ok) {
      const auto check_start = std::chrono::steady_clock::now();
      const bool integrity_ok = rp2350_fs_test_integrity_valid() == 1;
      const auto check_end = std::chrono::steady_clock::now();
      const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          check_end - check_start).count();
      integrity_check_total_us += static_cast<uint64_t>(elapsed);
      integrity_check_max_us = std::max(
          integrity_check_max_us, static_cast<uint64_t>(elapsed));
      mixed_live_ok = integrity_ok &&
          rp2350_fs_test_overlay_file_count() == mixed_live_initial_slots &&
          choupi_available_persistent_memory_bytes() + 128 >=
              mixed_live_initial_available &&
          verify_mixed_live_records() && fs_sync() == FS_STATUS_OK;
      mixed_live_min_available = std::min(
          mixed_live_min_available,
          static_cast<size_t>(choupi_available_persistent_memory_bytes()));
    }
    if (mixed_live_ok && (cycle + 1) % 4 == 0) {
      mixed_live_ok = fs_init() == FS_STATUS_OK &&
          rp2350_fs_test_integrity_valid() == 1 &&
          rp2350_fs_test_overlay_file_count() == mixed_live_initial_slots &&
          verify_mixed_live_records();
    }
  }
  expect(mixed_live_ok && mixed_live_compactions >= 10 &&
             mixed_live_min_available > 8000,
         "rp2350_overlay_mixed_live_set_survives_12_compactions_and_reloads");
  std::printf("TEST:METRIC rp2350_overlay_mixed_live records=%zu slots=%zu "
              "used_bytes=%zu available_floor=%zu compactions=%u integrity_avg_us=%llu "
              "integrity_max_us=%llu\n",
              mixed_live_records.size(), mixed_live_initial_slots,
              mixed_live_initial_bytes,
              mixed_live_min_available,
              static_cast<unsigned>(mixed_live_compactions),
              static_cast<unsigned long long>(mixed_live_compactions == 0
                  ? 0
                  : integrity_check_total_us / mixed_live_compactions),
              static_cast<unsigned long long>(integrity_check_max_us));

  auto verify_mixed_snapshot = [&](const bool updated_first_record) {
    for (size_t index = 0; index < mixed_live_records.size(); ++index) {
      const auto &record = mixed_live_records[index];
      std::vector<uint8_t> actual(record.value.size());
      if (fs_read(record.tag.data(), static_cast<uint8_t>(record.tag.size()),
                  actual.data(), actual.size()) != FS_STATUS_OK) {
        return false;
      }
      if (index == 0 && updated_first_record) {
        auto expected = record.value;
        expected[0] ^= 0x5a;
        if (actual != expected) return false;
      } else if (actual != record.value) {
        return false;
      }
    }
    return true;
  };
  if (mixed_live_records.empty()) {
    expect(false, "rp2350_overlay_mixed_live_interrupted_snapshots_restore_old_or_new_state");
    return failures == 0 ? 0 : 1;
  }
  std::vector<uint8_t> changed_first_record = mixed_live_records.front().value;
  changed_first_record[0] ^= 0x5a;
  committed_flash = fake_flash;
  bool mixed_snapshot_cut_ok = fs_write(
      mixed_live_records.front().tag.data(),
      static_cast<uint8_t>(mixed_live_records.front().tag.size()),
      changed_first_record.data(), changed_first_record.size()) == FS_STATUS_OK;
  reset_flash_failure();
  mixed_snapshot_cut_ok = mixed_snapshot_cut_ok && fs_sync() == FS_STATUS_OK;
  const size_t mixed_snapshot_mutations = flash_mutation_count;
  mixed_snapshot_cut_ok = mixed_snapshot_cut_ok &&
      mixed_snapshot_mutations > 4;
  const std::array<size_t, 5> mixed_snapshot_cuts = {
      0, mixed_snapshot_mutations / 4, mixed_snapshot_mutations / 2,
      (mixed_snapshot_mutations * 3) / 4, mixed_snapshot_mutations - 1};
  for (const size_t cut : mixed_snapshot_cuts) {
    if (!mixed_snapshot_cut_ok) break;
    fake_flash = committed_flash;
    mixed_snapshot_cut_ok = fs_init() == FS_STATUS_OK &&
        fs_write(mixed_live_records.front().tag.data(),
                 static_cast<uint8_t>(mixed_live_records.front().tag.size()),
                 changed_first_record.data(), changed_first_record.size()) ==
            FS_STATUS_OK;
    reset_flash_failure(cut);
    const bool interrupted = fs_sync() == FS_STATUS_IO_UNKNOWN;
    reset_flash_failure();
    mixed_snapshot_cut_ok = mixed_snapshot_cut_ok && interrupted &&
        fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1 &&
        (verify_mixed_snapshot(false) || verify_mixed_snapshot(true));
  }
  fake_flash = committed_flash;
  mixed_snapshot_cut_ok = mixed_snapshot_cut_ok &&
      fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1 &&
      verify_mixed_snapshot(false);
  expect(mixed_snapshot_cut_ok,
         "rp2350_overlay_mixed_live_interrupted_snapshots_restore_old_or_new_state");
  std::printf("TEST:METRIC rp2350_overlay_mixed_live_snapshot_cut mutations=%zu "
              "cuts=%zu\n",
              mixed_snapshot_mutations, mixed_snapshot_cuts.size());

  // Combine realistic live storage occupancy with the real persistent
  // inventory/reclaimer and a long-lived heap. Each reference-array root
  // aliases byte-array children to durable body tags; unrelated arrays are
  // created and reclaimed over multiple cycles to exercise both flash and RAM
  // reclamation without resetting the Context.
  fake_flash.fill(0xff);
  constexpr uint8_t graph_owner = 0x4c;
  constexpr uint8_t graph_parent_count = 20;
  constexpr uint8_t graph_children_per_parent = 14;
  constexpr uint8_t graph_garbage_per_cycle = 8;
  constexpr uint32_t graph_reclaim_cycles = 12;
  struct HeapGraphChild {
    jcvm::jref_t reference;
    jcvm::fs::Tag field_tag;
    jcvm::fs::Tag body_tag;
    std::vector<uint8_t> value;
  };
  struct HeapGraphRoot {
    jcvm::jref_t reference;
    jcvm::fs::Tag tag;
    std::vector<HeapGraphChild> children;
  };
  std::vector<jcvm::jref_t> graph_roots;
  std::vector<HeapGraphRoot> graph_records;
  graph_roots.reserve(graph_parent_count);
  graph_records.reserve(graph_parent_count);
  bool heap_graph_ok = fs_init() == FS_STATUS_OK;
  size_t heap_graph_peak_handles = 0;
  size_t heap_graph_min_available = 0;
  size_t heap_graph_initial_bytes = 0;
  uint64_t heap_graph_integrity_us = 0;
  uint64_t heap_graph_integrity_max_us = 0;
  uint32_t heap_graph_compactions = 0;
  uint32_t heap_graph_total_erased = 0;
  size_t heap_graph_last_candidate_count = 0;
  try {
    jcvm::Context graph_context(graph_owner, graph_owner);
    const auto previous_context =
        jcvm::Context::setActiveContext(&graph_context);
    auto &heap = graph_context.getHeap();
    for (uint8_t parent_id = 0;
         parent_id < graph_parent_count && heap_graph_ok; ++parent_id) {
      HeapGraphRoot root_record;
      root_record.tag.len = 5;
      const std::array<uint8_t, 5> root_tag = {
          0x03, graph_owner, parent_id, 0x42, 0x24};
      std::copy(root_tag.begin(), root_tag.end(), root_record.tag.value);
      root_record.reference = heap.addArray(
          graph_children_per_parent, jcvm::JAVA_ARRAY_T_REFERENCE, 0xffff);
      auto root_array = heap.getArray(root_record.reference);
      root_record.children.reserve(graph_children_per_parent);
      for (uint8_t child_index = 0;
           child_index < graph_children_per_parent && heap_graph_ok;
           ++child_index) {
        const size_t child_id =
            static_cast<size_t>(parent_id) * graph_children_per_parent +
            child_index;
        const size_t value_size = 160 + ((child_id * 53u) % 81u);
        const auto child_reference = heap.addArray(
            static_cast<uint16_t>(value_size), jcvm::JAVA_ARRAY_T_BYTE);
        auto child_array = heap.getArray(child_reference);
        HeapGraphChild child_record{};
        child_record.reference = child_reference;
        child_record.field_tag = jcvm::FlashMemory_Handler::computeTag(
            root_record.tag, child_index);
        child_record.value.resize(value_size);
        for (size_t byte = 0; byte < value_size; ++byte) {
          const uint8_t value = static_cast<uint8_t>(
              child_id * 29u + byte * 13u + parent_id);
          child_record.value[byte] = value;
          child_array->setByteEntry(static_cast<uint16_t>(byte), value);
        }
        root_array->setReferenceEntry(child_index, child_reference,
                                      graph_context, graph_owner);
        root_record.children.push_back(std::move(child_record));
      }
      if (heap_graph_ok) {
        jcvm::FlashMemory_Handler::setPersistentField_Array(
            root_record.tag, *root_array, heap);
        for (auto &child : root_record.children) {
          const auto child_array = heap.getArray(child.reference);
          child.body_tag = child_array->getPersistentTag();
          if (!child_array->hasPersistentTag()) heap_graph_ok = false;
        }
        graph_roots.push_back(root_record.reference);
        graph_records.push_back(std::move(root_record));
      }
    }
    heap_graph_initial_bytes = rp2350_fs_test_overlay_bytes_used();
    heap_graph_min_available =
        choupi_available_persistent_memory_bytes();
    heap_graph_ok = heap_graph_ok &&
        graph_roots.size() == graph_parent_count &&
        heap_graph_initial_bytes >= 60 * 1024 &&
        heap_graph_initial_bytes <= 75 * 1024 &&
        fs_sync() == FS_STATUS_OK;
    auto verify_heap_graph = [&]() {
      for (const auto &root_record : graph_records) {
        const auto root_array = heap.getArray(root_record.reference);
        const bool root_exists =
            fs_exists(root_record.tag.value, root_record.tag.len) != 0;
        if (root_array == nullptr ||
            root_array->size() != graph_children_per_parent || !root_exists) {
          std::printf("TEST:INFO heap_graph_verify root tag_len=%u array=%u "
                      "size=%u exists=%u\n",
                      static_cast<unsigned>(root_record.tag.len),
                      root_array != nullptr,
                      root_array == nullptr ? 0u : root_array->size(),
                      root_exists);
          return false;
        }
        for (uint16_t child_index = 0;
             child_index < root_record.children.size(); ++child_index) {
          const auto &child = root_record.children[child_index];
          const auto actual_reference = root_array->getReferenceEntry(
              child_index);
          const auto actual_array = heap.getArray(actual_reference);
          const bool field_exists =
              fs_exists(child.field_tag.value, child.field_tag.len) != 0;
          const bool body_exists =
              fs_exists(child.body_tag.value, child.body_tag.len) != 0;
          if (actual_reference != child.reference || actual_array == nullptr ||
              actual_array->size() != child.value.size() || !field_exists ||
              !body_exists) {
            std::printf("TEST:INFO heap_graph_verify child=%u handle=%u "
                        "size=%u/%zu field=%u body=%u\n",
                        static_cast<unsigned>(child_index),
                        actual_reference == child.reference,
                        actual_array == nullptr ? 0u : actual_array->size(),
                        child.value.size(), field_exists, body_exists);
            return false;
          }
          for (uint16_t byte = 0; byte < child.value.size(); ++byte) {
            const uint8_t actual_value = static_cast<uint8_t>(
                actual_array->getByteEntry(byte));
            if (actual_value != child.value[byte]) {
              std::printf("TEST:INFO heap_graph_verify child=%u byte=%u "
                          "actual=%02X expected=%02X\n",
                          static_cast<unsigned>(child_index), byte,
                          static_cast<unsigned>(actual_value),
                          static_cast<unsigned>(child.value[byte]));
              return false;
            }
          }
        }
      }
      return true;
    };
    const size_t heap_graph_initial_slots =
        rp2350_fs_test_overlay_file_count();
    heap_graph_ok = heap_graph_ok && verify_heap_graph();
    std::vector<jcvm::PersistentInventory::Tag> graph_candidates;
    for (uint32_t cycle = 0; cycle < graph_reclaim_cycles && heap_graph_ok;
         ++cycle) {
      auto &reassigned_child = graph_records.front().children.front();
      const jcvm::fs::Tag superseded_body_tag = reassigned_child.body_tag;
      const size_t replacement_size = 180 + ((cycle * 37u) % 81u);
      const auto replacement_reference = heap.addArray(
          static_cast<uint16_t>(replacement_size), jcvm::JAVA_ARRAY_T_BYTE);
      auto replacement_array = heap.getArray(replacement_reference);
      reassigned_child.value.resize(replacement_size);
      for (size_t byte = 0; byte < replacement_size; ++byte) {
        const uint8_t value = static_cast<uint8_t>(
            0x80u + cycle * 19u + byte * 7u);
        reassigned_child.value[byte] = value;
        replacement_array->setByteEntry(static_cast<uint16_t>(byte), value);
      }
      auto persistent_root_array =
          heap.getArray(graph_records.front().reference);
      persistent_root_array->setReferenceEntry(
          0, replacement_reference, graph_context, graph_owner);
      reassigned_child.reference = replacement_reference;
      reassigned_child.body_tag = replacement_array->getPersistentTag();
      heap_graph_ok = replacement_array->hasPersistentTag() &&
          fs_exists(superseded_body_tag.value, superseded_body_tag.len) != 0;

      std::array<std::array<uint8_t, 6>, graph_garbage_per_cycle> garbage_tags{};
      for (uint8_t index = 0;
           index < graph_garbage_per_cycle && heap_graph_ok; ++index) {
        garbage_tags[index] = {
            0x03, graph_owner, 0x70, static_cast<uint8_t>(cycle >> 8),
            static_cast<uint8_t>(cycle), index};
        const size_t value_size = 100 + ((cycle * 31u + index * 43u) % 201u);
        const auto garbage_reference = heap.addArray(
            static_cast<uint16_t>(value_size), jcvm::JAVA_ARRAY_T_BYTE);
        auto garbage_array = heap.getArray(garbage_reference);
        for (uint16_t byte = 0; byte < value_size; ++byte) {
          garbage_array->setByteEntry(
              byte, static_cast<uint8_t>(cycle * 17u + index * 23u + byte));
        }
        jcvm::fs::Tag garbage_tag{};
        garbage_tag.len = garbage_tags[index].size();
        std::memcpy(garbage_tag.value, garbage_tags[index].data(),
                    garbage_tag.len);
        jcvm::FlashMemory_Handler::setPersistentField_Array(
            garbage_tag, *garbage_array, heap);
      }
      graph_candidates.clear();
      const auto inventory =
          jcvm::PersistentInventory::visitUnreachableAppletTags(
              graph_owner, heap, graph_roots, collect_reclaimer_candidate,
              &graph_candidates);
      uint32_t erased = 0;
      ReclaimerRootContext root_context{&graph_roots};
      const auto reclaimed = inventory ==
              jcvm::PersistentInventory::VisitResult::Complete
          ? jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
                graph_owner, heap, enumerate_reclaimer_roots, &root_context,
                &erased)
          : jcvm::PersistentInventory::VisitResult::InvalidArgument;
      heap_graph_last_candidate_count = graph_candidates.size();
      heap_graph_ok = inventory ==
              jcvm::PersistentInventory::VisitResult::Complete &&
          reclaimed == jcvm::PersistentInventory::VisitResult::Complete &&
          erased == graph_garbage_per_cycle + 1 &&
          rp2350_fs_test_compact() == FS_STATUS_OK;
      if (heap_graph_ok) ++heap_graph_compactions;
      if (heap_graph_ok) heap_graph_total_erased += erased;
      for (const auto &garbage_tag : garbage_tags) {
        heap_graph_ok = heap_graph_ok &&
            fs_exists(garbage_tag.data(), garbage_tag.size()) == 0;
      }
      heap_graph_ok = heap_graph_ok &&
          fs_exists(superseded_body_tag.value, superseded_body_tag.len) == 0;
      const auto reachability = heap.traceReachableFrom(graph_roots);
      const auto ram_sweep = heap.collectUnreachableMarked();
      heap_graph_peak_handles = std::max(
          heap_graph_peak_handles,
          static_cast<size_t>(ram_sweep.arrays_total));
      const auto integrity_start = std::chrono::steady_clock::now();
      const bool integrity_valid = rp2350_fs_test_integrity_valid() == 1;
      const auto integrity_end = std::chrono::steady_clock::now();
      const auto integrity_elapsed =
          std::chrono::duration_cast<std::chrono::microseconds>(
              integrity_end - integrity_start).count();
      heap_graph_integrity_us += static_cast<uint64_t>(integrity_elapsed);
      heap_graph_integrity_max_us = std::max(
          heap_graph_integrity_max_us,
          static_cast<uint64_t>(integrity_elapsed));
      const size_t live_heap_arrays =
          graph_parent_count * (graph_children_per_parent + 1);
      const size_t graph_slots = rp2350_fs_test_overlay_file_count();
      const size_t graph_available =
          choupi_available_persistent_memory_bytes();
      const bool graph_values_ok = verify_heap_graph();
      heap_graph_ok = heap_graph_ok && reachability.complete &&
          reachability.arrays_reachable ==
              live_heap_arrays &&
          ram_sweep.complete &&
          ram_sweep.arrays_reclaimed == graph_garbage_per_cycle + 1 &&
          ram_sweep.arrays_reachable == live_heap_arrays &&
          heap_graph_peak_handles <= live_heap_arrays +
                                         graph_garbage_per_cycle + 1 &&
          integrity_valid &&
          graph_slots == heap_graph_initial_slots &&
          graph_available + 512 >= heap_graph_min_available &&
          graph_values_ok && fs_sync() == FS_STATUS_OK;
      heap_graph_min_available = std::min(
          heap_graph_min_available,
          static_cast<size_t>(choupi_available_persistent_memory_bytes()));
      if (heap_graph_ok && (cycle + 1) % 4 == 0) {
        heap_graph_ok = fs_init() == FS_STATUS_OK &&
            rp2350_fs_test_integrity_valid() == 1 && verify_heap_graph();
      }
      if (!heap_graph_ok) {
        std::printf("TEST:INFO rp2350_overlay_heap_graph cycle=%u inventory=%u "
                    "candidates=%zu reclaimed=%u erased=%u arrays=%u/%u "
                    "swept=%u peak_handles=%zu integrity=%u slots=%zu "
                    "available=%zu graph_values=%u\n",
                    static_cast<unsigned>(cycle),
                    static_cast<unsigned>(inventory), graph_candidates.size(),
                    static_cast<unsigned>(reclaimed),
                    static_cast<unsigned>(erased), reachability.arrays_reachable,
                    reachability.arrays_total, ram_sweep.arrays_reclaimed,
                    heap_graph_peak_handles, integrity_valid, graph_slots,
                    graph_available, graph_values_ok);
      }
    }
    expect(heap_graph_ok && heap_graph_compactions == graph_reclaim_cycles &&
               heap_graph_total_erased ==
                   graph_reclaim_cycles * (graph_garbage_per_cycle + 1) &&
               heap_graph_peak_handles <=
                   graph_parent_count * (graph_children_per_parent + 1) +
                       graph_garbage_per_cycle + 1 &&
               heap_graph_min_available > 8000,
           "rp2350_overlay_reclaimer_preserves_high_occupancy_heap_graph_and_sweeps_garbage");
    std::printf("TEST:METRIC rp2350_overlay_heap_graph roots=%u children=%u "
                "slots=%zu bytes=%zu available_floor=%zu compactions=%u "
                "erased=%u last_candidates=%zu ram_handle_peak=%zu integrity_avg_us=%llu "
                "integrity_max_us=%llu\n",
                static_cast<unsigned>(graph_parent_count),
                static_cast<unsigned>(graph_parent_count *
                                      graph_children_per_parent),
                heap_graph_initial_slots, heap_graph_initial_bytes,
                heap_graph_min_available,
                static_cast<unsigned>(heap_graph_compactions),
                static_cast<unsigned>(heap_graph_total_erased),
                heap_graph_last_candidate_count,
                heap_graph_peak_handles,
                static_cast<unsigned long long>(heap_graph_integrity_us /
                                                graph_reclaim_cycles),
                static_cast<unsigned long long>(heap_graph_integrity_max_us));
    jcvm::Context::setActiveContext(previous_context);
  } catch (...) {
    jcvm::Context::setActiveContext(nullptr);
    heap_graph_ok = false;
    expect(false,
           "rp2350_overlay_reclaimer_preserves_high_occupancy_heap_graph_and_sweeps_garbage");
  }

  // One enumerable tag whose index slot lies beyond an empty probe bucket
  // triggers T1 index repair, after which the whole batch is reclaimable.
  fake_flash.fill(0xff);
  constexpr uint8_t corruptible_owner = 0x47;
  std::array<std::array<uint8_t, 4>, 20> mixed_reclamation_tags{};
  constexpr uint8_t mixed_reclamation_value[] = {0x61, 0x62};
  bool mixed_reclamation_seeded = fs_init() == FS_STATUS_OK;
  for (uint8_t index = 0; index < mixed_reclamation_tags.size(); ++index) {
    mixed_reclamation_tags[index] = {
        0x03, corruptible_owner, 0x70, index};
    mixed_reclamation_seeded = mixed_reclamation_seeded &&
        fs_write(mixed_reclamation_tags[index].data(),
                 mixed_reclamation_tags[index].size(),
                 mixed_reclamation_value, sizeof(mixed_reclamation_value)) ==
            FS_STATUS_OK;
  }
  mixed_reclamation_seeded = mixed_reclamation_seeded &&
      rp2350_fs_test_move_slot_beyond_probe_gap(
          mixed_reclamation_tags[0].data(),
          mixed_reclamation_tags[0].size()) == FS_STATUS_OK &&
      fs_exists(mixed_reclamation_tags[0].data(),
                mixed_reclamation_tags[0].size()) == 0;
  jcvm::Heap mixed_reclamation_heap(corruptible_owner);
  const std::vector<jcvm::jref_t> no_reclaimer_roots;
  ReclaimerRootContext no_reclaimer_roots_context{&no_reclaimer_roots};
  uint32_t mixed_reclamation_erased = 0;
  const auto mixed_reclamation_result = mixed_reclamation_seeded
      ? jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            corruptible_owner, mixed_reclamation_heap,
            enumerate_reclaimer_roots, &no_reclaimer_roots_context,
            &mixed_reclamation_erased)
      : jcvm::PersistentInventory::VisitResult::InvalidArgument;
  bool mixed_reclamation_progress =
      mixed_reclamation_result ==
          jcvm::PersistentInventory::VisitResult::Complete &&
      mixed_reclamation_erased == mixed_reclamation_tags.size();
  for (size_t index = 0; index < mixed_reclamation_tags.size(); ++index) {
    mixed_reclamation_progress = mixed_reclamation_progress &&
        fs_exists(mixed_reclamation_tags[index].data(),
                  mixed_reclamation_tags[index].size()) == 0;
  }
  expect(mixed_reclamation_progress,
         "persistent_reclaimer_repairs_probe_gap_and_reclaims_all_candidates");

  // A candidate can still be genuinely unresolvable after the integrity
  // checker declines repair. It must remain untouched while the other 19
  // records are reclaimed, including records reached in later batches.
  fake_flash.fill(0xff);
  bool hidden_candidate_seeded = fs_init() == FS_STATUS_OK;
  for (uint8_t index = 0; index < mixed_reclamation_tags.size(); ++index) {
    hidden_candidate_seeded = hidden_candidate_seeded &&
        fs_write(mixed_reclamation_tags[index].data(),
                 mixed_reclamation_tags[index].size(), mixed_reclamation_value,
                 sizeof(mixed_reclamation_value)) == FS_STATUS_OK;
  }
  rp2350_fs_test_hide_exists_tag(mixed_reclamation_tags[0].data(),
                                 mixed_reclamation_tags[0].size());
  jcvm::Heap hidden_candidate_heap(corruptible_owner);
  uint32_t hidden_candidate_erased = 0;
  const auto hidden_candidate_result = hidden_candidate_seeded
      ? jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            corruptible_owner, hidden_candidate_heap,
            enumerate_reclaimer_roots, &no_reclaimer_roots_context,
            &hidden_candidate_erased)
      : jcvm::PersistentInventory::VisitResult::InvalidArgument;
  rp2350_fs_test_hide_exists_tag(nullptr, 0);
  bool hidden_candidate_progress =
      hidden_candidate_result ==
          jcvm::PersistentInventory::VisitResult::Complete &&
      hidden_candidate_erased == mixed_reclamation_tags.size() - 1 &&
      fs_exists(mixed_reclamation_tags[0].data(),
                mixed_reclamation_tags[0].size()) != 0;
  for (size_t index = 1; index < mixed_reclamation_tags.size(); ++index) {
    hidden_candidate_progress = hidden_candidate_progress &&
        fs_exists(mixed_reclamation_tags[index].data(),
                  mixed_reclamation_tags[index].size()) == 0;
  }
  expect(hidden_candidate_progress,
         "persistent_reclaimer_skips_bad_candidate_and_erases_19_across_batches");

  fake_flash.fill(0xff);
  constexpr uint8_t corruptible_package = 0x38;
  std::array<std::array<uint8_t, 6>, 20> package_candidate_tags{};
  bool package_candidates_seeded = fs_init() == FS_STATUS_OK;
  for (uint8_t index = 0; index < package_candidate_tags.size(); ++index) {
    package_candidate_tags[index] = {
        0x02, corruptible_package, 0xa0, index, 0x01, index};
    package_candidates_seeded = package_candidates_seeded &&
        fs_write(package_candidate_tags[index].data(),
                 package_candidate_tags[index].size(), mixed_reclamation_value,
                 sizeof(mixed_reclamation_value)) == FS_STATUS_OK;
  }
  rp2350_fs_test_hide_exists_tag(package_candidate_tags[0].data(),
                                 package_candidate_tags[0].size());
  jcvm::Heap package_candidate_heap(corruptible_package);
  uint32_t package_candidates_erased = 0;
  const auto package_candidates_result = package_candidates_seeded
      ? jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
            corruptible_package, package_candidate_heap,
            enumerate_reclaimer_roots, &no_reclaimer_roots_context,
            &package_candidates_erased)
      : jcvm::PersistentInventory::VisitResult::InvalidArgument;
  rp2350_fs_test_hide_exists_tag(nullptr, 0);
  bool package_candidates_progress =
      package_candidates_result ==
          jcvm::PersistentInventory::VisitResult::Complete &&
      package_candidates_erased == package_candidate_tags.size() - 1 &&
      fs_exists(package_candidate_tags[0].data(),
                package_candidate_tags[0].size()) != 0;
  for (size_t index = 1; index < package_candidate_tags.size(); ++index) {
    package_candidates_progress = package_candidates_progress &&
        fs_exists(package_candidate_tags[index].data(),
                  package_candidate_tags[index].size()) == 0;
  }
  expect(package_candidates_progress,
         "package_static_reclaimer_skips_bad_candidate_and_erases_19");

  fake_flash.fill(0xff);
  constexpr uint8_t nested_reclamation_owner = 0x48;
  constexpr std::array<uint8_t, 6> nested_reclamation_parent{
      0x03, nested_reclamation_owner, 0x30, 0x01, 0x11, 0x22};
  constexpr std::array<uint8_t, 8> nested_reclamation_child{
      0x03, nested_reclamation_owner, 0x30, 0x01, 0x11, 0x22, 0x02, 0x02};
  constexpr uint8_t nested_reclamation_value[] = {0x73};
  const bool nested_reclamation_seeded =
      fs_init() == FS_STATUS_OK &&
      fs_write(nested_reclamation_parent.data(),
               nested_reclamation_parent.size(), nested_reclamation_value,
               sizeof(nested_reclamation_value)) == FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          nested_reclamation_child.data(), nested_reclamation_child.size(),
          nested_reclamation_value,
          sizeof(nested_reclamation_value)) == FS_STATUS_OK;
  jcvm::Heap nested_reclamation_heap(nested_reclamation_owner);
  const std::vector<jcvm::jref_t> no_nested_roots;
  ReclaimerRootContext no_nested_roots_context{&no_nested_roots};
  uint32_t nested_reclamation_erased = 0;
  const auto nested_reclamation_result = nested_reclamation_seeded
      ? jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            nested_reclamation_owner, nested_reclamation_heap,
            enumerate_reclaimer_roots, &no_nested_roots_context,
            &nested_reclamation_erased)
      : jcvm::PersistentInventory::VisitResult::InvalidArgument;
  expect(nested_reclamation_result ==
                 jcvm::PersistentInventory::VisitResult::Complete &&
             nested_reclamation_erased == 2 &&
             fs_exists(nested_reclamation_parent.data(),
                       nested_reclamation_parent.size()) == 0 &&
             fs_exists(nested_reclamation_child.data(),
                       nested_reclamation_child.size()) == 0,
         "persistent_reclaimer_erases_nested_child_before_parent");

  constexpr uint8_t cross_store_journal_tag[] = {0x02, 0xff, 56, 3};
  constexpr uint8_t cross_store_journal_value[] = {
      'I', 'N', 'S', '1', 56, 5, 5, 5, 0x01, 0x23};
  constexpr uint8_t cross_store_package_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x38};
  constexpr uint8_t cross_store_applet_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x39};
  constexpr uint8_t cross_store_instance_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x3a};
  constexpr uint8_t cross_store_cap_code[] = {0xca, 0xfe, 0x56, 0x01};
  constexpr uint8_t cross_store_seed_tag[] = {0xa3, 0x01};
  constexpr uint8_t cross_store_seed_value[] = {0x5a};
  constexpr uint8_t cross_store_applet_state_tag[] = {
      0x03, 56, 0x01, 0x23, 0x00};
  constexpr uint8_t cross_store_applet_state_value[] = {0x2a, 0x39};
  fake_flash.fill(0xff);
  bool cross_store_baseline = fs_init() == 0 &&
      fs_write(cross_store_seed_tag, sizeof(cross_store_seed_tag),
               cross_store_seed_value, sizeof(cross_store_seed_value)) == 0 &&
      fs_sync() == 0 &&
      cap_store_write_package(56, cross_store_package_aid,
                              sizeof(cross_store_package_aid),
                              cross_store_cap_code,
                              sizeof(cross_store_cap_code)) &&
      fs_init() == 0;
  cross_store_empty_journal_flash = fake_flash;
  persistent_transaction_active = true;
  const bool journal_creation_staged = cross_store_baseline &&
      fs_write(cross_store_journal_tag, sizeof(cross_store_journal_tag),
               cross_store_journal_value,
               sizeof(cross_store_journal_value)) == 0 &&
      fs_write(cross_store_applet_state_tag,
               sizeof(cross_store_applet_state_tag),
               cross_store_applet_state_value,
               sizeof(cross_store_applet_state_value)) == 0 &&
      fs_sync() == 0;
  persistent_transaction_active = false;
  reset_flash_failure();
  const bool journal_creation_baseline =
      journal_creation_staged && fs_sync() == 0;
  const size_t journal_creation_mutations = flash_mutation_count;
  cross_store_committed_flash = fake_flash;
  expect(journal_creation_baseline && journal_creation_mutations > 0,
         "install_journal_and_applet_state_commit_baseline");

  bool journal_creation_atomic =
      journal_creation_baseline && journal_creation_mutations > 0;
  uint32_t journal_creation_cut_state = 0x56a93c17;
  for (size_t trial = 0; trial < 1000 && journal_creation_atomic; ++trial) {
    fake_flash = cross_store_empty_journal_flash;
    if (!cap_store_initialize() || fs_init() != 0) {
      journal_creation_atomic = false;
      break;
    }
    persistent_transaction_active = true;
    const bool staged =
        fs_write(cross_store_journal_tag, sizeof(cross_store_journal_tag),
                 cross_store_journal_value,
                 sizeof(cross_store_journal_value)) == 0 &&
        fs_write(cross_store_applet_state_tag,
                 sizeof(cross_store_applet_state_tag),
                 cross_store_applet_state_value,
                 sizeof(cross_store_applet_state_value)) == 0 &&
        fs_sync() == 0;
    persistent_transaction_active = false;
    if (!staged) {
      journal_creation_atomic = false;
      break;
    }
    journal_creation_cut_state ^= journal_creation_cut_state << 13;
    journal_creation_cut_state ^= journal_creation_cut_state >> 17;
    journal_creation_cut_state ^= journal_creation_cut_state << 5;
    const size_t cut =
        journal_creation_cut_state % journal_creation_mutations;
    reset_flash_failure(cut);
    const bool interrupted = fs_sync() == FS_STATUS_IO_UNKNOWN;
    reset_flash_failure();
    if (!interrupted || !cap_store_initialize() || fs_init() != 0) {
      journal_creation_atomic = false;
      break;
    }
    uint32_t journal_length = 0;
    uint32_t applet_state_length = 0;
    const bool journal_exists = fs_length(
        cross_store_journal_tag, sizeof(cross_store_journal_tag),
        &journal_length) == 0;
    const bool state_exists = fs_length(
        cross_store_applet_state_tag, sizeof(cross_store_applet_state_tag),
        &applet_state_length) == 0;
    const bool both_absent = !journal_exists && !state_exists;
    uint8_t journal_bytes[sizeof(cross_store_journal_value)]{};
    uint8_t applet_state_bytes[sizeof(cross_store_applet_state_value)]{};
    const bool both_committed = journal_exists && state_exists &&
        journal_length == sizeof(cross_store_journal_value) &&
        applet_state_length == sizeof(cross_store_applet_state_value) &&
        fs_read(cross_store_journal_tag, sizeof(cross_store_journal_tag),
                journal_bytes, sizeof(journal_bytes)) == 0 &&
        fs_read(cross_store_applet_state_tag,
                sizeof(cross_store_applet_state_tag), applet_state_bytes,
                sizeof(applet_state_bytes)) == 0 &&
        std::memcmp(journal_bytes, cross_store_journal_value,
                    sizeof(cross_store_journal_value)) == 0 &&
        std::memcmp(applet_state_bytes, cross_store_applet_state_value,
                    sizeof(cross_store_applet_state_value)) == 0;
    if (!both_absent && !both_committed) journal_creation_atomic = false;
  }
  expect(journal_creation_atomic,
         "install_journal_and_applet_state_recover_atomically_after_1000_cuts");

  fake_flash = cross_store_committed_flash;
  const bool committed_journal_rebooted =
      cap_store_initialize() && fs_init() == 0;
  reset_flash_failure();
  const bool cross_store_publication_baseline =
      committed_journal_rebooted &&
      cap_store_mark_package_installed(
          56, cross_store_applet_aid, sizeof(cross_store_applet_aid),
          cross_store_instance_aid, sizeof(cross_store_instance_aid),
          0x0123);
  const size_t publication_mutations = flash_mutation_count;
  expect(cross_store_publication_baseline && publication_mutations > 0,
         "install_recovery_cross_store_publication_baseline");

  bool cross_store_recovery_atomic =
      cross_store_baseline && publication_mutations > 0;
  uint32_t cross_store_cut_state = 0xc01dcafe;
  for (size_t trial = 0; trial < 1000 && cross_store_recovery_atomic;
       ++trial) {
    fake_flash = cross_store_committed_flash;
    if (!cap_store_initialize() || fs_init() != 0 ||
        fs_exists(cross_store_journal_tag,
                  sizeof(cross_store_journal_tag)) == 0) {
      cross_store_recovery_atomic = false;
      break;
    }
    cross_store_cut_state ^= cross_store_cut_state << 13;
    cross_store_cut_state ^= cross_store_cut_state >> 17;
    cross_store_cut_state ^= cross_store_cut_state << 5;
    const size_t cut = cross_store_cut_state % publication_mutations;
    reset_flash_failure(cut);
    try {
      (void)cap_store_mark_package_installed(
          56, cross_store_applet_aid, sizeof(cross_store_applet_aid),
          cross_store_instance_aid, sizeof(cross_store_instance_aid),
          0x0123);
    } catch (const SimulatedPowerCut &) {
    }
    reset_flash_failure();

    // A reboot must retain both the durable recovery journal and a readable
    // old-or-new CAP header. Complete registry publication idempotently.
    const bool rebooted = cap_store_initialize() && fs_init() == 0;
    const uint8_t published_count = cap_store_package_instance_count(56);
    const bool registration_usable = published_count == 1 ||
        (published_count == 0 && cap_store_mark_package_installed(
            56, cross_store_applet_aid, sizeof(cross_store_applet_aid),
            cross_store_instance_aid, sizeof(cross_store_instance_aid),
            0x0123));
    uint32_t recovered_journal_length = 0;
    uint8_t recovered_journal[sizeof(cross_store_journal_value)]{};
    const bool journal_still_durable =
        fs_length(cross_store_journal_tag,
                  sizeof(cross_store_journal_tag),
                  &recovered_journal_length) == 0 &&
        recovered_journal_length == sizeof(cross_store_journal_value) &&
        fs_read(cross_store_journal_tag,
                sizeof(cross_store_journal_tag), recovered_journal,
                sizeof(recovered_journal)) == 0 &&
        std::memcmp(recovered_journal, cross_store_journal_value,
                    sizeof(cross_store_journal_value)) == 0;
    persistent_transaction_active = true;
    const bool cleanup_staged = journal_still_durable &&
        fs_erase(cross_store_journal_tag,
                 sizeof(cross_store_journal_tag)) == 0 &&
        fs_sync() == 0;
    persistent_transaction_active = false;
    const bool cleanup_committed = cleanup_staged && fs_sync() == 0;
    const bool final_reboot = cleanup_committed &&
        cap_store_initialize() && fs_init() == 0;
    uint8_t stored_applet_aid[16]{};
    uint8_t stored_applet_aid_length = 0;
    uint8_t stored_instance_aid[16]{};
    uint8_t stored_instance_aid_length = 0;
    uint16_t stored_class_index = 0xffff;
    const bool final_registry = final_reboot &&
        cap_store_package_instance_at(
            56, 0, stored_applet_aid, stored_applet_aid_length,
            stored_instance_aid, stored_instance_aid_length,
            stored_class_index) &&
        stored_applet_aid_length == sizeof(cross_store_applet_aid) &&
        std::memcmp(stored_applet_aid, cross_store_applet_aid,
                    sizeof(cross_store_applet_aid)) == 0 &&
        stored_instance_aid_length == sizeof(cross_store_instance_aid) &&
        std::memcmp(stored_instance_aid, cross_store_instance_aid,
                    sizeof(cross_store_instance_aid)) == 0 &&
        stored_class_index == 0x0123 &&
        fs_exists(cross_store_journal_tag,
                  sizeof(cross_store_journal_tag)) == 0;
    if (!rebooted || !registration_usable || !journal_still_durable ||
        !cleanup_committed || !final_registry) {
      cross_store_recovery_atomic = false;
    }
  }
  expect(cross_store_recovery_atomic,
         "install_registry_and_filesystem_journal_recover_after_1000_header_cuts");

  expect(fs_write(persistent_tag, sizeof(persistent_tag), persisted_first,
                  sizeof(persisted_first)) == 0 &&
             fs_sync() == 0 &&
             fs_erase(persistent_tag, sizeof(persistent_tag)) == 0 &&
             fs_sync() == 0 && fs_init() == 0 &&
             fs_exists(persistent_tag, sizeof(persistent_tag)) == 0,
         "filesystem_tombstone_survives_restart");

  // Both stores now use the same fake flash. Every CAP slot must leave the
  // filesystem's committed snapshot intact, including the last CAP slot.
  fake_flash.fill(0xff);
  constexpr uint8_t shared_tag[] = {0xb1, 0x22};
  constexpr uint8_t shared_value[] = {0x4c, 0x59};
  constexpr uint8_t cap_code[] = {0xca, 0xfe, 0x01, 0x02};
  constexpr uint8_t package_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x01};
  bool stores_coexist = fs_init() == 0 &&
      fs_write(shared_tag, sizeof(shared_tag), shared_value,
               sizeof(shared_value)) == 0 && fs_sync() == 0;
  for (uint8_t package_id = 56; package_id < 64 && stores_coexist;
       ++package_id) {
    uint8_t aid[sizeof(package_aid)];
    std::memcpy(aid, package_aid, sizeof(aid));
    aid[sizeof(aid) - 1] = package_id;
    stores_coexist = cap_store_write_package(package_id, aid, sizeof(aid),
                                             cap_code, sizeof(cap_code)) &&
                     fs_init() == 0;
    uint8_t actual_value[sizeof(shared_value)]{};
    stores_coexist = stores_coexist &&
        fs_read(shared_tag, sizeof(shared_tag), actual_value,
                sizeof(actual_value)) == 0 &&
        std::memcmp(actual_value, shared_value, sizeof(shared_value)) == 0;
  }
  expect(stores_coexist, "all_cap_slots_preserve_filesystem_snapshot");
  bool cap_codes_survive_fs_sync = stores_coexist;
  if (cap_codes_survive_fs_sync) {
    constexpr uint8_t next_value[] = {0x45, 0x58};
    cap_codes_survive_fs_sync =
        fs_write(shared_tag, sizeof(shared_tag), next_value,
                 sizeof(next_value)) == 0 && fs_sync() == 0 && fs_init() == 0;
  }
  for (uint8_t package_id = 56; package_id < 64 && cap_codes_survive_fs_sync;
       ++package_id) {
    uint32_t length = 0;
    const uint8_t *code = cap_store_data_for_package(package_id, &length);
    cap_codes_survive_fs_sync = code != nullptr && length == sizeof(cap_code) &&
        std::memcmp(code, cap_code, sizeof(cap_code)) == 0;
  }
  expect(cap_codes_survive_fs_sync,
         "filesystem_snapshots_preserve_all_cap_slots");

  auto *legacy_header = reinterpret_cast<LegacyCapSlotHeaderV3 *>(
      fake_flash.data() + flash_layout::cap_offset +
      7 * flash_layout::cap_slot_size);
  legacy_header->version = 3;
  legacy_header->reserved = 0xffff;
  legacy_header->header_crc32 = test_crc32(
      reinterpret_cast<const uint8_t *>(legacy_header),
      offsetof(LegacyCapSlotHeaderV3, header_crc32));
  uint32_t legacy_code_length = 0;
  const bool legacy_cap_header_is_readable =
      cap_store_initialize() && cap_store_package_instance_count(63) == 0 &&
      cap_store_data_for_package(63, &legacy_code_length) != nullptr &&
      legacy_code_length == sizeof(cap_code);
  expect(legacy_cap_header_is_readable,
         "legacy_v3_cap_header_is_readable_before_metadata_upgrade");

  constexpr uint8_t applet_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x63};
  constexpr uint8_t instance_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x73};
  const bool installed_record_written = cap_store_mark_package_installed(
      63, applet_aid, sizeof(applet_aid), instance_aid,
      sizeof(instance_aid), 0x0123);
  expect(installed_record_written && cap_store_package_instance_count(63) == 1,
         "cap_instance_metadata_is_published");
  uint8_t first_owner = 0;
  const bool first_owner_found =
      cap_store_applet_owner(instance_aid, sizeof(instance_aid), first_owner);
  constexpr uint8_t second_instance_aid[] = {0xa0, 0x00, 0x00, 0x00, 0x74};
  uint8_t second_owner = 0;
  const bool second_instance_written =
      cap_store_allocate_applet_owner(second_owner) &&
      cap_store_mark_package_installed(
          63, applet_aid, sizeof(applet_aid), second_instance_aid,
          sizeof(second_instance_aid), 0x0123, second_owner);
  uint8_t persisted_first_owner = 0;
  uint8_t persisted_second_owner = 0;
  const bool owner_ids_persisted = cap_store_initialize() &&
      cap_store_applet_owner(instance_aid, sizeof(instance_aid),
                             persisted_first_owner) &&
      cap_store_applet_owner(second_instance_aid, sizeof(second_instance_aid),
                             persisted_second_owner) &&
      first_owner_found && second_instance_written && first_owner != 0 &&
      second_owner != 0 && first_owner != second_owner &&
      persisted_first_owner == first_owner &&
      persisted_second_owner == second_owner;
  expect(owner_ids_persisted,
         "same_applet_class_instances_have_distinct_stable_owners");
  const bool repeated_instance_publication_is_idempotent =
      cap_store_mark_package_installed(
          63, applet_aid, sizeof(applet_aid), instance_aid,
          sizeof(instance_aid), 0x0123, first_owner) &&
      cap_store_package_instance_count(63) == 2;
  expect(repeated_instance_publication_is_idempotent,
         "install_journal_replay_accepts_already_published_instance");
  const bool conflicting_instance_publication_is_rejected =
      !cap_store_mark_package_installed(
          63, applet_aid, sizeof(applet_aid), instance_aid,
          sizeof(instance_aid), 0x0123, second_owner + 1);
  expect(conflicting_instance_publication_is_rejected,
         "duplicate_instance_aid_with_different_owner_is_rejected");
  expect(cap_store_remove_package_instance(63, second_instance_aid,
                                           sizeof(second_instance_aid)) &&
             cap_store_package_instance_count(63) == 1,
         "removing_one_instance_preserves_other_owner_mapping");
  reset_flash_failure(4096);
  const bool interrupted_remove =
      !cap_store_remove_package_instance(63, instance_aid,
                                         sizeof(instance_aid));
  reset_flash_failure();
  const bool installed_metadata_survives_cut =
      interrupted_remove && cap_store_initialize() &&
      cap_store_package_instance_count(63) == 1;
  expect(installed_metadata_survives_cut,
         "cap_registry_mirror_preserves_installed_header_after_cut");
  const bool installed_record_removed = cap_store_remove_package_instance(
      63, instance_aid, sizeof(instance_aid));
  uint32_t retained_code_length = 0;
  const uint8_t *retained_code =
      cap_store_data_for_package(63, &retained_code_length);
  expect(installed_record_removed &&
             cap_store_package_instance_count(63) == 0 &&
             retained_code != nullptr && retained_code_length == sizeof(cap_code),
         "cap_instance_metadata_can_be_rolled_back_without_deleting_package");

  // A cut after erasing the copy-on-write destination but before programming
  // its replacement must leave the previous package/instance header readable.
  reset_flash_failure(4096);
  const bool interrupted_metadata_update =
      !cap_store_mark_package_installed(
          63, applet_aid, sizeof(applet_aid), instance_aid,
          sizeof(instance_aid), 0x0123);
  reset_flash_failure();
  const bool previous_metadata_survives_cut =
      interrupted_metadata_update && cap_store_initialize() &&
      cap_store_package_instance_count(63) == 0 &&
      cap_store_data_for_package(63, &retained_code_length) != nullptr &&
      retained_code_length == sizeof(cap_code);
  expect(previous_metadata_survives_cut,
         "cap_registry_metadata_update_recovers_previous_header_after_cut");

  fake_flash.fill(0xff);
  bool owner_allocator_initialized = fs_init() == 0 && cap_store_initialize();
  constexpr uint8_t owner_test_code[] = {0xca, 0xfe, 0x56, 0x01};
  uint16_t assigned_owner_count = 0;
  uint8_t maximum_owner_assigned = 0;
  uint8_t first_owner_instance_aid[6]{};
  bool owner_space_filled = owner_allocator_initialized;
  for (uint8_t package_id = 56; package_id <= 62 && owner_space_filled;
       ++package_id) {
    const uint8_t package_aid[] = {0xa0, 0x00, 0x00, 0x00, package_id};
    const uint8_t applet_aid[] = {0xa0, 0x10, 0x00, package_id, 0x01};
    owner_space_filled = cap_store_write_package(
        package_id, package_aid, sizeof(package_aid), owner_test_code,
        sizeof(owner_test_code));
    const uint8_t instance_count = package_id < 62 ? 40 : 15;
    for (uint8_t index = 0; index < instance_count && owner_space_filled;
         ++index) {
      uint8_t instance_aid[] = {
          0xa0, 0x20, package_id, 0x00, 0x00,
          static_cast<uint8_t>(index)};
      if (index == 0 && package_id == 56) {
        std::memcpy(first_owner_instance_aid, instance_aid,
                    sizeof(instance_aid));
      }
      uint8_t allocated_owner = 0;
      owner_space_filled = cap_store_allocate_applet_owner(allocated_owner) &&
          cap_store_mark_package_installed(
              package_id, applet_aid, sizeof(applet_aid), instance_aid,
              sizeof(instance_aid), 0x0123, allocated_owner);
      if (owner_space_filled) {
        ++assigned_owner_count;
        maximum_owner_assigned = allocated_owner;
      }
    }
  }
  uint8_t exhausted_owner = 0;
  const bool owner_allocator_exhausted = owner_space_filled &&
      assigned_owner_count == UINT8_MAX && maximum_owner_assigned == UINT8_MAX &&
      !cap_store_allocate_applet_owner(exhausted_owner);
  expect(owner_allocator_exhausted,
         "cap_owner_allocator_rejects_exhaustion_at_255");
  const bool owner_allocator_reuses_deleted_id =
      owner_allocator_exhausted &&
      cap_store_remove_package_instance(56, first_owner_instance_aid,
                                        sizeof(first_owner_instance_aid)) &&
      cap_store_allocate_applet_owner(exhausted_owner) &&
      exhausted_owner == 1;
  expect(owner_allocator_reuses_deleted_id,
         "cap_owner_allocator_reuses_id_after_instance_delete");

  // Simulate a previous v3 snapshot in the current COW slots. Migration must
  // write the compact v4 image to the other slot and preserve the source.
  fake_flash.fill(0xff);
  bool legacy_fixture = fs_init() == 0 &&
      fs_write(shared_tag, sizeof(shared_tag), shared_value,
               sizeof(shared_value)) == 0 && fs_sync() == 0;
  if (legacy_fixture) {
    convert_v4_snapshot_to_legacy(flash_layout::filesystem_slot(0), 3);
    std::fill(fake_flash.begin() + flash_layout::filesystem_slot(1),
              fake_flash.begin() + flash_layout::filesystem_slot(1) +
                  flash_layout::filesystem_slot_size,
              0xff);
    std::fill(fake_flash.begin() + flash_layout::legacy_cap2_offset,
              fake_flash.begin() + flash_layout::legacy_cap2_data_end, 0x5a);
    constexpr uint32_t cap2_magic = 0x32415043;
    std::memcpy(fake_flash.data() + flash_layout::legacy_cap2_offset,
                &cap2_magic, sizeof(cap2_magic));
    legacy_fixture = cap_store_write_package(
        63, package_aid, sizeof(package_aid), cap_code, sizeof(cap_code));
  }
  committed_flash = fake_flash;
  uint8_t migrated_value[sizeof(shared_value)]{};
  bool migrated = legacy_fixture && fs_init() == 0 &&
      fs_read(shared_tag, sizeof(shared_tag), migrated_value,
              sizeof(migrated_value)) == 0 &&
      std::memcmp(migrated_value, shared_value, sizeof(shared_value)) == 0 &&
      fake_flash[flash_layout::filesystem_slot(0)] == 'J';
  expect(migrated, "legacy_snapshot_migrates_to_new_layout");
  expect(migrated && std::equal(
             fake_flash.begin() + flash_layout::legacy_cap2_offset,
             fake_flash.begin() + flash_layout::legacy_cap2_data_end,
             committed_flash.begin() + flash_layout::legacy_cap2_offset),
         "migration_preserves_legacy_cap2_bytes");
  uint32_t migrated_cap_length = 0;
  const uint8_t *migrated_cap = cap_store_data_for_package(
      63, &migrated_cap_length);
  expect(migrated && migrated_cap != nullptr &&
             migrated_cap_length == sizeof(cap_code) &&
             std::memcmp(migrated_cap, cap_code, sizeof(cap_code)) == 0,
         "migration_preserves_last_cap_slot");
  if (migrated) {
    fake_flash[flash_layout::filesystem_slot(0)] = 0;
    migrated = fs_init() == 0 &&
        fs_read(shared_tag, sizeof(shared_tag), migrated_value,
                sizeof(migrated_value)) == 0 &&
        std::memcmp(migrated_value, shared_value, sizeof(shared_value)) == 0;
  }
  expect(migrated, "migrated_snapshot_restores_without_legacy_source");

  bool migration_recovers_after_cut = legacy_fixture;
  for (size_t cut : {size_t{0}, size_t{100}, size_t{4096}, size_t{8192}}) {
    if (!migration_recovers_after_cut) break;
    fake_flash = committed_flash;
    reset_flash_failure(cut);
    try {
      (void)fs_init();
    } catch (const SimulatedPowerCut &) {
    }
    reset_flash_failure();
    migration_recovers_after_cut = fs_init() == 0 &&
        fs_read(shared_tag, sizeof(shared_tag), migrated_value,
                sizeof(migrated_value)) == 0 &&
        std::memcmp(migrated_value, shared_value, sizeof(shared_value)) == 0;
  }
  expect(migration_recovers_after_cut,
         "legacy_migration_recovers_after_power_cut");

  fake_flash = committed_flash;
  fake_flash[flash_layout::filesystem_slot(0) + 4096] ^= 1;
  cross_store_empty_journal_flash = fake_flash;
  expect(fs_init() != 0 && fake_flash == cross_store_empty_journal_flash,
         "unrecoverable_legacy_snapshot_fails_without_flash_writes");

  constexpr uint8_t updated_value[] = {0x33, 0x44};
  fake_flash = committed_flash;
  const bool second_slot_preserves_cap2 = migration_recovers_after_cut &&
      fs_init() == 0 &&
      fs_write(shared_tag, sizeof(shared_tag), updated_value,
               sizeof(updated_value)) == 0 && fs_sync() == 0 &&
      std::equal(fake_flash.begin() + flash_layout::legacy_cap2_offset,
                 fake_flash.begin() + flash_layout::legacy_cap2_data_end,
                 committed_flash.begin() + flash_layout::legacy_cap2_offset);
  expect(second_slot_preserves_cap2,
         "second_filesystem_slot_preserves_legacy_cap2_bytes");

  // Keep a hand-downgraded v2 snapshot in a current COW slot readable, then
  // prove that the next committed generation uses the compact v4 format.
  fake_flash.fill(0xff);
  expect(fs_init() == 0, "filesystem_v2_migration_setup");
  constexpr uint8_t v2_tag[] = {0xe0, 0x01, 0x01};
  constexpr uint8_t v2_value[] = {0x19, 0x28, 0x37};
  expect(fs_write(v2_tag, sizeof(v2_tag), v2_value, sizeof(v2_value)) == 0 &&
             fs_sync() == 0,
         "filesystem_v2_migration_seed_snapshot");
  convert_v4_snapshot_to_legacy(flash_layout::filesystem_slot(0), 2);
  uint8_t v2_actual[sizeof(v2_value)]{};
  expect(fs_init() == 0 &&
             fs_read(v2_tag, sizeof(v2_tag), v2_actual,
                     sizeof(v2_actual)) == 0 &&
             std::memcmp(v2_actual, v2_value, sizeof(v2_value)) == 0,
         "filesystem_v2_snapshot_remains_readable");
  constexpr uint8_t v3_tag[] = {0xe0, 0x02, 0x02};
  constexpr uint8_t v3_value[] = {0x48, 0x57};
  expect(fs_write(v3_tag, sizeof(v3_tag), v3_value, sizeof(v3_value)) == 0 &&
             fs_sync() == 0 &&
             fake_flash[flash_layout::filesystem_slot(0) + 4] == 5,
         "filesystem_first_write_migrates_snapshot_to_v5");
  uint8_t v3_actual[sizeof(v3_value)]{};
  expect(fs_init() == 0 &&
             fs_read(v2_tag, sizeof(v2_tag), v2_actual,
                     sizeof(v2_actual)) == 0 &&
             std::memcmp(v2_actual, v2_value, sizeof(v2_value)) == 0 &&
             fs_read(v3_tag, sizeof(v3_tag), v3_actual,
                     sizeof(v3_actual)) == 0 &&
             std::memcmp(v3_actual, v3_value, sizeof(v3_value)) == 0,
         "filesystem_v4_migration_restores_v2_and_new_records");

  constexpr uint8_t v4_compatibility_tag[] = {0xe0, 0x04, 0x05};
  constexpr uint8_t v4_compatibility_value[] = {0x19, 0x28, 0x37};
  bool v4_compatibility_seeded =
      fs_write(v4_compatibility_tag, sizeof(v4_compatibility_tag),
               v4_compatibility_value, sizeof(v4_compatibility_value)) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  for (const uint32_t slot : {flash_layout::filesystem_slot(0),
                              flash_layout::filesystem_slot(1)}) {
    auto *header = reinterpret_cast<FilesystemHeaderTest *>(
        fake_flash.data() + slot);
    if (header->magic == 0x3253464a && header->version == 5) {
      header->version = 4;
      header->header_crc32 = test_crc32(
          reinterpret_cast<const uint8_t *>(header),
          offsetof(FilesystemHeaderTest, header_crc32));
    }
  }
  uint8_t v4_compatibility_actual[sizeof(v4_compatibility_value)]{};
  expect(v4_compatibility_seeded && fs_init() == FS_STATUS_OK &&
             fs_read(v4_compatibility_tag, sizeof(v4_compatibility_tag),
                     v4_compatibility_actual,
                     sizeof(v4_compatibility_actual)) == FS_STATUS_OK &&
             std::memcmp(v4_compatibility_actual, v4_compatibility_value,
                         sizeof(v4_compatibility_value)) == 0,
         "filesystem_v4_bitmap_snapshot_remains_readable");

  // Force a record above 64 KiB, requiring the high offset bit. Commit and
  // restore it, then compact earlier records and verify its shifted offset.
  fake_flash.fill(0xff);
  expect(fs_init() == 0, "filesystem_wide_offset_setup");
  constexpr uint8_t wide_blocker_tag[] = {0xf0, 0x01, 0x01};
  constexpr uint8_t wide_blocker2_tag[] = {0xf0, 0x03, 0x03};
  constexpr uint8_t wide_blocker3_tag[] = {0xf0, 0x05, 0x05};
  constexpr uint8_t wide_value_tag[] = {0xf0, 0x02, 0x02};
  const std::vector<uint8_t> wide_blocker(40000, 0xa6);
  const std::vector<uint8_t> wide_blocker2(26000, 0xb7);
  const std::vector<uint8_t> wide_blocker3(0, 0xc8);
  constexpr uint8_t high_offset_value[] = {0x62, 0x73, 0x84, 0x95};
  expect(fs_write(wide_blocker_tag, sizeof(wide_blocker_tag),
                  wide_blocker.data(), wide_blocker.size()) == 0 &&
             fs_write(wide_blocker2_tag, sizeof(wide_blocker2_tag),
                      wide_blocker2.data(), wide_blocker2.size()) == 0 &&
             fs_write(wide_blocker3_tag, sizeof(wide_blocker3_tag),
                      wide_blocker3.data(), wide_blocker3.size()) == 0 &&
             fs_write(wide_value_tag, sizeof(wide_value_tag),
                      high_offset_value, sizeof(high_offset_value)) == 0 &&
             fs_sync() == 0,
         "filesystem_v4_writes_records_past_64k");
  std::vector<uint8_t> restored_wide_blocker(wide_blocker.size());
  std::vector<uint8_t> restored_wide_blocker2(wide_blocker2.size());
  std::vector<uint8_t> restored_wide_blocker3(wide_blocker3.size());
  uint8_t restored_high_offset_value[sizeof(high_offset_value)]{};
  const bool wide_restore_init = fs_init() == 0;
  const bool wide_blocker_restore =
      wide_restore_init &&
      fs_read(wide_blocker_tag, sizeof(wide_blocker_tag),
              restored_wide_blocker.data(), restored_wide_blocker.size()) ==
          0;
  const bool high_offset_restore =
      wide_restore_init &&
      fs_read(wide_value_tag, sizeof(wide_value_tag),
              restored_high_offset_value,
              sizeof(restored_high_offset_value)) == 0;
  const bool wide_blocker2_restore =
      wide_restore_init &&
      fs_read(wide_blocker2_tag, sizeof(wide_blocker2_tag),
              restored_wide_blocker2.data(), restored_wide_blocker2.size()) ==
          0;
  const bool wide_blocker3_restore =
      wide_restore_init &&
      fs_read(wide_blocker3_tag, sizeof(wide_blocker3_tag),
              restored_wide_blocker3.data(), restored_wide_blocker3.size()) ==
          0;
  expect(wide_restore_init, "filesystem_v4_snapshot_reinitializes");
  expect(wide_blocker_restore && restored_wide_blocker == wide_blocker,
         "filesystem_v4_restores_low_offset_record");
  expect(wide_blocker2_restore && restored_wide_blocker2 == wide_blocker2,
         "filesystem_v4_restores_second_offset_record");
  expect(wide_blocker3_restore && restored_wide_blocker3 == wide_blocker3,
         "filesystem_v4_restores_third_offset_record");
  expect(high_offset_restore &&
             std::memcmp(restored_high_offset_value, high_offset_value,
                         sizeof(high_offset_value)) == 0,
         "filesystem_v4_restores_high_offset_record");

  // An interrupted generation must recover atomically, including the
  // high-offset record already in the previous v4 snapshot. Sample cuts
  // during erase, payload, and header programming.
  committed_flash = fake_flash;
  constexpr uint8_t interrupted_v4_tag[] = {0xf0, 0x04, 0x04};
  constexpr uint8_t interrupted_v4_value[] = {0xa9, 0xb8, 0xc7};
  reset_flash_failure();
  const bool v3_cut_baseline =
      fs_write(interrupted_v4_tag, sizeof(interrupted_v4_tag),
               interrupted_v4_value, sizeof(interrupted_v4_value)) == 0 &&
      fs_sync() == 0;
  const size_t v3_snapshot_mutations = flash_mutation_count;
  bool v3_high_offset_cuts_are_atomic =
      v3_cut_baseline && v3_snapshot_mutations > 1;
  const std::array<size_t, 7> v3_cut_points = {
      0,
      1,
      v3_snapshot_mutations / 4,
      v3_snapshot_mutations / 2,
      (v3_snapshot_mutations * 3) / 4,
      v3_snapshot_mutations - 256,
      v3_snapshot_mutations - 1};
  for (const size_t cut : v3_cut_points) {
    if (!v3_high_offset_cuts_are_atomic) break;
    fake_flash = committed_flash;
    if (fs_init() != 0 ||
        fs_write(interrupted_v4_tag, sizeof(interrupted_v4_tag),
                 interrupted_v4_value, sizeof(interrupted_v4_value)) != 0) {
      v3_high_offset_cuts_are_atomic = false;
      break;
    }
    reset_flash_failure(cut);
    const uint8_t interrupted_sync_status = fs_sync();
    if (interrupted_sync_status == 0) {
      std::printf("TEST:INFO v4 cut did not interrupt cut=%zu mutations=%zu\n",
                  cut, v3_snapshot_mutations);
      v3_high_offset_cuts_are_atomic = false;
      break;
    }
    reset_flash_failure();
    uint8_t recovered_byte = 0;
    const int recovered_init_status = fs_init();
    const uint8_t recovered_high_status =
        fs_read(wide_value_tag, sizeof(wide_value_tag),
                restored_high_offset_value,
                sizeof(restored_high_offset_value));
    const bool high_offset_matches =
        std::memcmp(restored_high_offset_value, high_offset_value,
                    sizeof(high_offset_value)) == 0;
    uint8_t recovered_interrupted_value[sizeof(interrupted_v4_value)]{};
    const bool interrupted_record_is_new =
        fs_read(interrupted_v4_tag, sizeof(interrupted_v4_tag),
                recovered_interrupted_value,
                sizeof(recovered_interrupted_value)) == 0 &&
        std::memcmp(recovered_interrupted_value, interrupted_v4_value,
                    sizeof(interrupted_v4_value)) == 0;
    const bool interrupted_record_is_old =
        fs_read(interrupted_v4_tag, sizeof(interrupted_v4_tag),
                &recovered_byte, sizeof(recovered_byte)) != 0;
    if (recovered_init_status != 0 || recovered_high_status != 0 ||
        !high_offset_matches ||
        (!interrupted_record_is_old && !interrupted_record_is_new)) {
      std::printf("TEST:INFO v4 recovery cut=%zu init=%d high=%u match=%u old=%u new=%u\n",
                  cut, recovered_init_status, recovered_high_status,
                  high_offset_matches ? 1u : 0u,
                  interrupted_record_is_old ? 1u : 0u,
                  interrupted_record_is_new ? 1u : 0u);
      v3_high_offset_cuts_are_atomic = false;
      break;
    }
  }
  expect(v3_high_offset_cuts_are_atomic,
         "filesystem_v4_interrupted_snapshot_preserves_high_offset_record");
  expect(fs_erase(wide_blocker_tag, sizeof(wide_blocker_tag)) == 0 &&
             fs_erase(wide_blocker2_tag, sizeof(wide_blocker2_tag)) == 0 &&
             fs_erase(wide_blocker3_tag, sizeof(wide_blocker3_tag)) == 0 &&
             fs_read(wide_value_tag, sizeof(wide_value_tag),
                     restored_high_offset_value,
                     sizeof(restored_high_offset_value)) == 0 &&
             std::memcmp(restored_high_offset_value, high_offset_value,
                         sizeof(high_offset_value)) == 0 &&
             fs_sync() == 0 && fs_init() == 0 &&
             fs_read(wide_value_tag, sizeof(wide_value_tag),
                     restored_high_offset_value,
                     sizeof(restored_high_offset_value)) == 0 &&
             std::memcmp(restored_high_offset_value, high_offset_value,
                         sizeof(high_offset_value)) == 0,
         "filesystem_v4_compacts_high_offset_and_restarts");

  // Exercise repeated delete-and-replace compaction with the long compressed
  // tags and 4-byte values used by persistent Java Card metadata. This keeps
  // the arena near its 96 KiB ceiling while forcing compaction across offsets
  // above 64 KiB, where the physical card previously rejected overlapping
  // records.
  fake_flash.fill(0xff);
  constexpr size_t churn_initial_records = 2400;
  constexpr size_t churn_records_per_cycle = 64;
  constexpr size_t churn_new_records_per_cycle = 65;
  constexpr size_t churn_cycles = 32;
  using ChurnTag = std::array<uint8_t, 64>;
  auto make_churn_tag = [](uint32_t id) {
    ChurnTag tag{};
    tag[0] = 0x03;
    tag[1] = 0xee;
    tag[2] = 0xa5;
    tag[3] = 0x5a;
    tag[4] = 0x01;
    tag[5] = 0x02;
    for (uint8_t pair = 0; pair < 29; ++pair) {
      const uint8_t byte = static_cast<uint8_t>(
          (id >> ((pair % 4) * 8)) ^ (0x5du * pair));
      tag[6 + pair * 2] = byte;
      tag[7 + pair * 2] = byte;
    }
    return tag;
  };
  auto churn_value = [](uint32_t id) {
    return std::array<uint8_t, 4>{
        static_cast<uint8_t>(id >> 24), static_cast<uint8_t>(id >> 16),
        static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id)};
  };
  auto write_churn_record = [&](uint32_t id) {
    const ChurnTag tag = make_churn_tag(id);
    const auto value = churn_value(id);
    return fs_write(tag.data(), static_cast<uint8_t>(tag.size()), value.data(),
                    static_cast<uint32_t>(value.size()));
  };
  auto verify_churn_records = [&](const std::vector<uint32_t> &ids) {
    for (const uint32_t id : ids) {
      const ChurnTag tag = make_churn_tag(id);
      const auto expected = churn_value(id);
      std::array<uint8_t, 4> actual{};
      const uint8_t status = fs_read(
          tag.data(), static_cast<uint8_t>(tag.size()), actual.data(),
          static_cast<uint32_t>(actual.size()));
      if (status != 0 || actual != expected) {
        std::printf("TEST:INFO filesystem_churn record id=%lu status=%u expected=%02X%02X%02X%02X actual=%02X%02X%02X%02X\n",
                    static_cast<unsigned long>(id),
                    static_cast<unsigned>(status), expected[0], expected[1],
                    expected[2], expected[3], actual[0], actual[1], actual[2],
                    actual[3]);
        return false;
      }
    }
    return true;
  };
  bool churn_ok = fs_init() == 0;
  if (!churn_ok) std::printf("TEST:INFO filesystem_churn stage=init\n");
  std::vector<uint32_t> live_churn_ids;
  live_churn_ids.reserve(churn_initial_records + churn_cycles);
  for (uint32_t id = 0; id < churn_initial_records && churn_ok; ++id) {
    const uint8_t status = write_churn_record(id);
    churn_ok = status == FS_STATUS_OK;
    if (!churn_ok) {
      std::printf("TEST:INFO filesystem_churn stage=initial_write id=%lu status=%u\n",
                  static_cast<unsigned long>(id),
                  static_cast<unsigned>(status));
    }
    if (churn_ok) live_churn_ids.push_back(id);
  }
  churn_ok = churn_ok &&
      rp2350_fs_test_overlay_file_count() == churn_initial_records;
  if (churn_ok == false && live_churn_ids.size() == churn_initial_records) {
    std::printf("TEST:INFO filesystem_churn stage=initial_count count=%lu\n",
                static_cast<unsigned long>(
                    rp2350_fs_test_overlay_file_count()));
  }
  for (uint32_t cycle = 0; cycle < churn_cycles && churn_ok; ++cycle) {
    const size_t count_before = rp2350_fs_test_overlay_file_count();
    for (size_t index = 0; index < churn_records_per_cycle && churn_ok;
         ++index) {
      const ChurnTag tag = make_churn_tag(live_churn_ids[index]);
      const uint8_t status =
          fs_erase(tag.data(), static_cast<uint8_t>(tag.size()));
      churn_ok = status == FS_STATUS_OK;
      if (!churn_ok) {
        std::printf("TEST:INFO filesystem_churn stage=erase cycle=%lu index=%lu id=%lu status=%u files=%lu\n",
                    static_cast<unsigned long>(cycle),
                    static_cast<unsigned long>(index),
                    static_cast<unsigned long>(live_churn_ids[index]),
                    static_cast<unsigned>(status),
                    static_cast<unsigned long>(
                        rp2350_fs_test_overlay_file_count()));
      }
    }
    live_churn_ids.erase(live_churn_ids.begin(),
                         live_churn_ids.begin() + churn_records_per_cycle);
    for (size_t index = 0; index < churn_new_records_per_cycle && churn_ok;
         ++index) {
      const uint32_t id = static_cast<uint32_t>(churn_initial_records) +
          cycle * churn_new_records_per_cycle + static_cast<uint32_t>(index);
      const uint8_t status = write_churn_record(id);
      churn_ok = status == FS_STATUS_OK;
      if (!churn_ok) {
        std::printf("TEST:INFO filesystem_churn stage=write cycle=%lu index=%lu id=%lu status=%u files=%lu\n",
                    static_cast<unsigned long>(cycle),
                    static_cast<unsigned long>(index),
                    static_cast<unsigned long>(id),
                    static_cast<unsigned>(status),
                    static_cast<unsigned long>(
                        rp2350_fs_test_overlay_file_count()));
      }
      if (churn_ok) live_churn_ids.push_back(id);
    }
    if (churn_ok &&
        rp2350_fs_test_overlay_file_count() != count_before + 1) {
      std::printf("TEST:INFO filesystem_churn stage=count cycle=%lu before=%lu after=%lu\n",
                  static_cast<unsigned long>(cycle),
                  static_cast<unsigned long>(count_before),
                  static_cast<unsigned long>(
                      rp2350_fs_test_overlay_file_count()));
      churn_ok = false;
    }
    if (churn_ok) {
      if (!verify_churn_records(live_churn_ids)) {
        std::printf("TEST:INFO filesystem_churn stage=verify cycle=%lu\n",
                    static_cast<unsigned long>(cycle));
        churn_ok = false;
      }
    }
  }
  if (churn_ok && !verify_churn_records(live_churn_ids)) {
    std::printf("TEST:INFO filesystem_churn stage=verify_final\n");
    churn_ok = false;
  }
  if (churn_ok && fs_sync() != FS_STATUS_OK) {
    std::printf("TEST:INFO filesystem_churn stage=sync\n");
    churn_ok = false;
  }
  if (churn_ok && fs_init() != FS_STATUS_OK) {
    std::printf("TEST:INFO filesystem_churn stage=restart\n");
    churn_ok = false;
  }
  if (churn_ok && !verify_churn_records(live_churn_ids)) {
    std::printf("TEST:INFO filesystem_churn stage=verify_restored\n");
    churn_ok = false;
  }
  expect(churn_ok,
         "filesystem_long_compressed_records_survive_repeated_compaction");

  // The physical JCAlgTest failure had 2,468 live records, a 29-byte
  // persistent tag, and a 37-byte value. Exercise the legacy parent-slot
  // encoding used by older snapshots at a similar byte occupancy, including
  // repeated compaction and snapshot restore.
  fake_flash.fill(0xff);
  constexpr uint32_t parent_encoded_initial = 2300;
  constexpr uint32_t parent_encoded_cycle_size = 100;
  constexpr uint32_t parent_encoded_cycles = 2;
  using ParentEncodedTag = std::array<uint8_t, 27>;
  using ParentEncodedChildTag = std::array<uint8_t, 29>;
  auto make_parent_encoded_parent = [](uint8_t parent_id) {
    ParentEncodedTag tag{};
    tag[0] = 0x03;
    tag[1] = 0x91;
    tag[2] = parent_id;
    tag[3] = 0x42;
    tag[4] = 0x24;
    for (uint8_t index = 5; index < tag.size(); ++index) {
      tag[index] = static_cast<uint8_t>(parent_id * 17u + index * 11u);
    }
    return tag;
  };
  auto make_parent_encoded_child = [&](uint32_t id) {
    const uint8_t parent_id = static_cast<uint8_t>(id / 256u);
    const uint8_t suffix = static_cast<uint8_t>(id % 256u);
    const ParentEncodedTag parent = make_parent_encoded_parent(parent_id);
    ParentEncodedChildTag child{};
    std::copy(parent.begin(), parent.end(), child.begin());
    child[27] = suffix;
    child[28] = suffix;
    return child;
  };
  auto make_parent_encoded_value = [](uint32_t id) {
    std::array<uint8_t, 37> value{};
    for (size_t index = 0; index < value.size(); ++index) {
      value[index] = static_cast<uint8_t>(id * 29u + index * 13u);
    }
    return value;
  };
  bool parent_encoded_ok = fs_init() == FS_STATUS_OK;
  for (uint8_t parent_id = 0; parent_id < 10 && parent_encoded_ok;
       ++parent_id) {
    const ParentEncodedTag parent = make_parent_encoded_parent(parent_id);
    parent_encoded_ok = fs_write(parent.data(), parent.size(), nullptr, 0) ==
                        FS_STATUS_OK;
  }
  std::vector<uint32_t> live_parent_encoded_ids;
  live_parent_encoded_ids.reserve(parent_encoded_initial);
  for (uint32_t id = 0; id < parent_encoded_initial && parent_encoded_ok;
       ++id) {
    const ParentEncodedChildTag child = make_parent_encoded_child(id);
    const auto value = make_parent_encoded_value(id);
    parent_encoded_ok = rp2350_fs_test_write_parent_encoded(
                            child.data(), child.size(), value.data(),
                            value.size()) == FS_STATUS_OK;
    if (parent_encoded_ok) live_parent_encoded_ids.push_back(id);
  }
  auto verify_parent_encoded_records = [&]() {
    for (const uint32_t id : live_parent_encoded_ids) {
      const ParentEncodedChildTag child = make_parent_encoded_child(id);
      const auto expected = make_parent_encoded_value(id);
      std::array<uint8_t, 37> actual{};
      const uint8_t status = fs_read(child.data(), child.size(), actual.data(),
                                     actual.size());
      if (status != FS_STATUS_OK || actual != expected) {
        std::printf("TEST:INFO parent_encoded id=%lu status=%u\n",
                    static_cast<unsigned long>(id),
                    static_cast<unsigned>(status));
        return false;
      }
    }
    return true;
  };
  for (uint32_t cycle = 0; cycle < parent_encoded_cycles &&
                           parent_encoded_ok;
       ++cycle) {
    const uint32_t delete_count = parent_encoded_cycle_size +
                                  (cycle == 0 ? 1u : 0u);
    for (uint32_t index = 0; index < delete_count && parent_encoded_ok;
         ++index) {
      if (live_parent_encoded_ids.empty()) {
        parent_encoded_ok = false;
        break;
      }
      const uint32_t id = live_parent_encoded_ids.front();
      const ParentEncodedChildTag child = make_parent_encoded_child(id);
      parent_encoded_ok = fs_erase(child.data(), child.size()) ==
                          FS_STATUS_OK;
      if (parent_encoded_ok) {
        live_parent_encoded_ids.erase(
            std::remove(live_parent_encoded_ids.begin(),
                        live_parent_encoded_ids.end(), id),
            live_parent_encoded_ids.end());
      }
    }
    if (parent_encoded_ok && cycle == 0) {
      // Leave the parent records tombstoned while their legacy children are
      // live. A capacity-triggered compaction must retain those ancestors.
      for (uint8_t parent_id = 0; parent_id < 10 && parent_encoded_ok;
           ++parent_id) {
        const ParentEncodedTag parent =
            make_parent_encoded_parent(parent_id);
        parent_encoded_ok = fs_erase(parent.data(), parent.size()) ==
                            FS_STATUS_OK;
      }
      constexpr uint8_t tombstoned_parent_probe_tag[] = {0xf1, 0x42, 0x01};
      const auto tombstoned_parent_probe_value =
          make_parent_encoded_value(0xfeedu);
      parent_encoded_ok = parent_encoded_ok &&
          fs_write(tombstoned_parent_probe_tag,
                   sizeof(tombstoned_parent_probe_tag),
                   tombstoned_parent_probe_value.data(),
                   tombstoned_parent_probe_value.size()) == FS_STATUS_OK;
      for (uint8_t parent_id = 0; parent_id < 10 && parent_encoded_ok;
           ++parent_id) {
        const ParentEncodedTag parent =
            make_parent_encoded_parent(parent_id);
        parent_encoded_ok = fs_write(parent.data(), parent.size(), nullptr, 0) ==
                            FS_STATUS_OK;
      }
      if (parent_encoded_ok && !verify_parent_encoded_records()) {
        std::printf("TEST:INFO parent_encoded stage=tombstoned_parent_compaction\n");
        parent_encoded_ok = false;
      }
    }
    for (uint32_t index = 0; index < parent_encoded_cycle_size &&
                             parent_encoded_ok;
         ++index) {
      const uint32_t id = parent_encoded_initial +
                          cycle * parent_encoded_cycle_size + index;
      const ParentEncodedChildTag child = make_parent_encoded_child(id);
      const auto value = make_parent_encoded_value(id);
      parent_encoded_ok = rp2350_fs_test_write_parent_encoded(
                              child.data(), child.size(), value.data(),
                              value.size()) == FS_STATUS_OK;
      if (parent_encoded_ok) live_parent_encoded_ids.push_back(id);
    }
    if (parent_encoded_ok && !verify_parent_encoded_records()) {
      std::printf("TEST:INFO parent_encoded stage=verify cycle=%lu\n",
                  static_cast<unsigned long>(cycle));
      parent_encoded_ok = false;
    }
  }
  if (parent_encoded_ok) {
    parent_encoded_ok = fs_sync() == FS_STATUS_OK &&
                        fs_init() == FS_STATUS_OK &&
                        verify_parent_encoded_records();
  }
  expect(parent_encoded_ok,
         "filesystem_parent_encoded_records_survive_repeated_compaction");

  // Exercise chained legacy parent references (child -> parent -> root) while
  // both ancestors are tombstoned. Compaction must retain and remap the full
  // chain, and the records must become readable again after the ancestors are
  // recreated.
  fake_flash.fill(0xff);
  constexpr uint8_t nested_parent_root[] = {0x03, 0x62, 0x11, 0x23, 0x45};
  constexpr uint8_t nested_parent_middle[] = {
      0x03, 0x62, 0x11, 0x23, 0x45, 0x67, 0x67};
  constexpr uint8_t nested_parent_leaf[] = {
      0x03, 0x62, 0x11, 0x23, 0x45, 0x67, 0x67, 0x89, 0x89};
  constexpr std::array<uint8_t, 4> nested_parent_value = {
      0x91, 0x82, 0x73, 0x64};
  bool nested_parent_ok = fs_init() == FS_STATUS_OK &&
      fs_write(nested_parent_root, sizeof(nested_parent_root), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          nested_parent_middle, sizeof(nested_parent_middle), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          nested_parent_leaf, sizeof(nested_parent_leaf),
          nested_parent_value.data(), nested_parent_value.size()) ==
          FS_STATUS_OK;
  if (nested_parent_ok) {
    nested_parent_ok =
        fs_erase(nested_parent_middle, sizeof(nested_parent_middle)) ==
            FS_STATUS_OK &&
        fs_erase(nested_parent_root, sizeof(nested_parent_root)) ==
            FS_STATUS_OK &&
        rp2350_fs_test_compact() == FS_STATUS_OK &&
        fs_write(nested_parent_root, sizeof(nested_parent_root), nullptr, 0) ==
            FS_STATUS_OK &&
        fs_write(nested_parent_middle, sizeof(nested_parent_middle), nullptr,
                 0) == FS_STATUS_OK;
  }
  std::array<uint8_t, sizeof(nested_parent_value)> nested_parent_actual{};
  if (nested_parent_ok) {
    nested_parent_ok =
        fs_read(nested_parent_leaf, sizeof(nested_parent_leaf),
                nested_parent_actual.data(), nested_parent_actual.size()) ==
            FS_STATUS_OK &&
        nested_parent_actual == nested_parent_value &&
        fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
        fs_read(nested_parent_leaf, sizeof(nested_parent_leaf),
                nested_parent_actual.data(), nested_parent_actual.size()) ==
            FS_STATUS_OK &&
        nested_parent_actual == nested_parent_value;
  }
  expect(nested_parent_ok,
         "filesystem_compaction_retains_nested_tombstoned_parent_chain");

  // Corrupt serialized metadata must be rejected before reconstruction reads
  // beyond the stored record. This mirrors malformed/stale index state without
  // requiring a physical flash image and ensures compaction fails closed.
  fake_flash.fill(0xff);
  constexpr uint8_t malformed_record_tag[] = {0xf1, 0x42, 0x01};
  expect(fs_init() == FS_STATUS_OK &&
             fs_write(malformed_record_tag, sizeof(malformed_record_tag),
                      first_value, sizeof(first_value)) == FS_STATUS_OK &&
             rp2350_fs_test_corrupt_record_header(
                 malformed_record_tag, sizeof(malformed_record_tag), 0x7f) ==
                 FS_STATUS_OK &&
             rp2350_fs_test_compact() == FS_STATUS_IO_UNKNOWN,
         "filesystem_compaction_rejects_out_of_bounds_serialized_key");

  // Parent-slot records depend on a live table index. If that index is stale
  // but still points at another valid same-length parent, structural bounds
  // alone cannot detect that reconstruction silently changed the canonical
  // child tag. The stored hash must make compaction fail closed before any
  // records or payload bytes move.
  fake_flash.fill(0xff);
  constexpr std::array<uint8_t, 5> hashed_parent_a = {
      0x03, 0x91, 0x02, 0x44, 0x44};
  constexpr std::array<uint8_t, 5> hashed_parent_b = {
      0x03, 0x91, 0x02, 0x55, 0x55};
  constexpr std::array<uint8_t, 7> hashed_child = {
      0x03, 0x91, 0x02, 0x44, 0x44, 0xaa, 0xaa};
  constexpr std::array<uint8_t, 4> hashed_child_value = {0x71, 0x72, 0x73,
                                                         0x74};
  bool stale_parent_rejected =
      fs_init() == FS_STATUS_OK &&
      fs_write(hashed_parent_a.data(), hashed_parent_a.size(), nullptr, 0) ==
          FS_STATUS_OK &&
      fs_write(hashed_parent_b.data(), hashed_parent_b.size(), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          hashed_child.data(), hashed_child.size(), hashed_child_value.data(),
          hashed_child_value.size()) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_repoint_parent_encoded(
          hashed_child.data(), hashed_child.size(), hashed_parent_b.data(),
          hashed_parent_b.size()) == FS_STATUS_OK &&
      rp2350_fs_test_compact() == FS_STATUS_IO_UNKNOWN &&
      rp2350_fs_test_overlay_file_count() == 3 &&
      rp2350_fs_test_integrity_valid() == 0 && fs_sync() == FS_STATUS_OK &&
      fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1;
  std::array<uint8_t, hashed_child_value.size()> hashed_child_actual{};
  stale_parent_rejected = stale_parent_rejected &&
      fs_read(hashed_child.data(), hashed_child.size(),
              hashed_child_actual.data(), hashed_child_actual.size()) ==
          FS_STATUS_OK &&
      hashed_child_actual == hashed_child_value;
  expect(stale_parent_rejected,
         "filesystem_restore_falls_back_on_ambiguous_parent_hash_mismatch");

  // A duplicate offset used to survive the relocation loops and leave one
  // index entry pointing into a different record. Compaction must reject the
  // inconsistent layout before moving any arena bytes.
  fake_flash.fill(0xff);
  constexpr uint8_t shared_offset_a[] = {0xf2, 0x21, 0x01};
  constexpr uint8_t shared_offset_b[] = {0xf2, 0x21, 0x02};
  constexpr uint8_t shared_offset_value_a[] = {0x81, 0x82, 0x83};
  constexpr uint8_t shared_offset_value_b[] = {0x91, 0x92, 0x93, 0x94};
  bool duplicate_offset_rejected =
      fs_init() == FS_STATUS_OK &&
      fs_write(shared_offset_a, sizeof(shared_offset_a),
               shared_offset_value_a, sizeof(shared_offset_value_a)) ==
          FS_STATUS_OK &&
      fs_write(shared_offset_b, sizeof(shared_offset_b),
               shared_offset_value_b, sizeof(shared_offset_value_b)) ==
          FS_STATUS_OK &&
      rp2350_fs_test_share_record_offset(
          shared_offset_a, sizeof(shared_offset_a), shared_offset_b,
          sizeof(shared_offset_b)) == FS_STATUS_OK;
  duplicate_offset_rejected = duplicate_offset_rejected &&
      rp2350_fs_test_compact() == FS_STATUS_IO_UNKNOWN &&
      rp2350_fs_test_overlay_file_count() == 2;
  expect(duplicate_offset_rejected,
         "filesystem_compaction_rejects_duplicate_record_offsets_without_flash_mutation");

  fake_flash.fill(0xff);
  std::array<uint8_t, sizeof(shared_offset_value_a)> restored_shared_a{};
  std::array<uint8_t, sizeof(shared_offset_value_b)> restored_shared_b{};
  const bool corrupt_snapshot_fallback =
      fs_init() == FS_STATUS_OK &&
      fs_write(shared_offset_a, sizeof(shared_offset_a),
               shared_offset_value_a, sizeof(shared_offset_value_a)) ==
          FS_STATUS_OK &&
      fs_write(shared_offset_b, sizeof(shared_offset_b),
               shared_offset_value_b, sizeof(shared_offset_value_b)) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_share_record_offset(
          shared_offset_a, sizeof(shared_offset_a), shared_offset_b,
          sizeof(shared_offset_b)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
      fs_read(shared_offset_a, sizeof(shared_offset_a),
              restored_shared_a.data(), restored_shared_a.size()) ==
          FS_STATUS_OK &&
      fs_read(shared_offset_b, sizeof(shared_offset_b),
              restored_shared_b.data(), restored_shared_b.size()) ==
          FS_STATUS_OK &&
      std::memcmp(restored_shared_a.data(), shared_offset_value_a,
                  restored_shared_a.size()) == 0 &&
      std::memcmp(restored_shared_b.data(), shared_offset_value_b,
                  restored_shared_b.size()) == 0;
  expect(corrupt_snapshot_fallback,
         "filesystem_restore_rejects_integrity_failure_and_uses_previous_snapshot");

  // If neither committed COW copy validates, a present snapshot header means
  // the store may contain data whose record boundaries cannot be recovered.
  // Startup must fail closed and leave both damaged copies untouched instead
  // of initializing an empty filesystem over the user's persistent state.
  fake_flash.fill(0xff);
  constexpr uint8_t both_bad_snapshot_tag[] = {0xf2, 0x22, 0x01};
  constexpr uint8_t both_bad_snapshot_value_a[] = {0x11, 0x22, 0x33};
  constexpr uint8_t both_bad_snapshot_value_b[] = {0x44, 0x55, 0x66};
  const bool both_snapshot_copies_seeded =
      fs_init() == FS_STATUS_OK &&
      fs_write(both_bad_snapshot_tag, sizeof(both_bad_snapshot_tag),
               both_bad_snapshot_value_a,
               sizeof(both_bad_snapshot_value_a)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      fs_write(both_bad_snapshot_tag, sizeof(both_bad_snapshot_tag),
               both_bad_snapshot_value_b,
               sizeof(both_bad_snapshot_value_b)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  const uint32_t snapshot0 = flash_layout::filesystem_slot(0);
  const uint32_t snapshot1 = flash_layout::filesystem_slot(1);
  if (both_snapshot_copies_seeded) {
    fake_flash[snapshot0 + 4096] ^= 1;
    fake_flash[snapshot1 + 4096] ^= 1;
  }
  const auto both_bad_snapshot_flash = fake_flash;
  const bool both_bad_snapshots_fail_closed =
      both_snapshot_copies_seeded && fs_init() != FS_STATUS_OK &&
      fake_flash == both_bad_snapshot_flash;
  expect(both_bad_snapshots_fail_closed,
         "filesystem_restore_fails_closed_without_mutating_two_invalid_snapshots");

  // Retain valid CRCs while making the serialized arena intervals
  // unprovable in both generations. Startup must reject both copies without
  // dropping records or writing an empty replacement snapshot.
  fake_flash.fill(0xff);
  constexpr uint8_t ambiguous_snapshot_tag_a[] = {0xf2, 0x23, 0x01};
  constexpr uint8_t ambiguous_snapshot_tag_b[] = {0xf2, 0x23, 0x02};
  constexpr uint8_t ambiguous_snapshot_value_a[] = {0x21, 0x22, 0x23};
  constexpr uint8_t ambiguous_snapshot_value_b[] = {0x31, 0x32, 0x33};
  const bool ambiguous_snapshots_seeded =
      fs_init() == FS_STATUS_OK &&
      fs_write(ambiguous_snapshot_tag_a, sizeof(ambiguous_snapshot_tag_a),
               ambiguous_snapshot_value_a,
               sizeof(ambiguous_snapshot_value_a)) == FS_STATUS_OK &&
      fs_write(ambiguous_snapshot_tag_b, sizeof(ambiguous_snapshot_tag_b),
               ambiguous_snapshot_value_b,
               sizeof(ambiguous_snapshot_value_b)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      fs_write(ambiguous_snapshot_tag_b, sizeof(ambiguous_snapshot_tag_b),
               ambiguous_snapshot_value_a,
               sizeof(ambiguous_snapshot_value_a)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  const bool ambiguous_snapshots_corrupted = ambiguous_snapshots_seeded &&
      make_v4_snapshot_record_offsets_overlap(
          flash_layout::filesystem_slot(0)) &&
      make_v4_snapshot_record_offsets_overlap(
          flash_layout::filesystem_slot(1)) &&
      v4_snapshot_checksums_valid(flash_layout::filesystem_slot(0)) &&
      v4_snapshot_checksums_valid(flash_layout::filesystem_slot(1));
  const auto ambiguous_snapshot_flash = fake_flash;
  const bool ambiguous_snapshots_fail_closed = ambiguous_snapshots_corrupted &&
      fs_init() != FS_STATUS_OK && fake_flash == ambiguous_snapshot_flash;
  expect(ambiguous_snapshots_fail_closed,
         "filesystem_restore_fails_closed_for_two_ambiguous_cow_copies_without_mutation");

  // A self-consistent snapshot envelope can still contain an unreachable
  // index slot while all serialized records remain intact. Rebuild the hash
  // placement from their canonical tags, then publish the repaired index.
  fake_flash.fill(0xff);
  std::array<uint8_t, sizeof(shared_offset_value_a)> restored_orphan_a{};
  std::array<uint8_t, sizeof(shared_offset_value_b)> restored_orphan_b{};
  std::array<uint8_t, hashed_child_value.size()> restored_parent_child{};
  bool repaired_probe_chain =
      fs_init() == FS_STATUS_OK &&
      fs_write(hashed_parent_a.data(), hashed_parent_a.size(), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          hashed_child.data(), hashed_child.size(), hashed_child_value.data(),
          hashed_child_value.size()) == FS_STATUS_OK &&
      fs_write(shared_offset_a, sizeof(shared_offset_a),
               shared_offset_value_a, sizeof(shared_offset_value_a)) ==
          FS_STATUS_OK &&
      fs_write(shared_offset_b, sizeof(shared_offset_b),
               shared_offset_value_b, sizeof(shared_offset_value_b)) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_move_slot_beyond_probe_gap(
          shared_offset_b, sizeof(shared_offset_b)) == FS_STATUS_OK &&
      rp2350_fs_test_integrity_valid() == 0 && fs_sync() == FS_STATUS_OK &&
      fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1 &&
      fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
      rp2350_fs_test_integrity_valid() == 1 &&
      fs_read(shared_offset_a, sizeof(shared_offset_a),
              restored_orphan_a.data(), restored_orphan_a.size()) ==
          FS_STATUS_OK &&
      fs_read(shared_offset_b, sizeof(shared_offset_b),
              restored_orphan_b.data(), restored_orphan_b.size()) ==
          FS_STATUS_OK &&
      fs_read(hashed_child.data(), hashed_child.size(),
              restored_parent_child.data(), restored_parent_child.size()) ==
          FS_STATUS_OK &&
      std::memcmp(restored_orphan_a.data(), shared_offset_value_a,
                  restored_orphan_a.size()) == 0 &&
      std::memcmp(restored_orphan_b.data(), shared_offset_value_b,
                  restored_orphan_b.size()) == 0 &&
      restored_parent_child == hashed_child_value &&
      rp2350_fs_test_compact() == FS_STATUS_OK &&
      fs_read(shared_offset_a, sizeof(shared_offset_a),
              restored_orphan_a.data(), restored_orphan_a.size()) ==
          FS_STATUS_OK &&
      fs_read(shared_offset_b, sizeof(shared_offset_b),
              restored_orphan_b.data(), restored_orphan_b.size()) ==
          FS_STATUS_OK &&
      fs_read(hashed_child.data(), hashed_child.size(),
              restored_parent_child.data(), restored_parent_child.size()) ==
          FS_STATUS_OK &&
      std::memcmp(restored_orphan_a.data(), shared_offset_value_a,
                  restored_orphan_a.size()) == 0 &&
      std::memcmp(restored_orphan_b.data(), shared_offset_value_b,
                  restored_orphan_b.size()) == 0 &&
      restored_parent_child == hashed_child_value;
  expect(repaired_probe_chain,
         "filesystem_restore_rebuilds_probe_chain_index_from_records");

  // If canonical tags, parent links and record intervals still decode, a
  // damaged stored hash is recoverable. Rebuild every hash from the arena and
  // remap the child record's parent slot while keeping the newest snapshot.
  fake_flash.fill(0xff);
  std::array<uint8_t, sizeof(shared_offset_value_a)> restored_hash_value{};
  std::array<uint8_t, hashed_child_value.size()> restored_hash_child{};
  const bool repaired_hash_index =
      fs_init() == FS_STATUS_OK &&
      fs_write(hashed_parent_a.data(), hashed_parent_a.size(), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          hashed_child.data(), hashed_child.size(), hashed_child_value.data(),
          hashed_child_value.size()) == FS_STATUS_OK &&
      fs_write(shared_offset_a, sizeof(shared_offset_a),
               shared_offset_value_a, sizeof(shared_offset_value_a)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_corrupt_record_hash(
          hashed_parent_a.data(), hashed_parent_a.size()) == FS_STATUS_OK &&
      rp2350_fs_test_integrity_valid() == 0 && fs_sync() == FS_STATUS_OK &&
      fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1 &&
      fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
      rp2350_fs_test_integrity_valid() == 1 &&
      fs_read(shared_offset_a, sizeof(shared_offset_a),
              restored_hash_value.data(), restored_hash_value.size()) ==
          FS_STATUS_OK &&
      fs_read(hashed_child.data(), hashed_child.size(),
              restored_hash_child.data(), restored_hash_child.size()) ==
          FS_STATUS_OK &&
      std::memcmp(restored_hash_value.data(), shared_offset_value_a,
                  restored_hash_value.size()) == 0 &&
      restored_hash_child == hashed_child_value &&
      rp2350_fs_test_compact() == FS_STATUS_OK;
  expect(repaired_hash_index,
         "filesystem_restore_recomputes_hash_and_remaps_parent_slot");

  fake_flash.fill(0xff);
  std::array<uint8_t, sizeof(shared_offset_value_a)> restored_overlap_a{};
  std::array<uint8_t, sizeof(shared_offset_value_b)> restored_overlap_b{};
  const bool overlap_snapshot_fallback =
      fs_init() == FS_STATUS_OK &&
      fs_write(shared_offset_a, sizeof(shared_offset_a),
               shared_offset_value_a, sizeof(shared_offset_value_a)) ==
          FS_STATUS_OK &&
      fs_write(shared_offset_b, sizeof(shared_offset_b),
               shared_offset_value_b, sizeof(shared_offset_value_b)) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_overlap_record_offset(
          shared_offset_a, sizeof(shared_offset_a), shared_offset_b,
          sizeof(shared_offset_b)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
      fs_init() == FS_STATUS_OK &&
      fs_read(shared_offset_a, sizeof(shared_offset_a),
              restored_overlap_a.data(), restored_overlap_a.size()) ==
          FS_STATUS_OK &&
      fs_read(shared_offset_b, sizeof(shared_offset_b),
              restored_overlap_b.data(), restored_overlap_b.size()) ==
          FS_STATUS_OK &&
      std::memcmp(restored_overlap_a.data(), shared_offset_value_a,
                  restored_overlap_a.size()) == 0 &&
      std::memcmp(restored_overlap_b.data(), shared_offset_value_b,
                  restored_overlap_b.size()) == 0 &&
      rp2350_fs_test_compact() == FS_STATUS_OK;
  expect(overlap_snapshot_fallback,
         "filesystem_restore_recovers_overlapping_records_from_previous_snapshot");

  // If one self-contained record cannot be decoded but every serialized
  // boundary remains provable, quarantine only that record and preserve the
  // valid parent graph and unrelated data. Invalidate the older COW copy so
  // this exercises quarantine recovery rather than generation rollback.
  fake_flash.fill(0xff);
  constexpr uint8_t quarantine_bad_tag[] = {0xf4, 0x20, 0x01};
  constexpr uint8_t quarantine_bad_value[] = {0xa1, 0xa2, 0xa3};
  std::array<uint8_t, sizeof(hashed_child_value)> quarantine_child_actual{};
  std::array<uint8_t, sizeof(shared_offset_value_a)> quarantine_other_actual{};
  bool quarantined_record_recovery =
      fs_init() == FS_STATUS_OK &&
      fs_write(hashed_parent_a.data(), hashed_parent_a.size(), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          hashed_child.data(), hashed_child.size(), hashed_child_value.data(),
          hashed_child_value.size()) == FS_STATUS_OK &&
      fs_write(shared_offset_a, sizeof(shared_offset_a),
               shared_offset_value_a, sizeof(shared_offset_value_a)) ==
          FS_STATUS_OK &&
      fs_write(quarantine_bad_tag, sizeof(quarantine_bad_tag),
               quarantine_bad_value, sizeof(quarantine_bad_value)) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_corrupt_record_tag_length(
          quarantine_bad_tag, sizeof(quarantine_bad_tag), 4) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  fake_flash[flash_layout::filesystem_slot(0)] = 0;
  quarantined_record_recovery = quarantined_record_recovery &&
      fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1 &&
      rp2350_fs_test_overlay_file_count() == 3 &&
      fs_read(quarantine_bad_tag, sizeof(quarantine_bad_tag), nullptr, 0) ==
          FS_STATUS_NO_SUCH_TAG &&
      fs_read(hashed_child.data(), hashed_child.size(),
              quarantine_child_actual.data(), quarantine_child_actual.size()) ==
          FS_STATUS_OK &&
      quarantine_child_actual == hashed_child_value &&
      fs_read(shared_offset_a, sizeof(shared_offset_a),
              quarantine_other_actual.data(), quarantine_other_actual.size()) ==
          FS_STATUS_OK &&
      std::memcmp(quarantine_other_actual.data(), shared_offset_value_a,
                  quarantine_other_actual.size()) == 0 &&
      fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
      rp2350_fs_test_integrity_valid() == 1 &&
      fs_read(hashed_child.data(), hashed_child.size(),
              quarantine_child_actual.data(), quarantine_child_actual.size()) ==
          FS_STATUS_OK &&
      quarantine_child_actual == hashed_child_value &&
      rp2350_fs_test_compact() == FS_STATUS_OK;
  expect(quarantined_record_recovery,
         "filesystem_restore_quarantines_one_undecodable_record_and_preserves_parent_graph");

  fake_flash.fill(0xff);
  const bool ambiguous_parent_fails_closed =
      fs_init() == FS_STATUS_OK &&
      fs_write(hashed_parent_a.data(), hashed_parent_a.size(), nullptr, 0) ==
          FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          hashed_child.data(), hashed_child.size(), hashed_child_value.data(),
          hashed_child_value.size()) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_invalidate_parent_reference(
          hashed_child.data(), hashed_child.size()) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  fake_flash[flash_layout::filesystem_slot(0)] = 0;
  const bool ambiguous_parent_rejected =
      ambiguous_parent_fails_closed && fs_init() != FS_STATUS_OK;
  expect(ambiguous_parent_rejected,
         "filesystem_restore_does_not_quarantine_ambiguous_parent_reference");
  fake_flash.fill(0xff);
  expect(fs_init() == FS_STATUS_OK,
         "filesystem_reinitializes_after_fail_closed_parent_recovery");

  // A self-contained bad parent makes its parent-encoded descendants
  // undecodable too. Quarantine that whole dependent chain, but retain a
  // separate record when the interval proof remains exact.
  fake_flash.fill(0xff);
  constexpr uint8_t cascade_other_tag[] = {0xb4, 0x33, 0x44};
  constexpr uint8_t cascade_child_value[] = {0xc1, 0xc2};
  constexpr uint8_t cascade_other_value[] = {0xd1, 0xd2, 0xd3};
  std::array<uint8_t, sizeof(cascade_other_value)> cascade_other_actual{};
  bool cascaded_quarantine =
      fs_init() == FS_STATUS_OK &&
      fs_write(hashed_parent_a.data(), hashed_parent_a.size(),
               nullptr, 0) == FS_STATUS_OK &&
      rp2350_fs_test_write_parent_encoded(
          hashed_child.data(), hashed_child.size(), cascade_child_value,
          sizeof(cascade_child_value)) == FS_STATUS_OK &&
      fs_write(cascade_other_tag, sizeof(cascade_other_tag), cascade_other_value,
               sizeof(cascade_other_value)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_corrupt_record_tag_length(
          hashed_parent_a.data(), hashed_parent_a.size(), 4) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  fake_flash[flash_layout::filesystem_slot(0)] = 0;
  cascaded_quarantine = cascaded_quarantine && fs_init() == FS_STATUS_OK &&
      rp2350_fs_test_integrity_valid() == 1 &&
      rp2350_fs_test_overlay_file_count() == 1 &&
      fs_read(hashed_parent_a.data(), hashed_parent_a.size(), nullptr, 0) ==
          FS_STATUS_NO_SUCH_TAG &&
      fs_read(hashed_child.data(), hashed_child.size(), nullptr, 0) ==
          FS_STATUS_NO_SUCH_TAG &&
      fs_read(cascade_other_tag, sizeof(cascade_other_tag),
              cascade_other_actual.data(), cascade_other_actual.size()) ==
          FS_STATUS_OK &&
      std::memcmp(cascade_other_actual.data(), cascade_other_value,
                  cascade_other_actual.size()) == 0 &&
      fs_sync() == FS_STATUS_OK &&
      fs_init() == FS_STATUS_OK && rp2350_fs_test_integrity_valid() == 1;
  expect(cascaded_quarantine,
         "filesystem_restore_quarantines_bad_parent_and_dependent_child");

  // A tombstone carries deletion semantics for possible ROM-backed content.
  // Even with provable record boundaries, an undecodable tombstone is never
  // removed by salvage because doing so could resurrect that ROM value.
  fake_flash.fill(0xff);
  constexpr uint8_t quarantine_tombstone_tag[] = {0xf5, 0x21, 0x02};
  bool malformed_tombstone_rejected =
      fs_init() == FS_STATUS_OK &&
      fs_write(quarantine_tombstone_tag, sizeof(quarantine_tombstone_tag),
               nullptr, 0) == FS_STATUS_OK &&
      fs_erase(quarantine_tombstone_tag,
               sizeof(quarantine_tombstone_tag)) == FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK &&
      rp2350_fs_test_corrupt_record_tag_length(
          quarantine_tombstone_tag, sizeof(quarantine_tombstone_tag), 4) ==
          FS_STATUS_OK &&
      fs_sync() == FS_STATUS_OK;
  fake_flash[flash_layout::filesystem_slot(0)] = 0;
  malformed_tombstone_rejected = malformed_tombstone_rejected &&
      fs_init() != FS_STATUS_OK;
  expect(malformed_tombstone_rejected,
         "filesystem_restore_never_quarantines_undecodable_tombstone");
  fake_flash.fill(0xff);
  expect(fs_init() == FS_STATUS_OK,
         "filesystem_reinitializes_after_tombstone_fail_closed_recovery");

  // A full overlay needs to erase a data-bearing record before a zero-byte
  // record. Pick tags whose hash slots make the zero-byte entry first in the
  // old slot-order traversal, reproducing the persistent-storage exhaustion
  // failure seen on cards after repeated install/delete attempts.
  expect(fs_init() == 0, "full_overlay_cleanup_setup");
  std::array<uint8_t, 3> empty_namespace_tag{};
  std::array<uint8_t, 3> payload_namespace_tag{};
  bool selected_ordered_tags = false;
  for (uint16_t empty_tail = 1; empty_tail <= 0xff && !selected_ordered_tags;
       ++empty_tail) {
    for (uint16_t payload_tail = 1; payload_tail <= 0xff; ++payload_tail) {
      const std::array<uint8_t, 3> empty_candidate = {
          0xb0, 0x01, static_cast<uint8_t>(empty_tail)};
      const std::array<uint8_t, 3> payload_candidate = {
          0xb0, 0x01, static_cast<uint8_t>(payload_tail)};
      if (empty_candidate != payload_candidate &&
          hash_bucket(empty_candidate) > hash_bucket(payload_candidate)) {
        empty_namespace_tag = empty_candidate;
        payload_namespace_tag = payload_candidate;
        selected_ordered_tags = true;
        break;
      }
    }
  }
  std::array<uint8_t, 512> large_namespace_payload{};
  large_namespace_payload.fill(0x5a);
  const bool namespace_seeded = selected_ordered_tags &&
      fs_write(empty_namespace_tag.data(), empty_namespace_tag.size(),
               nullptr, 0) == 0 &&
      fs_write(payload_namespace_tag.data(), payload_namespace_tag.size(),
               large_namespace_payload.data(),
               large_namespace_payload.size()) == 0;
  expect(namespace_seeded, "full_overlay_cleanup_namespace_seeded");

  constexpr std::array<uint8_t, 16> fill_payload = {};
  constexpr uint8_t preserved_unrelated_tag[] = {0xb3, 0x01, 0x7a};
  constexpr std::array<uint8_t, 4> preserved_unrelated_value = {
      0x19, 0x28, 0x37, 0x46};
  uint8_t preserved_unrelated_readback[preserved_unrelated_value.size()]{};
  uint32_t preserved_unrelated_length = 0;
  const bool unrelated_value_seeded =
      fs_write(preserved_unrelated_tag, sizeof(preserved_unrelated_tag),
               preserved_unrelated_value.data(),
               preserved_unrelated_value.size()) == FS_STATUS_OK;
  bool overlay_capacity_reached = false;
  for (uint32_t value = 0; value <= 0xffff; ++value) {
    const std::array<uint8_t, 3> tag = {
        0xb1, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    const uint8_t status = fs_write(tag.data(), tag.size(), fill_payload.data(),
                                    fill_payload.size());
    if (status != FS_STATUS_OK) {
      overlay_capacity_reached = status == FS_STATUS_OUT_OF_FLASH;
      break;
    }
  }
  expect(overlay_capacity_reached, "full_overlay_cleanup_reaches_capacity");
  const bool unrelated_value_survives_exhaustion = unrelated_value_seeded &&
      fs_length(preserved_unrelated_tag, sizeof(preserved_unrelated_tag),
                &preserved_unrelated_length) == FS_STATUS_OK &&
      preserved_unrelated_length == preserved_unrelated_value.size() &&
      fs_read(preserved_unrelated_tag, sizeof(preserved_unrelated_tag),
              preserved_unrelated_readback,
              sizeof(preserved_unrelated_readback)) == FS_STATUS_OK &&
      std::equal(preserved_unrelated_value.begin(),
                 preserved_unrelated_value.end(),
                 preserved_unrelated_readback);
  constexpr uint8_t full_overlay_prefix[] = {0xb0, 0x01};
  const bool namespace_erased =
      fs_erase_prefix(full_overlay_prefix, sizeof(full_overlay_prefix)) == 0;
  expect(namespace_erased && unrelated_value_survives_exhaustion,
         "full_overlay_cleanup_reclaims_payload_before_empty_record");
  expect(fs_exists(empty_namespace_tag.data(), empty_namespace_tag.size()) == 0 &&
             fs_exists(payload_namespace_tag.data(),
                       payload_namespace_tag.size()) == 0,
         "full_overlay_cleanup_erases_all_namespace_records");
  const std::array<uint8_t, 3> post_cleanup_tag = {0xb4, 0x01, 0x01};
  constexpr std::array<uint8_t, 4> post_cleanup_value = {
      0xa1, 0xb2, 0xc3, 0xd4};
  expect(namespace_erased &&
             fs_write(preserved_unrelated_tag,
                      sizeof(preserved_unrelated_tag),
                      preserved_unrelated_value.data(),
                      preserved_unrelated_value.size()) == FS_STATUS_OK &&
             fs_write(post_cleanup_tag.data(), post_cleanup_tag.size(),
                      post_cleanup_value.data(), post_cleanup_value.size()) ==
                 FS_STATUS_OK &&
             fs_read(preserved_unrelated_tag,
                     sizeof(preserved_unrelated_tag),
                     preserved_unrelated_readback,
                     sizeof(preserved_unrelated_readback)) == FS_STATUS_OK &&
             std::equal(preserved_unrelated_value.begin(),
                        preserved_unrelated_value.end(),
                        preserved_unrelated_readback) &&
             fs_read(post_cleanup_tag.data(), post_cleanup_tag.size(),
                     preserved_unrelated_readback,
                     sizeof(preserved_unrelated_readback)) == FS_STATUS_OK &&
             std::equal(post_cleanup_value.begin(), post_cleanup_value.end(),
                        preserved_unrelated_readback),
         "full_overlay_cleanup_preserves_existing_and_accepts_following_writes");

  fake_flash.fill(0xff);
  bool package_delete_reclaims_overlay_slots = fs_init() == 0;
  constexpr std::array<uint8_t, 3> unrelated_package_tag = {0xb2, 0x51, 0x01};
  constexpr uint8_t unrelated_package_value[] = {0x51};
  package_delete_reclaims_overlay_slots =
      package_delete_reclaims_overlay_slots &&
      fs_write(unrelated_package_tag.data(), unrelated_package_tag.size(),
               unrelated_package_value, sizeof(unrelated_package_value)) == 0;
  for (uint8_t generation = 0; generation < 8 &&
                                package_delete_reclaims_overlay_slots;
       ++generation) {
    const uint8_t dynamic_package_id = static_cast<uint8_t>(56 + generation);
    const std::array<uint8_t, 3> static_state_tag = {
        0x02, dynamic_package_id, generation};
    const std::array<uint8_t, 4> lifecycle_state_tag = {
        0x02, 0xff, dynamic_package_id, 0x02};
    const uint8_t package_value[] = {generation, 0x5a};
    package_delete_reclaims_overlay_slots =
        fs_write(static_state_tag.data(), static_state_tag.size(),
                 package_value, sizeof(package_value)) == 0 &&
        fs_write(lifecycle_state_tag.data(), lifecycle_state_tag.size(),
                 package_value, sizeof(package_value)) == 0;
    for (uint16_t item = 0; item < 128 &&
                             package_delete_reclaims_overlay_slots;
         ++item) {
      const std::array<uint8_t, 6> tag = {
          0x03, 0x07, dynamic_package_id, generation,
          static_cast<uint8_t>(item >> 8), static_cast<uint8_t>(item)};
      const uint8_t value[] = {generation, static_cast<uint8_t>(item)};
      package_delete_reclaims_overlay_slots =
          fs_write(tag.data(), tag.size(), value, sizeof(value)) == 0;
    }
    package_delete_reclaims_overlay_slots =
        package_delete_reclaims_overlay_slots &&
        fs_erase_package_state(dynamic_package_id) == 0 &&
        rp2350_fs_test_overlay_file_count() == 1 &&
        fs_exists(unrelated_package_tag.data(),
                  unrelated_package_tag.size()) == 1 &&
        fs_sync() == 0 && fs_init() == 0 &&
        rp2350_fs_test_overlay_file_count() == 1 &&
        fs_exists(unrelated_package_tag.data(),
                  unrelated_package_tag.size()) == 1;
  }
  expect(package_delete_reclaims_overlay_slots,
         "package_delete_reclaims_all_dynamic_namespaces_across_restarts");

  fake_flash.fill(0xff);
  constexpr std::array<uint8_t, 3> compact_unrelated_tag = {0xb2, 0x52, 0x01};
  constexpr uint8_t compact_unrelated_value[] = {0x52};
  constexpr uint8_t compact_package_id = 0x39;
  std::array<std::array<uint8_t, 6>, 24> compact_package_tags{};
  constexpr uint8_t compact_package_value[] = {0x39, 0x57};
  bool compact_power_cut_setup = fs_init() == 0 &&
      fs_write(compact_unrelated_tag.data(), compact_unrelated_tag.size(),
               compact_unrelated_value, sizeof(compact_unrelated_value)) == 0;
  for (size_t index = 0; index < compact_package_tags.size() &&
                          compact_power_cut_setup;
       ++index) {
    compact_package_tags[index] = {
        0x03, 0x08, compact_package_id, 0x11,
        static_cast<uint8_t>(index >> 8), static_cast<uint8_t>(index)};
    compact_power_cut_setup =
        fs_write(compact_package_tags[index].data(),
                 compact_package_tags[index].size(), compact_package_value,
                 sizeof(compact_package_value)) == 0;
  }
  compact_power_cut_setup = compact_power_cut_setup && fs_sync() == 0;
  committed_flash = fake_flash;
  if (compact_power_cut_setup) {
    persistent_transaction_active = true;
    compact_power_cut_setup =
        fs_erase_package_state(compact_package_id) == 0 && fs_sync() == 0;
    persistent_transaction_active = false;
    reset_flash_failure();
    compact_power_cut_setup = compact_power_cut_setup && fs_sync() == 0;
  }
  const size_t compact_snapshot_mutations = flash_mutation_count;
  expect(compact_power_cut_setup && compact_snapshot_mutations > 0,
         "package_delete_compaction_snapshot_staged");

  bool compact_snapshot_recovers_atomically =
      compact_power_cut_setup && compact_snapshot_mutations > 0;
  uint32_t compact_random_state = 0x39c0ffee;
  for (size_t trial = 0; trial < 64 &&
                         compact_snapshot_recovers_atomically;
       ++trial) {
    fake_flash = committed_flash;
    persistent_transaction_active = false;
    if (fs_init() != 0) {
      compact_snapshot_recovers_atomically = false;
      break;
    }
    persistent_transaction_active = true;
    const bool deletion_staged =
        fs_erase_package_state(compact_package_id) == 0 && fs_sync() == 0;
    persistent_transaction_active = false;
    if (!deletion_staged) {
      compact_snapshot_recovers_atomically = false;
      break;
    }
    compact_random_state ^= compact_random_state << 13;
    compact_random_state ^= compact_random_state >> 17;
    compact_random_state ^= compact_random_state << 5;
    reset_flash_failure(compact_random_state % compact_snapshot_mutations);
    const bool interrupted = fs_sync() != 0;
    reset_flash_failure();
    if (!interrupted || fs_init() != 0) {
      compact_snapshot_recovers_atomically = false;
      break;
    }
    bool old_state =
        fs_exists(compact_unrelated_tag.data(),
                  compact_unrelated_tag.size()) == 1;
    bool new_state = old_state;
    for (const auto &tag : compact_package_tags) {
      old_state = old_state && fs_exists(tag.data(), tag.size()) == 1;
      new_state = new_state && fs_exists(tag.data(), tag.size()) == 0;
    }
    if (!old_state && !new_state) compact_snapshot_recovers_atomically = false;
  }
  expect(compact_snapshot_recovers_atomically,
         "package_delete_compaction_recovers_atomically_after_power_cuts");

  fake_flash.fill(0xff);
  constexpr uint8_t full_overlay_package_id = 56;
  constexpr std::array<uint8_t, 3> full_overlay_unrelated_tag = {
      0xb2, 0x53, 0x01};
  constexpr uint8_t full_overlay_unrelated_value[] = {0x53};
  constexpr uint8_t full_overlay_package_value[] = {0x56, 0x82};
  bool full_overlay_package_setup =
      fs_init() == 0 &&
      fs_write(full_overlay_unrelated_tag.data(),
               full_overlay_unrelated_tag.size(),
               full_overlay_unrelated_value,
               sizeof(full_overlay_unrelated_value)) == 0;
  for (uint16_t item = 0; item < 8191 && full_overlay_package_setup; ++item) {
    const std::array<uint8_t, 6> tag = {
        0x03, 0x09, full_overlay_package_id, 0x22,
        static_cast<uint8_t>(item >> 8), static_cast<uint8_t>(item)};
    full_overlay_package_setup =
        fs_write(tag.data(), tag.size(), full_overlay_package_value,
                 sizeof(full_overlay_package_value)) == 0;
  }
  expect(full_overlay_package_setup &&
             rp2350_fs_test_overlay_file_count() == 8192,
         "package_delete_compaction_reaches_full_overlay_index");
  const bool full_overlay_package_reclaimed =
      full_overlay_package_setup &&
      fs_erase_package_state(full_overlay_package_id) == 0 &&
      rp2350_fs_test_overlay_file_count() == 1 &&
      fs_exists(full_overlay_unrelated_tag.data(),
                full_overlay_unrelated_tag.size()) == 1 &&
      fs_sync() == 0 && fs_init() == 0 &&
      rp2350_fs_test_overlay_file_count() == 1 &&
      fs_exists(full_overlay_unrelated_tag.data(),
                full_overlay_unrelated_tag.size()) == 1;
  expect(full_overlay_package_reclaimed,
         "package_delete_compaction_reclaims_full_index_and_restarts");

  fake_flash.fill(0xff);
  constexpr uint8_t auto_compact_package_id = 57;
  bool auto_compact_setup = fs_init() == 0 &&
      fs_write(full_overlay_unrelated_tag.data(),
               full_overlay_unrelated_tag.size(),
               full_overlay_unrelated_value,
               sizeof(full_overlay_unrelated_value)) == 0;
  for (uint16_t item = 0; item < 8189 && auto_compact_setup; ++item) {
    const std::array<uint8_t, 6> tag = {
        0x03, 0x09, auto_compact_package_id, 0x22,
        static_cast<uint8_t>(item >> 8), static_cast<uint8_t>(item)};
    auto_compact_setup = fs_write(tag.data(), tag.size(),
                                  full_overlay_package_value,
                                  sizeof(full_overlay_package_value)) == 0;
  }
  const std::array<uint8_t, 6> auto_compact_parent_tag = {
      0x03, 0x09, auto_compact_package_id, 0x44, 0x01, 0x01};
  const std::array<uint8_t, 8> auto_compact_child_tag = {
      0x03, 0x09, auto_compact_package_id, 0x44, 0x01, 0x01, 0xaa, 0xaa};
  auto_compact_setup = auto_compact_setup &&
      fs_write(auto_compact_parent_tag.data(), auto_compact_parent_tag.size(),
               full_overlay_package_value,
               sizeof(full_overlay_package_value)) == 0 &&
      rp2350_fs_test_write_parent_encoded(
          auto_compact_child_tag.data(), auto_compact_child_tag.size(),
          full_overlay_package_value,
          sizeof(full_overlay_package_value)) == 0;
  const std::array<uint8_t, 6> auto_compact_deleted_tag = {
      0x03, 0x09, auto_compact_package_id, 0x22, 0x00, 0x00};
  const std::array<uint8_t, 6> auto_compact_new_tag = {
      0x03, 0x09, auto_compact_package_id, 0x23, 0x00, 0x01};
  const bool new_write_compacts_full_index = auto_compact_setup &&
      rp2350_fs_test_overlay_file_count() == 8192 &&
      fs_erase(auto_compact_deleted_tag.data(),
               auto_compact_deleted_tag.size()) == 0 &&
      fs_write(auto_compact_new_tag.data(), auto_compact_new_tag.size(),
               full_overlay_package_value,
               sizeof(full_overlay_package_value)) == 0 &&
      fs_sync() == 0 && fs_init() == 0 &&
      rp2350_fs_test_overlay_file_count() == 8192 &&
      fs_exists(auto_compact_deleted_tag.data(),
                auto_compact_deleted_tag.size()) == 0 &&
      fs_exists(auto_compact_new_tag.data(), auto_compact_new_tag.size()) == 1 &&
      fs_exists(auto_compact_parent_tag.data(),
                auto_compact_parent_tag.size()) == 1 &&
      fs_exists(auto_compact_child_tag.data(),
                auto_compact_child_tag.size()) == 1;
  expect(new_write_compacts_full_index,
         "new_write_compacts_deleted_records_at_full_index");

  // A same-size rewrite reuses the record's bytes in place: arena usage and
  // neighbouring records are unchanged, and the value survives a sync and a
  // restore from flash.
  {
    fake_flash.fill(0xff);
    constexpr uint8_t in_place_a[] = {0xf2, 0x31, 0x01};
    constexpr uint8_t in_place_b[] = {0xf2, 0x31, 0x02};
    constexpr uint8_t value_a1[] = {0x01, 0x02, 0x03};
    constexpr uint8_t value_a2[] = {0x0a, 0x0b, 0x0c};
    constexpr uint8_t value_b[] = {0x21, 0x22};
    std::array<uint8_t, sizeof(value_a2)> read_a{};
    std::array<uint8_t, sizeof(value_b)> read_b{};
    bool ok = fs_init() == FS_STATUS_OK &&
              fs_write(in_place_a, sizeof(in_place_a), value_a1,
                       sizeof(value_a1)) == FS_STATUS_OK &&
              fs_write(in_place_b, sizeof(in_place_b), value_b,
                       sizeof(value_b)) == FS_STATUS_OK;
    const uint32_t bytes_before = rp2350_fs_test_overlay_bytes_used();
    ok = ok &&
         fs_write(in_place_a, sizeof(in_place_a), value_a2,
                  sizeof(value_a2)) == FS_STATUS_OK &&
         rp2350_fs_test_overlay_bytes_used() == bytes_before &&
         rp2350_fs_test_integrity_valid() != 0 &&
         fs_sync() == FS_STATUS_OK && fs_init() == FS_STATUS_OK &&
         fs_read(in_place_a, sizeof(in_place_a), read_a.data(),
                 read_a.size()) == FS_STATUS_OK &&
         fs_read(in_place_b, sizeof(in_place_b), read_b.data(),
                 read_b.size()) == FS_STATUS_OK &&
         std::memcmp(read_a.data(), value_a2, sizeof(value_a2)) == 0 &&
         std::memcmp(read_b.data(), value_b, sizeof(value_b)) == 0;
    expect(ok, "same_size_rewrite_updates_record_in_place");
  }

  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
