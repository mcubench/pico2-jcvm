#include "../src/jcvm_types/jcvmarray.hpp"

#include <cstdint>
#include <iostream>

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
  std::cout << "TEST:PASS jcvmarray_fill\n";
  return 0;
}
