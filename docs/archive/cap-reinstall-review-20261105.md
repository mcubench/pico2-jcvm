# Same-AID CAP replacement review — updated 2026-11-06

The initial review covered the current working tree, including pre-existing
edits to `src/cap_load.cpp`. The later changed-version regression added a
native CAP validation fix to `src/cap_constant_pool_validation.hpp`.

## Conclusion

Deleting only an applet instance does not remove its CAP package. Loading a
modified CAP with that package AID then fails; installing an instance from the
resident package runs the old code. This is a plausible explanation for an
apparent unsuccessful upgrade if host tooling ignores a LOAD failure or skips
LOAD when it finds a resident package.

After a successful full package DELETE and successful LOAD/INSTALL, the new
code runs. A changed-version VPCD regression now verifies replacement in both
code-size directions and after emulator restart. Physical CCID acceptance is
still pending because the Pico readers are currently absent from PC/SC.

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
- `tests/integration/cap_upgrade_vpcd_test.py`: test-only V1/V2 CAP fixtures share
  package AID `A000000151000006` and applet/instance AID
  `A00000015100000601`. Their load images are 392 and 479 bytes respectively.
  The VPCD regression checks V1→V2 and V2→V1 replacement, state mutation,
  same-AID duplicate LOAD rejection, and restoration after emulator restart.

## Upgrade procedure

1. DELETE the applet instance if needed. This removes its instance state and
   registration only; it leaves the package code and package static state.
2. DELETE the package AID with the related-objects flag and require `9000`.
   If instances remain and related deletion is not requested, expect `6985`.
3. LOAD the new CAP and check every response, including the final LOAD block.
   A resident package with the same AID is rejected with `6A80`; that is not a
   successful upgrade and must not be ignored by orchestration scripts.
4. INSTALL the applet and require `9000`, SELECT its instance, then verify a
   version-specific behavior or response before reporting upgrade success.
5. For persistence-sensitive changes, power-cycle and repeat SELECT and the
   behavior check.

Do not infer success from deleting an instance, from a host command returning
zero while suppressing APDU errors, or from an INSTALL against a package that
was already resident.

## Changed-version emulator result

The test-only V1 CAP has one static byte and one instance byte; V2 changes the
static layout to a byte plus short and the instance layout to a byte plus
short. It mutates each version to a distinct state. V1→V2 rejects duplicate
LOAD after instance-only DELETE with `6A80`, package DELETE returns `9000`,
V2 LOAD/INSTALL and SELECT succeed, V2 initial values are observed, and
mutated V2 state restores after emulator restart. V2→V1 package DELETE,
shorter LOAD/INSTALL and SELECT also succeed; V1 defaults appear instead of
V2 state, and mutated V1 state restores after a second restart.

Validation log: `artifacts/logs/cap-upgrade-vpcd-run-20261106T154810Z.log`
(`TEST:SUMMARY pass=1 fail=0`). The helper script also prints the assembled
load-image SHA-256 values. Exact generated artifacts from that run:

| Version | CAP archive bytes | CAP archive SHA-256 | Loaded component bytes | Loaded bytes SHA-256 |
|---|---:|---|---:|---|
| V1 | 4054 | `5e31a70b14f11aefaf0fc885f455b846f6d20e4352d1cdfd9e12d755e57684db` | 392 | `e70417f50bb64ea503a70599b0e55da236b8178577d41cbc4932b0f5e4c4cda7` |
| V2 | 4308 | `e1e4ff9346c9a4c9293ade6bee713b550387cf5bcac47b8e1475cce3e8dd3fc9` | 479 | `cc069da48ec7f881ada8c3ccee52326dbbcbcc0a604183bc8df1c045cdcdb01c` |

The run log SHA-256 is
`67029e601bdc4aa36cd6c1977c4885445b49c8d7a025c17cdac11362f585c2c5`.
Physical test remains pending: the authorized USB helper granted three device
nodes, but `timeout 8s pcsc_scan -r` returned 124 without enumerating a Pico
reader.

The package DELETE regression also installs an independent applet package
(`A000000151000007`) whose APDU returns `5A9000`. The unrelated applet remains
selectable and executable after successful service package DELETE and
same-AID reinstall. With injected package cleanup failure, DELETE returns
`6581`; after emulator restart/recovery the unrelated applet still returns
`5A9000`, and it remains usable after service package reinstall. Focused
captured outputs are recorded in the `sio-cross-aid-independent-client`
progress entry; the default SIO suite still passes 34/34.

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

## Earlier review validation and remaining coverage

- `./tools/test-gp-delete`: exit 0, `TEST:SUMMARY pass=26 fail=0`.
- `./tools/test-rp2350-fs-overlay`: exit 0, `TEST:SUMMARY fail=0`, including
  lifecycle recovery, mirrored registry metadata, package namespace cleanup,
  and power-cut tests. It deliberately prints `FAULT FS snapshot unavailable;
  preserve flash for recovery` while testing unrecoverable migration; under
  the repository's strict FAULT rule this is not a clean fault-free validation.
- `tests/integration/same_cap_delete_reinstall_vpcd_test.py` reloads identical CAP
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
