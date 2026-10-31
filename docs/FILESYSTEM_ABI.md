# Filesystem C ABI contract

This document records the current `fs_*` contract used by the VM and loader.
The PC emulator delegates record operations to the CHOUPI OS filesystem; Pico
uses the bounded filesystem overlay in `src/choupi_pico2.cpp`. They share the
success convention but do not yet share detailed error codes or all bounds.

## Call and buffer rules

- Call `fs_init()` successfully before any other filesystem operation. Its
  nonzero result means initialization failed. The PC implementation initializes
  its emulated flash filesystem during emulator startup.
- `tag` and input `data` are borrowed only for the duration of the call. Output
  buffers for `fs_read`, scalar reads, `fs_length`, and `fs_read_inplace` must
  be writable for the documented output size. Pointers must be non-null when
  the corresponding length is nonzero. The FFI does not validate every pointer
  or buffer length; invalid caller memory can fault.
- The OS filesystem accepts tags of 1–29 bytes. The Pico overlay supports
  tags of 1–128 bytes. Keep portable callers at 29 bytes or less until this
  discrepancy is resolved. `fs_next_tag` accepts prefixes through 128 bytes
  and returns tags in its 129-byte length-prefixed field.
- The maximum record size is backend and available-storage dependent. Pico
  overlay writes fail if its record-count, byte-capacity, or snapshot limits
  would be exceeded. The OS backend can return `OutOfFlash` after sector
  defragmentation cannot make enough room.

## Return values

`0` means success for operations that return status. `fs_exists` is special:
it returns `1` when present and `0` when absent. `fs_next_tag` returns `0` for
a result, `1` at end, and `2` for invalid request/cursor state. Its cursor is
invalid after any filesystem mutation and must be restarted from zero.

For `fs_read`, the OS backend copies `min(requested_length, record_length)`
bytes and succeeds; Pico rejects a request larger than the record. Prefer
`fs_length` followed by a read of exactly that length. Scalar reads and writes
use byte offsets for 1-byte operations and element indexes for 2- and 4-byte
operations. Reads/writes outside the record fail; integer overflow in scaled
offsets is rejected.

On Pico, most filesystem errors collapse to status `1`. On PC, operations
using the OS filesystem return `1` for out of flash, `2` for missing tags, `3`
for invalid tag length, and `0x80 | flash_error` for flash I/O errors. Thus a
nonzero result is portable failure; its exact meaning is not. `fs_next_tag`
uses its separate 0/1/2 status contract.

## `fs_read_inplace` ownership and lifetime

On success, `*data` points into read-only backing storage and `*length` is the
record length. The caller does not own the bytes and must not free or write
through the pointer. An empty record is valid: length is zero and the returned
pointer remains non-null, but no byte may be dereferenced.

ROM-mask data on Pico remains valid for the firmware lifetime. Data returned
from the mutable Pico overlay or PC emulated flash must be treated as borrowed
only until the next operation that can mutate or rebuild the filesystem,
including write, erase, prefix erase, initialization, rollback, or reboot.
Do not retain such a pointer across those operations. A successful `fs_sync`
alone does not relocate the in-memory overlay or PC flash mapping, but callers
should still reacquire the pointer after any mutation.

The current OS implementation releases its internal read-borrow lock when the
syscall returns, so this lifetime is a caller-side rule rather than an enforced
Rust borrow. Prefer `fs_read` when data must survive mutation. The empty-record
case is explicitly supported; typed out-of-range reads return a nonzero status
instead of indexing beyond the record.

## Sync, transactions, and durability

- Pico `fs_write` and erase operations modify the RAM overlay. They are visible
  to later reads immediately but are not durable until `fs_sync()` returns
  zero. Sync writes a new copy-on-write snapshot and validates it before
  switching the active generation. On failure it returns nonzero and the
  previously committed snapshot remains the recovery source.
- During a Java Card persistent-write transaction, `fs_sync()` returns zero
  without committing while the transaction is active. Transaction commit
  disables that deferral and performs the snapshot sync. Rollback restores the
  transaction's saved records and syncs the restored state. Therefore the
  transaction commit result, not an in-transaction `fs_sync()` result, is the
  durability boundary.
- The PC emulator's `fs_sync()` shim is a no-op returning zero: OS filesystem
  writes synchronously update the process's emulated flash image. This does
  not by itself checkpoint that image to a host file or make it survive process
  exit. Persistence across emulator runs depends on the VPCD memory/sidecar
  save-and-resume path being used.
- `fs_drop()` calls `fs_sync()` and discards its result. It is cleanup, not a
  reliable commit API. Call and check `fs_sync()` explicitly when durability
  matters.

## Known backend discrepancies to close

- Tag maximum: 29 bytes in the OS filesystem versus 128 bytes in the Pico
  overlay.
- `fs_read` oversized-buffer behavior differs as described above.
- Detailed errors differ; most Pico errors collapse to `1`.
- PC `fs_sync()` only reports that the emulator has no separate flush phase;
  it does not persist the emulated flash image to disk.

These differences are documented behavior, not evidence of equivalent backend
semantics. Portable callers must use the common subset and check every status.
