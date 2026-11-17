// Review evidence: real Static_Handler/FlashMemory_Handler/Heap over host fake records.
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_static.hpp"
#include "heap.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
namespace jcvm { jpackage_ID_t Package::getPackageID() const noexcept { return this->ID; } }
namespace persistent_transaction_test {
extern uint16_t instance_field_count;
void fail_one_write();
bool track_array_allocations = false;
unsigned int tracked_array_allocations = 0, tracked_array_deallocations = 0;
}
extern "C" void path_static(uint8_t package, uint8_t id,
    uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 2; (*tag)[1] = package; (*tag)[2] = id; *length = 3;
}
int main(int argc, char **argv) {
  using namespace jcvm;
  const bool alias_mode = argc > 1 && std::strcmp(argv[1], "array-retarget") == 0;
  Heap heap(7);
  Static_Handler handler{Package(20)};
  if (argc > 1 && std::strcmp(argv[1], "short-serialization-overflow") == 0) {
    const auto ref = heap.addArray(32767, JAVA_ARRAY_T_SHORT);
    handler.setPersistentRef(1, ref, heap);
    std::puts("SHORT_SERIALIZATION unexpectedly accepted wrapped record length");
    return 1;
  }
  if (argc > 1 && (std::strcmp(argv[1], "object-retarget") == 0 ||
                   std::strcmp(argv[1], "instance-write-failure") == 0 ||
                   std::strcmp(argv[1], "failed-first-persist") == 0 ||
                   std::strcmp(argv[1], "object-self-assignment") == 0)) {
    persistent_transaction_test::instance_field_count = 1;
    const auto old_ref = heap.addInstance(20, 0);
    const auto new_ref = heap.addInstance(20, 0);
    auto old_object = heap.getInstance(old_ref);
    auto new_object = heap.getInstance(new_ref);
    old_object->setField_Short(0, 0x11);
    new_object->setField_Short(0, 0x22);
    if (std::strcmp(argv[1], "failed-first-persist") == 0) {
      persistent_transaction_test::fail_one_write();
      bool threw = false;
      try { handler.setPersistentRef(1, old_ref, heap); } catch (...) { threw = true; }
      bool retry_succeeded = false, fresh_restore_succeeded = false;
      try { handler.setPersistentRef(1, old_ref, heap); retry_succeeded = true; } catch (...) {}
      Heap fresh(7);
      const auto tag = FlashMemory_Handler::getStaticFieldTag(20, 1);
      try {
        const auto ref = FlashMemory_Handler::getPersistentField_ReferenceStrict(tag, fresh);
        fresh_restore_succeeded = fresh.getInstance(ref)->getField_Short(0) == 0x11;
      } catch (...) {}
      std::printf("FAILED_FIRST_PERSIST threw=%u has_tag=%u retry_succeeded=%u fresh_restore_succeeded=%u expected=1\n",
          threw, old_object->hasPersistentTag(), retry_succeeded, fresh_restore_succeeded);
      return threw && retry_succeeded && fresh_restore_succeeded ? 0 : 1;
    }
    handler.setPersistentRef(1, old_ref, heap);
    if (std::strcmp(argv[1], "object-self-assignment") == 0) {
      handler.setPersistentRef(1, old_ref, heap);
      Heap fresh(7);
      const auto tag = FlashMemory_Handler::getStaticFieldTag(20, 1);
      bool restored = false;
      try {
        const auto ref = FlashMemory_Handler::getPersistentField_ReferenceStrict(tag, fresh);
        restored = fresh.getInstance(ref)->getField_Short(0) == 0x11;
      } catch (...) {}
      std::printf("OBJECT_SELF_ASSIGNMENT fresh_restore_succeeded=%u expected=1\n", restored);
      return restored ? 0 : 1;
    }
    if (std::strcmp(argv[1], "instance-write-failure") == 0) {
      persistent_transaction_test::fail_one_write();
      bool threw = false;
      try { old_object->setField_Short(0, 0x22); } catch (...) { threw = true; }
      const auto observed = old_object->getField_Short(0);
      Heap fresh(7);
      const auto tag = FlashMemory_Handler::getStaticFieldTag(20, 1);
      const auto restored = FlashMemory_Handler::getPersistentField_Reference(tag, fresh);
      const auto durable = fresh.getInstance(restored)->getField_Short(0);
      std::printf("INSTANCE_WRITE_FAILURE threw=%u cached=%04x durable=%04x expected=0011/0011\n",
          threw, static_cast<uint16_t>(observed), static_cast<uint16_t>(durable));
      return threw && observed == 0x11 && durable == 0x11 ? 0 : 1;
    }
    handler.setPersistentRef(2, old_ref, heap);
    handler.setPersistentRef(1, new_ref, heap);
    Heap fresh(7);
    const auto alias_tag = FlashMemory_Handler::getStaticFieldTag(20, 2);
    const auto restored_alias = FlashMemory_Handler::getPersistentField_Reference(alias_tag, fresh);
    const auto value = fresh.getInstance(restored_alias)->getField_Short(0);
    std::printf("OBJECT_RETARGET alias_after_restore=%04x expected=0011\n", static_cast<uint16_t>(value));
    return value == 0x11 ? 0 : 1;
  }

  const auto old_ref = heap.addArray(1, JAVA_ARRAY_T_BYTE);
  const auto new_ref = heap.addArray(1, JAVA_ARRAY_T_BYTE);
  heap.getArray(old_ref)->setByteEntry(0, 0x11);
  heap.getArray(new_ref)->setByteEntry(0, 0x22);
  handler.setPersistentRef(1, old_ref, heap);
  if (alias_mode) {
    handler.setPersistentRef(2, old_ref, heap);
    handler.setPersistentRef(1, new_ref, heap);
    const auto old_byte = heap.getArray(old_ref)->getByteEntry(0);
    const auto alias = handler.getPersistentRef(2, heap);
    const auto alias_byte = heap.getArray(alias)->getByteEntry(0);
    std::printf("ARRAY_RETARGET old=%04x new=%04x old_byte=%02x alias_byte=%02x expected=11/11\n",
        old_ref.compact(), new_ref.compact(), static_cast<unsigned char>(old_byte), static_cast<unsigned char>(alias_byte));
    return old_byte == 0x11 && alias_byte == 0x11 ? 0 : 1;
  }
  const bool begun = FlashMemory_Handler::beginPersistentWriteTransaction();
  handler.setPersistentRef(1, jref_t{}, heap);
  const bool aborted = FlashMemory_Handler::rollbackPersistentWriteTransaction();
  const auto observed = handler.getPersistentRef(1, heap);
  const auto tag = FlashMemory_Handler::getStaticFieldTag(20, 1);
  Heap fresh_heap(7);
  const auto durable = FlashMemory_Handler::getPersistentField_Reference(tag, fresh_heap);
  const auto durable_byte = fresh_heap.getArray(durable)->getByteEntry(0);
  std::printf("STATIC_ABORT begin=%u abort=%u old=%04x cached=%04x restored_record_byte=%02x expected_ref=%04x\n",
      begun, aborted, old_ref.compact(), observed.compact(),
      static_cast<unsigned char>(durable_byte), old_ref.compact());
  return observed == old_ref && durable_byte == 0x11 ? 0 : 1;
}
