#include "jc_handlers/flashmemory.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace persistent_transaction_test {
std::vector<uint8_t> read(const uint8_t *tag, uint8_t length);
void seed(const uint8_t *tag, uint8_t tag_length, std::vector<uint8_t> value);
void fail_one_write();
void fail_one_read();
void fail_one_sync();
unsigned int sync_count_value();
void start_array_allocation_tracking();
std::pair<unsigned int, unsigned int> stop_array_allocation_tracking();
bool track_array_allocations = false;
unsigned int tracked_array_allocations = 0;
unsigned int tracked_array_deallocations = 0;
extern bool dynamic_cap_present[8];
}
extern uint16_t persistent_transaction_test_pending_reason;

void *operator new[](const std::size_t size) {
  if (void *memory = std::malloc(size == 0 ? 1 : size)) {
    if (persistent_transaction_test::track_array_allocations)
      ++persistent_transaction_test::tracked_array_allocations;
    return memory;
  }
  throw std::bad_alloc();
}

void operator delete[](void *memory) noexcept {
  if (persistent_transaction_test::track_array_allocations)
    ++persistent_transaction_test::tracked_array_deallocations;
  std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
  if (persistent_transaction_test::track_array_allocations)
    ++persistent_transaction_test::tracked_array_deallocations;
  std::free(memory);
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

jcvm::fs::Tag make_tag(uint8_t first) {
  jcvm::fs::Tag tag;
  tag.len = 3;
  tag.value[0] = 0xa0;
  tag.value[1] = 0x01;
  tag.value[2] = first;
  return tag;
}

std::vector<uint8_t> byte_record(uint8_t value) {
  return {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_BYTE), value};
}
} // namespace

