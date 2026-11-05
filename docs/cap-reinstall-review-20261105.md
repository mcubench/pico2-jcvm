# Same-AID CAP reinstall review — 2026-11-05

Reviewed the current working tree, including pre-existing edits to
`src/cap_load.cpp`. No firmware source was changed for this review.

## Conclusion

Deleting only an applet instance does not remove its CAP package. Loading a
modified CAP with that package AID then fails; installing an instance from the
resident package runs the old code. This is a plausible explanation for an
apparent unsuccessful upgrade if host tooling ignores a LOAD failure or skips
LOAD when it finds a resident package.

After a successful full package DELETE and successful LOAD/INSTALL, the normal
path should use the new code. No normal-path silent bytecode replacement bug
was identified. This is a source-review conclusion, not a changed-version
hardware reproduction.

## Evidence

- `src/cap_load.cpp:754`: `finishLoad()` rejects an existing package AID. It
  does not overwrite or compare versions of an existing package. The embedded
  CCID handler maps rejected LOAD to `6A80`.
- `src/cap_load.cpp:1029`: instance DELETE clears instance roots and registry
  metadata, retaining package code and package static state. Package DELETE
  clears package persistent state, disables the package, and deletes its slot.
  Package DELETE with instances and without the related-object flag returns
  `6985`.
- `src/cap_store.cpp:275`: package writes erase the entire slot and metadata
  mirror before writing. `cap_store_write_package()` checks the new length
  and compares every code byte after cache reconstruction. Shorter replacement
  code therefore does not normally retain an executable old tail.
- `choupi/src/jc_handlers/flashmemory.cpp:2340`: dynamic `getCap()` obtains the
  current slot data and does not fall back to a ROM CAP after deletion.
- `src/ccid_device.cpp:705` and `choupi/src/main.cpp:1076`: successful DELETE
  invalidates matching runtime contexts; package DELETE discards contexts
  belonging to that package.

## Runtime cleanup findings requiring reproduction

1. **Package DELETE leaves other packages' SIO proxy routes intact.**
   `choupi_vm_forget_deleted_applet_contexts()` calls
   `Context::forgetAppletInstance()` only for instance deletion. That method
   invalidates server-AID routes in client contexts. Package deletion resets
   server contexts but skips this operation in surviving client contexts.
   `ShareableProxyEntry` has an AID and object reference, with no installation
   generation; `bc_invoke.cpp:1103` resolves the current server by that AID
   and uses the saved reference. Reusing its compact handle after reinstall
   could revive an old proxy or target a different object. This concerns stale
   object identity, not old CAP bytes. The existing cross-AID regression
   deletes the service instance, not its package. Deselect does not clear
   shareable session state; card reset does.

2. **Late DELETE failure can leave runtime contexts after durable deletion.**
   Persistent DELETE commits before CAP-slot erase or instance metadata
   removal. A subsequent failure returns `6581`, so the CCID/PC handlers skip
   runtime invalidation (which runs only for `9000`). Lifecycle recovery can
   later finish durable cleanup without invalidating these contexts. A same
   package-slot/AID reinstall in the same process could then encounter old
   heap/instance state. This requires a storage failure; it is not established
   for successful DELETE. Test failure injection after persistent commit,
   then recovery/reinstall without reset.

## Validation and missing coverage

- `./tools/test-gp-delete`: exit 0, `TEST:SUMMARY pass=26 fail=0`.
- `./tools/test-rp2350-fs-overlay`: exit 0, `TEST:SUMMARY fail=0`, including
  lifecycle recovery, mirrored registry metadata, package namespace cleanup,
  and power-cut tests. It deliberately prints `FAULT FS snapshot unavailable;
  preserve flash for recovery` while testing unrecoverable migration; under
  the repository's strict FAULT rule this is not a clean fault-free validation.
- `tools/tests/same_cap_delete_reinstall_vpcd_test.py` reloads identical CAP
  code. It verifies owner reuse and shared-state lifetime, not changed bytecode.
- No physical device operations, target builds, CAP modifications, or new
  runtime reproduction were performed. Firmware source did not change.

For an upgrade, delete the package AID with related objects (or delete all its
instances and then the package), verify the package is absent, LOAD the new
CAP and check every status, INSTALL, and query behavior that differs between
versions. A decisive regression should check both versions in one uninterrupted
process and again after restart, with shorter/longer code and changed static
layout. Extend the cross-AID regression to package deletion and add the late
DELETE failure case before treating these cleanup risks as resolved.
