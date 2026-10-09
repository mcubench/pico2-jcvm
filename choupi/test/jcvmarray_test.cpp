#include "../src/jcvm_types/jcvmarray.hpp"

#include <cstdint>
#include <iostream>
#include <utility>

int main() {
  jcvm::JCVMArray<uint16_t> values(4);
  values.fill(0xBEEF);

  for (uint16_t index = 0; index < values.size(); ++index) {
    if (values[index] != 0xBEEF) {
      std::cerr << "fill left element " << index << " unchanged\n";
      return 1;
    }
  }

  jcvm::JCVMArray<uint16_t> empty(0);
  empty.fill(0x1234);

  jcvm::JCVMArray<uint16_t> copied(values);
  copied[0] = 0x1234;
  if (values[0] != 0xBEEF) {
    std::cerr << "copy shares mutable storage\n";
    return 1;
  }

  jcvm::JCVMArray<uint16_t> moved(std::move(copied));
  if (moved.size() != values.size() || moved[0] != 0x1234 ||
      copied.size() != 0) {
    std::cerr << "move did not transfer storage\n";
    return 1;
  }

  uint16_t external_data[] = {0x1111, 0x2222};
  jcvm::JCVMArray<const uint16_t> view(2, external_data);
  jcvm::JCVMArray<const uint16_t> copied_view(view);
  external_data[0] = 0x3333;
  if (copied_view[0] != 0x3333) {
    std::cerr << "copy of read-only view lost its backing storage\n";
    return 1;
  }

  std::cout << "TEST:PASS jcvmarray_fill\n";
  std::cout << "TEST:PASS jcvmarray_copy_move\n";
  return 0;
}
