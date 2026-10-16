#include "jc_handlers/flashmemory.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace persistent_transaction_test {
std::vector<uint8_t> read(const uint8_t *tag, uint8_t length);
void seed(const uint8_t *tag, uint8_t tag_length, std::vector<uint8_t> value);
void fail_one_write();
void fail_one_sync();
unsigned int sync_count_value();
} // namespace persistent_transaction_test

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
