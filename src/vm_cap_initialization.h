#ifndef PICO2_JCVM_VM_CAP_INITIALIZATION_H
#define PICO2_JCVM_VM_CAP_INITIALIZATION_H

#include <cstdint>

namespace jcvm {

// Initialize the CAP-declared static arrays for a package before its install
// method executes. Heap allocation and persistent static-reference details
// remain inside the VM implementation.
bool initializeCapStaticArrays(uint8_t package_id);

} // namespace jcvm

#endif // PICO2_JCVM_VM_CAP_INITIALIZATION_H
