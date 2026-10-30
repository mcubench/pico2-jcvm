#include "ffi.h"
#include "cap_store.hpp"
#include "flash_layout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <cstring>

extern "C" const uint8_t choupi_rommask_start[] = {0};
extern "C" const uint8_t choupi_rommask_end[] = {0};

extern "C" uint8_t fs_sync(void);
extern "C" uint8_t fs_erase_prefix(const uint8_t *prefix,
                                    uint8_t prefix_length);
extern "C" uint8_t rp2350_fs_test_write_parent_encoded(
    const uint8_t *tag, uint8_t tag_length, const uint8_t *data,
    uint32_t length);

namespace {
std::array<uint8_t, 2 * 1024 * 1024> fake_flash;
std::array<uint8_t, 2 * 1024 * 1024> committed_flash;
std::array<uint8_t, 2 * 1024 * 1024> cross_store_empty_journal_flash;
std::array<uint8_t, 2 * 1024 * 1024> cross_store_committed_flash;
size_t flash_mutation_count = 0;
size_t fail_after_mutations = static_cast<size_t>(-1);
bool persistent_transaction_active = false;

#pragma pack(push, 1)
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
}

extern "C" const uint8_t *rp2350_fs_test_flash_base(void) {
  return fake_flash.data();
}

extern "C" uint8_t choupi_persistent_write_transaction_active(void) {
  return persistent_transaction_active ? 1u : 0u;
}

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

int main() {
  fake_flash.fill(0xff);
  expect(fs_init() == 0, "filesystem_initializes");

  constexpr uint8_t reusable_tag[] = {0xa0, 0x01, 0x02};
  constexpr uint8_t first_value[] = {0x10, 0x20, 0x30};
  uint8_t actual[3]{};
  expect(fs_write(reusable_tag, sizeof(reusable_tag), first_value,
                  sizeof(first_value)) == 0,
         "write_before_erase");
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
  const auto committed_journal_flash = fake_flash;
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
    fake_flash = committed_journal_flash;
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
    const bool interrupted = fs_sync() != 0;
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
    const bool interrupted = fs_sync() != 0;
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

  // Simulate a card created by the previous layout: copy a real JFS2 snapshot
  // to its old address, remove the new copy, then boot and migrate it.
  fake_flash.fill(0xff);
  bool legacy_fixture = fs_init() == 0 &&
      fs_write(shared_tag, sizeof(shared_tag), shared_value,
               sizeof(shared_value)) == 0 && fs_sync() == 0;
  if (legacy_fixture) {
    std::copy_n(fake_flash.begin() + flash_layout::filesystem_slot(0),
                flash_layout::filesystem_slot_size,
                fake_flash.begin() + flash_layout::legacy_filesystem_slot(0));
    std::fill(fake_flash.begin() + flash_layout::filesystem_offset,
              fake_flash.begin() + flash_layout::filesystem_end, 0xff);
    std::fill(fake_flash.begin() + flash_layout::legacy_cap2_offset,
              fake_flash.begin() + flash_layout::legacy_cap2_data_end, 0x5a);
    constexpr uint32_t cap2_magic = 0x32415043;
    std::memcpy(fake_flash.data() + flash_layout::legacy_cap2_offset,
                &cap2_magic, sizeof(cap2_magic));
    legacy_fixture = cap_store_write_package(
        63, package_aid, sizeof(package_aid), cap_code, sizeof(cap_code));
  }
  const auto legacy_flash = fake_flash;
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
             legacy_flash.begin() + flash_layout::legacy_cap2_offset),
         "migration_preserves_legacy_cap2_bytes");
  uint32_t migrated_cap_length = 0;
  const uint8_t *migrated_cap = cap_store_data_for_package(
      63, &migrated_cap_length);
  expect(migrated && migrated_cap != nullptr &&
             migrated_cap_length == sizeof(cap_code) &&
             std::memcmp(migrated_cap, cap_code, sizeof(cap_code)) == 0,
         "migration_preserves_last_cap_slot");
  if (migrated) {
    fake_flash[flash_layout::legacy_filesystem_slot(0)] = 0;
    migrated = fs_init() == 0 &&
        fs_read(shared_tag, sizeof(shared_tag), migrated_value,
                sizeof(migrated_value)) == 0 &&
        std::memcmp(migrated_value, shared_value, sizeof(shared_value)) == 0;
  }
  expect(migrated, "migrated_snapshot_restores_without_legacy_source");

  bool migration_recovers_after_cut = legacy_fixture;
  for (size_t cut : {size_t{0}, size_t{100}, size_t{4096}, size_t{8192}}) {
    if (!migration_recovers_after_cut) break;
    fake_flash = legacy_flash;
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

  fake_flash = legacy_flash;
  fake_flash[flash_layout::legacy_filesystem_slot(0) + 4096] ^= 1;
  const auto damaged_flash = fake_flash;
  expect(fs_init() != 0 && fake_flash == damaged_flash,
         "unrecoverable_legacy_snapshot_fails_without_flash_writes");

  constexpr uint8_t updated_value[] = {0x33, 0x44};
  const bool second_slot_preserves_cap2 = migration_recovers_after_cut &&
      fs_write(shared_tag, sizeof(shared_tag), updated_value,
               sizeof(updated_value)) == 0 && fs_sync() == 0 &&
      std::equal(fake_flash.begin() + flash_layout::legacy_cap2_offset,
                 fake_flash.begin() + flash_layout::legacy_cap2_data_end,
                 legacy_flash.begin() + flash_layout::legacy_cap2_offset);
  expect(second_slot_preserves_cap2,
         "second_filesystem_slot_preserves_legacy_cap2_bytes");

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
  bool overlay_capacity_reached = false;
  for (uint32_t value = 0; value <= 0xffff; ++value) {
    const std::array<uint8_t, 3> tag = {
        0xb1, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    if (fs_write(tag.data(), tag.size(), fill_payload.data(),
                 fill_payload.size()) != 0) {
      overlay_capacity_reached = true;
      break;
    }
  }
  expect(overlay_capacity_reached, "full_overlay_cleanup_reaches_capacity");
  constexpr uint8_t full_overlay_prefix[] = {0xb0, 0x01};
  expect(fs_erase_prefix(full_overlay_prefix, sizeof(full_overlay_prefix)) == 0,
         "full_overlay_cleanup_reclaims_payload_before_empty_record");
  expect(fs_exists(empty_namespace_tag.data(), empty_namespace_tag.size()) == 0 &&
             fs_exists(payload_namespace_tag.data(),
                       payload_namespace_tag.size()) == 0,
         "full_overlay_cleanup_erases_all_namespace_records");

  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
