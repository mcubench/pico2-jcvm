#include "../src/heap.hpp"
#include "../src/jc_types/jc_array.hpp"

#include <cstdint>
#include <iostream>

int main() {
  jcvm::Heap heap(0);
  jcvm::JC_Array bytes(heap, 7, jcvm::JAVA_ARRAY_T_BYTE, true,
                       jcvm::CLEAR_ON_DESELECT);
  jcvm::JC_Array shorts(heap, 5, jcvm::JAVA_ARRAY_T_SHORT, true,
                        jcvm::CLEAR_ON_RESET);

  if (bytes.size() != 7 || shorts.size() != 5 ||
      !bytes.isTransientArray() || !shorts.isTransientArray() ||
      bytes.getClearEvent() != jcvm::CLEAR_ON_DESELECT ||
      shorts.getClearEvent() != jcvm::CLEAR_ON_RESET) {
    std::cerr << "transient array size or clear event mismatch\n";
    return 1;
  }

  for (uint16_t index = 0; index < bytes.size(); ++index) {
    bytes.setByteEntry(index, static_cast<jcvm::jbyte_t>(index + 1));
  }
  for (uint16_t index = 0; index < shorts.size(); ++index) {
    shorts.setShortEntry(index, static_cast<jcvm::jshort_t>(index + 1));
  }

  const jcvm::jref_t bytesRef = heap.addArray(bytes);
  const jcvm::jref_t shortsRef = heap.addArray(shorts);
  heap.clearTransientArrays(jcvm::CLEAR_ON_RESET);

  auto storedBytes = heap.getArray(bytesRef);
  auto storedShorts = heap.getArray(shortsRef);
  if (storedBytes->getByteEntry(0) != 1 ||
      storedShorts->getShortEntry(0) != 0) {
    std::cerr << "clear-on-reset event cleared the wrong arrays\n";
    return 1;
  }

  heap.clearTransientArrays(jcvm::CLEAR_ON_DESELECT);
  if (storedBytes->getByteEntry(0) != 0) {
    std::cerr << "clear-on-deselect event did not clear its array\n";
    return 1;
  }

  std::cout << "TEST:PASS jcarray_transient_size\n";
  std::cout << "TEST:PASS jcarray_clear_events\n";
  return 0;
}