int main() {
  using jcvm::FlashMemory_Handler;
  using persistent_transaction_test::read;
  using persistent_transaction_test::seed;

  expect(FlashMemory_Handler::maxCommitCapacity() == 32767,
         "max_commit_capacity_is_saturated_java_card_short");
  expect(FlashMemory_Handler::unusedCommitCapacity() == 32767,
         "unused_commit_capacity_is_full_outside_transaction");

  // A stale package-list bit must not cause import lookup to parse a dynamic
  // CAP slot that has already been removed.
  const uint8_t package_list_tag[] = {0x00};
  std::vector<uint8_t> package_bits(JCVM_MAX_PACKAGES / 8, 0);
  package_bits[56 / 8] |= static_cast<uint8_t>(1u << (56 % 8));
  package_bits[57 / 8] |= static_cast<uint8_t>(1u << (57 % 8));
  seed(package_list_tag, sizeof(package_list_tag), package_bits);
  persistent_transaction_test::dynamic_cap_present[1] = true;
  expect(!FlashMemory_Handler::isPackageExist(56),
         "stale_dynamic_package_bitmap_without_cap_slot_is_absent");
  expect(FlashMemory_Handler::isPackageExist(57),
         "dynamic_package_bitmap_with_cap_slot_is_present");
  expect(!FlashMemory_Handler::isPackageExist(58),
         "dynamic_package_without_bitmap_bit_is_absent");

  jcvm::fs::Tag journal_tag;
  journal_tag.len = 2;
  journal_tag.value[0] = 0xa7;
  journal_tag.value[1] = 0x10;
  const uint8_t journal_record[] = {0x4a, 0x49, 0x4e, 0x53, 0x01};
  expect(FlashMemory_Handler::beginPersistentWriteTransaction() &&
             FlashMemory_Handler::writePersistentRecord(
                 journal_tag, journal_record, sizeof(journal_record)) &&
             FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
             read(journal_tag.value, journal_tag.len).empty(),
         "opaque_install_journal_record_rolls_back_with_transaction");
  bool journal_round_trip =
      FlashMemory_Handler::beginPersistentWriteTransaction() &&
      FlashMemory_Handler::writePersistentRecord(
          journal_tag, journal_record, sizeof(journal_record)) &&
      FlashMemory_Handler::commitPersistentWriteTransaction();
  if (journal_round_trip) {
    auto [journal_length, journal_data] =
        FlashMemory_Handler::readPersistentRecord(journal_tag);
    journal_round_trip = journal_length == sizeof(journal_record) &&
        std::memcmp(journal_data.get(), journal_record,
                    sizeof(journal_record)) == 0;
  }
  expect(journal_round_trip,
         "opaque_install_journal_record_commits_and_reads_back");
  bool failed_journal_commit_rolls_back =
      FlashMemory_Handler::beginPersistentWriteTransaction() &&
      FlashMemory_Handler::writePersistentRecord(
          journal_tag, journal_record, sizeof(journal_record));
  if (failed_journal_commit_rolls_back) {
    persistent_transaction_test::fail_one_sync();
    failed_journal_commit_rolls_back =
        !FlashMemory_Handler::commitPersistentWriteTransaction() &&
        !FlashMemory_Handler::persistentWriteTransactionActive() &&
        read(journal_tag.value, journal_tag.len) ==
            std::vector<uint8_t>(journal_record,
                                 journal_record + sizeof(journal_record));
  }
  expect(failed_journal_commit_rolls_back,
         "failed_install_journal_commit_preserves_previous_record");
  const bool journal_erase_rollback =
      FlashMemory_Handler::beginPersistentWriteTransaction() &&
      FlashMemory_Handler::erasePersistentTag(journal_tag) &&
      FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
      read(journal_tag.value, journal_tag.len) ==
          std::vector<uint8_t>(journal_record,
                               journal_record + sizeof(journal_record));
  expect(journal_erase_rollback,
         "opaque_install_journal_erase_rolls_back");
  const bool journal_cleanup =
      FlashMemory_Handler::beginPersistentWriteTransaction() &&
      FlashMemory_Handler::erasePersistentTag(journal_tag) &&
      FlashMemory_Handler::commitPersistentWriteTransaction() &&
      read(journal_tag.value, journal_tag.len).empty();
  expect(journal_cleanup, "opaque_install_journal_cleanup_commits");

  const auto capacity_record = make_tag(0x71);
  const std::vector<uint8_t> capacity_payload(30000, 0xa5);
  const std::vector<uint8_t> capacity_update(30000, 0x3c);
  const std::vector<uint8_t> capacity_second_update(30000, 0x7e);
  const auto new_capacity_record = make_tag(0x73);
  const bool new_record_capacity =
      FlashMemory_Handler::beginPersistentWriteTransaction(true) &&
      FlashMemory_Handler::writePersistentRecord(
          new_capacity_record, capacity_payload.data(), capacity_payload.size()) &&
      FlashMemory_Handler::unusedCommitCapacity() == 32761 &&
      FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
      read(new_capacity_record.value, new_capacity_record.len).empty();
  expect(new_record_capacity,
         "new_record_capacity_charges_erase_journal_not_payload");

  const auto rejected_capacity_record = make_tag(0x72);
  const std::vector<uint8_t> over_capacity_payload(3000, 0x5a);
  const std::vector<uint8_t> rejected_capacity_update(3000, 0xc3);
  seed(capacity_record.value, capacity_record.len, capacity_payload);
  seed(rejected_capacity_record.value, rejected_capacity_record.len,
       over_capacity_payload);
  persistent_transaction_test_pending_reason = 0;
  bool capacity_accounting =
      FlashMemory_Handler::beginPersistentWriteTransaction(true) &&
      FlashMemory_Handler::unusedCommitCapacity() == 32767 &&
      FlashMemory_Handler::writePersistentRecord(
          capacity_record, capacity_update.data(), capacity_update.size()) &&
      FlashMemory_Handler::unusedCommitCapacity() == 2761 &&
      FlashMemory_Handler::writePersistentRecord(
          capacity_record, capacity_second_update.data(),
          capacity_second_update.size()) &&
      FlashMemory_Handler::unusedCommitCapacity() == 2761 &&
      !FlashMemory_Handler::writePersistentRecord(
          rejected_capacity_record, rejected_capacity_update.data(),
          rejected_capacity_update.size()) &&
      persistent_transaction_test_pending_reason == 3 &&
      FlashMemory_Handler::unusedCommitCapacity() == 2761 &&
      FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
      FlashMemory_Handler::unusedCommitCapacity() == 32767 &&
      read(capacity_record.value, capacity_record.len) == capacity_payload &&
      read(rejected_capacity_record.value,
           rejected_capacity_record.len) == over_capacity_payload;
  expect(capacity_accounting,
         "transaction_commit_capacity_is_accounted_and_overflow_rolls_back");

  const auto scalar = make_tag(6);
  seed(scalar.value, scalar.len,
       {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_BYTE), 0x5a});
  persistent_transaction_test::start_array_allocation_tracking();
  expect(FlashMemory_Handler::getPersistentField_Byte(scalar) == 0x5a,
         "persistent_byte_read_returns_stored_value");
  auto [successful_reads_allocated, successful_reads_freed] =
      persistent_transaction_test::stop_array_allocation_tracking();
  expect(successful_reads_allocated == 1 && successful_reads_freed == 1,
         "persistent_byte_read_releases_temporary_buffer");

  persistent_transaction_test::start_array_allocation_tracking();
  persistent_transaction_test::fail_one_read();
  bool read_failed = false;
  try {
    (void)FlashMemory_Handler::getPersistentField_Byte(scalar);
  } catch (jcvm::Exceptions) {
    read_failed = true;
  }
  expect(read_failed, "persistent_byte_read_error_is_reported");
  auto [failed_reads_allocated, failed_reads_freed] =
      persistent_transaction_test::stop_array_allocation_tracking();
  expect(failed_reads_allocated == 1 && failed_reads_freed == 1,
         "failed_persistent_byte_read_releases_temporary_buffer");

  const auto malformed_alias = make_tag(0x75);
  seed(malformed_alias.value, malformed_alias.len, {0xfd});
  bool truncated_alias_rejected = false;
  try {
    (void)FlashMemory_Handler::getPersistentField_Type(malformed_alias);
  } catch (jcvm::Exceptions) {
    truncated_alias_rejected = true;
  }
  expect(truncated_alias_rejected,
         "persistent_array_alias_rejects_truncated_target_header");

  seed(malformed_alias.value, malformed_alias.len,
       {0xfd, 2, 0xa0});
  bool mismatched_alias_length_rejected = false;
  try {
    (void)FlashMemory_Handler::getPersistentField_Type(malformed_alias);
  } catch (jcvm::Exceptions) {
    mismatched_alias_length_rejected = true;
  }
  expect(mismatched_alias_length_rejected,
         "persistent_array_alias_rejects_mismatched_target_length");

  const auto scalar_alias_target = make_tag(0x77);
  seed(scalar_alias_target.value, scalar_alias_target.len,
       byte_record(0x5a));
  std::vector<uint8_t> scalar_alias_record{0xfd, scalar_alias_target.len};
  scalar_alias_record.insert(
      scalar_alias_record.end(), scalar_alias_target.value,
      scalar_alias_target.value + scalar_alias_target.len);
  seed(malformed_alias.value, malformed_alias.len,
       std::move(scalar_alias_record));
  bool scalar_alias_target_rejected = false;
  try {
    (void)FlashMemory_Handler::getPersistentField_Type(malformed_alias);
  } catch (jcvm::Exceptions) {
    scalar_alias_target_rejected = true;
  }
  expect(scalar_alias_target_rejected,
         "persistent_array_alias_rejects_non_array_target");

  expect(!FlashMemory_Handler::persistentWriteTransactionActive(),
         "depth_zero_initially");
  expect(!FlashMemory_Handler::commitPersistentWriteTransaction(),
         "commit_without_transaction_rejected");
  expect(!FlashMemory_Handler::rollbackPersistentWriteTransaction(),
         "abort_without_transaction_rejected");

  const auto existing = make_tag(1);
  const std::vector<uint8_t> original{0xaa, 0x55, 0x00};
  seed(existing.value, existing.len, original);
  const unsigned int syncs_before_abort =
      persistent_transaction_test::sync_count_value();
  expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
         "begin_existing");
  expect(FlashMemory_Handler::persistentWriteTransactionActive(),
         "depth_active_after_begin");
  expect(!FlashMemory_Handler::beginPersistentWriteTransaction(),
         "reject_nested_transaction");
  FlashMemory_Handler::setPersistentField_Byte(existing, 0x42);
  FlashMemory_Handler::setPersistentField_Byte(existing, 0x43);
  expect(FlashMemory_Handler::rollbackPersistentWriteTransaction(),
         "rollback_existing_succeeds");
  expect(!FlashMemory_Handler::persistentWriteTransactionActive(),
         "depth_zero_after_abort");
  expect(read(existing.value, existing.len) == original,
         "rollback_restores_original_value_once");
  expect(persistent_transaction_test::sync_count_value() == syncs_before_abort + 1,
         "abort_persists_restored_snapshot");

  const auto created = make_tag(2);
  expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
         "begin_new_record");
  FlashMemory_Handler::setPersistentField_Byte(created, 0x31);
  expect(FlashMemory_Handler::rollbackPersistentWriteTransaction(),
         "rollback_new_record_succeeds");
  expect(read(created.value, created.len).empty(), "rollback_erases_new_record");

  const auto high_class_field =
      FlashMemory_Handler::getPersistentFieldTag(0, 63, 0x1234, 7);
  const auto legacy_high_class_field =
      FlashMemory_Handler::getLegacyPersistentFieldTag(0, 63, 0x1234, 7);
  expect(high_class_field.len == 6 && high_class_field.value[3] == 0x12 &&
             high_class_field.value[4] == 0x34 &&
             legacy_high_class_field.len == 5 &&
             legacy_high_class_field.value[3] == 0x34,
         "16_bit_class_tag_keeps_high_byte_distinct");
  seed(legacy_high_class_field.value, legacy_high_class_field.len,
       byte_record(0x5c));
  expect(FlashMemory_Handler::getPersistentField_Byte(high_class_field) ==
             0x5c,
         "wide_class_tag_reads_legacy_persistent_value");
  seed(high_class_field.value, high_class_field.len, byte_record(0x6d));
  expect(FlashMemory_Handler::getPersistentField_Byte(high_class_field) ==
             0x6d,
         "wide_class_tag_read_prefers_native_record");
  expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
         "begin_high_class_delete");
  expect(FlashMemory_Handler::erasePersistentTag(high_class_field),
         "erase_persistent_field_for_16_bit_class_index");
  expect(FlashMemory_Handler::erasePersistentTag(legacy_high_class_field),
         "erase_legacy_persistent_field_for_high_class_index");
  expect(FlashMemory_Handler::commitPersistentWriteTransaction(),
         "commit_high_class_delete");
  expect(read(high_class_field.value, high_class_field.len).empty() &&
             read(legacy_high_class_field.value,
                  legacy_high_class_field.len).empty(),
         "high_class_field_encodings_are_erased");

  expect(FlashMemory_Handler::beginPersistentWriteTransaction(), "begin_commit");
  FlashMemory_Handler::setPersistentField_Byte(existing, 0x77);
  const unsigned int syncs_before_commit =
      persistent_transaction_test::sync_count_value();
  expect(FlashMemory_Handler::commitPersistentWriteTransaction(),
         "commit_succeeds");
  expect(persistent_transaction_test::sync_count_value() == syncs_before_commit + 1,
         "commit_flushes_snapshot_before_return");
  expect(!FlashMemory_Handler::persistentWriteTransactionActive(),
         "depth_zero_after_commit");
  expect(read(existing.value, existing.len) == byte_record(0x77),
         "commit_keeps_new_value");

  const auto failed_write = make_tag(3);
  const std::vector<uint8_t> before_failure{0x10, 0x20};
  seed(failed_write.value, failed_write.len, before_failure);
  expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
         "begin_failed_write");
  persistent_transaction_test::fail_one_write();
  bool write_failed = false;
  try {
    FlashMemory_Handler::setPersistentField_Byte(failed_write, 0x11);
  } catch (...) {
    write_failed = true;
  }
  expect(write_failed, "filesystem_error_is_reported");
  expect(FlashMemory_Handler::rollbackPersistentWriteTransaction(),
         "rollback_after_write_error_succeeds");
  expect(read(failed_write.value, failed_write.len) == before_failure,
         "rollback_after_write_error_preserves_original");

  const auto failed_new_write = make_tag(4);
  expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
         "begin_failed_new_write");
  persistent_transaction_test::fail_one_write();
  write_failed = false;
  try {
    FlashMemory_Handler::setPersistentField_Byte(failed_new_write, 0x11);
  } catch (...) {
    write_failed = true;
  }
  expect(write_failed, "new_record_write_error_is_reported");
  expect(FlashMemory_Handler::rollbackPersistentWriteTransaction(),
         "rollback_failed_new_write_succeeds");
  expect(read(failed_new_write.value, failed_new_write.len).empty(),
         "rollback_failed_new_write_leaves_no_record");

  const auto failed_commit = make_tag(5);
  const std::vector<uint8_t> before_failed_commit{0x21, 0x43};
  seed(failed_commit.value, failed_commit.len, before_failed_commit);
  expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
         "begin_failed_commit");
  FlashMemory_Handler::setPersistentField_Byte(failed_commit, 0x65);
  persistent_transaction_test::fail_one_sync();
  expect(!FlashMemory_Handler::commitPersistentWriteTransaction(),
         "failed_snapshot_commit_is_reported");
  expect(read(failed_commit.value, failed_commit.len) == before_failed_commit,
         "failed_snapshot_commit_rolls_back_values");
  expect(!FlashMemory_Handler::persistentWriteTransactionActive(),
         "failed_snapshot_commit_closes_transaction");

  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
