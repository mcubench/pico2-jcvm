# RESOURCE-01 implementation plan

Date: 2026-11-08. Status: proposed implementation; documentation only.
Priority: highest. Backlog: [RESOURCE-01](../TODO.md#resource-01).

The goal is to make the unchanged JCAlgTest cleanup effective and keep resource
use bounded during long runs, while preserving live objects through resets and
interrupted flash operations. Implement persistent reclamation before allocation
and write optimizations. Coordinate PERSIST-01/02/03, STORE-01, API-01/03 and
JC-RESET-01; their existing safety and acceptance gates remain in force.

## 1. Measure resource growth

Add resource accounting around key creation, reset/free-cache commands and
post-APDU collection. Measure live and unreachable persistent records/bytes,
filesystem free space and obsolete versions, compaction time, RAM backing bytes,
object/array handles, and native crypto allocations/retained references.

Allocation failures should identify the exhausted resource and allocation site
in internal diagnostics. Separate leaks from legitimate live-capacity limits.
Record backend, firmware/profile, fixture version, iteration and APDU so host
and physical results can be compared without treating their capacities as equal.

## 2. Complete the existing persistent reclaimer

Extend the existing inventory/reclaimer rather than introduce a second
collector. Trace every durable root and follow references with owner identity,
including package statics, applet fields, arrays and cross-owner aliases. Audit
native and retained-context roots as part of the complete graph.

Collect at a safe boundary after interpreter/native frames have unwound and
transaction handling has completed. Incomplete root scans, malformed records,
unreadable inventory or active transactions must prevent reclamation.

Finish PERSIST-01 multi-owner and interrupted-flash acceptance before production
enablement. After interruption, all live objects must remain readable and a
subsequent collection must safely complete. Host fault injection supplements
physical acceptance; it does not replace it.

## 3. Implement deferred JCSystem deletion requests

Make requestObjectDeletion() record a pending request for the requesting context
and return, without immediately traversing or deleting objects. After the APDU
finishes, service the request before the next Applet.process() invocation.
Clear it only after successful collection; internal failures must not silently
count as success. Define and test the failure/reset recovery policy explicitly.
Meet the API's transient reclamation deadline as well as its persistent deadline.

This ordering must work:

1. Applet requests deletion.
2. Applet clears RSA-cache references.
3. Applet returns from process().
4. Runtime traces the resulting graph and reclaims unreachable objects.
5. The next process() invocation begins.

Only advertise isObjectDeletionSupported() == true once the required guarantees
hold. Keep the supplied CAP unchanged; a successful no-op is insufficient.

## 4. Make reference replacement and failed construction reclaimable

Replacing a field with a new key leaves the previous key eligible for collection
only when every reference to it is gone. Do not eagerly delete during assignment:
other objects may share the key, and transaction rollback may restore the old
reference. Preserve shared aliases and stable object identities.

Exercise allocation failures at each key-construction stage. Partial arrays,
persistent records and native state must become reclaimable without corrupting
previously committed state. Audit stale native roots and obsolete body tags.

## 5. Keep key reuse bounded and reduce flash work

Same-key setters should overwrite existing component buffers. clearKey() erases
material and resets initialized state while retaining reusable capacity. Reusing
a key should not depend on object deletion; repeatedly replacing keys does need
reclamation after the previous keys become unreachable.

After establishing correctness, optimize:

- Allocate only the RSA components required by the key type.
- Replace repeated per-byte persistent writes with bounded bulk updates while
  preserving transaction and failure semantics.
- Keep native crypto scratch in RAM with explicit ownership and release.
- Keep transient key material and associated initialized state in RAM and clear
  them at the required events.

Ordinary objects retain persistent semantics, including state reachable through
persistent applet fields/statics. Short-lived objects may stay in RAM where those
semantics are preserved. Transient-array identity/allocation metadata may be
durable, but transient operational contents must not be persisted.

## 6. Demonstrate long-run stability and diagnose resets

Test independently:

- Repeated updates and clear/reinitialize operations on one key.
- Repeated creation/replacement of keys followed by deletion requests.
- The unchanged JCAlgTest 1.8.4 / jc305 reset/free-cache sequence, using its
  matching versioned client and verified fixture manifest.

Success requires retained RAM, handles and persistent records to reach a measured
plateau over repeated cycles. Include shared references, cycles, multiple owners,
transactions, partial-construction failures and interruption during reclamation.
Record duration, iteration count, high-water marks and before/after collection
counts; distinguish live allocation from obsolete filesystem storage.

Physical runs must capture reset cause, watchdog information, last APDU/stage and
filesystem recovery outcome. Exhaustion may explain some resets, but establish
that with evidence. Track USB/PCSC disappearance independently. Acceptance must
not contain NO_RESOURCE, FAULT, unexpected resets or transport loss.

## Delivery and validation

Use small, independently validated commits, each with a progress record and
handoff update. Follow repository wrappers and both ARM/RISC-V build gates for
platform-independent native changes. Run relevant unit/integration suites before
bounded physical validation through the required host-access path. Do not enable
production persistent reclamation until existing safety gates pass. No firmware,
CAP or test implementation is performed by saving this plan.

## References

- [Java Card 3.0.5 JCSystem](https://docs.oracle.com/cd/E59935_01/api/javacard/framework/JCSystem.html)
- [Java Card 3.0.5 Key](https://docs.oracle.com/cd/E59935_01/api/javacard/security/Key.html)
- [Java Card 3.0.5 KeyBuilder](https://docs.oracle.com/cd/E59935_01/api/javacard/security/KeyBuilder.html)
- [Repository issue knowledge base](../.agent/QKB.md)
