# Filesystem C ABI contract

This document records the current `fs_*` contract used by the VM and loader.
The PC emulator delegates record operations to the CHOUPI OS filesystem; Pico
uses the bounded filesystem overlay in `src/pico_filesystem.cpp`. Filesystem
status-returning operations use the same error codes on both backends.

## Call and buffer rules

- Call `fs_init()` successfully before any other filesystem operation. Its
  nonzero result means initialization failed. The PC implementation initializes
  its emulated flash filesystem during emulator startup.
- `tag` and input `data` are borrowed only for the duration of the call. Output
  buffers for `fs_read`, scalar reads, `fs_length`, and `fs_read_inplace` must
  be writable for the documented output size. Pointers must be non-null when
  the corresponding length is nonzero. The FFI does not validate every pointer
  or buffer length; invalid caller memory can fault.
- Both OS and Pico filesystems accept tags of 1–128 bytes. The OS filesystem
  retains its original header encoding for tags through 29 bytes; tags of
  30–128 bytes use the reserved header length code followed by one length byte.
  Existing OS records remain readable without migration. `fs_erase_prefix()`
  and `fs_next_tag()` accept prefixes through 128 bytes; `fs_next_tag()` returns
  tags in its 129-byte length-prefixed field.
- The maximum record size is backend and available-storage dependent. Pico
  overlay writes fail if its record-count, byte-capacity, or snapshot limits
  would be exceeded. The OS backend can return `OutOfFlash` after sector
  defragmentation cannot make enough room.

## Return values

For `fs_read`, `fs_write`, `fs_erase`, `fs_length`, `fs_read_inplace`, typed
offset access, prefix erase, and `fs_sync`, the status values are:

| Status | Meaning |
| --- | --- |
| `0` | Success |
| `1` | Not enough filesystem capacity (`OutOfFlash`) |
| `2` | Tag does not exist (`NoSuchTag`) |
| `3` | Invalid tag or prefix length |
| `0x81` | Flash region is locked |
| `0x82` | Typed offset is out of range or overflowed |
| `0x83` | Other flash or snapshot I/O failure |

The OS filesystem maps unknown flash-controller error details to `0x83`; Pico
reports the same code when snapshot persistence fails without a more specific
cause. A function returns only the codes that apply to it: capacity errors are
returned by mutations, missing-tag errors by lookup/read/erase operations, and
out-of-bounds errors by typed offset operations. `fs_init()` has a separate
initialization status contract: zero means success and any nonzero result means
initialization failed. `fs_exists()` is special: it returns `1` when present
and `0` when absent, including invalid tags. `fs_next_tag()` returns `0` for a
result, `1` at end, and `2` for invalid request/cursor state. Its cursor is
invalid after any filesystem mutation and must be restarted from zero.

For `fs_read`, both backends copy `min(requested_length, record_length)` bytes
and succeed. Bytes beyond the copied record prefix in a larger destination
remain unchanged. Call `fs_length` first when the complete record is required.
Scalar reads and writes use byte offsets for 1-byte operations and element
indexes for 2- and 4-byte operations. Reads/writes outside the record fail;
integer overflow in scaled offsets is rejected.

The PC emulator's `fs_sync()` is a no-op and returns zero; Pico maps snapshot
write failures to `0x83`. `fs_next_tag()` and `fs_exists()` use their separate
result conventions above.

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
  not checkpoint that image to a host file. Disk checkpointing is enabled by
  `--save` (`--install` requires it). The image is written only after normal
  control reaches the emulator's save block, including after VPCD disconnect
  or idle timeout. An abrupt process termination before then loses changes
  since the previous checkpoint.
- A checkpoint writes the full emulated flash image to the `--memory` path by
  truncating and rewriting that file, then saves `<memory>.cap-registry` for
  dynamic CAP packages and applet instances. The registry sidecar is written
  through a temporary file and renamed; the image write is not atomic, and
  neither file is explicitly `fsync`ed. The pair is therefore not atomic or
  guaranteed durable against host power loss. A write failure can leave a
  partial image or an image/sidecar mismatch.
- The sidecar includes a CRC of the saved image. `--resume` validates that CRC
  and fails closed on a stale, corrupt, or missing sidecar when dynamic
  packages are present in the image. Such a mismatch requires restoring the
  dynamic package metadata (for example with `--restore-applet`) or recovering
  the matching image/sidecar pair. If there are no dynamic packages, an absent
  sidecar is valid. Treat successful emulator exit with `--save` as a completed
  process-level checkpoint, not as a host power-loss durability guarantee.
- `fs_drop()` calls `fs_sync()` and discards its result. It is cleanup, not a
  reliable commit API. Call and check `fs_sync()` explicitly when durability
  matters.

## Known backend discrepancies to close

- PC `fs_sync()` only reports that the emulator has no separate flush phase;
  it does not persist the emulated flash image to disk. Checkpoint atomicity
  and power-loss durability remain weaker than Pico's copy-on-write snapshot
  contract.

These differences are documented behavior, not evidence of equivalent backend
semantics. Portable callers must use the common subset and check every status.
