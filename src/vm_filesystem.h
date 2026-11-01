#ifndef PICO2_JCVM_VM_FILESYSTEM_H
#define PICO2_JCVM_VM_FILESYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Backend extensions implemented by the repository's PC and RP2350
// filesystem adapters. These are not exports from the pinned Rust OS FFI.
uint8_t fs_sync(void);
uint8_t fs_erase_prefix(const uint8_t *prefix, uint8_t prefix_length);
uint8_t fs_erase_package_state(uint8_t package_id);

#ifdef __cplusplus
}
#endif

#endif // PICO2_JCVM_VM_FILESYSTEM_H
