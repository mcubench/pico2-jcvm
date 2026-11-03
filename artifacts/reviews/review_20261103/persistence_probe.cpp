// Review evidence: real Static_Handler/FlashMemory_Handler/Heap over host fake records.
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_static.hpp"
#include "heap.hpp"
#include "jc_types/jc_array.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
namespace jcvm { jpackage_ID_t Package::getPackageID() const noexcept { return this->ID; } }
namespace persistent_transaction_test {
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
