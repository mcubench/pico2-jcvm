#include "ffi.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <cstring>

extern "C" const uint8_t choupi_rommask_start[] = {0};
extern "C" const uint8_t choupi_rommask_end[] = {0};

bool cap_store_initialize() noexcept { return true; }

extern "C" uint8_t fs_sync(void);

namespace {
std::array<uint8_t, 2 * 1024 * 1024> fake_flash;
std::array<uint8_t, 2 * 1024 * 1024> committed_flash;
size_t flash_mutation_count = 0;
size_t fail_after_mutations = static_cast<size_t>(-1);
bool persistent_transaction_active = false;

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
  func(param);
  return 0;
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
  fake_flash[0x140000 + 4096 + 17] ^= 0x01;
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
    try {
      (void)fs_sync();
      every_cut_recovers_atomically = false;
      break;
    } catch (const SimulatedPowerCut &) {
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

  expect(fs_erase(persistent_tag, sizeof(persistent_tag)) == 0 &&
             fs_sync() == 0 && fs_init() == 0 &&
             fs_exists(persistent_tag, sizeof(persistent_tag)) == 0,
         "filesystem_tombstone_survives_restart");

  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
