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

// RP2350 filesystem usage snapshot for bounded low-space reclamation and
// telemetry. Each output pointer is optional.
void choupi_persistent_memory_usage(uint32_t *files_used,
                                    uint32_t *bytes_used,
                                    uint32_t *byte_capacity);
uint8_t choupi_compact_persistent_memory(void);
// Tombstones created since the last successful compaction.
uint32_t choupi_persistent_tombstones_since_compaction(void);

// RP2350 filesystem activity since the previous call: record writes and
// erases in the RAM overlay, and how syncs were committed: full flash
// snapshot rewrites (`syncs_written`, `sync_bytes` erased and programmed),
// journal appends (`syncs_journaled`, `journal_bytes` programmed), or
// skipped because the net content equalled the committed state
// (`syncs_elided`).
struct ChoupiFsApduStats {
  uint32_t writes;
  uint32_t write_bytes;
  uint32_t erases;
  uint32_t syncs_written;
  uint32_t syncs_elided;
  uint32_t sync_bytes;
  uint32_t syncs_journaled;
  uint32_t journal_bytes;
};
void choupi_fs_take_apdu_stats(struct ChoupiFsApduStats *stats);

#ifdef __cplusplus
}
#endif

#endif // PICO2_JCVM_VM_FILESYSTEM_H
