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

  std::cout << "TEST:PASS jcarray_transient_size\n";
  return 0;
}
