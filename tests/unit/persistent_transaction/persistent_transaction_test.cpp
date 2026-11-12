#include "jc_handlers/flashmemory.hpp"
#include "heap.hpp"
#include "context.hpp"
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
void reset_counted_fs_calls();
unsigned int counted_fs_call_count();
void force_cursor_stall_after_one_result(uint8_t applet_owner);
void fail_erase_after(unsigned int successful_erases);
void fail_prefix_erase_after_one();
void fail_package_state_erase_after_one();
extern uint16_t instance_field_count;
std::vector<uint8_t> read(const uint8_t *tag, uint8_t length);
std::vector<uint8_t> read_durable(const uint8_t *tag, uint8_t length);
void seed(const uint8_t *tag, uint8_t tag_length, std::vector<uint8_t> value);
void fail_one_write();
void fail_write_after(unsigned int successful_writes);
std::vector<uint8_t> failed_write_tag();
void fail_one_read();
void fail_one_sync();
void fail_sync_times(unsigned int count);
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
  {
    constexpr uint8_t aid[] = {0xA0, 0x00, 0x00, 0x00, 0x62};
    jcvm::Context first_context(0x71, 0x31);
    std::vector<jcvm::jref_t> initial_roots;
    const bool initial_package_context_is_unique =
        first_context.appendRetainedRootsForPackage(0x31, initial_roots);
    bool peer_package_context_is_supported = false;
    {
      jcvm::Context second_context(0x72, 0x32);
      std::vector<jcvm::jref_t> roots_with_peer;
      peer_package_context_is_supported =
          first_context.appendRetainedRootsForPackage(0x31, roots_with_peer);
    }
    const auto original_applet_root = first_context.getHeap().addArray(
        1, jcvm::jc_array_type::JAVA_ARRAY_T_BYTE);
    first_context.markAppletRegistered(original_applet_root);
    const bool sibling_applet_accepted =
        first_context.activateAppletInstance(0x73, aid, sizeof(aid));
    const auto sibling_applet_root = first_context.getHeap().addArray(
        1, jcvm::jc_array_type::JAVA_ARRAY_T_BYTE);
    first_context.markAppletRegistered(sibling_applet_root);
    std::vector<jcvm::jref_t> sibling_roots;
    first_context.appendRootReferences(sibling_roots);
    const auto contains_root = [&sibling_roots](jcvm::jref_t root) {
      return std::find_if(sibling_roots.begin(), sibling_roots.end(),
                          [root](jcvm::jref_t candidate) {
                            return candidate.compact() == root.compact();
                          }) != sibling_roots.end();
    };
    const bool sibling_applet_roots_are_enumerated =
        contains_root(original_applet_root) && contains_root(sibling_applet_root);
    expect(initial_package_context_is_unique &&
               peer_package_context_is_supported && sibling_applet_accepted &&
               sibling_applet_roots_are_enumerated,
           "persistent_deletion_roots_sibling_applets_with_peer_package_context");
  }

  {
    constexpr uint8_t server_aid[] = {0xA0, 0x00, 0x00, 0x00, 0x63};
    jcvm::Context server_context(0x31, 0x41);
    jcvm::Context client_context(0x32, 0x42);
    const auto server_object = server_context.getHeap().addInstance(
        jcvm::JC_Instance(server_context.getHeap(), 0x41, 7));
    const auto cached_static_object = server_context.getHeap().addInstance(
        jcvm::JC_Instance(server_context.getHeap(), 0x41, 8));
    jcvm::jref_t proxy_object;
    proxy_object.setOffset(3);
    proxy_object.setAsInstance(true);
    const bool proxy_registered = client_context.registerShareableProxy(
        proxy_object, server_aid, sizeof(server_aid), server_object, 0x41);
    client_context.getHeap().setStaticReferenceRoot(0x42, 2,
                                                     cached_static_object,
                                                     0x41);
    std::vector<jcvm::jref_t> roots;
    const bool roots_complete =
        server_context.appendRetainedRootsForPackage(0x41, roots);
    const auto summary = server_context.getHeap().traceReachableFrom(roots);
    expect(proxy_registered && roots_complete && summary.complete &&
               summary.instances_total == 2 &&
               summary.instances_reachable == 2,
           "persistent_deletion_roots_include_peer_shareable_proxy");
  }

  {
    jcvm::Context server_context(0x31, 0x41);
    jcvm::Context client_context(0x32, 0x42);
    const uint8_t proxy_tag_bytes[] = {0x03, 0x31, 0x10, 0x01};
    const uint8_t static_tag_bytes[] = {0x03, 0x31, 0x10, 0x02};
    const uint8_t orphan_tag_bytes[] = {0x03, 0x31, 0x10, 0x03};
    jcvm::fs::Tag proxy_tag{};
    proxy_tag.len = sizeof(proxy_tag_bytes);
    std::memcpy(proxy_tag.value, proxy_tag_bytes, sizeof(proxy_tag_bytes));
    jcvm::fs::Tag static_tag{};
    static_tag.len = sizeof(static_tag_bytes);
    std::memcpy(static_tag.value, static_tag_bytes, sizeof(static_tag_bytes));
    const auto second_static_object = server_context.getHeap().addArray(
        1, jcvm::jc_array_type::JAVA_ARRAY_T_BYTE);
    const auto cached_static_object = server_context.getHeap().addArray(
        1, jcvm::jc_array_type::JAVA_ARRAY_T_BYTE);
    const auto orphan_object = server_context.getHeap().addArray(
        1, jcvm::jc_array_type::JAVA_ARRAY_T_BYTE);
    jcvm::fs::Tag orphan_tag{};
    orphan_tag.len = sizeof(orphan_tag_bytes);
    std::memcpy(orphan_tag.value, orphan_tag_bytes, sizeof(orphan_tag_bytes));
    jcvm::FlashMemory_Handler::setPersistentField_Array(
        static_tag, *server_context.getHeap().getArray(cached_static_object),
        server_context.getHeap());
    jcvm::FlashMemory_Handler::setPersistentField_Array(
        proxy_tag, *server_context.getHeap().getArray(second_static_object),
        server_context.getHeap());
    jcvm::FlashMemory_Handler::setPersistentField_Array(
        orphan_tag, *server_context.getHeap().getArray(orphan_object),
        server_context.getHeap());
    client_context.getHeap().setStaticReferenceRoot(0x42, 2,
                                                     cached_static_object,
                                                     0x41);
    client_context.getHeap().setStaticReferenceRoot(0x42, 3,
                                                     second_static_object,
                                                     0x41);
    std::vector<jcvm::jref_t> roots;
    const bool roots_complete =
        server_context.appendRetainedRootsForPackage(0x41, roots);
    const auto summary = server_context.getHeap().traceReachableFrom(roots);
    RootVectorContext root_vector{&roots};
    uint32_t erased = 0;
    const auto reclaim_result =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            0x31, server_context.getHeap(), enumerate_root_vector, &root_vector,
            &erased);
    const bool rooted_records_survive_reclaim =
        reclaim_result == jcvm::PersistentInventory::VisitResult::Complete &&
        erased == 1 &&
        !persistent_transaction_test::read(proxy_tag_bytes,
                                           sizeof(proxy_tag_bytes)).empty() &&
        !persistent_transaction_test::read(static_tag_bytes,
                                           sizeof(static_tag_bytes)).empty() &&
        persistent_transaction_test::read(orphan_tag_bytes,
                                           sizeof(orphan_tag_bytes)).empty();
    bool duplicate_target_context_fails_closed = false;
    {
      jcvm::Context duplicate_server_context(0x33, 0x41);
      std::vector<jcvm::jref_t> ambiguous_roots;
      duplicate_target_context_fails_closed =
          !server_context.appendRetainedRootsForPackage(
              0x41, ambiguous_roots) &&
          ambiguous_roots.empty();
    }
    expect(roots_complete &&
               summary.complete && summary.arrays_total == 3 &&
               summary.arrays_reachable == 2 &&
               rooted_records_survive_reclaim &&
               duplicate_target_context_fails_closed,
           "persistent_deletion_reclaimer_preserves_peer_static_roots_and_rejects_ambiguous_heap");
  }

  {
    constexpr uint8_t server_aid[] = {0xA0, 0x00, 0x00, 0x00, 0x64};
    constexpr uint8_t owner = 0x6d;
    constexpr uint8_t server_package = 0x6e;
    constexpr uint8_t client_package = 0x70;
    const uint8_t server_tag_bytes[] = {0x03, owner, 0x20, 0x01};
    const uint8_t remote_tag_bytes[] = {0x03, owner, 0x20, 0x02};
    const uint8_t orphan_tag_bytes[] = {0x03, owner, 0x20, 0x03};
    jcvm::fs::Tag server_tag{};
    server_tag.len = sizeof(server_tag_bytes);
    std::memcpy(server_tag.value, server_tag_bytes, sizeof(server_tag_bytes));
    jcvm::fs::Tag orphan_tag{};
    orphan_tag.len = sizeof(orphan_tag_bytes);
    std::memcpy(orphan_tag.value, orphan_tag_bytes, sizeof(orphan_tag_bytes));
    jcvm::fs::Tag remote_tag{};
    remote_tag.len = sizeof(remote_tag_bytes);
    std::memcpy(remote_tag.value, remote_tag_bytes, sizeof(remote_tag_bytes));
    jcvm::Context server_context(owner, server_package);
    jcvm::Context client_context(0x6f, client_package);
    const uint16_t previous_field_count =
        persistent_transaction_test::instance_field_count;
    persistent_transaction_test::instance_field_count = 0;
    const auto server_reference = server_context.getHeap().addInstance(
        jcvm::JC_Instance(server_context.getHeap(), server_package, 7));
    const auto orphan_reference = server_context.getHeap().addInstance(
        jcvm::JC_Instance(server_context.getHeap(), server_package, 8));
    const auto remote_reference = server_context.getHeap().addInstance(
        jcvm::JC_Instance(server_context.getHeap(), server_package, 9));
    const auto server_object =
        server_context.getHeap().getInstance(server_reference);
    const auto orphan_object =
        server_context.getHeap().getInstance(orphan_reference);
    const auto remote_object =
        server_context.getHeap().getInstance(remote_reference);
    jcvm::FlashMemory_Handler::setPersistentField_Instance(
        server_tag, *server_object, server_context.getHeap());
    jcvm::FlashMemory_Handler::setPersistentField_Instance(
        orphan_tag, *orphan_object, server_context.getHeap());
    jcvm::FlashMemory_Handler::setPersistentField_Instance(
        remote_tag, *remote_object, server_context.getHeap());
    const uint16_t remote_object_id =
        server_context.exportRemoteObject(remote_reference);
    jcvm::jref_t proxy_handle;
    proxy_handle.setOffset(3);
    proxy_handle.setAsInstance(true);
    const bool proxy_registered = client_context.registerShareableProxy(
        proxy_handle, server_aid, sizeof(server_aid), server_reference,
        server_package);
    std::vector<jcvm::jref_t> roots;
    const bool roots_complete =
        server_context.appendRetainedRootsForPackage(server_package, roots);
    server_context.appendRootReferences(roots);
    const auto root_summary =
        server_context.getHeap().traceReachableFrom(roots);
    RootVectorContext root_vector{&roots};
    uint32_t erased = 0;
    const auto reclaim_result =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, server_context.getHeap(), enumerate_root_vector, &root_vector,
            &erased);
    const auto server_body_tag = server_object->getPersistentTag();
    const auto remote_body_tag = remote_object->getPersistentTag();
    const auto orphan_body_tag = orphan_object->getPersistentTag();
    const bool proxy_root_survives_reclaim =
        reclaim_result == jcvm::PersistentInventory::VisitResult::Complete &&
        erased > 0 && root_summary.complete &&
        remote_object_id != 0xffff && root_summary.instances_reachable == 2 &&
        !persistent_transaction_test::read(server_body_tag.value,
                                           server_body_tag.len).empty() &&
        !persistent_transaction_test::read(remote_body_tag.value,
                                           remote_body_tag.len).empty() &&
        persistent_transaction_test::read(orphan_body_tag.value,
                                           orphan_body_tag.len).empty();
    static_cast<void>(
        jcvm::FlashMemory_Handler::erasePersistentTag(server_tag));
    static_cast<void>(jcvm::FlashMemory_Handler::erasePersistentTag(
        server_body_tag));
    static_cast<void>(
        jcvm::FlashMemory_Handler::erasePersistentTag(remote_tag));
    static_cast<void>(jcvm::FlashMemory_Handler::erasePersistentTag(
        remote_body_tag));
    static_cast<void>(
        jcvm::FlashMemory_Handler::erasePersistentTag(orphan_tag));
    static_cast<void>(jcvm::FlashMemory_Handler::erasePersistentTag(
        orphan_body_tag));
    persistent_transaction_test::instance_field_count = previous_field_count;
    expect(proxy_registered && roots_complete && proxy_root_survives_reclaim,
           "persistent_deletion_reclaimer_preserves_peer_proxy_and_rmi_roots");
  }

  {
    using jcvm::FlashMemory_Handler;
    jcvm::Heap heap(7);
    const auto empty_tag = make_tag(0xd0);
    const auto empty_reference =
        heap.addArray(0, jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN);
    FlashMemory_Handler::setPersistentField_Array(
        empty_tag, *heap.getArray(empty_reference), heap);
    bool empty_array_restored = false;
    try {
      jcvm::Heap restored_heap(7);
      const auto empty =
          FlashMemory_Handler::getPersistentField_Array(empty_tag,
                                                        restored_heap);
      empty_array_restored = empty->getType() ==
                                 jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN &&
                             !empty->isTransientArray() && empty->size() == 0;
    } catch (...) {
    }
    expect(empty_array_restored,
           "persistent_zero_length_boolean_array_restores_without_clear_event");

    const auto one_tag = make_tag(0xd1);
    const auto one_reference =
        heap.addArray(1, jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN);
    auto one = heap.getArray(one_reference);
    one->setByteEntry(0, 1);
    FlashMemory_Handler::setPersistentField_Array(one_tag, *one, heap);
    bool one_element_restored = false;
    try {
      jcvm::Heap one_heap(7);
      const auto restored_one =
          FlashMemory_Handler::getPersistentField_Array(one_tag, one_heap);
      one_element_restored = restored_one->size() == 1 &&
                             restored_one->getByteEntry(0) == 1;
    } catch (...) {
    }
    expect(one_element_restored,
           "persistent_one_element_boolean_array_restores_value");

    const auto values_tag = make_tag(0xd2);
    const auto values_reference =
        heap.addArray(3, jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN);
    auto values = heap.getArray(values_reference);
    values->setByteEntry(0, 0);
    values->setByteEntry(1, 1);
    values->setByteEntry(2, 0);
    FlashMemory_Handler::setPersistentField_Array(values_tag, *values, heap);

    jcvm::Heap restored_heap(7);
    auto restored =
        FlashMemory_Handler::getPersistentField_Array(values_tag,
                                                      restored_heap);
    bool initial_values_read = false;
    try {
      initial_values_read =
          restored->getByteEntry(0) == 0 && restored->getByteEntry(1) == 1 &&
          restored->getByteEntry(2) == 0;
    } catch (...) {
    }
    expect(initial_values_read,
           "persistent_boolean_array_reads_first_middle_and_last_values");

    const bool transaction_started =
        FlashMemory_Handler::beginPersistentWriteTransaction();
    bool mutation_visible = false;
    bool transaction_rolled_back = false;
    try {
      restored->setByteEntry(0, 1);
      mutation_visible = restored->getByteEntry(0) == 1;
      transaction_rolled_back =
          FlashMemory_Handler::rollbackPersistentWriteTransaction() &&
          restored->getByteEntry(0) == 0 && restored->getByteEntry(1) == 1 &&
          restored->getByteEntry(2) == 0;
    } catch (...) {
      if (FlashMemory_Handler::persistentWriteTransactionActive()) {
        (void)FlashMemory_Handler::rollbackPersistentWriteTransaction();
      }
    }
    expect(transaction_started && mutation_visible && transaction_rolled_back,
           "persistent_boolean_array_mutation_abort_restores_prior_contents");

    jcvm::Heap fresh_heap(7);
    const auto cold_restored =
        FlashMemory_Handler::getPersistentField_Array(values_tag, fresh_heap);
    bool cold_restore_preserves_values = false;
    try {
      cold_restore_preserves_values =
          cold_restored->getType() ==
                  jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN &&
              cold_restored->size() == 3 &&
              cold_restored->getByteEntry(0) == 0 &&
              cold_restored->getByteEntry(1) == 1 &&
              cold_restored->getByteEntry(2) == 0;
    } catch (...) {
    }
    expect(cold_restore_preserves_values,
           "persistent_boolean_array_fresh_heap_restore_preserves_values");

    const auto static_tag = FlashMemory_Handler::getStaticFieldTag(0x72, 0x73);
    const auto static_reference =
        heap.addArray(2, jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN);
    auto static_values = heap.getArray(static_reference);
    static_values->setByteEntry(0, 1);
    static_values->setByteEntry(1, 0);
    FlashMemory_Handler::setPersistentField_Reference(
        static_tag, static_reference, heap);
    bool static_array_restored = false;
    try {
      jcvm::Heap static_heap(7);
      const auto restored_reference =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(
              static_tag, static_heap);
      const auto restored_static = static_heap.getArray(restored_reference);
      static_array_restored =
          restored_static->getType() ==
                  jcvm::jc_array_type::JAVA_ARRAY_T_BOOLEAN &&
              restored_static->size() == 2 &&
              restored_static->getByteEntry(0) == 1 &&
              restored_static->getByteEntry(1) == 0;
    } catch (...) {
    }
    expect(static_array_restored,
           "persistent_static_boolean_array_reference_restores_initialized_values");

    const auto truncated_tag = make_tag(0xd3);
    persistent_transaction_test::seed(
        truncated_tag.value, truncated_tag.len,
        {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BOOLEAN),
         0, 2, 1});
    bool truncated_rejected = false;
    try {
      jcvm::Heap malformed_heap(7);
      (void)FlashMemory_Handler::getPersistentField_Array(truncated_tag,
                                                          malformed_heap);
    } catch (jcvm::Exceptions exception) {
      truncated_rejected = exception == jcvm::Exceptions::SecurityException;
    }
    expect(truncated_rejected,
           "truncated_persistent_boolean_array_record_is_rejected");

    const auto extra_tag = make_tag(0xd4);
    persistent_transaction_test::seed(
        extra_tag.value, extra_tag.len,
        {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BOOLEAN),
         0, 0, 0});
    bool extra_byte_rejected = false;
    try {
      jcvm::Heap malformed_heap(7);
      (void)FlashMemory_Handler::getPersistentField_Array(extra_tag,
                                                          malformed_heap);
    } catch (jcvm::Exceptions exception) {
      extra_byte_rejected = exception == jcvm::Exceptions::SecurityException;
    }
    expect(extra_byte_rejected,
           "persistent_boolean_array_rejects_spurious_clear_event_byte");
  }

  {
    persistent_transaction_test::instance_field_count = 4;
    jcvm::Heap heap(7);
    const auto reference = heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    const auto instance = heap.getInstance(reference);
    expect(instance->getField_Byte(0) == 0 &&
               instance->getField_Short(0) == 0 &&
               instance->getField_Reference(0).isNullPointer() &&
               instance->getField_Int(2) == 0,
           "new_instance_fields_have_typed_zero_and_null_defaults");

    instance->setField_Byte(0, 0x23);
    bool numeric_as_reference_rejected = false;
    try {
      (void)instance->getField_Reference(0);
    } catch (jcvm::Exceptions exception) {
      numeric_as_reference_rejected =
          exception == jcvm::Exceptions::SecurityException;
    }
    bool reference_over_numeric_rejected = false;
    try {
      instance->setField_Reference(0, jcvm::jref_t());
    } catch (jcvm::Exceptions exception) {
      reference_over_numeric_rejected =
          exception == jcvm::Exceptions::SecurityException;
    }
    instance->setField_Int(2, static_cast<jcvm::jint_t>(0x12345678));
    bool int_low_word_as_short_rejected = false;
    try {
      (void)instance->getField_Short(3);
    } catch (jcvm::Exceptions exception) {
      int_low_word_as_short_rejected =
          exception == jcvm::Exceptions::SecurityException;
    }
    expect(numeric_as_reference_rejected && reference_over_numeric_rejected &&
               int_low_word_as_short_rejected &&
               instance->getField_Int(2) == 0x12345678,
           "instance_field_access_enforces_runtime_kinds_and_int_word_pair");
    instance->getFields()->at(3).type = jcvm::FieldType::FIELD_TYPE_SHORT;
    bool malformed_int_pair_rejected = false;
    try {
      (void)instance->getField_Int(2);
    } catch (jcvm::Exceptions exception) {
      malformed_int_pair_rejected =
          exception == jcvm::Exceptions::SecurityException;
    }
    expect(malformed_int_pair_rejected,
           "malformed_int_field_pair_is_rejected");
    persistent_transaction_test::instance_field_count = 0;
  }
  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto instance_tag =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(0x7a, 2, 3);
    const auto field_tag =
        jcvm::FlashMemory_Handler::computeTag(instance_tag, 0);
    const std::vector<uint8_t> original_record = {
        static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_SHORT), 0x12, 0x34};
    persistent_transaction_test::seed(field_tag.value, field_tag.len,
                                      original_record);
    jcvm::JC_Instance instance(heap, 2, 3, instance_tag);
    const bool original_loaded = instance.getField_Short(0) == 0x1234;
    const bool began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    persistent_transaction_test::fail_one_write();
    bool failed_write = false;
    try {
      instance.setField_Short(0, 0x5678);
    } catch (jcvm::Exceptions) {
      failed_write = true;
    }
    const bool cache_unchanged_before_abort =
        instance.getField_Short(0) == 0x1234;
    const bool aborted =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool storage_unchanged =
        persistent_transaction_test::read(field_tag.value, field_tag.len) ==
        original_record;
    expect(original_loaded && began && failed_write &&
               cache_unchanged_before_abort && aborted && storage_unchanged,
           "failed_persistent_short_write_keeps_cache_and_storage_canonical");
    persistent_transaction_test::instance_field_count = 0;
  }
  {
    persistent_transaction_test::instance_field_count = 2;
    jcvm::Heap heap(7);
    const auto unrelated_tag = make_tag(0xf0);
    const auto unrelated_value = byte_record(0x5a);
    const uint32_t generation_before =
        jcvm::FlashMemory_Handler::persistentWriteRollbackGeneration();

    const auto child_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    auto child = heap.getInstance(child_reference);
    child->setField_Short(0, 0x1357);
    const auto parent_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    auto parent = heap.getInstance(parent_reference);
    parent->setField_Short(0, 0x2468);
    parent->setField_Reference(1, child_reference);

    const bool rollback_began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    const bool unrelated_write =
        jcvm::FlashMemory_Handler::writePersistentRecord(
            unrelated_tag, unrelated_value.data(),
            static_cast<uint32_t>(unrelated_value.size()));
    const bool rollback_succeeded =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool generation_advanced =
        jcvm::FlashMemory_Handler::persistentWriteRollbackGeneration() !=
        generation_before;

    // This is a valid internal access while the instances are still
    // nonpersistent. It must not mark their authoritative in-memory fields
    // stale merely because persistent storage rolled back elsewhere.
    (void)parent->getFields();
    (void)child->getFields();
    const auto parent_root = make_tag(0xf1);
    persistent_transaction_test::fail_one_write();
    bool first_promotion_failed = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          parent_root, parent_reference, heap);
    } catch (jcvm::Exceptions) {
      first_promotion_failed = true;
    }
    const bool failed_promotion_kept_live_fields =
        !parent->hasPersistentTag() &&
        parent->getField_Short(0) == 0x2468 &&
        parent->getField_Reference(1) == child_reference &&
        child->getField_Short(0) == 0x1357;

    bool old_object_retry_restored = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          parent_root, parent_reference, heap);
      const auto child_root = make_tag(0xf3);
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          child_root, child_reference, heap);
      const bool immediate_values =
          parent->getField_Short(0) == 0x2468 &&
          parent->getField_Reference(1) == child_reference &&
          child->getField_Short(0) == 0x1357;
      jcvm::Heap restored_heap(7);
      const auto restored_parent_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              parent_root, restored_heap);
      const auto restored_parent =
          restored_heap.getInstance(restored_parent_reference);
      const auto restored_child_reference =
          restored_parent->getField_Reference(1);
      const auto restored_child_root_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              child_root, restored_heap);
      const bool restored_values =
          restored_parent->getField_Short(0) == 0x2468 &&
          restored_heap.getInstance(restored_child_reference)
                  ->getField_Short(0) == 0x1357 &&
          restored_child_reference == restored_child_root_reference;
      old_object_retry_restored = immediate_values && restored_values;
    } catch (...) {
      old_object_retry_restored = false;
    }

    const auto post_rollback_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    auto post_rollback_instance = heap.getInstance(post_rollback_reference);
    post_rollback_instance->setField_Short(0, 0x369c);
    (void)post_rollback_instance->getFields();
    const auto post_rollback_root = make_tag(0xf2);
    bool post_rollback_instance_restored = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          post_rollback_root, post_rollback_reference, heap);
      jcvm::Heap restored_heap(7);
      const auto restored_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              post_rollback_root, restored_heap);
      post_rollback_instance_restored =
          post_rollback_instance->getField_Short(0) == 0x369c &&
          restored_heap.getInstance(restored_reference)->getField_Short(0) ==
              0x369c;
    } catch (...) {
      post_rollback_instance_restored = false;
    }

    expect(rollback_began && unrelated_write && rollback_succeeded &&
               generation_advanced,
           "unrelated_persistent_rollback_advances_generation_for_rv31");
    expect(first_promotion_failed && failed_promotion_kept_live_fields &&
               old_object_retry_restored,
           "pre_rollback_object_failed_promotion_preserves_fields_and_retries");
    expect(post_rollback_instance_restored,
           "post_rollback_new_object_fields_survive_first_promotion_and_restore");
    persistent_transaction_test::instance_field_count = 0;
  }
  {
    persistent_transaction_test::instance_field_count = 2;
    jcvm::Heap heap(7);
    const auto instance_tag =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(1, 2, 3);
    const auto int_field_tag =
        jcvm::FlashMemory_Handler::computeTag(instance_tag, 0);
    persistent_transaction_test::seed(
        int_field_tag.value, int_field_tag.len,
        {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_INT), 0x12, 0x34,
         0x56, 0x78});
    jcvm::JC_Instance instance(heap, 2, 3, instance_tag);
    const bool int_restored = instance.getField_Int(0) == 0x12345678;
    bool restored_int_low_word_rejected = false;
    try {
      (void)instance.getField_Short(1);
    } catch (jcvm::Exceptions exception) {
      restored_int_low_word_rejected =
          exception == jcvm::Exceptions::SecurityException;
    }
    expect(int_restored && restored_int_low_word_rejected,
           "restored_int_field_preserves_and_checks_low_word_marker");
    persistent_transaction_test::instance_field_count = 0;
  }
  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto first_root = make_tag(0xe1);
    const auto second_root = make_tag(0xe2);
    const auto first_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    auto first = heap.getInstance(first_reference);
    first->setField_Short(0, 0x11);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        first_root, first_reference, heap);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        second_root, first_reference, heap);

    const auto first_slot = persistent_transaction_test::read(
        first_root.value, first_root.len);
    const auto second_slot = persistent_transaction_test::read(
        second_root.value, second_root.len);
    const auto alias_target = [](const std::vector<uint8_t> &record,
                                 jcvm::fs::Tag &target) {
      if (record.size() < 3 || record[0] != 0xfe || record[1] == 0 ||
          record.size() != static_cast<std::size_t>(record[1]) + 2 ||
          record[1] > sizeof(target.value)) {
        return false;
      }
      target.len = record[1];
      std::memcpy(target.value, record.data() + 2, target.len);
      return true;
    };
    jcvm::fs::Tag first_target{};
    jcvm::fs::Tag second_target{};
    const bool shared_body =
        alias_target(first_slot, first_target) &&
        alias_target(second_slot, second_target) &&
        first_target.len == second_target.len &&
        std::memcmp(first_target.value, second_target.value,
                    first_target.len) == 0;
    expect(shared_body,
           "ordinary_object_references_use_one_independent_identity_body");
    const auto replacement_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    auto replacement = heap.getInstance(replacement_reference);
    replacement->setField_Short(0, 0x22);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        first_root, replacement_reference, heap);

    bool replacement_preserves_original_alias = false;
    jcvm::jref_t restored_first_reference;
    jcvm::jref_t restored_second_reference;
    try {
      jcvm::Heap restored_heap(7);
      restored_first_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              first_root, restored_heap);
      restored_second_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              second_root, restored_heap);
      replacement_preserves_original_alias =
          restored_first_reference != restored_second_reference &&
          restored_heap.getInstance(restored_first_reference)
                  ->getField_Short(0) == 0x22 &&
          restored_heap.getInstance(restored_second_reference)
                  ->getField_Short(0) == 0x11;
    } catch (jcvm::Exceptions exception) {
      std::printf("OBJECT_ID_REPLACE_EXCEPTION id=%u\n",
                  static_cast<unsigned>(exception));
    } catch (...) {
      std::printf("OBJECT_ID_REPLACE_EXCEPTION unknown\n");
    }
    expect(replacement_preserves_original_alias,
           "replacing_first_reference_preserves_other_object_identity_after_restore");

    bool clearing_first_preserves_second = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          first_root, jcvm::jref_t(), heap);
      jcvm::Heap restored_heap(7);
      const auto cleared =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              first_root, restored_heap);
      const auto retained =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              second_root, restored_heap);
      clearing_first_preserves_second =
          cleared.isNullPointer() &&
          restored_heap.getInstance(retained)->getField_Short(0) == 0x11;
    } catch (jcvm::Exceptions exception) {
      std::printf("OBJECT_ID_CLEAR_EXCEPTION id=%u\n",
                  static_cast<unsigned>(exception));
    } catch (...) {
      std::printf("OBJECT_ID_CLEAR_EXCEPTION unknown\n");
    }
    expect(clearing_first_preserves_second,
           "clearing_first_reference_keeps_object_reachable_through_other_alias");

    persistent_transaction_test::instance_field_count = 0;
  }

  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto legacy_root = make_tag(0xe4);
    const auto second_root = make_tag(0xe5);
    persistent_transaction_test::seed(
        legacy_root.value, legacy_root.len,
        {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_OBJECT), 2, 0, 3});
    const auto legacy_field =
        jcvm::FlashMemory_Handler::computeTag(legacy_root, 0);
    persistent_transaction_test::seed(
        legacy_field.value, legacy_field.len,
        {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_SHORT), 0, 0x31});
    const auto legacy_reference =
        jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
            legacy_root, heap);

    const bool began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    bool migration_written = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          second_root, legacy_reference, heap);
      const auto migrated_root = persistent_transaction_test::read(
          legacy_root.value, legacy_root.len);
      const auto migrated_second = persistent_transaction_test::read(
          second_root.value, second_root.len);
      migration_written = migrated_root.size() > 0 &&
                         migrated_root[0] == 0xfe &&
                         migrated_second.size() > 0 &&
                         migrated_second[0] == 0xfe;
    } catch (...) {
    }
    const bool aborted =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const auto restored_legacy_record = persistent_transaction_test::read(
        legacy_root.value, legacy_root.len);
    const auto absent_second_record = persistent_transaction_test::read(
        second_root.value, second_root.len);
    bool retry_preserved_identity = false;
    bool aliases_mutate_and_survive_gc = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          second_root, legacy_reference, heap);
      const auto retried_root = persistent_transaction_test::read(
          legacy_root.value, legacy_root.len);
      const auto retried_second = persistent_transaction_test::read(
          second_root.value, second_root.len);
      jcvm::Heap restored_heap(7);
      const auto first_alias =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              legacy_root, restored_heap);
      const auto second_alias =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              second_root, restored_heap);
      retry_preserved_identity =
          retried_root.size() > 0 && retried_root[0] == 0xfe &&
          retried_second.size() > 0 && retried_second[0] == 0xfe &&
          first_alias == second_alias &&
          restored_heap.getInstance(first_alias)->getField_Short(0) == 0x31;

      const auto garbage_reference =
          restored_heap.addInstance(jcvm::JC_Instance(restored_heap, 2, 3));
      const auto summary = restored_heap.collectUnreachableFrom(
          {first_alias, second_alias});
      auto shared_instance = restored_heap.getInstance(first_alias);
      shared_instance->setField_Short(0, 0x44);
      bool garbage_handle_invalidated = false;
      try {
        (void)restored_heap.getInstance(garbage_reference);
      } catch (jcvm::Exceptions) {
        garbage_handle_invalidated = true;
      }
      aliases_mutate_and_survive_gc =
          summary.complete && summary.instances_reclaimed == 1 &&
          first_alias == second_alias &&
          garbage_handle_invalidated &&
          restored_heap.getInstance(second_alias)->getField_Short(0) == 0x44;

      jcvm::Heap final_heap(7);
      const auto final_first =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              legacy_root, final_heap);
      const auto final_second =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              second_root, final_heap);
      aliases_mutate_and_survive_gc = aliases_mutate_and_survive_gc &&
          final_first == final_second &&
          final_heap.getInstance(final_first)->getField_Short(0) == 0x44;
    } catch (jcvm::Exceptions exception) {
      std::printf("OBJECT_ID_ABORT_RETRY_EXCEPTION id=%u\n",
                  static_cast<unsigned>(exception));
    } catch (...) {
      std::printf("OBJECT_ID_ABORT_RETRY_EXCEPTION unknown\n");
    }
    expect(began && migration_written && aborted &&
               restored_legacy_record.size() == 4 &&
               restored_legacy_record[0] ==
                   static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_OBJECT) &&
               absent_second_record.empty() && retry_preserved_identity,
           "legacy_object_migration_abort_retries_without_splitting_identity");
    expect(aliases_mutate_and_survive_gc,
           "object_alias_mutation_and_gc_preserve_shared_identity");
    persistent_transaction_test::instance_field_count = 0;
  }

  {
    persistent_transaction_test::instance_field_count = 2;
    jcvm::Heap heap(7);
    const auto root = make_tag(0xe3);
    const auto a_ref = heap.addInstance(jcvm::JC_Instance(heap, 2, 4));
    const auto b_ref = heap.addInstance(jcvm::JC_Instance(heap, 2, 4));
    const auto c_ref = heap.addInstance(jcvm::JC_Instance(heap, 2, 4));
    const auto d_ref = heap.addInstance(jcvm::JC_Instance(heap, 2, 4));
    auto a = heap.getInstance(a_ref);
    auto b = heap.getInstance(b_ref);
    auto c = heap.getInstance(c_ref);
    auto d = heap.getInstance(d_ref);
    a->setField_Reference(0, b_ref);
    a->setField_Reference(1, c_ref);
    b->setField_Reference(0, d_ref);
    c->setField_Reference(0, d_ref);
    d->setField_Reference(0, a_ref);
    bool graph_persisted = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(root, a_ref,
                                                              heap);
      graph_persisted = true;
    } catch (jcvm::Exceptions exception) {
      std::printf("OBJECT_ID_GRAPH_PERSIST_EXCEPTION id=%u\n",
                  static_cast<unsigned>(exception));
    } catch (...) {
      std::printf("OBJECT_ID_GRAPH_PERSIST_EXCEPTION unknown\n");
    }
    bool graph_identity_restored = false;
    try {
      if (!graph_persisted) {
        throw jcvm::Exceptions::SecurityException;
      }
      jcvm::Heap restored_heap(7);
      const auto restored_a =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root, restored_heap);
      const auto ra = restored_heap.getInstance(restored_a);
      const auto rb_ref = ra->getField_Reference(0);
      const auto rc_ref = ra->getField_Reference(1);
      const auto rb = restored_heap.getInstance(rb_ref);
      const auto rc = restored_heap.getInstance(rc_ref);
      const auto rd_from_b = rb->getField_Reference(0);
      const auto rd_from_c = rc->getField_Reference(0);
      const auto rd = restored_heap.getInstance(rd_from_b);
      graph_identity_restored =
          rd_from_b == rd_from_c &&
          rd->getField_Reference(0) == restored_a;
    } catch (jcvm::Exceptions exception) {
      std::printf("OBJECT_ID_GRAPH_EXCEPTION id=%u\n",
                  static_cast<unsigned>(exception));
    } catch (...) {
      std::printf("OBJECT_ID_GRAPH_EXCEPTION unknown\n");
    }
    expect(graph_identity_restored,
           "persistent_object_graph_preserves_diamond_sharing_and_cycle");
    persistent_transaction_test::instance_field_count = 0;
  }

  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto parent_root = make_tag(0xe6);
    const auto child_root = make_tag(0xe7);
    const auto static_root =
        jcvm::FlashMemory_Handler::getStaticFieldTag(2, 0x1234);
    const auto child_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    const auto parent_reference =
        heap.addInstance(jcvm::JC_Instance(heap, 2, 4));
    auto child = heap.getInstance(child_reference);
    auto parent = heap.getInstance(parent_reference);
    child->setField_Short(0, 0x55);
    parent->setField_Reference(0, child_reference);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        child_root, child_reference, heap);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        parent_root, parent_reference, heap);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        static_root, child_reference, heap);

    const auto alias_target = [](const std::vector<uint8_t> &record,
                                 jcvm::fs::Tag &target) {
      if (record.size() < 3 || record[0] != 0xfe || record[1] == 0 ||
          record.size() != static_cast<std::size_t>(record[1]) + 2 ||
          record[1] > sizeof(target.value)) {
        return false;
      }
      target.len = record[1];
      std::memcpy(target.value, record.data() + 2, target.len);
      return true;
    };
    jcvm::fs::Tag parent_body{};
    jcvm::fs::Tag child_body{};
    jcvm::fs::Tag parent_field_target{};
    const auto parent_record = persistent_transaction_test::read(
        parent_root.value, parent_root.len);
    const auto child_record = persistent_transaction_test::read(
        child_root.value, child_root.len);
    const bool initial_aliases_valid =
        alias_target(parent_record, parent_body) &&
        alias_target(child_record, child_body) &&
        alias_target(persistent_transaction_test::read(
                         static_root.value, static_root.len),
                     parent_field_target);
    const auto parent_field =
        jcvm::FlashMemory_Handler::computeTag(parent_body, 0);
    jcvm::fs::Tag instance_child_body{};
    jcvm::fs::Tag static_child_body{};
    const bool initial_targets_are_distinct =
        initial_aliases_valid &&
        alias_target(persistent_transaction_test::read(
                         parent_field.value, parent_field.len),
                     instance_child_body) &&
        alias_target(persistent_transaction_test::read(
                         static_root.value, static_root.len),
                     static_child_body) &&
        instance_child_body.len == child_body.len &&
        std::memcmp(instance_child_body.value, child_body.value,
                    child_body.len) == 0 &&
        static_child_body.len == child_body.len &&
        std::memcmp(static_child_body.value, child_body.value,
                    child_body.len) == 0 &&
        !(instance_child_body.len == parent_field.len &&
          std::memcmp(instance_child_body.value, parent_field.value,
                      parent_field.len) == 0) &&
        !(static_child_body.len == static_root.len &&
          std::memcmp(static_child_body.value, static_root.value,
                      static_root.len) == 0);

    parent->setField_Reference(0, child_reference);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        static_root, child_reference, heap);
    const bool began_commit =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    parent->setField_Reference(0, child_reference);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        static_root, child_reference, heap);
    const bool committed =
        jcvm::FlashMemory_Handler::commitPersistentWriteTransaction();
    const bool began_abort =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    parent->setField_Reference(0, child_reference);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        static_root, child_reference, heap);
    const bool aborted =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();

    bool same_identity_after_restore = false;
    bool mutation_after_restore = false;
    try {
      jcvm::Heap restored_heap(7);
      const auto restored_parent =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              parent_root, restored_heap);
      const auto restored_child_from_parent =
          restored_heap.getInstance(restored_parent)->getField_Reference(0);
      const auto restored_child_from_static =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              static_root, restored_heap);
      const auto restored_child_from_root =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              child_root, restored_heap);
      same_identity_after_restore =
          restored_child_from_parent == restored_child_from_static &&
          restored_child_from_parent == restored_child_from_root &&
          restored_heap.getInstance(restored_child_from_parent)
                  ->getField_Short(0) == 0x55;
      restored_heap.getInstance(restored_child_from_parent)
          ->setField_Short(0, 0x66);

      jcvm::Heap final_heap(7);
      const auto final_parent =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              parent_root, final_heap);
      const auto final_child_from_parent =
          final_heap.getInstance(final_parent)->getField_Reference(0);
      const auto final_child_from_static =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              static_root, final_heap);
      mutation_after_restore =
          final_child_from_parent == final_child_from_static &&
          final_heap.getInstance(final_child_from_parent)->getField_Short(0) ==
              0x66;
    } catch (jcvm::Exceptions exception) {
      std::printf("OBJECT_ID_SELF_ASSIGN_EXCEPTION id=%u\n",
                  static_cast<unsigned>(exception));
    } catch (...) {
      std::printf("OBJECT_ID_SELF_ASSIGN_EXCEPTION unknown\n");
    }
    expect(initial_targets_are_distinct,
           "same_value_assignment_initial_aliases_target_distinct_bodies");
    expect(began_commit && committed && began_abort && aborted,
           "same_value_instance_and_static_assignments_survive_transactions");
    expect(same_identity_after_restore,
           "same_value_instance_and_static_assignments_preserve_identity_after_restore");
    expect(mutation_after_restore,
           "same_value_assignment_allows_mutation_after_fresh_restore");
    persistent_transaction_test::instance_field_count = 0;
  }

  {
    persistent_transaction_test::instance_field_count = 2;
    const auto verify_failed_first_publication =
        [](uint8_t root_index, unsigned int successful_writes,
           uint8_t expected_failure_stage, const char *test_name) {
          jcvm::Heap heap(7);
          const auto root = make_tag(root_index);
          const auto reference =
              heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
          auto instance = heap.getInstance(reference);
          instance->setField_Short(0, 0x31);
          instance->setField_Short(1, 0x42);

          persistent_transaction_test::fail_write_after(successful_writes);
          bool failed = false;
          try {
            jcvm::FlashMemory_Handler::setPersistentField_Reference(
                root, reference, heap);
          } catch (jcvm::Exceptions) {
            failed = true;
          }
          const auto failed_tag =
              persistent_transaction_test::failed_write_tag();
          bool failure_stage_matches = false;
          if (expected_failure_stage == 0) {
            failure_stage_matches =
                failed_tag.size() == static_cast<std::size_t>(root.len) + 6 &&
                failed_tag[root.len] == 0xff &&
                failed_tag[root.len + 1] == 0xfe;
          } else if (expected_failure_stage == 1) {
            failure_stage_matches =
                failed_tag.size() == static_cast<std::size_t>(root.len) + 8 &&
                failed_tag[root.len] == 0xff &&
                failed_tag[root.len + 1] == 0xfe &&
                failed_tag[failed_tag.size() - 2] == 0 &&
                failed_tag[failed_tag.size() - 1] == 0;
          } else {
            failure_stage_matches =
                failed_tag.size() == root.len &&
                std::memcmp(failed_tag.data(), root.value, root.len) == 0;
          }
          const bool unpublished_and_retryable =
              failed && failure_stage_matches &&
              persistent_transaction_test::read(root.value, root.len).empty() &&
              !instance->hasPersistentTag() &&
              instance->getField_Short(0) == 0x31 &&
              instance->getField_Short(1) == 0x42;

          bool retry_restored = false;
          try {
            jcvm::FlashMemory_Handler::setPersistentField_Reference(
                root, reference, heap);
            const auto live_body_tag = instance->getPersistentTag();
            const auto root_record = persistent_transaction_test::read(
                root.value, root.len);
            const bool root_targets_live_body =
                root_record.size() ==
                    static_cast<std::size_t>(live_body_tag.len) + 2 &&
                root_record[0] == 0xfe &&
                root_record[1] == live_body_tag.len &&
                std::memcmp(root_record.data() + 2, live_body_tag.value,
                            live_body_tag.len) == 0;
            jcvm::Heap restored_heap(7);
            const auto restored_reference =
                jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
                    root, restored_heap);
            const auto restored_reference_again =
                jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
                    root, restored_heap);
            const auto restored = restored_heap.getInstance(restored_reference);
            retry_restored = restored->getField_Short(0) == 0x31 &&
                             restored->getField_Short(1) == 0x42 &&
                             root_targets_live_body &&
                             restored_reference.compact() ==
                                 restored_reference_again.compact() &&
                             restored->getPersistentTag().len ==
                                 live_body_tag.len &&
                             std::memcmp(restored->getPersistentTag().value,
                                         live_body_tag.value,
                                         live_body_tag.len) == 0;
          } catch (...) {
            retry_restored = false;
          }
          expect(unpublished_and_retryable && retry_restored, test_name);
        };

    verify_failed_first_publication(
        0xe8, 0, 0,
        "first_object_publication_header_failure_retries_same_live_object");
    verify_failed_first_publication(
        0xe9, 1, 1,
        "first_object_publication_child_failure_retries_same_live_object");
    verify_failed_first_publication(
        0xea, 3, 2,
        "first_object_publication_root_failure_retries_same_live_object");

    jcvm::Heap heap(7);
    const auto root = make_tag(0xeb);
    const auto reference = heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
    auto instance = heap.getInstance(reference);
    instance->setField_Short(0, 0x53);
    instance->setField_Short(1, 0x64);
    const bool began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    jcvm::FlashMemory_Handler::setPersistentField_Reference(root, reference,
                                                            heap);
    const auto staged_root =
        persistent_transaction_test::read(root.value, root.len);
    const bool rolled_back =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool root_rolled_back =
        persistent_transaction_test::read(root.value, root.len).empty();
    const auto rolled_back_body_tag = instance->getPersistentTag();
    const bool body_rolled_back =
        persistent_transaction_test::read(rolled_back_body_tag.value,
                                          rolled_back_body_tag.len)
            .empty();
    bool abort_retry_restored = false;
    try {
      jcvm::FlashMemory_Handler::setPersistentField_Reference(root, reference,
                                                              heap);
      const auto live_body_tag = instance->getPersistentTag();
      const auto root_record =
          persistent_transaction_test::read(root.value, root.len);
      const bool root_targets_live_body =
          root_record.size() ==
              static_cast<std::size_t>(live_body_tag.len) + 2 &&
          root_record[0] == 0xfe &&
          root_record[1] == live_body_tag.len &&
          std::memcmp(root_record.data() + 2, live_body_tag.value,
                      live_body_tag.len) == 0;
      jcvm::Heap restored_heap(7);
      const auto restored_reference =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root, restored_heap);
      const auto restored_reference_again =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root, restored_heap);
      const auto restored = restored_heap.getInstance(restored_reference);
      abort_retry_restored = restored->getField_Short(0) == 0x53 &&
                             restored->getField_Short(1) == 0x64 &&
                             root_targets_live_body &&
                             restored_reference.compact() ==
                                 restored_reference_again.compact() &&
                             restored->getPersistentTag().len ==
                                 live_body_tag.len &&
                             std::memcmp(restored->getPersistentTag().value,
                                         live_body_tag.value,
                                         live_body_tag.len) == 0;
    } catch (...) {
      abort_retry_restored = false;
    }
    expect(began && staged_root.size() > 0 && staged_root[0] == 0xfe &&
               rolled_back && root_rolled_back && body_rolled_back &&
               abort_retry_restored,
           "first_object_promotion_abort_retries_same_preexisting_object");
    persistent_transaction_test::instance_field_count = 0;
  }

  {
    persistent_transaction_test::instance_field_count = 4;
    jcvm::Heap heap(7);
    const auto instance_tag =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(0x77, 2, 3);
    const auto field_tag =
        jcvm::FlashMemory_Handler::computeTag(instance_tag, 0);
    persistent_transaction_test::seed(
        field_tag.value, field_tag.len, byte_record(0x42));
    jcvm::JC_Instance instance(heap, 2, 3, instance_tag);
    const bool original_loaded = instance.getField_Byte(0) == 0x42;
    const bool began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    persistent_transaction_test::fail_one_write();
    bool failed_write = false;
    try {
      instance.setField_Byte(0, 0x7f);
    } catch (jcvm::Exceptions) {
      failed_write = true;
    }
    const bool cache_unchanged_before_abort =
        instance.getField_Byte(0) == 0x42;
    const bool aborted =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool storage_unchanged =
        persistent_transaction_test::read(field_tag.value, field_tag.len) ==
        byte_record(0x42);
    expect(original_loaded && began && failed_write &&
               cache_unchanged_before_abort && aborted && storage_unchanged,
           "failed_persistent_byte_write_keeps_cache_and_storage_canonical");
    persistent_transaction_test::instance_field_count = 0;
  }
  {
    persistent_transaction_test::instance_field_count = 2;
    jcvm::Heap heap(7);
    const auto instance_tag =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(0x78, 2, 3);
    const auto field_tag =
        jcvm::FlashMemory_Handler::computeTag(instance_tag, 0);
    const jcvm::jint_t original_value =
        static_cast<jcvm::jint_t>(0x12345678);
    persistent_transaction_test::seed(
        field_tag.value, field_tag.len,
        {static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_INT), 0x12, 0x34,
         0x56, 0x78});
    const auto original_record =
        persistent_transaction_test::read(field_tag.value, field_tag.len);
    jcvm::JC_Instance instance(heap, 2, 3, instance_tag);
    const bool loaded = instance.getField_Int(0) == original_value;
    const bool began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    persistent_transaction_test::fail_one_write();
    bool failed_write = false;
    try {
      instance.setField_Int(0, static_cast<jcvm::jint_t>(0x76543210));
    } catch (jcvm::Exceptions) {
      failed_write = true;
    }
    const bool cache_unchanged_before_abort =
        instance.getField_Int(0) == original_value;
    const bool aborted =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool storage_unchanged =
        persistent_transaction_test::read(field_tag.value, field_tag.len) ==
        original_record;
    expect(loaded && began && failed_write && cache_unchanged_before_abort &&
               aborted && storage_unchanged,
           "failed_persistent_int_write_keeps_both_words_canonical");
    persistent_transaction_test::instance_field_count = 0;
  }
  {
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(7);
    const auto instance_tag =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(0x79, 2, 3);
    const auto field_tag =
        jcvm::FlashMemory_Handler::computeTag(instance_tag, 0);
    const auto original_array =
        heap.addArray(2, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(original_array)->setByteEntry(0, 0x31);
    heap.getArray(original_array)->setByteEntry(1, 0x32);
    jcvm::FlashMemory_Handler::setPersistentField_Reference(
        field_tag, original_array, heap);
    const auto original_record =
        persistent_transaction_test::read(field_tag.value, field_tag.len);
    jcvm::JC_Instance instance(heap, 2, 3, instance_tag);
    const auto original_reference = instance.getField_Reference(0);
    const auto replacement_array =
        heap.addArray(2, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(replacement_array)->setByteEntry(0, 0x41);
    heap.getArray(replacement_array)->setByteEntry(1, 0x42);
    const bool began =
        jcvm::FlashMemory_Handler::beginPersistentWriteTransaction();
    persistent_transaction_test::fail_one_write();
    bool failed_write = false;
    try {
      instance.setField_Reference(0, replacement_array);
    } catch (jcvm::Exceptions) {
      failed_write = true;
    }
    const bool cache_unchanged_before_abort =
        instance.getField_Reference(0) == original_reference;
    const bool aborted =
        jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool storage_unchanged =
        persistent_transaction_test::read(field_tag.value, field_tag.len) ==
        original_record;
    expect(began && failed_write && cache_unchanged_before_abort && aborted &&
               storage_unchanged,
           "failed_persistent_reference_write_keeps_old_root_canonical");
    persistent_transaction_test::instance_field_count = 0;
  }
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
    const auto malformed_edge = jcvm::FlashMemory_Handler::computeTag(parent_tag, 0);
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
    const auto edge_tag = jcvm::FlashMemory_Handler::computeTag(parent_tag, 0);
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
  const auto alias_edge_tag = jcvm::FlashMemory_Handler::computeTag(alias_parent_tag, 0);
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
    const auto edge_tag = jcvm::FlashMemory_Handler::computeTag(parent_tag, 0);
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
        jcvm::FlashMemory_Handler::computeTag(instance_tag, 0);
    seed(malformed_field.value, malformed_field.len, {0xfe});
    const auto summary = heap.collectUnreachableFrom({instance_reference});
    expect(!summary.complete && summary.edge_load_failures == 1 &&
               summary.arrays_reclaimed == 0 &&
               heap.getInstance(instance_reference) != nullptr &&
               heap.getArray(unreachable_reference) != nullptr,
           "malformed_serialized_object_field_blocks_heap_reclamation");
    jcvm::fs::Tag malformed_field_tag{};
    malformed_field_tag.len = malformed_field.len;
    std::memcpy(malformed_field_tag.value, malformed_field.value,
                malformed_field.len);
    expect(jcvm::FlashMemory_Handler::erasePersistentTag(malformed_field_tag),
           "malformed_serialized_object_field_fixture_cleanup");
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
    const auto element_tag = jcvm::FlashMemory_Handler::computeTag(root_tag, 0);
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

  // A live handle must not publish another alias if its durable array body
  // has disappeared. Doing so hides the missing body until a later graph walk
  // and turns a storage error into an incomplete persistent-GC trace.
  {
    jcvm::Heap heap(7);
    const auto body_field = FlashMemory_Handler::getStaticFieldTag(91, 1);
    const auto alias_field = FlashMemory_Handler::getStaticFieldTag(91, 2);
    const auto array = heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(array)->setByteEntry(0, 0x5a);
    FlashMemory_Handler::setPersistentField_Reference(body_field, array, heap);

    const auto body_alias =
        persistent_transaction_test::read(body_field.value, body_field.len);
    jcvm::fs::Tag body_tag{};
    if (body_alias.size() >= 3 && body_alias[0] == 0xfd &&
        body_alias[1] <= sizeof(body_tag.value) &&
        static_cast<size_t>(body_alias[1]) + 2u == body_alias.size()) {
      body_tag.len = body_alias[1];
      std::memcpy(body_tag.value, body_alias.data() + 2, body_tag.len);
    }
    const bool body_erased = body_tag.len != 0 &&
        FlashMemory_Handler::erasePersistentTag(body_tag);

    bool missing_body_rejected = false;
    try {
      FlashMemory_Handler::setPersistentField_Reference(alias_field, array,
                                                        heap);
    } catch (jcvm::Exceptions exception) {
      missing_body_rejected = exception == jcvm::Exceptions::IOException;
    }
    const auto dangling_alias = persistent_transaction_test::read(
        alias_field.value, alias_field.len);
    expect(body_erased && missing_body_rejected && dangling_alias.empty(),
           "persistent_array_handle_rejects_realiasing_missing_body");
    static_cast<void>(FlashMemory_Handler::erasePersistentTag(body_field));
    static_cast<void>(FlashMemory_Handler::erasePersistentTag(alias_field));
    if (body_tag.len != 0) {
      static_cast<void>(FlashMemory_Handler::erasePersistentTag(body_tag));
    }
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
        jcvm::FlashMemory_Handler::computeTag(serialized_root, 0);
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
    const auto edge_tag = jcvm::FlashMemory_Handler::computeTag(serialized_root, 0);
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
    const auto malformed_root_edge =
        jcvm::FlashMemory_Handler::computeTag(serialized_root, 0);
    expect(jcvm::FlashMemory_Handler::erasePersistentTag(malformed_root_edge),
           "incomplete_reachability_fixture_cleanup");
  }

  {
    constexpr uint8_t owner = 0x39;
    const uint8_t applet_instance_root[] = {0x03, owner, 0x00, 0x00, 0xff};
    const uint8_t orphan_object[] = {0x03, owner, 0x44, 0x55};
    seed(applet_instance_root, sizeof(applet_instance_root), byte_record(0x31));
    seed(orphan_object, sizeof(orphan_object), byte_record(0x32));

    jcvm::Heap heap(owner);
    std::vector<jcvm::jref_t> no_roots;
    CandidateTags candidates;
    const auto inventory_result =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            owner, heap, no_roots, collect_candidate, &candidates);
    const bool root_excluded =
        inventory_result == jcvm::PersistentInventory::VisitResult::Complete &&
        std::find(candidates.tags.begin(), candidates.tags.end(),
                  std::vector<uint8_t>(applet_instance_root,
                                      applet_instance_root +
                                          sizeof(applet_instance_root))) ==
            candidates.tags.end();
    uint32_t erased = 0;
    RootVectorContext roots{&no_roots};
    const auto reclaim_result =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased);
    expect(root_excluded &&
               reclaim_result ==
                   jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 1 &&
               !persistent_transaction_test::read(applet_instance_root,
                                                    sizeof(applet_instance_root))
                    .empty() &&
               persistent_transaction_test::read(orphan_object,
                                                  sizeof(orphan_object))
                   .empty(),
           "reclaimer_preserves_registered_applet_instance_root");
  }

  {
    constexpr uint8_t owner = 0x31;
    const uint8_t first_tag[] = {0x03, owner, 0x10, 0x01};
    const uint8_t second_tag[] = {0x03, owner, 0x10, 0x02};
    seed(first_tag, sizeof(first_tag), byte_record(0x11));
    seed(second_tag, sizeof(second_tag), byte_record(0x22));
    persistent_transaction_test::force_cursor_stall_after_one_result(owner);

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
    constexpr uint8_t owner = 0x04;
    const auto applet_root =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(owner, 0x3a, 0x00);
    const uint8_t backing_record_tag_bytes[] = {
        0x03, owner, 0x3a, 0x00, 0xff, 0x00, 0x00};
    const uint8_t other_applet_record_tag_bytes[] = {
        0x03, owner, 0x3a, 0x01, 0xff, 0x00, 0x00};
    jcvm::Heap heap(owner);
    const auto backing_record =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    auto backing_array = heap.getArray(backing_record);
    jcvm::fs::Tag backing_tag{};
    backing_tag.len = sizeof(backing_record_tag_bytes);
    std::memcpy(backing_tag.value, backing_record_tag_bytes,
                sizeof(backing_record_tag_bytes));
    backing_array->setPersistentTag(backing_tag);
    const auto other_applet_record =
        heap.addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
    auto other_applet_array = heap.getArray(other_applet_record);
    jcvm::fs::Tag other_applet_tag{};
    other_applet_tag.len = sizeof(other_applet_record_tag_bytes);
    std::memcpy(other_applet_tag.value, other_applet_record_tag_bytes,
                sizeof(other_applet_record_tag_bytes));
    other_applet_array->setPersistentTag(other_applet_tag);
    const auto other_applet_root =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(owner, 0x3a, 0x02);
    const auto reachability =
        heap.traceReachableFrom({backing_record, other_applet_record});
    expect(applet_root.len == 5 && reachability.complete &&
               heap.hasReachablePersistentTagPrefix(applet_root) &&
               !heap.hasReachablePersistentTagPrefix(other_applet_root),
           "reachable_applet_root_prefix_protects_nested_backing_record");
  }

  {
    constexpr uint8_t owner = 0x45;
    constexpr uint8_t source_package = 0x61;
    const uint8_t source_static_tag[] = {0x02, source_package, 0x01};
    std::array<uint8_t, 9> target_tag{
        0x03, owner, source_package, 0xff, 0xfc,
        static_cast<uint8_t>(sizeof(source_static_tag)), 0x02,
        source_package, 0x01};
    std::array<uint8_t, 11> target_child_tag{};
    std::copy(target_tag.begin(), target_tag.end(), target_child_tag.begin());
    target_child_tag[9] = 0;
    target_child_tag[10] = 1;
    seed(target_tag.data(), target_tag.size(), byte_record(0x51));
    seed(target_child_tag.data(), target_child_tag.size(), byte_record(0x52));
    std::vector<uint8_t> wrapper{8, source_package,
                                 static_cast<uint8_t>(target_tag.size())};
    wrapper.insert(wrapper.end(), target_tag.begin(), target_tag.end());
    seed(source_static_tag, sizeof(source_static_tag), wrapper);

    jcvm::Heap heap(owner);
    const std::vector<jcvm::jref_t> no_roots;
    CandidateTags retained_candidates;
    const auto while_referenced =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            owner, heap, no_roots, collect_candidate, &retained_candidates);
    const std::vector<uint8_t> target_bytes(target_tag.begin(), target_tag.end());
    const std::vector<uint8_t> child_bytes(target_child_tag.begin(),
                                          target_child_tag.end());
    const bool target_retained =
        while_referenced == jcvm::PersistentInventory::VisitResult::Complete &&
        std::find(retained_candidates.tags.begin(), retained_candidates.tags.end(),
                  target_bytes) == retained_candidates.tags.end() &&
        std::find(retained_candidates.tags.begin(), retained_candidates.tags.end(),
                  child_bytes) == retained_candidates.tags.end();

    bool alias_found = false;
    const auto valid_alias_result =
        jcvm::PersistentInventory::findAliasesToAppletOwner(owner, alias_found);
    const bool valid_alias_detected =
        valid_alias_result == jcvm::PersistentInventory::VisitResult::Stopped &&
        alias_found;

    std::vector<uint8_t> reserved_owner_wrapper = wrapper;
    reserved_owner_wrapper[4] = 0xfe;
    seed(source_static_tag, sizeof(source_static_tag), reserved_owner_wrapper);
    alias_found = false;
    const auto reserved_owner_alias_result =
        jcvm::PersistentInventory::findAliasesToAppletOwner(owner, alias_found);
    CandidateTags reserved_owner_candidates;
    const auto reserved_owner_inventory_result =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            owner, heap, no_roots, collect_candidate,
            &reserved_owner_candidates);
    const bool reserved_owner_fails_closed =
        reserved_owner_alias_result ==
            jcvm::PersistentInventory::VisitResult::InvalidTag &&
        !alias_found &&
        reserved_owner_inventory_result !=
            jcvm::PersistentInventory::VisitResult::Complete &&
        reserved_owner_candidates.tags.empty();

    std::vector<uint8_t> malformed_wrapper = wrapper;
    malformed_wrapper.back() ^= 1;
    seed(source_static_tag, sizeof(source_static_tag), malformed_wrapper);
    alias_found = false;
    const auto malformed_alias_result =
        jcvm::PersistentInventory::findAliasesToAppletOwner(owner, alias_found);
    const bool malformed_alias_fails_closed =
        malformed_alias_result ==
            jcvm::PersistentInventory::VisitResult::InvalidTag &&
        !alias_found;

    constexpr uint8_t replacement_owner = 0x46;
    auto replacement_target_tag = target_tag;
    replacement_target_tag[1] = replacement_owner;
    auto replacement_child_tag = target_child_tag;
    replacement_child_tag[1] = replacement_owner;
    seed(replacement_target_tag.data(), replacement_target_tag.size(),
         byte_record(0x53));
    seed(replacement_child_tag.data(), replacement_child_tag.size(),
         byte_record(0x54));
    std::vector<uint8_t> replacement_wrapper{8, source_package,
        static_cast<uint8_t>(replacement_target_tag.size())};
    replacement_wrapper.insert(replacement_wrapper.end(),
                               replacement_target_tag.begin(),
                               replacement_target_tag.end());
    seed(source_static_tag, sizeof(source_static_tag), replacement_wrapper);

    CandidateTags replaced_owner_candidates;
    const auto after_replace_old_owner =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            owner, heap, no_roots, collect_candidate,
            &replaced_owner_candidates);
    const std::vector<uint8_t> replacement_target_bytes(
        replacement_target_tag.begin(), replacement_target_tag.end());
    const std::vector<uint8_t> replacement_child_bytes(
        replacement_child_tag.begin(), replacement_child_tag.end());
    const bool replaced_target_becomes_orphan =
        after_replace_old_owner ==
            jcvm::PersistentInventory::VisitResult::Complete &&
        replaced_owner_candidates.tags.size() == 2 &&
        std::find(replaced_owner_candidates.tags.begin(),
                  replaced_owner_candidates.tags.end(), target_bytes) !=
            replaced_owner_candidates.tags.end() &&
        std::find(replaced_owner_candidates.tags.begin(),
                  replaced_owner_candidates.tags.end(), child_bytes) !=
            replaced_owner_candidates.tags.end();

    jcvm::Heap replacement_heap(replacement_owner);
    CandidateTags replacement_owner_candidates;
    const auto after_replace_new_owner =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            replacement_owner, replacement_heap, no_roots, collect_candidate,
            &replacement_owner_candidates);
    const bool replacement_target_stays_rooted =
        after_replace_new_owner ==
            jcvm::PersistentInventory::VisitResult::Complete &&
        std::find(replacement_owner_candidates.tags.begin(),
                  replacement_owner_candidates.tags.end(),
                  replacement_target_bytes) ==
            replacement_owner_candidates.tags.end() &&
        std::find(replacement_owner_candidates.tags.begin(),
                  replacement_owner_candidates.tags.end(),
                  replacement_child_bytes) ==
            replacement_owner_candidates.tags.end();

    seed(source_static_tag, sizeof(source_static_tag), {0});
    CandidateTags orphan_candidates;
    const auto after_clear =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            replacement_owner, replacement_heap, no_roots, collect_candidate,
            &orphan_candidates);
    const bool orphan_reclaimed_as_candidate =
        after_clear == jcvm::PersistentInventory::VisitResult::Complete &&
        orphan_candidates.tags.size() == 2 &&
        std::find(orphan_candidates.tags.begin(), orphan_candidates.tags.end(),
                  replacement_target_bytes) != orphan_candidates.tags.end() &&
        std::find(orphan_candidates.tags.begin(), orphan_candidates.tags.end(),
                  replacement_child_bytes) != orphan_candidates.tags.end();
    expect(target_retained,
           "foreign_static_wrapper_roots_target_and_nested_records");
    expect(valid_alias_detected,
           "foreign_static_wrapper_blocks_delete_of_its_target_owner");
    expect(reserved_owner_fails_closed,
           "foreign_static_wrapper_rejects_reserved_target_owner");
    expect(malformed_alias_fails_closed,
           "malformed_foreign_static_wrapper_fails_delete_alias_scan_closed");
    expect(replaced_target_becomes_orphan && replacement_target_stays_rooted,
           "replacing_foreign_static_wrapper_orphans_old_owner_only");
    expect(orphan_reclaimed_as_candidate,
           "cleared_foreign_static_wrapper_releases_target_records");
  }

  {
    constexpr uint8_t owner = 0x54;
    constexpr uint8_t package_id = 0x56;
    const uint16_t previous_field_count =
        persistent_transaction_test::instance_field_count;
    persistent_transaction_test::instance_field_count = 1;
    jcvm::Heap heap(owner);
    const auto root_tag =
        jcvm::FlashMemory_Handler::getAppletInstanceTag(1, package_id, 3);
    const auto instance_reference = heap.addInstance(
        jcvm::JC_Instance(heap, package_id, 3));
    const auto array_reference = heap.addArray(4, jcvm::JAVA_ARRAY_T_BYTE);
    heap.getArray(array_reference)->setByteEntry(0, 0x5a);
    auto instance = heap.getInstance(instance_reference);
    instance->setField_Reference(0, array_reference);
    jcvm::FlashMemory_Handler::setPersistentField_Instance(root_tag, *instance,
                                                           heap);
    std::array<std::array<uint8_t, 4>, 8> orphan_tags{};
    for (uint8_t index = 0; index < orphan_tags.size(); ++index) {
      orphan_tags[index] = {0x03, owner, 0x30,
                            static_cast<uint8_t>(index + 1)};
      persistent_transaction_test::seed(orphan_tags[index].data(),
                                        orphan_tags[index].size(),
                                        byte_record(index));
    }
    const auto instance_body = instance->getPersistentTag();
    const auto array_field =
        jcvm::FlashMemory_Handler::computeTag(instance_body, 0);
    const auto alias_record = persistent_transaction_test::read(
        array_field.value, array_field.len);
    jcvm::fs::Tag array_body{};
    bool alias_valid = alias_record.size() >= 3 && alias_record[0] == 0xfd &&
                       alias_record[1] + 2u == alias_record.size() &&
                       alias_record[1] <= sizeof(array_body.value);
    if (alias_valid) {
      array_body.len = alias_record[1];
      std::memcpy(array_body.value, alias_record.data() + 2, array_body.len);
    }

    std::vector<jcvm::jref_t> root_references{instance_reference};
    RootVectorContext roots{&root_references};
    uint32_t erased = 0;
    const auto result = jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
        owner, heap, enumerate_root_vector, &roots, &erased, 8);
    bool restored_array_survives = false;
    try {
      jcvm::Heap restored(owner);
      const auto restored_instance =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              root_tag, restored);
      const auto restored_field =
          jcvm::FlashMemory_Handler::computeTag(
              restored.getInstance(restored_instance)->getPersistentTag(), 0);
      const auto restored_array =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              restored_field, restored);
      const auto post_batch_gc =
          restored.collectUnreachableFrom({restored_instance});
      restored_array_survives =
          restored.getArray(restored_array)->getByteEntry(0) == 0x5a &&
          post_batch_gc.complete && post_batch_gc.arrays_reachable == 1 &&
          post_batch_gc.arrays_reclaimed == 0;
    } catch (jcvm::Exceptions) {
      restored_array_survives = false;
    }
    expect(alias_valid && result == jcvm::PersistentInventory::VisitResult::Stopped &&
               erased == 8 &&
               !persistent_transaction_test::read(array_body.value,
                                                  array_body.len).empty() &&
               restored_array_survives,
           "reclaimer_batch_preserves_instance_array_through_next_heap_collection");
    persistent_transaction_test::instance_field_count = previous_field_count;
  }

  {
    constexpr uint8_t owner = 0x55;
    constexpr uint8_t array_alias_owner = 0x56;
    constexpr uint8_t object_alias_owner = 0x57;
    const uint8_t target_tag[] = {0x03, owner, 0x22, 0x01};
    const uint8_t target_child_tag[] = {0x03, owner, 0x22, 0x01, 0x00, 0x01};
    const uint8_t orphan_tag[] = {0x03, owner, 0x44, 0x01};
    const uint8_t array_alias_tag[] = {0x03, array_alias_owner, 0x30, 0x01};
    const uint8_t object_alias_tag[] = {0x03, object_alias_owner, 0x30, 0x02};
    seed(target_tag, sizeof(target_tag), byte_record(0x61));
    seed(target_child_tag, sizeof(target_child_tag), byte_record(0x62));
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x63));

    std::vector<uint8_t> object_alias{0xfe,
                                      static_cast<uint8_t>(sizeof(target_tag))};
    object_alias.insert(object_alias.end(), target_tag,
                        target_tag + sizeof(target_tag));
    seed(object_alias_tag, sizeof(object_alias_tag), object_alias);
    std::vector<uint8_t> array_alias{0xfd,
                                     static_cast<uint8_t>(sizeof(object_alias_tag))};
    array_alias.insert(array_alias.end(), object_alias_tag,
                       object_alias_tag + sizeof(object_alias_tag));
    seed(array_alias_tag, sizeof(array_alias_tag), array_alias);

    jcvm::Heap heap(owner);
    const std::vector<jcvm::jref_t> no_roots;
    CandidateTags candidates;
    const auto inventory_result =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            owner, heap, no_roots, collect_candidate, &candidates);
    const std::vector<uint8_t> target_bytes(
        target_tag, target_tag + sizeof(target_tag));
    const std::vector<uint8_t> child_bytes(
        target_child_tag, target_child_tag + sizeof(target_child_tag));
    const bool inventory_protects_alias_subtree =
        inventory_result == jcvm::PersistentInventory::VisitResult::Complete &&
        std::find(candidates.tags.begin(), candidates.tags.end(), target_bytes) ==
            candidates.tags.end() &&
        std::find(candidates.tags.begin(), candidates.tags.end(), child_bytes) ==
            candidates.tags.end() &&
        std::find(candidates.tags.begin(), candidates.tags.end(),
                  std::vector<uint8_t>(orphan_tag,
                                       orphan_tag + sizeof(orphan_tag))) !=
            candidates.tags.end();

    RootVectorContext roots{&no_roots};
    uint32_t erased = 0;
    const auto first_reclaim =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased);
    const bool aliases_preserved =
        first_reclaim == jcvm::PersistentInventory::VisitResult::Complete &&
        erased == 1 &&
        !persistent_transaction_test::read(target_tag, sizeof(target_tag)).empty() &&
        !persistent_transaction_test::read(target_child_tag,
                                           sizeof(target_child_tag)).empty() &&
        persistent_transaction_test::read(orphan_tag, sizeof(orphan_tag)).empty();

    jcvm::fs::Tag array_alias_fs_tag{};
    array_alias_fs_tag.len = sizeof(array_alias_tag);
    std::memcpy(array_alias_fs_tag.value, array_alias_tag,
                sizeof(array_alias_tag));
    jcvm::fs::Tag object_alias_fs_tag{};
    object_alias_fs_tag.len = sizeof(object_alias_tag);
    std::memcpy(object_alias_fs_tag.value, object_alias_tag,
                sizeof(object_alias_tag));
    const bool aliases_erased =
        jcvm::FlashMemory_Handler::erasePersistentTag(array_alias_fs_tag) &&
        jcvm::FlashMemory_Handler::erasePersistentTag(object_alias_fs_tag);
    erased = 0;
    const auto second_reclaim =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased);
    expect(inventory_protects_alias_subtree && aliases_preserved &&
               aliases_erased &&
               second_reclaim ==
                   jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 2 &&
               persistent_transaction_test::read(target_tag,
                                                  sizeof(target_tag)).empty() &&
               persistent_transaction_test::read(target_child_tag,
                                                  sizeof(target_child_tag)).empty(),
           "reclaimer_preserves_foreign_object_and_array_alias_chains");
  }

  {
    constexpr uint8_t owner = 0x5a;
    const uint8_t first_tag[] = {0x03, owner, 0x20, 0x01};
    const uint8_t second_tag[] = {0x03, owner, 0x20, 0x02};
    seed(first_tag, sizeof(first_tag), byte_record(0x31));
    seed(second_tag, sizeof(second_tag), byte_record(0x32));

    jcvm::Heap heap(owner);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    uint32_t erased = 0;
    const auto first_pass =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased, 1);
    const bool one_tag_per_pass =
        first_pass == jcvm::PersistentInventory::VisitResult::Stopped &&
        erased == 1 &&
        (read(first_tag, sizeof(first_tag)).empty() !=
         read(second_tag, sizeof(second_tag)).empty());

    erased = 0;
    const auto second_pass =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased, 1);
    const bool resumed_to_completion =
        second_pass == jcvm::PersistentInventory::VisitResult::Stopped &&
        erased == 1;
    erased = 0;
    const auto final_pass =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased, 1);
    expect(one_tag_per_pass && resumed_to_completion &&
               final_pass == jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 0 && read(first_tag, sizeof(first_tag)).empty() &&
               read(second_tag, sizeof(second_tag)).empty(),
           "reclaimer_resumes_bounded_applet_deletion_until_complete");
  }

  {
    constexpr uint8_t owner = 0x43;
    constexpr uint8_t unrelated_owner = 0x44;
    constexpr uint8_t record_count = 19;
    constexpr uint32_t batch_limit = 8;
    std::array<std::array<uint8_t, 4>, record_count> tags{};
    for (uint8_t index = 0; index < record_count; ++index) {
      tags[index] = {0x03, owner, 0x20,
                     static_cast<uint8_t>(index + 1)};
      seed(tags[index].data(), tags[index].size(), byte_record(index));
    }
    const uint8_t unrelated_tag[] = {0x03, unrelated_owner, 0x20, 0x01};
    seed(unrelated_tag, sizeof(unrelated_tag), byte_record(0x7f));

    jcvm::Heap heap(owner);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    auto remaining_records = [&]() {
      uint32_t count = 0;
      for (const auto &tag : tags) {
        if (!read(tag.data(), tag.size()).empty()) ++count;
      }
      return count;
    };
    uint32_t erased = 0;
    const auto first_pass =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased, batch_limit);
    const bool first_batch_bounded =
        first_pass == jcvm::PersistentInventory::VisitResult::Stopped &&
        erased == batch_limit && remaining_records() == record_count - batch_limit;

    erased = 0;
    const auto second_pass =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased, batch_limit);
    const bool second_batch_bounded =
        second_pass == jcvm::PersistentInventory::VisitResult::Stopped &&
        erased == batch_limit && remaining_records() == record_count - 2 * batch_limit;

    erased = 0;
    const auto final_pass =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased, batch_limit);
    const bool unrelated_owner_preserved =
        !read(unrelated_tag, sizeof(unrelated_tag)).empty();
    jcvm::fs::Tag unrelated_fs_tag{};
    unrelated_fs_tag.len = sizeof(unrelated_tag);
    std::memcpy(unrelated_fs_tag.value, unrelated_tag, sizeof(unrelated_tag));
    const bool unrelated_fixture_cleaned =
        jcvm::FlashMemory_Handler::erasePersistentTag(unrelated_fs_tag);
    expect(first_batch_bounded && second_batch_bounded &&
               final_pass == jcvm::PersistentInventory::VisitResult::Complete &&
               erased == record_count - 2 * batch_limit &&
               remaining_records() == 0 && unrelated_owner_preserved &&
               unrelated_fixture_cleaned,
           "reclaimer_caps_each_batch_and_resumes_across_multiple_batches");
  }

  {
    constexpr uint8_t applet_owner = 0x47;
    constexpr uint8_t package_id = 0xf4;
    constexpr uint8_t applet_records = 5;
    constexpr uint8_t package_records = 17;
    constexpr uint32_t batch_limit = 8;
    std::array<std::array<uint8_t, 4>, applet_records> applet_tags{};
    for (uint8_t index = 0; index < applet_records; ++index) {
      applet_tags[index] = {0x03, applet_owner, 0x20,
                            static_cast<uint8_t>(index + 1)};
      seed(applet_tags[index].data(), applet_tags[index].size(),
           byte_record(index));
    }
    std::array<std::array<uint8_t, 5>, package_records> package_tags{};
    for (uint8_t index = 0; index < package_records; ++index) {
      package_tags[index] = {0x02, package_id, 0x20,
                             static_cast<uint8_t>(index + 1), 0xfe};
      seed(package_tags[index].data(), package_tags[index].size(),
           byte_record(index));
    }

    jcvm::Heap heap(applet_owner);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    const auto first =
        jcvm::PersistentReclaimer::reclaimUnreachableObjectDeletionBatch(
            applet_owner, package_id, heap, enumerate_root_vector, &roots,
            batch_limit);
    const auto second =
        jcvm::PersistentReclaimer::reclaimUnreachableObjectDeletionBatch(
            applet_owner, package_id, heap, enumerate_root_vector, &roots,
            batch_limit);
    const auto third =
        jcvm::PersistentReclaimer::reclaimUnreachableObjectDeletionBatch(
            applet_owner, package_id, heap, enumerate_root_vector, &roots,
            batch_limit);
    const auto all_removed = [&]() {
      return std::all_of(
                 applet_tags.begin(), applet_tags.end(), [](const auto &tag) {
                   return read(tag.data(), tag.size()).empty();
                 }) &&
             std::all_of(
                 package_tags.begin(), package_tags.end(), [](const auto &tag) {
                   return read(tag.data(), tag.size()).empty();
                 });
    };
    expect(first.erased_applet == 5 && first.erased_package == 3 &&
               first.erasedTotal() == batch_limit && !first.complete() &&
               second.erased_applet == 0 && second.erased_package == 8 &&
               second.erasedTotal() == batch_limit && !second.complete() &&
               third.erased_applet == 0 && third.erased_package == 6 &&
               third.erasedTotal() == 6 && third.complete() && all_removed(),
           "object_deletion_batch_shares_limit_across_applet_and_package_tags");
  }

  {
    constexpr uint8_t owner = 0x42;
    const uint8_t first_tag[] = {0x03, owner, 0x20, 0x01};
    const uint8_t second_tag[] = {0x03, owner, 0x20, 0x02};
    seed(first_tag, sizeof(first_tag), byte_record(0x21));
    seed(second_tag, sizeof(second_tag), byte_record(0x22));
    persistent_transaction_test::force_cursor_stall_after_one_result(owner);
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
    constexpr uint8_t owner = 0x58;
    constexpr uint8_t foreign_owner = 0x59;
    const uint8_t target_tag[] = {0x03, owner, 0x20, 0x01};
    const uint8_t orphan_tag[] = {0x03, owner, 0x30, 0x01};
    const uint8_t malformed_alias_tag[] = {0x03, foreign_owner, 0x40, 0x01};
    seed(target_tag, sizeof(target_tag), byte_record(0x71));
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x72));
    // Claims a five-byte alias target but stores only three target bytes.
    seed(malformed_alias_tag, sizeof(malformed_alias_tag),
         {0xfd, 0x05, 0x03, owner, 0x20});

    jcvm::Heap heap(owner);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    uint32_t erased = 99;
    const auto blocked =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased);
    const bool blocked_without_mutation =
        blocked == jcvm::PersistentInventory::VisitResult::InvalidTag &&
        erased == 0 &&
        read(target_tag, sizeof(target_tag)) == byte_record(0x71) &&
        read(orphan_tag, sizeof(orphan_tag)) == byte_record(0x72);

    jcvm::fs::Tag malformed_alias_fs_tag{};
    malformed_alias_fs_tag.len = sizeof(malformed_alias_tag);
    std::memcpy(malformed_alias_fs_tag.value, malformed_alias_tag,
                sizeof(malformed_alias_tag));
    const bool malformed_alias_erased =
        jcvm::FlashMemory_Handler::erasePersistentTag(malformed_alias_fs_tag);
    erased = 0;
    const auto retried =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            owner, heap, enumerate_root_vector, &roots, &erased);
    expect(blocked_without_mutation && malformed_alias_erased &&
               retried == jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 2 && read(target_tag, sizeof(target_tag)).empty() &&
               read(orphan_tag, sizeof(orphan_tag)).empty(),
           "malformed_foreign_alias_blocks_reclamation_then_retry_succeeds");
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

  {
    constexpr uint8_t candidate_count = 10;
    constexpr std::array<unsigned int, 5> interruption_points{0, 1, 7, 8, 9};
    bool every_interruption_recovers = true;
    bool other_owners_survive = true;
    for (uint8_t run = 0; run < interruption_points.size(); ++run) {
      const uint8_t owner = static_cast<uint8_t>(0x80 + run);
      const uint8_t other_owner = static_cast<uint8_t>(0x90 + run);
      std::array<std::array<uint8_t, 4>, candidate_count> candidates{};
      for (uint8_t index = 0; index < candidate_count; ++index) {
        candidates[index] = {0x03, owner, 0x40, index};
        seed(candidates[index].data(), candidates[index].size(),
             byte_record(index));
      }
      const std::array<uint8_t, 4> unrelated{
          0x03, other_owner, 0x40, 0x01};
      seed(unrelated.data(), unrelated.size(), byte_record(0xee));

      const std::vector<jcvm::jref_t> no_roots;
      RootVectorContext roots{&no_roots};
      jcvm::Heap before_reboot(owner);
      const unsigned int interruption = interruption_points[run];
      persistent_transaction_test::fail_erase_after(interruption);
      uint32_t erased_before_reboot = UINT32_MAX;
      const auto interrupted =
          jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
              owner, before_reboot, enumerate_root_vector, &roots,
              &erased_before_reboot);
      bool valid_interrupted_state =
          interrupted == jcvm::PersistentInventory::VisitResult::EraseFailure &&
          erased_before_reboot == interruption;
      for (uint8_t index = 0; index < candidate_count; ++index) {
        const auto value = persistent_transaction_test::read(
            candidates[index].data(), candidates[index].size());
        valid_interrupted_state &=
            value.empty() == (index < interruption);
      }
      valid_interrupted_state &=
          !persistent_transaction_test::read(unrelated.data(),
                                             unrelated.size()).empty();

      // A new Heap models the post-reset VM. Inventory is rebuilt from durable
      // records and retry must finish without touching another owner.
      jcvm::Heap after_reboot(owner);
      uint32_t erased_after_reboot = UINT32_MAX;
      const auto recovered =
          jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
              owner, after_reboot, enumerate_root_vector, &roots,
              &erased_after_reboot);
      bool valid_recovered_state =
          recovered == jcvm::PersistentInventory::VisitResult::Complete &&
          erased_after_reboot == candidate_count - interruption;
      for (const auto &tag : candidates) {
        valid_recovered_state &= persistent_transaction_test::read(
                                     tag.data(), tag.size()).empty();
      }
      other_owners_survive &=
          persistent_transaction_test::read(unrelated.data(),
                                            unrelated.size()) ==
          byte_record(0xee);
      every_interruption_recovers &=
          valid_interrupted_state && valid_recovered_state;
    }
    expect(every_interruption_recovers,
           "reclaimer_recovers_at_each_batch_boundary_and_neighbor");
    expect(other_owners_survive,
           "reclaimer_retries_never_erase_another_owner");
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

  {
    persistent_transaction_test::instance_field_count = 2;
    constexpr uint8_t package_id = 0xf0;
    const jcvm::fs::Tag static_root{3, {0x02, package_id, 0x08}};
    const jcvm::fs::Tag old_external_alias{3, {0x02, 0xf1, 0x09}};
    const jcvm::fs::Tag middle_external_alias{3, {0x02, 0xf1, 0x0a}};
    bool package_static_reclaimer_recovers_after_interruption = false;
    try {
      jcvm::Heap heap(0x40);
      const auto old_reference =
          heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
      auto old_instance = heap.getInstance(old_reference);
      old_instance->setField_Short(0, 0x1234);
      old_instance->setField_Short(1, 0x5678);
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          static_root, old_reference, heap);
      const jcvm::fs::Tag old_body = old_instance->getPersistentTag();

      const auto middle_reference =
          heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
      auto middle_instance = heap.getInstance(middle_reference);
      middle_instance->setField_Short(0, 0x3456);
      middle_instance->setField_Short(1, 0x789a);
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          static_root, middle_reference, heap);
      const jcvm::fs::Tag middle_body = middle_instance->getPersistentTag();

      const auto current_reference =
          heap.addInstance(jcvm::JC_Instance(heap, 2, 3));
      auto current_instance = heap.getInstance(current_reference);
      current_instance->setField_Short(0, 0x2345);
      current_instance->setField_Short(1, 0x6789);
      jcvm::FlashMemory_Handler::setPersistentField_Reference(
          static_root, current_reference, heap);
      const jcvm::fs::Tag current_body = current_instance->getPersistentTag();

      std::vector<uint8_t> old_alias_record{0xfe, old_body.len};
      old_alias_record.insert(old_alias_record.end(), old_body.value,
                              old_body.value + old_body.len);
      seed(old_external_alias.value, old_external_alias.len,
           std::move(old_alias_record));
      std::vector<uint8_t> middle_alias_record{0xfe, middle_body.len};
      middle_alias_record.insert(middle_alias_record.end(), middle_body.value,
                                 middle_body.value + middle_body.len);
      seed(middle_external_alias.value, middle_external_alias.len,
           std::move(middle_alias_record));

      CandidateTags protected_candidates;
      const auto protected_result =
          jcvm::PersistentInventory::visitUnreachablePackageStaticTags(
              package_id, heap, std::vector<jcvm::jref_t>{current_reference},
              collect_candidate, &protected_candidates);
      const bool alias_protected =
          protected_result ==
              jcvm::PersistentInventory::VisitResult::Complete &&
          protected_candidates.tags.empty();

      const bool aliases_removed =
          jcvm::FlashMemory_Handler::erasePersistentTag(old_external_alias) &&
          jcvm::FlashMemory_Handler::erasePersistentTag(
              middle_external_alias);
      CandidateTags orphan_candidates;
      const auto orphan_result =
          jcvm::PersistentInventory::visitUnreachablePackageStaticTags(
              package_id, heap, std::vector<jcvm::jref_t>{current_reference},
              collect_candidate, &orphan_candidates);
      const std::vector<jcvm::jref_t> current_roots{current_reference};
      RootVectorContext root_context{&current_roots};
      persistent_transaction_test::fail_erase_after(1);
      uint32_t erased = 0;
      const auto interrupted_result =
          jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
              package_id, heap, enumerate_root_vector, &root_context, &erased);
      CandidateTags remaining_candidates;
      const auto remaining_result =
          jcvm::PersistentInventory::visitUnreachablePackageStaticTags(
              package_id, heap, current_roots, collect_candidate,
              &remaining_candidates);

      jcvm::Heap recovered_heap(0x40);
      const auto recovered_current =
          jcvm::FlashMemory_Handler::getPersistentField_ReferenceStrict(
              static_root, recovered_heap);
      const std::vector<jcvm::jref_t> recovered_roots{recovered_current};
      RootVectorContext recovered_root_context{&recovered_roots};
      uint32_t recovered_erased = 0;
      const auto recovered_result =
          jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
              package_id, recovered_heap, enumerate_root_vector,
              &recovered_root_context, &recovered_erased);
      const auto has_prefix = [&orphan_candidates](const jcvm::fs::Tag &prefix) {
        return std::any_of(
            orphan_candidates.tags.begin(), orphan_candidates.tags.end(),
            [&prefix](const std::vector<uint8_t> &candidate) {
              return candidate.size() >= prefix.len &&
                     std::memcmp(candidate.data(), prefix.value,
                                 prefix.len) == 0;
            });
      };
      const std::vector<uint8_t> static_root_bytes(
          static_root.value, static_root.value + static_root.len);
      package_static_reclaimer_recovers_after_interruption =
          alias_protected && aliases_removed &&
          orphan_result ==
              jcvm::PersistentInventory::VisitResult::Complete &&
          orphan_candidates.tags.size() == 6 &&
          has_prefix(old_body) && has_prefix(middle_body) &&
          !has_prefix(current_body) &&
          std::find(orphan_candidates.tags.begin(), orphan_candidates.tags.end(),
                    static_root_bytes) == orphan_candidates.tags.end() &&
          interrupted_result ==
              jcvm::PersistentInventory::VisitResult::EraseFailure &&
          erased == 1 &&
          remaining_result ==
              jcvm::PersistentInventory::VisitResult::Complete &&
          remaining_candidates.tags.size() == 5 &&
          recovered_result ==
              jcvm::PersistentInventory::VisitResult::Complete &&
          recovered_erased == 5 &&
          persistent_transaction_test::read(old_body.value, old_body.len)
              .empty() &&
          persistent_transaction_test::read(middle_body.value,
                                             middle_body.len).empty() &&
          !persistent_transaction_test::read(current_body.value,
                                               current_body.len).empty() &&
          !persistent_transaction_test::read(static_root.value,
                                               static_root.len).empty();
    } catch (...) {
      package_static_reclaimer_recovers_after_interruption = false;
    }
    persistent_transaction_test::instance_field_count = 0;
    expect(package_static_reclaimer_recovers_after_interruption,
           "package_static_reclaimer_preserves_aliases_and_recovers_after_interruption");
  }

  {
    constexpr uint8_t package_id = 0xf2;
    const uint8_t target_tag[] = {0x02, package_id, 0x20, 0xff, 0xfe};
    const uint8_t orphan_tag[] = {0x02, package_id, 0x21, 0xff, 0xfe};
    const uint8_t malformed_alias_tag[] = {0x02, 0xf3, 0x20};
    seed(target_tag, sizeof(target_tag), byte_record(0x31));
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x32));
    seed(malformed_alias_tag, sizeof(malformed_alias_tag),
         {0xfe, 0x05, 0x02, package_id, 0x20});

    jcvm::Heap heap(0x41);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    uint32_t erased = 99;
    const auto blocked =
        jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
            package_id, heap, enumerate_root_vector, &roots, &erased);
    const bool blocked_without_mutation =
        blocked == jcvm::PersistentInventory::VisitResult::InvalidTag &&
        erased == 0 &&
        read(target_tag, sizeof(target_tag)) == byte_record(0x31) &&
        read(orphan_tag, sizeof(orphan_tag)) == byte_record(0x32);

    jcvm::fs::Tag malformed_alias_fs_tag{};
    malformed_alias_fs_tag.len = sizeof(malformed_alias_tag);
    std::memcpy(malformed_alias_fs_tag.value, malformed_alias_tag,
                sizeof(malformed_alias_tag));
    const bool malformed_alias_removed =
        jcvm::FlashMemory_Handler::erasePersistentTag(malformed_alias_fs_tag);
    erased = 0;
    const auto retried =
        jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
            package_id, heap, enumerate_root_vector, &roots, &erased);
    expect(blocked_without_mutation && malformed_alias_removed &&
               retried == jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 2 && read(target_tag, sizeof(target_tag)).empty() &&
               read(orphan_tag, sizeof(orphan_tag)).empty(),
           "package_static_reclaimer_fails_closed_on_malformed_alias");
  }

  {
    constexpr uint8_t package_id = 0xf4;
    const uint8_t orphan_tag[] = {0x02, package_id, 0x20, 0xff, 0xfe};
    seed(orphan_tag, sizeof(orphan_tag), byte_record(0x41));
    jcvm::Heap heap(0x42);
    const std::vector<jcvm::jref_t> no_roots;
    RootVectorContext roots{&no_roots};
    expect(FlashMemory_Handler::beginPersistentWriteTransaction(),
           "package_static_reclaimer_guard_begin_transaction");
    uint32_t erased = 99;
    const auto blocked =
        jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
            package_id, heap, enumerate_root_vector, &roots, &erased);
    const bool no_mutation =
        blocked == jcvm::PersistentInventory::VisitResult::TransactionActive &&
        erased == 0 && read(orphan_tag, sizeof(orphan_tag)) == byte_record(0x41);
    const bool rolled_back =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    erased = 0;
    const auto retried =
        jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
            package_id, heap, enumerate_root_vector, &roots, &erased);
    expect(no_mutation && rolled_back &&
               retried == jcvm::PersistentInventory::VisitResult::Complete &&
               erased == 1 && read(orphan_tag, sizeof(orphan_tag)).empty(),
           "package_static_reclaimer_refuses_active_transaction");
  }

  {
    constexpr uint16_t array_length = 128;
    const uint8_t tag[] = {0xa0, 0x01, 0xf5};
    std::vector<uint8_t> record{
        static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE),
        static_cast<uint8_t>(array_length >> 8),
        static_cast<uint8_t>(array_length)};
    for (uint16_t index = 0; index < array_length; ++index)
      record.push_back(static_cast<uint8_t>(index));
    seed(tag, sizeof(tag), record);

    jcvm::fs::Tag fs_tag{};
    fs_tag.len = sizeof(tag);
    std::memcpy(fs_tag.value, tag, sizeof(tag));
    std::array<uint8_t, array_length> copied{};
    persistent_transaction_test::reset_counted_fs_calls();
    jcvm::FlashMemory_Handler::readPersistentArrayRange(
        fs_tag, jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE, array_length, 0,
        array_length, 1, copied.data());
    const unsigned int read_calls =
        persistent_transaction_test::counted_fs_call_count();

    std::array<uint8_t, array_length> replacement{};
    replacement.fill(0xa5);
    persistent_transaction_test::reset_counted_fs_calls();
    jcvm::FlashMemory_Handler::writePersistentArrayRange(
        fs_tag, jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE, array_length, 0,
        array_length, 1, replacement.data(), true);
    const unsigned int write_calls =
        persistent_transaction_test::counted_fs_call_count();
    const auto updated = read(tag, sizeof(tag));
    const bool data_matches =
        std::equal(copied.begin(), copied.end(), record.begin() + 3) &&
        updated.size() == record.size() &&
        std::equal(replacement.begin(), replacement.end(), updated.begin() + 3);
    expect(data_matches && read_calls == 1 && write_calls == 2,
           "persistent_array_range_uses_one_read_and_one_validated_write");
    std::printf("BENCHMARK:PERSISTENT_ARRAY_RANGE bytes=%u read_fs_calls=%u "
                "write_fs_calls=%u total_fs_calls=%u\n",
                static_cast<unsigned>(array_length), read_calls,
                write_calls, read_calls + write_calls);

    jcvm::fs::Tag legacy_source_tag = fs_tag;
    jcvm::fs::Tag legacy_destination_tag = fs_tag;
    legacy_source_tag.value[2] = 0xf6;
    legacy_destination_tag.value[2] = 0xf7;
    seed(legacy_source_tag.value, legacy_source_tag.len, record);
    seed(legacy_destination_tag.value, legacy_destination_tag.len, record);
    jcvm::Heap legacy_heap(7);
    persistent_transaction_test::reset_counted_fs_calls();
    for (uint16_t index = 0; index < array_length; ++index) {
      const auto value = jcvm::FlashMemory_Handler::getPersistentField_Array_Byte(
          legacy_source_tag, index, legacy_heap);
      jcvm::FlashMemory_Handler::setPersistentField_Array_Byte(
          legacy_destination_tag, index, value, legacy_heap);
    }
    const unsigned int scalar_copy_calls =
        persistent_transaction_test::counted_fs_call_count();
    const unsigned int range_copy_calls = read_calls + write_calls;
    expect(scalar_copy_calls == 768 && range_copy_calls == 3,
           "persistent_array_copy_fs_calls_reduced_from_1024_to_768_and_3");
    std::printf("BENCHMARK:PERSISTENT_ARRAY_COPY bytes=%u "
                "baseline_fs_calls=1024 cached_scalar_fs_calls=%u "
                "range_fs_calls=%u\n",
                static_cast<unsigned>(array_length), scalar_copy_calls,
                range_copy_calls);
  }

  {
    const uint8_t tag[] = {0xa0, 0x01, 0xfa};
    constexpr uint16_t array_length = 16;
    std::vector<uint8_t> record{
        static_cast<uint8_t>(jcvm::FieldType::FIELD_TYPE_ARRAY_BYTE),
        static_cast<uint8_t>(array_length >> 8),
        static_cast<uint8_t>(array_length)};
    record.resize(3 + array_length, 0x5a);
    seed(tag, sizeof(tag), record);
    jcvm::fs::Tag fs_tag{};
    fs_tag.len = sizeof(tag);
    std::memcpy(fs_tag.value, tag, sizeof(tag));
    jcvm::Heap cache_heap(7);
    jcvm::JC_Array cached_array(cache_heap, jcvm::JAVA_ARRAY_T_BYTE, 0,
                                fs_tag, false, jcvm::None, 0);

    persistent_transaction_test::reset_counted_fs_calls();
    bool sizes_match = true;
    for (unsigned int i = 0; i < 128; ++i)
      sizes_match = sizes_match && cached_array.size() == array_length;
    const unsigned int warm_size_calls =
        persistent_transaction_test::counted_fs_call_count();
    expect(sizes_match && warm_size_calls == 1,
           "persistent_array_size_metadata_is_cached_after_validation");
    std::printf("BENCHMARK:PERSISTENT_ARRAY_SIZE iterations=128 "
                "baseline_fs_reads=256 cached_fs_reads=%u\n",
                warm_size_calls);

    expect(jcvm::FlashMemory_Handler::beginPersistentWriteTransaction(false),
           "persistent_array_metadata_cache_begin_rollback");
    expect(jcvm::FlashMemory_Handler::erasePersistentTag(fs_tag),
           "persistent_array_metadata_cache_erase_snapshot");
    expect(jcvm::FlashMemory_Handler::rollbackPersistentWriteTransaction(),
           "persistent_array_metadata_cache_rollback");
    persistent_transaction_test::reset_counted_fs_calls();
    expect(cached_array.size() == array_length &&
               persistent_transaction_test::counted_fs_call_count() == 1,
           "persistent_array_metadata_cache_reloads_after_rollback");

    const uint8_t replacement_tag[] = {0xa0, 0x01, 0xf9};
    seed(replacement_tag, sizeof(replacement_tag), record);
    fs_tag.value[2] = replacement_tag[2];
    persistent_transaction_test::reset_counted_fs_calls();
    cached_array.setPersistentTag(fs_tag);
    expect(cached_array.size() == array_length &&
               persistent_transaction_test::counted_fs_call_count() == 1,
           "persistent_array_metadata_cache_reloads_after_tag_change");
  }

  {
    const auto first = make_tag(0xfc);
    const auto second = make_tag(0xfd);
    const std::vector<uint8_t> first_original{0x10};
    const std::vector<uint8_t> second_original{0x20};
    const std::vector<uint8_t> first_tentative{0x11};
    const std::vector<uint8_t> second_tentative{0x21};
    persistent_transaction_test::seed(first.value, first.len, first_original);
    persistent_transaction_test::seed(second.value, second.len,
                                      second_original);
    bool began = FlashMemory_Handler::beginPersistentWriteTransaction();
    const bool writes_applied =
        FlashMemory_Handler::writePersistentRecord(
            first, first_tentative.data(), first_tentative.size()) &&
        FlashMemory_Handler::writePersistentRecord(
            second, second_tentative.data(), second_tentative.size());
    persistent_transaction_test::fail_write_after(1);
    const unsigned int syncs_before =
        persistent_transaction_test::sync_count_value();
    const bool rollback_failed =
        !FlashMemory_Handler::rollbackPersistentWriteTransaction();
    bool quarantined_read_blocked = false;
    try {
      (void)FlashMemory_Handler::readPersistentRecord(first);
    } catch (...) {
      quarantined_read_blocked = true;
    }
    const bool failed_rollback_quarantined =
        FlashMemory_Handler::persistentWriteTransactionActive() &&
        FlashMemory_Handler::persistentWriteRecoveryRequired() &&
        !FlashMemory_Handler::beginPersistentWriteTransaction() &&
        quarantined_read_blocked &&
        !FlashMemory_Handler::writePersistentRecord(
            first, first_original.data(), first_original.size()) &&
        persistent_transaction_test::sync_count_value() == syncs_before &&
        persistent_transaction_test::read_durable(first.value, first.len) ==
            first_original &&
        persistent_transaction_test::read_durable(second.value, second.len) ==
            second_original;
    const bool retry_succeeded =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool retry_restored_durable_state =
        !FlashMemory_Handler::persistentWriteTransactionActive() &&
        !FlashMemory_Handler::persistentWriteRecoveryRequired() &&
        persistent_transaction_test::read_durable(first.value, first.len) ==
            first_original &&
        persistent_transaction_test::read_durable(second.value, second.len) ==
            second_original;
    expect(began && writes_applied && rollback_failed &&
               quarantined_read_blocked &&
               failed_rollback_quarantined && retry_succeeded &&
               retry_restored_durable_state,
           "rollback_write_failure_retains_journal_and_retries_without_syncing_partial_state");

    const auto created = make_tag(0xfe);
    const std::vector<uint8_t> created_value{0x31};
    began = FlashMemory_Handler::beginPersistentWriteTransaction();
    const bool created_write = FlashMemory_Handler::writePersistentRecord(
        created, created_value.data(), created_value.size());
    persistent_transaction_test::fail_erase_after(0);
    const unsigned int erase_syncs_before =
        persistent_transaction_test::sync_count_value();
    const bool erase_rollback_failed =
        !FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool erase_failure_quarantined =
        FlashMemory_Handler::persistentWriteTransactionActive() &&
        FlashMemory_Handler::persistentWriteRecoveryRequired() &&
        persistent_transaction_test::sync_count_value() == erase_syncs_before &&
        persistent_transaction_test::read_durable(created.value, created.len)
            .empty();
    const bool erase_retry_succeeded =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    expect(began && created_write && erase_rollback_failed &&
               erase_failure_quarantined && erase_retry_succeeded &&
               !FlashMemory_Handler::persistentWriteTransactionActive() &&
               !FlashMemory_Handler::persistentWriteRecoveryRequired() &&
               persistent_transaction_test::read_durable(created.value,
                                                          created.len)
                   .empty(),
           "rollback_erase_failure_retains_journal_and_retries");

    const auto sync_tag = make_tag(0xfb);
    const std::vector<uint8_t> sync_original{0x41};
    const std::vector<uint8_t> sync_tentative{0x42};
    persistent_transaction_test::seed(sync_tag.value, sync_tag.len,
                                      sync_original);
    began = FlashMemory_Handler::beginPersistentWriteTransaction();
    const bool sync_write = FlashMemory_Handler::writePersistentRecord(
        sync_tag, sync_tentative.data(), sync_tentative.size());
    persistent_transaction_test::fail_one_sync();
    const bool rollback_sync_failed =
        !FlashMemory_Handler::rollbackPersistentWriteTransaction();
    const bool sync_failure_quarantined =
        FlashMemory_Handler::persistentWriteTransactionActive() &&
        FlashMemory_Handler::persistentWriteRecoveryRequired() &&
        persistent_transaction_test::read_durable(sync_tag.value, sync_tag.len)
            == sync_original;
    const bool sync_retry_succeeded =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    expect(began && sync_write && rollback_sync_failed &&
               sync_failure_quarantined && sync_retry_succeeded &&
               !FlashMemory_Handler::persistentWriteTransactionActive() &&
               !FlashMemory_Handler::persistentWriteRecoveryRequired() &&
               persistent_transaction_test::read_durable(sync_tag.value,
                                                          sync_tag.len) ==
                   sync_original,
           "rollback_sync_failure_retains_journal_and_retries");

    const auto commit_tag = make_tag(0xfa);
    const std::vector<uint8_t> commit_original{0x51};
    const std::vector<uint8_t> commit_tentative{0x52};
    persistent_transaction_test::seed(commit_tag.value, commit_tag.len,
                                      commit_original);
    began = FlashMemory_Handler::beginPersistentWriteTransaction();
    const bool commit_write = FlashMemory_Handler::writePersistentRecord(
        commit_tag, commit_tentative.data(), commit_tentative.size());
    persistent_transaction_test::fail_sync_times(2);
    const bool commit_sync_failed =
        !FlashMemory_Handler::commitPersistentWriteTransaction();
    const bool commit_rollback_sync_failed =
        FlashMemory_Handler::persistentWriteTransactionActive() &&
        FlashMemory_Handler::persistentWriteRecoveryRequired() &&
        persistent_transaction_test::read_durable(commit_tag.value,
                                                  commit_tag.len) ==
            commit_original;
    const bool commit_recovery_succeeded =
        FlashMemory_Handler::rollbackPersistentWriteTransaction();
    expect(began && commit_write && commit_sync_failed &&
               commit_rollback_sync_failed && commit_recovery_succeeded &&
               !FlashMemory_Handler::persistentWriteTransactionActive() &&
               !FlashMemory_Handler::persistentWriteRecoveryRequired() &&
               persistent_transaction_test::read_durable(commit_tag.value,
                                                          commit_tag.len) ==
                   commit_original,
           "commit_and_rollback_sync_failures_preserve_recovery_journal");
  }

  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
