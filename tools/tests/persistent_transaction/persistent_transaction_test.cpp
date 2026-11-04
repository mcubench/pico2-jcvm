#include "jc_handlers/flashmemory.hpp"
#include "heap.hpp"
#include "persistent_inventory.hpp"
#include "jc_types/jc_array.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace persistent_transaction_test {
void force_cursor_stall_after_one_result();
void fail_erase_after(unsigned int successful_erases);
void fail_prefix_erase_after_one();
void fail_package_state_erase_after_one();
extern uint16_t instance_field_count;
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

struct CandidateTags {
  std::vector<std::vector<uint8_t>> tags;
};

bool collect_candidate(const jcvm::PersistentInventory::Tag &tag,
                       void *opaque) {
  auto &captured = *static_cast<CandidateTags *>(opaque);
  captured.tags.emplace_back(tag.value, tag.value + tag.len);
  return true;
}

struct RootVectorContext {
  const std::vector<jcvm::jref_t> *roots;
};

bool enumerate_root_vector(jcvm::Heap::ReferenceVisitor visitor,
                           void *visitor_context, void *root_context) {
  if (visitor == nullptr || root_context == nullptr) return false;
  const auto &roots = *static_cast<RootVectorContext *>(root_context)->roots;
  for (const auto &root : roots) visitor(root, visitor_context);
  return true;
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

  // Exercise the production strict reference decoder through the real heap
  // collector. A one-byte object-alias record is present at the serialized
  // array-element tag, so it must fail closed before either heap slot is freed.
  {
    jcvm::Heap heap(7);
    const auto parent_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_REFERENCE, 0xffff);
    const auto unreachable_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    const auto parent = heap.getArray(parent_reference);
    const auto parent_tag = make_tag(0x79);
    parent->setPersistentTag(parent_tag);
    seed(parent_tag.value, parent_tag.len,
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_OBJECT),
          0, 1, 0xff, 0xff});
    const auto malformed_edge = FlashMemory_Handler::computeTag(parent_tag, 0);
    seed(malformed_edge.value, malformed_edge.len, {0xfe});
    const auto summary = heap.collectUnreachableFrom({parent_reference});
    expect(!summary.complete && summary.edge_load_failures == 1 &&
               summary.arrays_reclaimed == 0 &&
               heap.getArray(parent_reference) != nullptr &&
               heap.getArray(unreachable_reference) != nullptr,
           "malformed_serialized_object_alias_blocks_heap_reclamation");
  }

  const auto exercise_bad_array_edge = [&](const uint8_t suffix,
                                           std::vector<uint8_t> record,
                                           const char *test_name) {
    jcvm::Heap heap(7);
    const auto parent_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_REFERENCE, 0xffff);
    const auto unreachable_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    const auto parent = heap.getArray(parent_reference);
    const auto parent_tag = make_tag(suffix);
    parent->setPersistentTag(parent_tag);
    seed(parent_tag.value, parent_tag.len,
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_OBJECT),
          0, 1, 0xff, 0xff});
    const auto edge_tag = FlashMemory_Handler::computeTag(parent_tag, 0);
    seed(edge_tag.value, edge_tag.len, std::move(record));
    const auto summary = heap.collectUnreachableFrom({parent_reference});
    expect(!summary.complete && summary.edge_load_failures == 1 &&
               summary.arrays_reclaimed == 0 &&
               heap.getArray(parent_reference) != nullptr &&
               heap.getArray(unreachable_reference) != nullptr,
           test_name);
  };
  exercise_bad_array_edge(0x7a, {0xfd},
                          "truncated_serialized_array_alias_blocks_reclamation");

  const auto alias_parent_tag = make_tag(0x7b);
  const auto alias_edge_tag = FlashMemory_Handler::computeTag(alias_parent_tag, 0);
  std::vector<uint8_t> cyclic_alias{0xfd, alias_edge_tag.len};
  cyclic_alias.insert(cyclic_alias.end(), alias_edge_tag.value,
                      alias_edge_tag.value + alias_edge_tag.len);
  exercise_bad_array_edge(0x7b, std::move(cyclic_alias),
                          "cyclic_serialized_array_alias_is_bounded_and_blocks_reclamation");

  {
    jcvm::Heap heap(7);
    const auto parent_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_REFERENCE, 0xffff);
    const auto garbage_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    const auto parent = heap.getArray(parent_reference);
    const auto parent_tag = make_tag(0x7e);
    const auto middle_tag = make_tag(0x7f);
    const auto target_tag = make_tag(0x80);
    parent->setPersistentTag(parent_tag);
    seed(parent_tag.value, parent_tag.len,
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_OBJECT),
          0, 1, 0xff, 0xff});
    const auto edge_tag = FlashMemory_Handler::computeTag(parent_tag, 0);
    std::vector<uint8_t> edge_alias{0xfd, middle_tag.len};
    edge_alias.insert(edge_alias.end(), middle_tag.value,
                      middle_tag.value + middle_tag.len);
    seed(edge_tag.value, edge_tag.len, std::move(edge_alias));
    std::vector<uint8_t> middle_alias{0xfd, target_tag.len};
    middle_alias.insert(middle_alias.end(), target_tag.value,
                        target_tag.value + target_tag.len);
    seed(middle_tag.value, middle_tag.len, std::move(middle_alias));
    seed(target_tag.value, target_tag.len,
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE),
          0, 1, 0x42});

    const auto summary = heap.collectUnreachableFrom({parent_reference});
    bool garbage_handle_invalidated = false;
    try {
      (void)heap.getArray(garbage_reference);
    } catch (jcvm::Exceptions) {
      garbage_handle_invalidated = true;
    }
    expect(summary.complete && summary.edge_load_failures == 0 &&
               summary.arrays_reachable == 2 &&
               summary.arrays_reclaimed == 1 &&
               heap.getArray(parent_reference) != nullptr &&
               garbage_handle_invalidated &&
               FlashMemory_Handler::getPersistentField_Type(edge_tag) ==
                   jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE,
           "nested_persistent_array_alias_chain_traces_to_target");
  }

  {
    jcvm::Heap heap(7);
    const auto first_alias = make_tag(0x81);
    const auto second_alias = make_tag(0x82);
    std::vector<uint8_t> first_record{0xfd, second_alias.len};
    first_record.insert(first_record.end(), second_alias.value,
                        second_alias.value + second_alias.len);
    seed(first_alias.value, first_alias.len, std::move(first_record));
    std::vector<uint8_t> second_record{0xfd, first_alias.len};
    second_record.insert(second_record.end(), first_alias.value,
                         first_alias.value + first_alias.len);
    seed(second_alias.value, second_alias.len, std::move(second_record));
    bool strict_cycle_rejected = false;
    try {
      (void)FlashMemory_Handler::getPersistentField_ReferenceStrict(first_alias,
                                                                      heap);
    } catch (jcvm::Exceptions) {
      strict_cycle_rejected = true;
    }
    expect(strict_cycle_rejected,
           "cyclic_array_alias_strict_restore_is_bounded");
  }

  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto instance_tag =
        FlashMemory_Handler::getAppletInstanceTag(1, 2, 3);
    const auto instance_reference = heap.addInstance(
        jcvm::JC_Instance(heap, 2, 3, instance_tag));
    const auto unreachable_reference =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    const auto malformed_field =
        FlashMemory_Handler::computeTag(instance_tag, 0);
    seed(malformed_field.value, malformed_field.len, {0xfe});
    const auto summary = heap.collectUnreachableFrom({instance_reference});
    expect(!summary.complete && summary.edge_load_failures == 1 &&
               summary.arrays_reclaimed == 0 &&
               heap.getInstance(instance_reference) != nullptr &&
               heap.getArray(unreachable_reference) != nullptr,
           "malformed_serialized_object_field_blocks_heap_reclamation");
    persistent_transaction_test::instance_field_count = 0;
  }

  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto instance_tag =
        FlashMemory_Handler::getAppletInstanceTag(4, 2, 3);
    const auto parent_reference = heap.addInstance(
        jcvm::JC_Instance(heap, 2, 3, instance_tag));
    const auto parent = heap.getInstance(parent_reference);
    const auto cached_fields = parent->getFields();
    const auto child_tag =
        FlashMemory_Handler::getAppletInstanceTag(4, 2, 4);
    const auto cached_child_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 4, child_tag));
    cached_fields->at(0).type = jcvm::FieldType::FIELD_TYPE_OBJECT;
    cached_fields->at(0).value =
        static_cast<jcvm::jword_t>(cached_child_reference.compact());
    heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);

    const auto summary = heap.collectUnreachableFrom({parent_reference});
    expect(summary.complete && summary.arrays_reclaimed == 1 &&
               heap.getInstance(parent_reference) != nullptr &&
               heap.getInstance(cached_child_reference) != nullptr &&
               summary.arrays_total == 1 && summary.arrays_reachable == 0,
           "loaded_persistent_cached_reference_survives_collection");
    persistent_transaction_test::instance_field_count = 0;
  }

  // Reconstruct heap handles over the same durable-record fixture, then let
  // strict traversal restore the array referenced by an alias after restart.
  {
    const auto root_tag = make_tag(0x7c);
    const auto target_tag = make_tag(0x7d);
    seed(root_tag.value, root_tag.len,
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_OBJECT),
          0, 1, 0xff, 0xff});
    seed(target_tag.value, target_tag.len,
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE),
          0, 1, 0x42});
    const auto element_tag = FlashMemory_Handler::computeTag(root_tag, 0);
    std::vector<uint8_t> array_alias{0xfd, target_tag.len};
    array_alias.insert(array_alias.end(), target_tag.value,
                       target_tag.value + target_tag.len);
    seed(element_tag.value, element_tag.len, std::move(array_alias));

    bool restart_restore_passed = false;
    try {
      jcvm::Heap restarted_heap(7);
      const auto restored_root =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root_tag, restarted_heap);
      const auto unreachable =
          restarted_heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
      const auto summary =
          restarted_heap.collectUnreachableFrom({restored_root});
      bool unreachable_handle_invalidated = false;
      try {
        (void)restarted_heap.getArray(unreachable);
      } catch (jcvm::Exceptions) {
        unreachable_handle_invalidated = true;
      }
      restart_restore_passed =
          summary.complete && summary.arrays_reachable == 2 &&
          summary.arrays_reclaimed == 1 &&
          restarted_heap.getArray(restored_root) != nullptr &&
          unreachable_handle_invalidated &&
          read(root_tag.value, root_tag.len).size() == 5 &&
          read(target_tag.value, target_tag.len).size() == 4;
    } catch (jcvm::Exceptions) {
      restart_restore_passed = false;
    }
    expect(restart_restore_passed,
           "recreated_heap_restores_alias_graph_and_reclaims_only_garbage");
  }

  // Bulk namespace deletion may fail after its backend has erased a real
  // record. The snapshot preflight must let the enclosing transaction restore
  // both the erased record and records not reached by the failing operation.
  {
    jcvm::fs::Tag prefix{};
    prefix.len = 2;
    prefix.value[0] = 0xa2;
    prefix.value[1] = 0x10;
    jcvm::fs::Tag first = prefix;
    first.value[first.len++] = 0x01;
    jcvm::fs::Tag second = prefix;
    second.value[second.len++] = 0x02;
    const std::vector<uint8_t> first_value{0x11, 0x12};
    const std::vector<uint8_t> second_value{0x21, 0x22};
    persistent_transaction_test::seed(first.value, first.len, first_value);
    persistent_transaction_test::seed(second.value, second.len, second_value);

    bool began = FlashMemory_Handler::beginPersistentWriteTransaction();
    persistent_transaction_test::fail_prefix_erase_after_one();
    const bool erase_rejected = began &&
        !FlashMemory_Handler::erasePersistentPrefix(prefix);
    const bool rolled_back =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    expect(erase_rejected && rolled_back &&
               persistent_transaction_test::read(first.value, first.len) ==
                   first_value &&
               persistent_transaction_test::read(second.value, second.len) ==
                   second_value,
           "failed_prefix_delete_restores_all_preimage_records");
  }

  {
    const uint8_t package_id = 58;
    const uint8_t static_tag[] = {0x02, package_id, 0x01};
    const uint8_t applet_tag[] = {0x03, 0x07, package_id, 0x02};
    const uint8_t lifecycle_tag[] = {0x02, 0xff, package_id, 0x01};
    const uint8_t unrelated_tag[] = {0x02, 59, 0x01};
    const std::vector<uint8_t> static_value{0x31};
    const std::vector<uint8_t> applet_value{0x32};
    const std::vector<uint8_t> lifecycle_value{0x33};
    const std::vector<uint8_t> unrelated_value{0x34};
    persistent_transaction_test::seed(static_tag, sizeof(static_tag), static_value);
    persistent_transaction_test::seed(applet_tag, sizeof(applet_tag), applet_value);
    persistent_transaction_test::seed(lifecycle_tag, sizeof(lifecycle_tag),
                                      lifecycle_value);
    persistent_transaction_test::seed(unrelated_tag, sizeof(unrelated_tag),
                                      unrelated_value);
    const bool began = FlashMemory_Handler::beginPersistentWriteTransaction();
    persistent_transaction_test::fail_package_state_erase_after_one();
    const bool erase_rejected =
        began && !FlashMemory_Handler::erasePersistentPackageState(package_id);
    const bool rolled_back =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    expect(erase_rejected && rolled_back &&
               persistent_transaction_test::read(static_tag, sizeof(static_tag)) ==
                   static_value &&
               persistent_transaction_test::read(applet_tag, sizeof(applet_tag)) ==
                   applet_value &&
               persistent_transaction_test::read(lifecycle_tag,
                                                 sizeof(lifecycle_tag)) ==
                   lifecycle_value &&
               persistent_transaction_test::read(unrelated_tag,
                                                 sizeof(unrelated_tag)) ==
                   unrelated_value,
           "failed_package_state_delete_restores_matching_records_only");
  }

  // Static roots cached during a transaction must not survive rollback.
  // The restored value should resolve to the canonical pre-transaction handle.
  {
    jcvm::Heap heap(7);
    const auto field = FlashMemory_Handler::getStaticFieldTag(22, 1);
    const auto original = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(original)->setByteEntry(0, 0x55);
    FlashMemory_Handler::setPersistentField_Reference(field, original, heap);
    heap.setStaticReferenceRoot(22, 1, original);

    const bool began = FlashMemory_Handler::beginPersistentWriteTransaction();
    if (began) {
      FlashMemory_Handler::setPersistentField_Reference(field, {}, heap);
      heap.setStaticReferenceRoot(22, 1, {});
    }
    const bool aborted = FlashMemory_Handler::rollbackPersistentWriteTransaction();

    jcvm::jref_t cached_after_abort;
    const bool stale_cache_was_invalidated =
        !heap.getStaticReferenceRoot(22, 1, cached_after_abort);
    bool restored_canonical_reference = false;
    try {
      const auto restored = FlashMemory_Handler::getPersistentField_ReferenceStrict(
          field, heap);
      restored_canonical_reference = restored == original &&
          heap.getArray(restored)->getByteEntry(0) == 0x55;
      heap.setStaticReferenceRoot(22, 1, restored);
    } catch (jcvm::Exceptions) {
      restored_canonical_reference = false;
    }
    expect(began && aborted && stale_cache_was_invalidated,
           "static_reference_cache_invalidated_after_transaction_abort");
    expect(restored_canonical_reference,
           "static_reference_abort_reload_preserves_canonical_handle");

    const auto replacement_instance_tag =
        FlashMemory_Handler::getAppletInstanceTag(22, 2, 3);
    const auto replacement_instance = heap.addInstance(
        jcvm::JC_Instance(heap, 2, 3, replacement_instance_tag));
    bool failed_commit_invalidates_cache =
        FlashMemory_Handler::beginPersistentWriteTransaction();
    if (failed_commit_invalidates_cache) {
      FlashMemory_Handler::setPersistentField_Reference(
          field, replacement_instance, heap);
      heap.setStaticReferenceRoot(22, 1, replacement_instance);
      persistent_transaction_test::fail_one_sync();
      failed_commit_invalidates_cache =
          !FlashMemory_Handler::commitPersistentWriteTransaction();
    }
    jcvm::jref_t cached_after_failed_commit;
    failed_commit_invalidates_cache = failed_commit_invalidates_cache &&
        !heap.getStaticReferenceRoot(22, 1, cached_after_failed_commit);
    bool failed_commit_restores_canonical_array = false;
    try {
      const auto restored = FlashMemory_Handler::getPersistentField_ReferenceStrict(
          field, heap);
      failed_commit_restores_canonical_array = restored == original &&
          heap.getArray(restored)->getByteEntry(0) == 0x55;
      heap.setStaticReferenceRoot(22, 1, restored);
    } catch (jcvm::Exceptions) {
      failed_commit_restores_canonical_array = false;
    }
    expect(failed_commit_invalidates_cache,
           "static_reference_cache_invalidated_after_failed_commit");
    expect(failed_commit_restores_canonical_array,
           "failed_static_reference_commit_restores_canonical_array");

    std::vector<jcvm::jref_t> static_roots;
    heap.appendRootReferences(static_roots);
    auto collected = heap.collectUnreachableFrom(static_roots);
    expect(collected.complete && collected.arrays_reachable == 1 &&
               collected.arrays_reclaimed == 0,
           "restored_static_reference_is_a_gc_root_after_abort");
  }

  // Retarget the original field after another field aliases its array. The
  // first field's new array body must get a distinct persistent identity;
  // clearing and aborting replacements must not change the old alias.
  {
    jcvm::Heap heap(7);
    const auto first_field = FlashMemory_Handler::getStaticFieldTag(20, 1);
    const auto alias_field = FlashMemory_Handler::getStaticFieldTag(20, 2);
    const auto old_array = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    const auto replacement = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(old_array)->setByteEntry(0, 0x11);
    heap.getArray(replacement)->setByteEntry(0, 0x22);

    FlashMemory_Handler::setPersistentField_Reference(first_field, old_array,
                                                        heap);
    FlashMemory_Handler::setPersistentField_Reference(alias_field, old_array,
                                                        heap);
    const auto old_alias_record =
        persistent_transaction_test::read(alias_field.value, alias_field.len);
    const auto old_body = [&]() {
      if (old_alias_record.size() < 3 || old_alias_record[0] != 0xfd ||
          static_cast<size_t>(old_alias_record[1]) + 2u !=
              old_alias_record.size()) {
        return std::vector<uint8_t>{};
      }
      return std::vector<uint8_t>(old_alias_record.begin() + 2,
                                  old_alias_record.end());
    }();

    FlashMemory_Handler::setPersistentField_Reference(first_field, {}, heap);
    FlashMemory_Handler::setPersistentField_Reference(first_field, replacement,
                                                        heap);
    const auto replacement_record =
        persistent_transaction_test::read(first_field.value, first_field.len);
    const std::vector<uint8_t> replacement_body =
        replacement_record.size() >= 3 && replacement_record[0] == 0xfd &&
                static_cast<size_t>(replacement_record[1]) + 2u ==
                    replacement_record.size()
            ? std::vector<uint8_t>(replacement_record.begin() + 2,
                                   replacement_record.end())
            : std::vector<uint8_t>{};

    bool aliases_survive_fresh_heap = false;
    bool collector_preserves_both = false;
    bool collector_keeps_old_alias = false;
    try {
      jcvm::Heap restored(7);
      const auto restored_old =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(alias_field,
                                                                     restored);
      const auto restored_new =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(first_field,
                                                                     restored);
      aliases_survive_fresh_heap =
          !old_body.empty() && !replacement_body.empty() &&
          old_body != replacement_body &&
          restored.getArray(restored_old)->getByteEntry(0) == 0x11 &&
          restored.getArray(restored_new)->getByteEntry(0) == 0x22;
      const auto both_reachable =
          restored.collectUnreachableFrom({restored_old, restored_new});
      collector_preserves_both = both_reachable.complete &&
          both_reachable.arrays_reachable == 2 &&
          both_reachable.arrays_reclaimed == 0;
      const auto old_only = restored.collectUnreachableFrom({restored_old});
      collector_keeps_old_alias = old_only.complete &&
          old_only.arrays_reachable == 1 && old_only.arrays_reclaimed == 1 &&
          restored.getArray(restored_old)->getByteEntry(0) == 0x11;
    } catch (jcvm::Exceptions) {
      aliases_survive_fresh_heap = false;
      collector_preserves_both = false;
      collector_keeps_old_alias = false;
    }
    expect(aliases_survive_fresh_heap,
           "persistent_array_reassignment_preserves_old_alias_after_restore");
    expect(collector_preserves_both && collector_keeps_old_alias,
           "persistent_array_identity_survives_collection");

    const auto transaction_array = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(transaction_array)->setByteEntry(0, 0x33);
    const bool began = FlashMemory_Handler::beginPersistentWriteTransaction();
    if (began) {
      FlashMemory_Handler::setPersistentField_Reference(
          first_field, transaction_array, heap);
    }
    const bool aborted = FlashMemory_Handler::rollbackPersistentWriteTransaction();
    bool abort_restores_both_fields = false;
    try {
      jcvm::Heap after_abort(7);
      const auto after_abort_old =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(alias_field,
                                                                     after_abort);
      const auto after_abort_new =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(first_field,
                                                                     after_abort);
      abort_restores_both_fields =
          after_abort.getArray(after_abort_old)->getByteEntry(0) == 0x11 &&
          after_abort.getArray(after_abort_new)->getByteEntry(0) == 0x22;
    } catch (jcvm::Exceptions) {
      abort_restores_both_fields = false;
    }
    expect(began && aborted && abort_restores_both_fields,
           "persistent_array_retarget_abort_preserves_prior_identities");

    const auto after_abort_array = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(after_abort_array)->setByteEntry(0, 0x44);
    FlashMemory_Handler::setPersistentField_Reference(first_field,
                                                        after_abort_array, heap);
    const auto after_abort_record = persistent_transaction_test::read(
        first_field.value, first_field.len);
    const std::vector<uint8_t> after_abort_body =
        after_abort_record.size() >= 3 && after_abort_record[0] == 0xfd &&
                static_cast<size_t>(after_abort_record[1]) + 2u ==
                    after_abort_record.size()
            ? std::vector<uint8_t>(after_abort_record.begin() + 2,
                                   after_abort_record.end())
            : std::vector<uint8_t>{};
    expect(!after_abort_body.empty() && after_abort_body != old_body &&
               after_abort_body != replacement_body,
           "persistent_array_identity_counter_survives_transaction_abort");
    constexpr uint8_t identity_counter_tag[] = {
        0x02, 0xfe, 'J', 'C', 'V', 'M', 'A', 'R', 'R', 'I', 'D'};
    const auto identity_counter = persistent_transaction_test::read(
        identity_counter_tag, sizeof(identity_counter_tag));
    expect(identity_counter.size() == 5 && identity_counter[0] == 1 &&
               identity_counter[4] != 0,
           "persistent_array_identity_counter_uses_valid_static_tag");
  }

  {
    constexpr uint8_t owner = 0x21;
    const uint8_t root_tag[] = {0x03, owner, 0x44, 0x55, 0xff};
    const uint8_t target_tag[] = {0x03, owner, 0x66, 0x77};
    const uint8_t orphan_tag[] = {0x03, owner, 0x88, 0x99};
    const uint8_t orphan_under_root_tag[] = {
        0x03, owner, 0x44, 0x55, 0xff, 0x00, 0x00, 0xff, 0xff};
    const uint8_t other_owner_tag[] = {0x03, 0x22, 0x11, 0x33};
    seed(root_tag, sizeof(root_tag),
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_OBJECT),
          0, 1, 0xff, 0xff});
    seed(target_tag, sizeof(target_tag),
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE),
          0, 1, 0x42});
    jcvm::fs::Tag serialized_root{};
    serialized_root.len = sizeof(root_tag);
    std::memcpy(serialized_root.value, root_tag, sizeof(root_tag));
    const auto element_tag =
        FlashMemory_Handler::computeTag(serialized_root, 0);
    std::vector<uint8_t> element_alias{0xfd, sizeof(target_tag)};
    element_alias.insert(element_alias.end(), target_tag,
                         target_tag + sizeof(target_tag));
    seed(element_tag.value, element_tag.len, std::move(element_alias));
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x19));
    seed(orphan_under_root_tag, sizeof(orphan_under_root_tag),
         byte_record(0x1a));
    seed(other_owner_tag, sizeof(other_owner_tag), byte_record(0x29));

    bool inventory_passed = false;
    try {
      jcvm::Heap heap(owner);
      jcvm::fs::Tag serialized_root{};
      serialized_root.len = sizeof(root_tag);
      std::memcpy(serialized_root.value, root_tag, sizeof(root_tag));
      const auto root_reference =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(
              serialized_root, heap);
      CandidateTags candidates;
      const auto result = jcvm::PersistentInventory::visitUnreachableAppletTags(
          owner, heap, std::vector<jcvm::jref_t>{root_reference},
          collect_candidate, &candidates);
      const std::vector<uint8_t> orphan_bytes(
          orphan_tag, orphan_tag + sizeof(orphan_tag));
      const std::vector<uint8_t> nested_orphan_bytes(
          orphan_under_root_tag,
          orphan_under_root_tag + sizeof(orphan_under_root_tag));
      inventory_passed =
          result == jcvm::PersistentInventory::VisitResult::Complete &&
          candidates.tags.size() == 2 &&
          std::find(candidates.tags.begin(), candidates.tags.end(),
                    orphan_bytes) != candidates.tags.end() &&
          std::find(candidates.tags.begin(), candidates.tags.end(),
                    nested_orphan_bytes) != candidates.tags.end() &&
          !persistent_transaction_test::read(root_tag, sizeof(root_tag)).empty() &&
          !persistent_transaction_test::read(target_tag, sizeof(target_tag)).empty() &&
          !persistent_transaction_test::read(orphan_tag, sizeof(orphan_tag)).empty() &&
          !persistent_transaction_test::read(other_owner_tag,
                                               sizeof(other_owner_tag)).empty();
    } catch (jcvm::Exceptions) {
      inventory_passed = false;
    }
    expect(inventory_passed,
           "read_only_inventory_excludes_reachable_alias_subtree_and_other_owners");
  }

  {
    constexpr uint8_t owner = 0x23;
    const uint8_t root_tag[] = {0x03, owner, 0x44, 0x55, 0xff};
    const uint8_t orphan_tag[] = {0x03, owner, 0x88, 0x99};
    seed(root_tag, sizeof(root_tag),
         {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_OBJECT),
          0, 1, 0xff, 0xff});
    jcvm::fs::Tag serialized_root{};
    serialized_root.len = sizeof(root_tag);
    std::memcpy(serialized_root.value, root_tag, sizeof(root_tag));
    const auto edge_tag = FlashMemory_Handler::computeTag(serialized_root, 0);
    seed(edge_tag.value, edge_tag.len, {0xfe});
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x19));

    bool failed_closed = false;
    try {
      jcvm::Heap heap(owner);
      const auto root_reference =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(
              serialized_root, heap);
      CandidateTags candidates;
      const auto result = jcvm::PersistentInventory::visitUnreachableAppletTags(
          owner, heap, std::vector<jcvm::jref_t>{root_reference},
          collect_candidate, &candidates);
      failed_closed =
          result == jcvm::PersistentInventory::VisitResult::IncompleteReachability &&
          candidates.tags.empty() &&
          !persistent_transaction_test::read(root_tag, sizeof(root_tag)).empty() &&
          !persistent_transaction_test::read(orphan_tag,
                                               sizeof(orphan_tag)).empty();
      const std::vector<jcvm::jref_t> roots{root_reference};
      RootVectorContext root_context{&roots};
      uint32_t erased = 99;
      const auto reclaim_result =
          jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
              owner, heap, enumerate_root_vector, &root_context, &erased);
      failed_closed =
          failed_closed &&
          reclaim_result ==
              jcvm::PersistentInventory::VisitResult::IncompleteReachability &&
          erased == 0 &&
          !persistent_transaction_test::read(root_tag,
                                               sizeof(root_tag)).empty() &&
          !persistent_transaction_test::read(orphan_tag,
                                               sizeof(orphan_tag)).empty();
    } catch (jcvm::Exceptions) {
      failed_closed = false;
    }
    expect(failed_closed,
           "incomplete_reachability_reports_no_persistent_inventory_candidates");
  }

  {
    constexpr uint8_t owner = 0x31;
    const uint8_t first_tag[] = {0x03, owner, 0x10, 0x01};
    const uint8_t second_tag[] = {0x03, owner, 0x10, 0x02};
    seed(first_tag, sizeof(first_tag), byte_record(0x11));
    seed(second_tag, sizeof(second_tag), byte_record(0x22));
    persistent_transaction_test::force_cursor_stall_after_one_result();

    jcvm::Heap heap(owner);
    CandidateTags candidates;
    const auto result = jcvm::PersistentInventory::visitUnreachableAppletTags(
        owner, heap, std::vector<jcvm::jref_t>{}, collect_candidate,
        &candidates);
    expect(result == jcvm::PersistentInventory::VisitResult::InvalidCursor &&
               candidates.tags.size() == 1 &&
               !persistent_transaction_test::read(first_tag,
                                                    sizeof(first_tag)).empty() &&
               !persistent_transaction_test::read(second_tag,
                                                    sizeof(second_tag)).empty(),
           "cursor_failure_marks_streamed_candidates_incomplete_without_mutation");
  }

  {
    constexpr uint8_t owner = 0x41;
    const uint8_t root_tag_bytes[] = {0x03, owner, 0x10, 0x01};
    const uint8_t body_tag_bytes[] = {0x03, owner, 0x10, 0x01, 0xff, 0xff};
    const uint8_t nested_tag_bytes[] = {
        0x03, owner, 0x10, 0x01, 0x00, 0x01};
    const uint8_t orphan_tag_bytes[] = {0x03, owner, 0x20, 0x01};
    const uint8_t other_owner_tag_bytes[] = {0x03, 0x42, 0x20, 0x01};
    const uint8_t static_tag_bytes[] = {0x02, owner, 0x00};
    seed(root_tag_bytes, sizeof(root_tag_bytes), byte_record(0x01));
    seed(body_tag_bytes, sizeof(body_tag_bytes), byte_record(0x02));
    seed(nested_tag_bytes, sizeof(nested_tag_bytes), byte_record(0x03));
    seed(orphan_tag_bytes, sizeof(orphan_tag_bytes), byte_record(0x04));
    seed(other_owner_tag_bytes, sizeof(other_owner_tag_bytes), byte_record(0x05));
    seed(static_tag_bytes, sizeof(static_tag_bytes), byte_record(0x06));

    jcvm::Heap heap(owner);
    const jcvm::jref_t root = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    jcvm::fs::Tag root_tag{};
    root_tag.len = sizeof(root_tag_bytes);
    std::memcpy(root_tag.value, root_tag_bytes, sizeof(root_tag_bytes));
    heap.getArray(root)->setPersistentTag(root_tag);
    const std::vector<jcvm::jref_t> root_references{root};
    RootVectorContext roots{&root_references};
    uint32_t erased = 0;
    const auto result = jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
        owner, heap, enumerate_root_vector, &roots, &erased);
    expect(result == jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 1 && read(root_tag_bytes, sizeof(root_tag_bytes)) ==
                                  byte_record(0x01) &&
               read(body_tag_bytes, sizeof(body_tag_bytes)) == byte_record(0x02) &&
               read(nested_tag_bytes, sizeof(nested_tag_bytes)) ==
                   byte_record(0x03) &&
               read(orphan_tag_bytes, sizeof(orphan_tag_bytes)).empty() &&
               read(other_owner_tag_bytes, sizeof(other_owner_tag_bytes)) ==
                   byte_record(0x05) &&
               read(static_tag_bytes, sizeof(static_tag_bytes)) ==
                   byte_record(0x06),
           "reclaimer_deletes_only_unreachable_owner_tags_and_preserves_subtrees");
  }

  {
    constexpr uint8_t owner = 0x42;
    const uint8_t first_tag[] = {0x03, owner, 0x20, 0x01};
    const uint8_t second_tag[] = {0x03, owner, 0x20, 0x02};
    seed(first_tag, sizeof(first_tag), byte_record(0x21));
    seed(second_tag, sizeof(second_tag), byte_record(0x22));
    persistent_transaction_test::force_cursor_stall_after_one_result();
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    jcvm::Heap heap(owner);
    uint32_t erased = 99;
    const auto result = jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
        owner, heap, enumerate_root_vector, &roots, &erased);
    expect(result == jcvm::PersistentInventory::VisitResult::InvalidCursor &&
               erased == 0 && read(first_tag, sizeof(first_tag)) ==
                                  byte_record(0x21) &&
               read(second_tag, sizeof(second_tag)) == byte_record(0x22),
           "reclaimer_cursor_preflight_fails_closed_without_deletion");
  }

  {
    constexpr uint8_t owner = 0x43;
    const uint8_t orphan_tag[] = {0x03, owner, 0x20, 0x01};
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x11));
    jcvm::Heap heap(owner);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
           "reclaimer_guard_begin_transaction");
    uint32_t erased = 99;
    const auto result = jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
        owner, heap, enumerate_root_vector, &roots, &erased);
    expect(result == jcvm::PersistentInventory::VisitResult::TransactionActive &&
               erased == 0 && read(orphan_tag, sizeof(orphan_tag)) ==
                                  byte_record(0x11),
           "reclaimer_refuses_mutation_during_java_card_transaction");
    expect(FlashMemory_Handler::rollbackPersistentWriteTransaction(),
           "reclaimer_guard_rollback_transaction");
  }

  {
    constexpr uint8_t owner = 0x44;
    std::array<std::array<uint8_t, 4>, 10> orphan_tags{};
    for (uint8_t index = 0; index < orphan_tags.size(); ++index) {
      orphan_tags[index] = {0x03, owner, 0x30, index};
      seed(orphan_tags[index].data(), orphan_tags[index].size(),
           byte_record(index));
    }
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    jcvm::Heap first_heap(owner);
    persistent_transaction_test::fail_erase_after(2);
    uint32_t erased = 0;
    const auto interrupted =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, first_heap, enumerate_root_vector, &roots, &erased);
    bool retained_after_interruption = interrupted ==
                                           jcvm::PersistentInventory::VisitResult::EraseFailure &&
                                       erased == 2;
    for (uint8_t index = 0; index < orphan_tags.size(); ++index) {
      const bool exists =
          !read(orphan_tags[index].data(), orphan_tags[index].size()).empty();
      retained_after_interruption &= exists == (index >= 2);
    }

    // A fresh heap models reboot: the incomplete pass removed only proven
    // garbage, and a later full scan can safely finish reclaiming the rest.
    jcvm::Heap recovered_heap(owner);
    uint32_t recovered_erased = 0;
    const auto recovered =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, recovered_heap, enumerate_root_vector, &roots,
            &recovered_erased);
    bool all_reclaimed = recovered ==
                             jcvm::PersistentInventory::VisitResult::Complete &&
                         recovered_erased == 8;
    for (const auto &tag : orphan_tags) {
      all_reclaimed &= read(tag.data(), tag.size()).empty();
    }
    expect(retained_after_interruption,
           "reclaimer_interruption_keeps_only_unprocessed_orphans");
    expect(all_reclaimed,
           "reclaimer_retry_after_restart_finishes_remaining_orphans");
  }

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

  const auto transaction_owner_tag = make_tag(6);
  const std::vector<uint8_t> transaction_owner_original{0x31, 0x41};
  seed(transaction_owner_tag.value, transaction_owner_tag.len,
       transaction_owner_original);
  bool internal_transaction_isolated =
      FlashMemory_Handler::beginPersistentWriteTransaction() &&
      FlashMemory_Handler::writePersistentRecord(
          transaction_owner_tag, journal_record, sizeof(journal_record)) &&
      !FlashMemory_Handler::appletWriteTransactionActive() &&
      FlashMemory_Handler::abortAppletWriteTransaction() &&
      FlashMemory_Handler::persistentWriteTransactionActive() &&
      read(transaction_owner_tag.value, transaction_owner_tag.len) ==
          std::vector<uint8_t>(journal_record,
                               journal_record + sizeof(journal_record));
  if (FlashMemory_Handler::persistentWriteTransactionActive()) {
    internal_transaction_isolated =
        FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
        internal_transaction_isolated;
  }
  internal_transaction_isolated = internal_transaction_isolated &&
      read(transaction_owner_tag.value, transaction_owner_tag.len) ==
          transaction_owner_original;
  expect(internal_transaction_isolated,
         "applet_abort_guard_does_not_rollback_runtime_transaction");

  const bool applet_transaction_aborted =
      FlashMemory_Handler::beginAppletWriteTransaction() &&
      FlashMemory_Handler::appletWriteTransactionActive() &&
      FlashMemory_Handler::writePersistentRecord(
          transaction_owner_tag, journal_record, sizeof(journal_record)) &&
      FlashMemory_Handler::abortAppletWriteTransaction() &&
      !FlashMemory_Handler::persistentWriteTransactionActive() &&
      !FlashMemory_Handler::appletWriteTransactionActive() &&
      read(transaction_owner_tag.value, transaction_owner_tag.len) ==
          transaction_owner_original;
  expect(applet_transaction_aborted,
         "applet_transaction_guard_rolls_back_owned_transaction");

  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
