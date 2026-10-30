# Issue knowledge base — compacted 2026-10-29

Use this summary first, then the recent issue records or historical topic index.
Full original: [archived QKB](archive/2026-10-29-pre-compaction/QKB.md).
Archive text is immutable historical evidence; later fixes supersede earlier
"unresolved" statements. Current tasks/results: [HANDOFF.md](HANDOFF.md) and
opening prioritized backlog in `TODO.md`. Detailed work history: `progress.md`.
All original bytes, measurements, hashes, commands, and logs are preserved in
archives with SHA-256 manifests; logs referenced there may be local/Git-ignored.

## Physical `/deploy` Service profile recheck and Java PC/SC library — 2026-10-30

- The 3.0.5 Service CAP produced an earlier asymmetric-profile report of 102
  `EXC_RUNTIME` results out of 124 calls on two boards. A controlled rerun on
  physical Pico 2 `8E464F4E2BBC0A19` with the normal `asymm_crypto` image
  passed all 124/124 calls, including CAP upload and cleanup. A diagnostic
  build also passed 124/124; its temporary bytecode field trace was removed.
  Do not treat the older 102-exception result as a confirmed firmware defect:
  it has not been reproduced under a correctly pinned Java SmartcardIO reader.
- The host-side cause found was a reader-index resolver omission in
  `AlgTest_JavaCard/deploy/api_card_runner.py`: `--pcsc-lib` was applied to the
  official client but not the helper that maps an exact reader name to the
  client's numeric index. With the system Java, the helper saw no readers
  until `-Dsun.security.smartcardio.library=/usr/lib/x86_64-linux-gnu/libpcsclite.so.1`
  was supplied; then physical 8E resolved to index 3 (two VPCD entries precede
  it). The resolver now receives the same property when `--pcsc-lib` is set.
- Validation without `JAVA_TOOL_OPTIONS`: `python3 -m py_compile
  AlgTest_JavaCard/deploy/api_card_runner.py` and `git diff --check` passed;
  the repository deploy runner with only `--pcsc-lib` then uploaded, tested,
  and removed the unchanged Service CAP on physical 8E: 124 calls, all `OK`.
  The concurrent serial capture showed physical APDUs and normal passing
  runtime summaries. The normal `asymm_crypto` firmware had passed ARM/RISC-V
  wrapper builds and a verified physical cycle before the test. No firmware
  source changed. Physical board 8E is running normal `asymm_crypto`; 2BED
  remains on normal `symm_crypto`.
- Reproduction artifacts: `artifacts/results/deploy/Service_normal_asymm_pcscopt_8E_20261030.csv`
  (SHA-256 `8b534206de57e52e762d1727ad67c24fa38af1277cc893272e471c03fe1fd39b`);
  client log `artifacts/logs/service-normal-asymm-pcscopt-client-8E-20261030.log`
  (SHA-256 `75dc2bfd266e77ce1c4be8588c04cce1211cba040959e59595d9a9d3d2b91a35`);
  serial log `artifacts/logs/service-normal-asymm-pcscopt-monitor-8E-20261030.log`
  (SHA-256 `ca7ee5e1e55755fbbe2d62e0433015c5bc0f7b9fde6d4f9bbb49e652ddb0617b`);
  image `build/arm/pico2_choupi.uf2` (SHA-256
  `b34630800ced9b7104bf47e937656cbe4639d26d4ddda0ba1e17166825bb80d3`).

## Physical JCAlgTest `symm_crypto` support report — 2026-10-30

- Board `2BED26ED20556535` was selected by exact PC/SC reader name and was
  already running the `symm_crypto` ARM image (`build/javacard-support/profile.txt`;
  latest recorded flash cycle `artifacts/logs/cycle-choupi-20261030T071532Z.log`).
  No firmware was flashed and no CAP was installed or deleted for this test.
- The first `ALG_SUPPORT_EXTENDED` attempt used the runner's default 900-second
  bound and ended with status 124 at 2,264 rows. This was a command timeout, not
  a reader or firmware failure. The viable workaround is to set
  `JCALGTEST_TIMEOUT_SECONDS=3600`; the fresh run completed in about 27 minutes.
- Full unchanged JCAlgTest 1.8.2 report: 4,525 normalized rows, 73 yes,
  4,452 no, zero other. It exactly matched the generated `symm_crypto` support
  declaration: 46 expected yes, 46 actual, zero unexpected, zero missing.
  jCardSim row identities all matched. Its reference distribution was 1,469
  yes, 2,935 no, and 121 other; that is a separate simulator profile and not
  the expected support declaration for this firmware.
- Comparator output:
  `artifacts/logs/compare-jcalgtest-symm-support-2BED-20261030.log`
  (SHA-256 `679cb172d3ffcefc671eb57d27debfbba723ef081d931a50989ce4dff06cf54b`).
  Full client transcript:
  `artifacts/logs/jcalgtest-symm-support-physical-2BED-20261030.log`
  (SHA-256 `8c3ec90f4aa3034ee7e029389c8b7fd90b1232553e9364235e48b5e83d4c86ec`).
  CSV SHA-256 `e84b29fb770d8795372ac5f042b1f007a4d6994a54b87fea8078f4f9fc5b921f`;
  JCAlgTest report log SHA-256
  `0fba635228ba7847db859120e3b39f6c22a1165c408f203cc44edb4a105a5f67`.
  Post-run monitor log SHA-256
  `5bab064d1d555950f5f0e2aa6701d5643c7f15adfb46b25982989dba5f6411d8`.
  The 8-second monitor passed repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL` or `FAULT`.
- The timed-out partial report remains in
  `artifacts/results/jcalgtest/symm_crypto_support_physical_2BED_20261030/`;
  use the `_long/` report for comparisons.

## Symmetric profile parity with the supplied JCOP4 P71 matrix — 2026-10-30

- Filtered the supplied JCAlgTest 1.7.9 reference to symmetric KeyBuilder
  AES/DES key types, AES/DES Cipher selectors and factory aliases, and AES/DES
  MAC algorithms. It has 41 positive identities. The generated `symm_crypto`
  profile selects exactly those 41 (zero missing, zero extra).
- Canonicalized the full physical JCAlgTest 1.8.2 report from Pico 2
  `2BED26ED20556535`; it reports the exact same 41 symmetric identities
  (zero missing, zero extra). The independent physical symmetric known-answer
  suite previously passed 71/71
  (`artifacts/logs/symm-crypto-kat-pcsc-20261027T195919Z.log`).
- The saved comparison is
  `artifacts/logs/compare-symm-jcop4-profile-and-physical-2BED-20261030.log`
  (SHA-256 `d4b05870cce57ec9ab9444c66b67102aa5553e108e3e11181cf87b4f020bb807`).
  The reference contains no `ALG_AES_CTR` row; its support remains unclaimed.
- Decision: No missing symmetric implementation was found against this
  matrix. Continue with the other open project work, including physical
  USB power-cycle recovery before any persistent-record deletion.

## Asymmetric profile parity with the supplied JCOP4 P71 matrix — 2026-10-30

- The cached `build/external-test/expected-support-asymm_crypto.csv` is stale
  (90 expected identities) and yields false missing-support output. Regenerate
  expected support from the current JSON before comparing:
  `python3 tools/javacard-support.py --config config/javacard-support.json
  --profile asymm_crypto --out-dir artifacts/results/profile-audits/asymm_crypto`.
- The current manifest has 145 entries and matches all 136 positive identities
  shared with the supplied JCAlgTest 1.7.9 matrix. Reproducible output:
  `artifacts/logs/compare-asymm-profile-jcop4-target-20261030.log` (SHA-256
  `98867eecdc3f0c978ee262ca8904e74557302ccc491963109eb88eca377093fd`). The
  generated manifest and CSV are in `artifacts/results/profile-audits/asymm_crypto/`.
- Full physical JCAlgTest 1.8.2 report from 8E has 4,525 matching rows, 196
  yes, 4,329 no, and zero other values. Against the generated 145-entry
  declaration, 143 canonical identities match; the two missing report
  identities are `ALG_RSA LENGTH_RSA_3072` and
  `ALG_RSA_CRT LENGTH_RSA_3072`. The unchanged upstream CAP sends length 4096
  for these queries. Separate physical RSA-3072 and RSA-CRT-3072 generation
  vectors pass; do not change the CAP or native support to mask its query.
- Full support comparison output:
  `artifacts/logs/compare-asymm-physical-support-8E-20261030.log` (SHA-256
  `6f5c08dfda64cc859a44f0e71e50a7f840e28a6152d118bb4753b83b85dc5606`). The
  comparator exits 1 for these two upstream-labeled rows; it has zero
  unexpected positives and zero non-yes/no results. Profile parity is
  complete; broader physical `/deploy` result corrections and RSA
  private-operation hardening remain separate.

## Persistent filesystem tag cursor — 2026-10-30

- The root Pico firmware does not link `choupi/os`; that Rust filesystem and
  its trusted cursor syscall serve the host emulator. Pico uses a separate
  persistent C++ overlay in `src/choupi_pico2.cpp`.
- Both backends now expose the same 134-byte cursor request ABI and 128-byte
  tag capacity. The Pico cursor enumerates active mutable-overlay records,
  filters by prefix, skips erased records, and rejects a nonzero offset.
  Immutable ROM defaults are intentionally excluded because they consume no
  persistent storage. Cursor state is invalid after filesystem mutation.
- `./tools/test-rp2350-fs-overlay` exercises the Pico cursor, including
  80-byte tags and tombstones; host Rust tests cover cursor paging and syscall
  validation. The ARM image was flashed to identified board
  `2BED26ED20556535`; the finite monitor passed `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. This cycle did **not** call the cursor from the
  VM, so physical cursor-call behavior remains unverified. Log:
  `artifacts/logs/cycle-choupi-20261030T010244Z.log`.
- Native read-only visitors now page applet-owned `0x03, owner` tags and
  package-static `0x02, package` tags. They validate cursor progress, returned
  tag length, and prefix, and support early stop without mutation. Focused host
  tests cover owner isolation, root/body-shaped records, a 128-byte tag, early
  stop, stalled cursor, and malformed output. They do not decode references or
  feed heap GC, and have not been called on physical hardware. ARM/RISC-V
  wrappers and PC build pass; latest startup-only flash log is
  `artifacts/logs/cycle-choupi-20261030T012137Z.log`.
- CAP reference-static slots are enumerated from `reference_count`, after
  checking `2 * reference_count <= image_size`; the visitor uses two-byte
  offsets and switches to the 16-bit static tag form beyond offset 255. This
  follows the official Java Card VM spec's Static Field Image layout: reference
  fields occupy the initial segment and each is two bytes
  ([Oracle Java Card VM Spec 3.1, Static Field Component](https://docs.oracle.com/en/java/javacard/3.1/jc-vm-spec/F12650_05.pdf)).
  Tests cover 129 references crossing the 8-bit offset boundary and invalid
  metadata. The visitor only returns candidate tags; it does not fetch values
  or establish non-null roots.
- Static root loading now has a strict path: a missing package static-reference
  tag is the default null, while an existing record that cannot be read or
  decoded propagates failure instead of becoming null. Strict decoding limits
  alias recursion to 32 hops and rejects array aliases resolving to null or a
  non-array. Normal VM getters retain their tolerant legacy behavior. The
  installed-CAP adapter obtains big-endian static metadata from that CAP and
  consults the heap's package/field cache before restoring roots.
- Registered applet instance roots are identified by applet namespace tags
  ending in `0xFF`; both 8-bit and 16-bit class-index tag forms are decoded.
  Restored objects are checked against package/class fields encoded in the
  root tag. The PC-only `PICO2_PERSISTENT_INVENTORY=1` post-APDU diagnostic now
  invokes the installed-CAP static adapter and applet-root adapter. On the
  unchanged JCSystem transaction CAP, cases 14–16 passed with both adapters
  returning `Complete`; each scan reported six non-null static roots and one
  registered applet root. VPCD log:
  `artifacts/logs/persistent-inventory-vpcd-fixed-20261030.log`.
  These checks exercise root restoration but do not yet walk object fields,
  reference-array bodies, or full alias graphs, and do not authorize persistent
  record deletion. The physical cycle at
  `artifacts/logs/cycle-choupi-20261030T021444Z.log` tested startup only; the
  PC-only diagnostic did not run on hardware.

### Static root adapter callback context corrupted its heap pointer — fixed

- The generic static-root walker intentionally supplies the same opaque context
  to both its reference reader and root visitor. The adapter had passed its
  private heap-reader context to the caller's visitor, which treated it as its
  own context. The first callback overwrote the stored heap pointer; the next
  static slot then faulted in cache lookup with SIGSEGV / unauthorized address
  `0x0`.
- The adapter now stores the caller's visitor and context alongside its heap
  pointer and uses a forwarding callback. A null visitor returns
  `InvalidArgument`. The previous fault reproduces only with the old wiring;
  after the fix, the unchanged VPCD JCSystem cases 14–16 pass (5/5 harness
  checks) and both adapters report `Complete` for all four post-APDU scans.
- The same opt-in diagnostic also calls the existing read-only
  `Heap::traceReachableFrom()` with selected-context roots plus static/applet
  roots. Cases 14–16 and nested-alias case 82 pass (6/6 harness checks). After
  setup, four scans report 11/11 arrays and 7/7 instances reachable, zero
  invalid refs, and zero edge-load failures. This walk still uses the ordinary
  tolerant child getters; the result is diagnostic and does not prove strict
  fail-closed edge validation or complete coverage of every context.
- `./tools/test-persistent-inventory`, PC/ARM/RISC-V builds, and the identified
  2BED ARM physical cycle all passed. The physical monitor emitted
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`; it did not invoke
  these adapters. No persistent records were deleted or changed.
- Logs: `artifacts/logs/persistent-inventory-static-vpcd-20261030e.log` (fault),
  `artifacts/logs/persistent-inventory-vpcd-fixed-20261030.log` (fixed run),
  `artifacts/logs/cycle-choupi-20261030T021444Z.log` (physical startup).

### Strict heap traversal of serialized persistent edges — 2026-10-30

- The opt-in read-only reachability tracer now treats serialized persistent
  object fields and persistent reference-array elements as strict edges. It
  restores them with `getPersistentField_ReferenceStrict`; malformed field
  types and tags too wide to append a two-byte field/element index cause the
  walk to fail instead of silently treating an edge as null. Missing object
  fields remain default values. Transient arrays and objects that have not yet
  acquired persistent tags continue to use their in-memory references.
- PC VPCD JCSystem cases 14–16 and nested-alias case 82 pass with strict edge
  traversal (`trace_complete=1`, arrays 11/11, instances 7/7, invalid refs=0,
  edge failures=0). ARM/RISC-V builds and two identified physical startup
  cycles passed. The physical JCSystem transaction probe on 8E passed 9/9,
  including rollback/commit, large array, catchable buffer-full, and card power
  cycle persistence checks. This is regression evidence; the PC-only inventory
  trace itself has not been run on physical hardware.
- The test package/applet AID collided with installed upstream JCAlgTest and
  prevented physical test installation. The repository-owned test fixture now
  uses package `A0000000620301` / applet `A000000062030101`; only that temporary
  fixture was installed/deleted. The upstream CAP was not changed.
- No persistent records were deleted. Remaining safety gaps: malformed edge
  and alias negative cases; roots from every installed package, applet,
  context, and native owner; physical invocation of inventory; and evidence
  that an incomplete mark cannot authorize reclamation.

### Malformed aliases fail closed in the actual heap collector — 2026-10-30

- The persistent-transaction host target now links production `heap.cpp`,
  `jc_array.cpp`, `jc_instance.cpp`, `jref_t.cpp`, and the actual
  `flashmemory.cpp` decoder. Its filesystem remains an in-memory fixture.
- Real `Heap::collectUnreachableFrom()` walks persistent object fields and
  reference-array elements through the production strict decoder. A truncated
  object alias in each edge kind, a truncated array alias, and a
  self-referential array-alias chain each produce `complete=false`, one
  edge-load failure, and zero reclaimed heap slots. The wrapper passes with
  `TEST:SUMMARY fail=0`.
- Fixture correction: the persistent reference arrays now have valid
  serialized `FIELD_TYPE_ARRAY_OBJECT` headers and length fields. Earlier test
  revisions assigned a persistent tag to an array but omitted its serialized
  header, so they could fail on parent length before reaching the malformed
  child. Re-run after adding the headers still passes all collector assertions;
  the self-reference case is evidence of fail-closed alias rejection, not a
  direct measurement of the recursion-depth guard.
- A follow-up valid-chain fixture found that strict reference restoration
  already followed array aliases recursively, but `getPersistentField_Type`
  inspected only one target. Since heap traversal queries the type first, a
  valid alias-to-alias chain failed before reaching the recursive reader. The
  type decoder now recursively resolves array aliases, accepts only array
  terminal types, and rejects cycles after the same 32-alias limit used by
  strict reference restoration. Host tests cover a two-link chain through the
  actual collector and a cycle through both collector and strict restore.
- After the fix, persistent-transaction tests pass; ARM and RISC-V wrappers
  pass; the updated ARM image was flashed/verified on 2BED; the finite runtime
  monitor passed; and physical JCAlgTest SELECT, GETVERSION, and JCSystem-info
  all returned `9000`. The existing VPCD persistence/restart suite also passes
  3 install/commit and 38 restore/GC checks. Image SHA-256
  `b4a26add8ef5bfc28b948633d2f4790caaf48aaaa7addcae8f236f57c1ab8de6`; cycle
  log SHA-256 `264a3e499c8c3f06f96176e20e5fc8899418fc4d8a28a03a67b0bf8adc1ac39d`;
  physical APDU log SHA-256
  `69af8d88d141cf607dbb43153801f3ecb9c62812cb274884ac2421d99baa6601`.
- This is host-only fail-closed behavior evidence. It does not emulate power
  loss/restart, prove persistent record cleanup or recovery, or authorize
  record deletion. Keep persistent reclamation disabled until restart recovery
  and stable-handle semantics are tested.
- A separate host case then drops the volatile heap, preserves the in-memory
  filesystem fixture, creates a new Heap, restores a persistent reference
  array whose element aliases a persistent byte array, and collects with that
  root. It observes two reachable arrays, one unrelated heap slot reclaimed,
  and unchanged serialized root/target records. This verifies heap
  reconstruction over retained fixture records only; it is not a process,
  emulator, filesystem, or physical power-cycle recovery test.
- The existing `./tools/test-vpcd-persistence` suite provides a full emulator
  process restart check using the unchanged JCSystem transaction CAP. It passed
  install/commit (3/3) and post-restart restore/GC (38/38), including restoration
  of a persistent array after clearing its static alias, nested alias identity,
  and four repeated GC-pressure rounds. The first default-sandbox attempt was
  denied opening the local VPCD socket (`Operation not permitted`); rerunning
  the same wrapper with `PICO2_HOST_USB_ACCESS=1` and elevated host execution
  passed. Logs and hashes:
  `vpcd-persistence-commit-20261030T044356Z.log`
  (`df4578db168deb7b1751623e486f3e9d696623eaadd3af56890d6ca916ee9192`) and
  `vpcd-persistence-restore-20261030T044356Z.log`
  (`68a9b4f9169ec699143095dec88f64c840043a16485f125c9849aed797fe8c75`).
  This establishes PC emulator process restart only; physical power-cycle and
  interrupted-write recovery remain open.

### Read-only physical persistence probe — 2026-10-30

- The read-only `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup
  'Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 00 00'` check passed on
  identified board 2BED. GPPro listed the JCAlgTest 1.8.2 applet/package;
  SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem-info each returned `9000`.
  Log SHA-256:
  `bfcc4cc3fdf299ab3654d8be46e1b00926f5b2b795aa9b4e58551a160fa5d2ab`.
- This run did not initiate a reset or power cycle, so it only confirms the
  current card/app state and is not physical power-loss recovery evidence.
- A captured Java SmartcardIO `Card.disconnect(true)` reset test passed on the
  identified board: SELECT returned `9000` before reset; after PC/SC reset and
  reconnect, SELECT returned `9000`, GETVERSION returned
  `1.8.2_jc305`/`9000`, and JCSystem-info returned `9000`. Log:
  `artifacts/logs/pcsc-card-reset-2BED-20261030T0455Z.log`. This invokes CCID
  card power-off/on, not USB power removal or RP2350 cold reboot.
- Re-running `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passed
  25/25. Both initial and post-process-restart inventory logs report
  `static_roots=9 applet_roots=3 trace_complete=1`, 14/14 arrays, 4/4
  instances, and zero invalid references or edge/root failures. Distinct owner
  IDs remained stable; deleting one instance preserved the others. Logs:
  `host-vpcd-multipackage-apdu-20261030T045329Z.log`
  (`f5d8ce2f8575477a63b16bacd50344916d40c37abd64833369f741143ba967cd`) and
  `host-vpcd-multipackage-restart-20261030T045329Z.log`
  (`dd132b4f47944c3f720b2d5997e0384e78aa5322d8d9b2e63d58ce820ec94c00`).

### Simulated RP2350 overlay interruption recovery — 2026-10-30

- `./tools/test-rp2350-fs-overlay` passed 75 checks. It includes 1,000
  randomized multi-record snapshot power cuts, 1,000 install-journal clear
  cuts, 1,000 cross-store registry/filesystem header cuts, and simulated reset
  restoring the last committed transaction state. The expected diagnostic
  `FAULT FS snapshot unavailable; preserve flash for recovery` is emitted by
  an explicit negative recovery fixture; the overall result remains
  `TEST:SUMMARY fail=0`.
- This is host-model storage-layer evidence. It does not exercise Pico 2 flash
  under actual power loss while heap GC is operating, nor prove interrupted
  persistent-object reclamation safe. Keep actual persistent-record deletion
  disabled pending physical power-cycle recovery.

### Device collector now scans selected-package and applet roots; `0xFF` body-tag false positive fixed — 2026-10-30

- The firmware collector now uses a boolean root enumerator. It restores roots
  from the active package CAP's static reference slots and scans the selected
  applet owner's registered root tags before marking. Any adapter error or
  exception increments `root_enumeration_failures`, makes the mark incomplete,
  and prevents the collector from freeing heap slots. This is limited to the
  selected applet/package; all installed package roots and other live contexts
  remain uncovered. No persistent filesystem record deletion is implemented.
- Physical 2BED `2BED26ED20556535` was flashed with ARM UF2 SHA-256
  `58001f50ed7501b772afceacc8a071c0d62f39901a4b894ff7a4efbdd4fbf5c5`;
  cycle `artifacts/logs/cycle-choupi-20261030T030140Z.log` passed the finite
  runtime monitor. The unchanged installed JCAlgTest 1.8.2 SELECT, GETVERSION,
  and JCSystem-info APDUs all returned `9000`. Serial shows each of their three
  `interpreter_end` / `gc_begin` / `gc_end` sequences, with no
  `CHOUPI:HEAP_GC_SKIP` or root failure. Logs:
  `artifacts/logs/persistent-inventory-device-apdu-2BED-20261030-retry.log`
  (SHA-256 `a835343133df15784e387a82072e5ff27545a89212a2b9f339671a8f56b3181e`)
  and `artifacts/logs/persistent-inventory-device-gc-monitor-2BED-20261030-retry.log`
  (SHA-256 `de7a2d8b883c08e947893a29ab104dc9f3561aaa7f91ce30b74d05a0c5ffae47`).
- Root scanning had classified every owner tag ending in `0xFF` as a registered
  applet root. A reference-array body tag can also end in `0xFF`; the 7-byte
  array element tag then caused `InvalidTag` and skipped collection. Root
  recognition now accepts only the exact 5-byte and 6-byte forms. The inventory
  unit test includes a 7-byte array-body tag ending in `0xFF` and passes.
- A repeat physical transaction-probe attempt could not install its test CAP
  on 2BED: the card reported `FS_WRITE_FAIL ... bytes=56714/65535` when setup
  attempted a 32,770-byte persistent record, and GPPro returned `6A80`. No
  existing package was removed. The 8E cycle verified its flash but the board
  did not re-enumerate; use an available board as failover and leave full
  transaction-probe retest open.

### Standalone RMI unit build needs the CHOUPI OS include path — 2026-10-30

- `heap.cpp` now includes `ffi.h` for strict persistent-edge reads. The
  `tools/test-rmi-registry` CMake target compiles `heap.cpp` outside the main
  wrapper and must add `choupi/os/src` to its include paths. Add the include
  directory without linking or probing USB; the wrapper then builds and reports
  `RMI_REGISTRY:PASS` again.

### Persistent collector must scan every installed package's static roots — 2026-10-30

- Static reference fields are package-owned. Scanning only the selected applet
  package can miss objects retained by an imported package's initialized
  statics. Enumerate IDs through `JCVM_MAX_PACKAGES` and gate each walk with
  `FlashMemory_Handler::isPackageExist`; this also filters stale dynamic slot
  bits. Fail closed if package presence or root enumeration throws/fails.
- A valid CAP can omit the Static Field component. Treat that as an empty root
  set, not malformed metadata; otherwise scanning every installed package
  would stop at ordinary CAPs with no static fields.
- The PC diagnostic and ARM collector now share this enumeration. On physical
  2BED, JCAlgTest SELECT, GETVERSION, and JCSystem-info all returned `9000`,
  and captured serial showed three `gc_begin`/`gc_end` pairs without
  `HEAP_GC_SKIP`. Log:
  `artifacts/logs/persistent-inventory-all-package-device-monitor-2BED-20261030.log`.
- This covers package static roots only. Other live applet/context/native
  roots remain unaccounted; do not enable persistent record deletion.

## Canonical operating fixes and unresolved boundaries

### Stable owner IDs distinguish same-class applet instances — implemented 2026-10-30

- Registered applet root tags encode owner/package/class/root suffix, but not
  instance AID. Assign each new installed instance a unique owner ID and
  persist the AID-to-owner mapping in unused metadata-sector tail bytes. CAP
  header version 5 validates this owner map and includes it in the existing
  mirrored metadata publication. Install journal `INS2` carries the owner;
  `INS1` recovery and CAP metadata versions 3/4 preserve the legacy owner 0.
- Install, restore, and DELETE cleanup now use the recorded owner. Multiple
  instances of one applet class are allowed when their instance AIDs differ;
  the package may still have only one package AID. Owner IDs are allocated
  from 1..255 globally across installed packages. Do not reuse an owner for
  live instances.
- PC VPCD registry sidecars now write VCR2 and preserve each owner ID across
  process restart. VCR1 is still accepted and restores its historic owner 0,
  allowing old snapshots to remain usable. The Rust filesystem allows the
  trusted RuntimeEnvironment/Installer contexts to access explicit logical
  applet-owner tags; ordinary applet contexts remain scoped to their own ID.
- Validation: overlay power-cut/metadata tests pass, including distinct owner
  mapping for two same-class instances, registry reinitialization, and removal
  of one mapping without changing the other. PC VPCD multi-package restart and
  delete suite passes 23/23; JCSystem transaction applet suite passes 85/85.
  ARM/RISC-V builds pass, and identified 2BED firmware flash/finite startup
  monitor pass. Physical install of multiple instances and their root restore
  has not been exercised. The collector still only enumerates active-applet
  roots, so complete all-live-owner coverage and reclamation remain blocked.

- **Host USB/PCSC access — viable workflow:** follow root `AGENTS.md` from the
  first attempt: elevated host execution and `PICO2_HOST_USB_ACCESS=1` on each
  repository USB wrapper. The permitted ACL helper can restore access after
  re-enumeration, but does not guarantee recovery from a stuck daemon or absent
  USB passthrough. If host access is unavailable, continue offline. Never infer
  a firmware defect from absent USB/PCSC enumeration alone.
- **Board selection — viable workflow:** re-enumerate exact reader strings;
  suffixes such as `00 00`/`01 00` change. Select by confirmed chip ID, not
  reader index. BOOTSEL is not a PC/SC reader. Flash supports targeted chip-ID
  selection from runtime or BOOTSEL. Use an identified available board as
  failover and retain per-board image/results; don't transfer pass attribution.
- **Clean card — viable workflow:** flash preserves persistent CAP storage.
  Verify GPPro listing and successful temporary-CAP cleanup. Avoid assumptions
  based on older empty-card or missing-reader reports.
- **GET STATUS exact USB packet stall — FIXED:** 54-byte APDU + 10-byte CCID
  header filled one 64-byte packet; the host awaited termination. Submit an IN
  zero-length packet for nonempty exact-multiple bulk responses, rearm OUT only
  after completion, and clear pending state on reset/error. Physical GPPro list
  passed on D92; `artifacts/logs/gppro-get-status-zlp-D92-20261025.log`.
  Earlier module-query investigations are historical, not an open blocker.
- **Long native RSA — FIXED transport/progress path:** native RNG progress
  refreshes interpreter inactivity, watchdog, and USB service; periodic CCID
  time extensions preserve the active receive buffer until final response.
  Physical runner bound is 600 s, asymmetric VPCD APDU bound 180 s. Generation
  time varies; compare actual workload before diagnosing an algorithm defect.
- **Stale emulator — viable fix:** rebuild PC explicitly after native source
  changes; ARM/RISC-V builds do not update its binary.
- **VPCD storage/startup — viable workflow:** fresh zero-length image and matching
  fresh sidecar for isolated sweeps, dummy package metadata where required,
  explicit presence probe, `pipefail` on probe pipelines, one runner per reader.
  Wait for cleanup/session completion. Resume tests instead retain matching
  flash, registry, and assigned package ID and require clean process exit.
  Restricted local-socket errors require the authorized host execution path;
  they are not crypto/USB test results. Do not change machine configuration.
- **VPCD framing — viable fix:** no-input response APDU is five-byte case 2;
  option data plus response uses case 4, not `Lc=0` plus a sixth byte.
  Deploy runner changes cwd; use absolute output paths and area-specific AIDs.
- **Evidence classification:** unsupported setup can invalidate downstream test
  expectations. Require complete runner output/normal exit for suite passes;
  partial CSVs, filtered rows, emulator passes, physical runtime boot, and actual
  physical API acceptance establish different things.
- **Native API identity — established fixes:** exported NativeKey class offset
  must be resolved through export metadata, not a ROM token. JC305 interface
  and virtual tokens differ from embedded ROM; map concrete/redeclared Key
  prefixes, key getters, EC methods, and descriptor-specific native overrides.
  Refine marker checks with key type and return false for null `instanceof`.
- **GC/persistence — established constraints:** retain static/APDU/native mirror,
  typed top-level reference results, exception, transient lifecycle, exported RMI,
  and instance/reference-array graph roots. Preserve Java Card handles and
  persistent identity; test-only host stubs do not prove CAP lifecycle or power
  recovery. Archive records retain individual regression and recovery details.
- **Persistent write exhaustion — fixed error mapping; reclamation remains open:**
  filesystem status 1 is `OutOfFlash`/capacity exhaustion, so Java Card writes
  now throw `SystemException.NO_RESOURCE` (reason `0x0005`) rather than
  `IOException`/`6F00`; other filesystem errors remain `IOException`. Physical
  2BED API 3.0.5 Security retest completed 901 calls: 396 `OK`, 504
  `UNSUPPORTED_PER_SPEC`, one `NO_MEMORY`, no `SW_ERROR` or wrong results, and
  successful CAP cleanup. Full-run RSA CRT 2048 `setP` now reports the
  resource exception. `OverlayFile` and `FilesystemRecord` offsets are
  `uint16_t`; cap the arena at 65,535 bytes. A 77,824-byte trial overflowed
  these offsets, causing D92 `tag-reconstruct` erase failure and DELETE `6581`.
  D92 was subsequently sent the corrected image and flash verification passed,
  but it did not return as a PC/SC reader; its runtime/recovery state is
  unconfirmed. Never increase the arena without a storage format/offset-width
  migration. Do not reclaim persistent objects until roots and stable handles
  are safely accounted for. Evidence: corrected ARM UF2 SHA-256
  `e99fe641f95d45a8775edf8996bade276103134dbfd6ba05855f0f10847856ea`, CSV
  `artifacts/results/deploy/asymm-storage-20261029/2BED-security-bounded.csv`
  SHA-256 `cdc05f4904d39fddfda86b5233b8a3a61ea112b2b31f013a3455a9e55c918ea1`.
- **RSA-512 JCAlgTest support APDU stalled on persistent byte writes — batching fix:**
  VPCD trace showed RSA-512 primes ready in 539 ms, then `putRsaComponent()`
  performed repeated per-byte persistent writes. Added one bulk persistent
  array write per key component, keeping the byte-wise path for transient
  arrays. On a fresh PC VPCD image, unchanged APDU
  `B07519000301020000` improved from a 240 s timeout to 16.419 s and returned
  `9000`; native probe APDU `801A000000` took 0.288 s. ARM/RISC-V builds passed
  and physical 2BED flash/startup passed. The exact physical JCAlgTest APDU was
  then run three times on `2BED26ED20556535`: 1.532, 1.707, and 1.650 s,
  each returned status 00 and SW 9000. The cycle logged one
  `WATCHDOG_RECOVERY marker=494E5354 cause=other`; the marker interpretation
  is resolved below. A complete official support report passed on 8E; 2BED's
  retry reached 4,484/4,525 comparable rows before the explicit 900-second
  runner bound, with all 196 yes results present. Logs are indexed in progress
  under `rsa512-persistent-component-batch` and
  `rsa512-physical-apdu-validation`.
- **Reinstall test CAP after changing firmware capability profiles:** On 8E,
  the previously installed JCAlgTest CAP still selected and returned its
  version after flashing `asymm_crypto`, but JCSystem info and algorithm
  support APDUs returned `FF01`. Deleting only JCAlgTest's applet/package and
  reinstalling the unchanged upstream CAP restored JCSystem info and RSA
  support to `9000`; the complete physical support report then passed. Use
  this recovery for stale installed test-app state after a ROM/profile change.
  Preserve unrelated card packages.
- **`WATCHDOG_RECOVERY cause=other` is retained reset context, not proof of a
  watchdog fault:** Runtime startup derives this label from
  `watchdog_enable_caused_reboot()`. The marker is stored in RP2350 watchdog
  scratch registers, survives non-watchdog reset/reflash, and is consumed once.
  The 8E cycle emitted `cause=other` with stale-looking trace fields, followed
  by repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`; no
  watchdog reset or runtime failure was observed. Do not treat this line alone
  as a failure. Revisit if `cause=watchdog`, repeated resets, or a failing
  runtime check occurs.
- **Native-only boundary:** old Java crypto-provider plans are superseded.
  Implement behavior in native C++ API dispatch; preserve upstream CAPs.
- **RSA support-label mismatch:** JCAlgTest 1.8.2's 3072 KeyPair rows send 4096.
  Manifest remains 143/145 despite passing true physical 3072 vectors. Do not
  alter firmware/CAP semantics to hide the two upstream mislabeled rows.
- **Current status overrides:** physical `Applet.deselect()` discrepancy is
  resolved (`-19` both boards), and the repeated Core `6F00` cluster is fixed:
  successful GP DELETE now discards the selected applet context/heap, preventing
  a reinstall from reusing stale object fields after persistent array records
  are removed. Updated 2BED passed two consecutive complete Core runs. CPLC
  open/SCP03 hardware validation is complete. Latest board observations and
  evidence are in HANDOFF; old access failures are not current enumeration.

## Recent detailed issue records

The following retained records cover active investigations and recent fixes.
Their dates and validation scopes remain significant. Later follow-ups in
HANDOFF/TODO take precedence over an earlier statement that hardware was absent.

## OS trusted tag-cursor bridge and persistent-inventory groundwork — 2026-10-30

- **Persistent-storage GC groundwork:** The OS `FileSystem` owns an in-memory
  active-tag `HashSet`. It now has both a bounded-memory visitor and a trusted
  cursor syscall that returns one active tag per call, resumes by bucket and
  offset, filters by prefix, validates user pointers, and skips tags unreadable
  from the caller's context. Cursor stability requires the active index not to
  be mutated between pages. Tests cover paging, end-of-scan, and invalid prefix
  length. This bridge does not authorize deletion. Next, consume it in a native
  owner-scoped VM inventory that includes static roots, applet roots, object
  fields, array bodies, and aliases before any reclamation.
- **C++ cursor wrapper integration:** `FileSystem` construction calls `fs_init`,
  so a root-inventory caller must not instantiate it just to page tags. The
  `nextTag` wrapper is static and can be called without storage reinitialization.
  PC host build compiles the wrapper; it is not yet consumed by a VM root scan.
- **Validation:** Host functional suite passed 67, 0 failed, 2 ignored (generated
  `bindgen_test_layout*` tests skipped because they abort with `null reference
  produced`). ARM and RISC-V repository builds passed. Identified physical
  board 2BED `2BED26ED20556535` was flashed through the elevated host-access
  wrapper; finite CHOUPI runtime passed with `TEST:SUMMARY pass=1 fail=0`.
  Runtime log: `artifacts/logs/cycle-choupi-20261030T004223Z.log`.
- **Host OS test invocation:** `javacard-os` unit tests need host `PTRACE_ATTACH`;
  run the repository toolchain wrapper with elevated host execution. The
  generated `bindgen_test_layout*` tests currently abort with `null reference
  produced`; skip those when running functional OS tests. The remaining suite
  passed 66 tests with two ignored after updating one stale expected
  `assert_eq!` panic string to the current Rust format. This panic-text update
  only repairs test compatibility; it does not alter runtime behavior.

## RSA temporary byte vectors retained secret material after use — 2026-10-29

- **Symptom:** RSA native code copied private key components, decrypted
  plaintext, padding state, and encoded private-operation blocks into ordinary
  vectors, whose storage was released without an explicit wipe.
- **Cause:** The vectors had no cleanup policy, so normal return and Java Card
  exception unwinding both left cleanup to the general allocator.
- **Viable fix:** Attach a non-copyable scoped zeroization guard immediately
  after creating sensitive buffers. Use it for byte vectors, fixed byte
  arrays, and arrays of byte vectors; keep returned signature/ciphertext or
  decrypted result buffers alive until their caller has consumed them.
- **Validation:** PC, ARM, and RISC-V `asymm_crypto` builds pass, and the
  existing VPCD asymmetric suite passes 99/99. Physical validation was not
  performed because no attached Pico had been established in this step.
- **Limit:** This reduces residual heap copies but does not solve private RSA
  input blinding or CRT fault verification; those still need a public exponent
  association/design.
- **Follow-up:** The RSA-PSS helper's generated salt, hash input, MGF1
  scratch/masks, and decoded data block are also guarded and wiped. PC, ARM,
  RISC-V, and VPCD 99/99 validations pass (implementation commit `4ecd522`).
- **Follow-up:** RSA Signature entry points now wipe copied message/hash/signature
  inputs and outputs, RSA verification scratch, and ISO9796 recovered-message
  intermediates (commit `df2d2d9`). All target builds and VPCD 99/99 pass.
- **Follow-up:** The remaining RSA CRT factor copies and OAEP MGF1 digest
  scratch are guarded (commit `bedf23a`). Audit of the pinned SDK confirms
  `mbedtls_mpi_free()` wipes allocated limbs through
  `mbedtls_mpi_zeroize_and_free()`.


## Long synchronous RSA operations time out on hardware (2026-10-27)

- **Symptom:** Physical RSA-3072 key generation returned `6400` after about
  150 seconds despite a healthy device. A subsequent full test harness also
  terminated during RSA-CRT-3072 at its 180-second host timeout.
- **Cause:** The firmware applied a fixed 15-second interpreter wall-clock
  limit around the entire applet call. Long native RSA generation is
  synchronous; Mbed TLS repeatedly calls its RNG callback while searching
  candidates. CCID clients also need time-extension responses during that call.
- **Viable fix:** Refresh the interpreter's applet progress deadline from the
  native RNG callback, while feeding the watchdog and servicing USB. Send
  periodic CCID time extensions during native crypto and keep Bulk-OUT
  unarmed until the original APDU completes, avoiding reuse of the APDU receive
  buffer during execution. Raise the finite physical harness timeout to 600
  seconds to accommodate RP2350 RSA-CRT-3072 variance.
- **Validation:** On D92, isolated RSA-3072 passed in 137.724 s and isolated
  RSA-CRT-3072 passed in 171.517 s. The complete physical suite passed 79/79;
  its RSA-3072 and RSA-CRT-3072 generation cases took 138.521 s and 110.769 s.
  ARM/RISC-V wrapper builds, finite serial monitor, and PC/VPCD 99/99 vector
  suite passed. Logs are recorded in `.agent/HANDOFF.md`.
# Physical API 3.0.5 KeyPair and marker-interface failures (2026-10-28)

- **Symptom:** On physical combined `asymm_crypto` firmware, the unchanged
  Security 3.0.5 CAP got `6F00` from every `KeyPair(byte, short)` constructor
  probe (RSA, RSA-CRT, and EC_FP across tested sizes). Seven selected
  `genKeyPair` probes also got `6F00`. The same run's marker-interface checks
  for `SecretKey`, `PrivateKey`, and `PublicKey` returned native key type IDs
  rather than booleans. One null EC parameter-copy probe had an uncaught VM
  exception. Crypto had additional `6F00` and exception-reason mismatches.
- **Cause:** Not yet established. Direct physical RSA-3072 and RSA-CRT-3072
  vectors passed, so constructor dispatch/object setup needs isolation before
  attributing this to cryptographic arithmetic. Marker results suggest a
  native return-type or `invokeinterface` contract issue, but this is only a
  hypothesis pending trace/reproduction.
- **Viable fix:** Not established. Reproduce on current PC/VPCD with the
  unchanged deploy CAP and isolate one constructor first. Trace API native
  dispatch and interpreter return handling; preserve all upstream CAPs and
  implement behavior only in native VM code.
- **Validation:** Physical D92 API 3.0.5 Core/Security/Crypto sweep completed
  in 12 minutes, CSV
  `artifacts/results/deploy/asymm_crypto_D92_20261028/core_security_crypto_retry.csv`.
  Per-area counts and cleanup are in `.agent/HANDOFF.md`. No fix has been
  applied yet; do not claim these paths pass.


## JC305 NativeKey marker-interface checks must use the key type (2026-10-28)

- **Symptom:** Focused API 3.0.5 Security tests for `SecretKey`, `PrivateKey`,
  `PublicKey`, and concrete key marker interfaces returned `WRONG_RESULT` on
  the native VM despite the corresponding object types being correct.
- **Cause:** The ROM models key instances with one `NativeKey` class. That
  class declares a union of interfaces used by several key families, while
  Java Card assignability depends on the stored key type. Applying the ROM
  class metadata directly therefore reported AES/DES/RSA/EC interfaces too
  broadly. JC305 interface tokens also differ from the embedded API's token
  numbers, so matching must use the source interface token only for CAPs
  importing the JC305 security package AID.
- **Viable fix:** `Bytecodes::docheck()` now refines known JC305 security
  interface checks against the `NativeKey` type field. The token generator
  emits stable names for the JC305 source interface tokens. `instanceof(null)`
  now returns false and pushes its result as a byte boolean. Keep this logic
  in native VM code and do not modify API sources or CAPs.
- **Validation:** Final-source focused VPCD Security run completed 901 calls:
  11 supported marker checks `OK`, eight unsupported cases
  `UNSUPPORTED_PER_SPEC`, 882 intentionally filtered; upload and cleanup
  passed. Artifact:
  `artifacts/results/deploy/marker-interface-vpcd-20261028-type-aware10/`.
  PC, ARM, and RISC-V asymm_crypto builds passed. Physical D92 flash and finite
  monitor passed on the immediately prior source (before the null-byte stack
  correction); final source was not flashed because no Pico enumerated after
  the subsequent cycle's BOOTSEL request timed out.
- **VPCD setup note:** The host emulator invocation must include
  `--applet-aid`, `--package-id 63`, the contiguous dummy `--cap-code`, and
  `--rom-mask`. Start the empty card without `--install`, wait briefly, then
  run `VpcdPresenceProbe` against the matching reader (`Virtual PCD 00 01` is
  port `35964`) with `--require-present` before starting the deploy client.
  Omitting cap code or the presence probe led to `TEST_NO_APPLET`/probe timeout;
  those were harness setup failures, not VM results.


## VPCD API 3.0.5 reruns can encounter stale or duplicate Security CAP state (2026-10-28)

- **Symptom:** After a focused Security API run uploaded a CAP but the client
  timed out before cleanup, a later deploy run reported
  `UPLOAD_OTHER` with `Package with AID 4A43416C6754657374417069536563 is
  already present on card`. A `--test-only` retry then reported
  `TEST_NO_APPLET`. These occurred while the VPCD presence probe returned
  `present=true`.
- **Cause:** Not established. Possible stale package state, applet registry
  state, or selecting a different Virtual PCD reader than the active emulator;
  do not assume the VM rejected the tested method. GPPro inventory through a
  fresh emulator session listed only ISD, but a subsequent upload still
  reported the Security package present, so inventory and upload appear to
  address different state or reader contexts.
- **Viable fix:** None yet. Before a rerun, confirm reader-to-endpoint mapping,
  query package/app registry on the same reader, and uninstall/restore a clean
  image before retrying. Keep each emulator image and log in a separate
  artifact directory. The known-good marker validation setup uses
  `Virtual PCD 00 01` at `127.0.0.1:35964`, dummy package metadata/cap-code,
  and a successful presence probe.
- **Validation:** Focused `getK`/`copyDomainParametersFrom` attempt uploaded
  the Security CAP but timed out before producing call rows. A repeat hit the
  duplicate-package warning; test-only had no applet. `lsusb` showed no
  physical Pico at the time. Logs are under
  `artifacts/results/deploy/ec-key-guards-vpcd-20261028*/`. `pcsc_scan -r`
  showed two Virtual PCD readers; the allowed physical helper found zero Pico
  devices. A GPPro inventory against a fresh `Virtual PCD 00 01` session listed
  only ISD. A direct GPPro Security CAP install later printed `CAP loaded` then
  exited 1 before reporting INSTALL status. Do not infer successful install
  from the LOAD message alone.


## JC305 redeclared Key method prefixes need per-interface token remapping (2026-10-28)

- **Symptom:** API 3.0.5 Security `KeyPair(byte, short)` constructor calls
  returned `6F00`; tracing showed a later `IF_SCMPNE` stack underflow after
  `INVOKEINTERFACE`. The constructor had invoked `PublicKey.getSize()`.
- **Cause:** JC305's imported `PublicKey` (class token 3) redeclares the four
  inherited `Key` methods, whose method order differs from the embedded API.
  The JC305-to-embedded token translation `{1,3,2,0}` had only been applied to
  `Key` class token 0. The call therefore reached `NativeKey.clearKey()`
  instead of `getSize()S`, leaving a value on the operand stack. The same
  redeclared prefix exists on `PrivateKey`, `SecretKey`, DSAPrivateKey,
  DSAPublicKey, KoreanSEEDKey, DHPrivateKey, DHPublicKey, and RSA key
  interfaces already handled by specialized maps. Do not apply the prefix map
  to DSAKey, whose token 1 represents a DSA-specific operation.
- **Viable fix:** Apply the common four-method translation to imported JC305
  key interfaces that redeclare the `Key` prefix, before specialized full
  interface maps. Current implementation is in
  `choupi/src/jc_handlers/jc_cp.cpp`.
- **Validation:** Unchanged API 3.0.5 Security CAP RSA-512 constructor failed
  with `6F00` on current-source VPCD before the fix and passed afterward
  (one selected call; 900 excluded). PC, ARM, and RISC-V asymm_crypto builds
  passed. Identified physical D92 flash verification and finite startup monitor
  passed, but the constructor itself has not yet been tested physically.
  Commit: `a6bdb2f`.


## Full JC305 Security sweep loses PC/SC after RSA public 4096 KeyBuilder (2026-10-28)

- **Symptom:** With the corrected JC305 key-interface method mapping, focused
  physical D92 API 3.0.5 tests passed all 39 `KeyPair(byte, short)` constructor
  probes (27 supported OK, 12 `UNSUPPORTED_PER_SPEC`) and 14 `genKeyPair()`
  probes (9 supported RSA/RSA-CRT/EC_FP OK, five `UNSUPPORTED_PER_SPEC`). A
  subsequent full Security sweep aborted after 42 calls while testing
  `KeyBuilder.buildKey(TYPE_RSA_PUBLIC, LENGTH_RSA_4096)`: the first failed APDU
  surfaced `SCARD_E_NOT_TRANSACTED`; following calls reported a disconnected
  card and cleanup could not connect.
- **Cause:** Not established. This is a PC/SC transport loss, not a card status
  word or explicit unsupported response. The physical USB device and serial
  console remained present; a finite serial monitor afterward passed repeated
  runtime checks. PC/SC then listed only the two Virtual PCD readers.
- **Viable fix:** None established. After PC/SC reader enumeration recovers,
  isolate only the RSA public 4096 `KeyBuilder` probe, capture finite serial
  output immediately after failure, and distinguish a firmware reset/CCID state
  issue from host PC/SC loss. Do not relabel the 4096 result as expected
  unsupported without a card response.
- **Validation:** Focused physical CSVs are
  `AlgTest_JavaCard/deploy/results/keypair-constructor-physical-20261028/`
  (`all-keypair-constructors.csv` and `keypair-generation.csv`). Full-run CSV
  `security-full.csv`; post-failure D92 monitor passed.

## API 3.0.5 Security sweep exhausts persistent filesystem at RSA CRT setP (2026-10-29)

- **Symptom:** Two full physical Security runs on 2BED completed 901 calls but
  returned `6F00` from `RSAPrivateCrtKey.setP(TYPE_RSA_CRT_PRIVATE,
  LENGTH_RSA_2048)` at the same point. The exact call passed when isolated.
  A narrowed rerun including the preceding API test classes reproduced the
  same failure; the next `setQ`, `setDP1`, `setDQ1`, and `setPQ` rows passed.
- **Cause:** Serial trace proved persistent filesystem exhaustion, not RSA
  arithmetic failure. `fs_write` rejected a new 131-byte array record with
  5,929 records and 65,409/65,536 bytes occupied. The old `OutOfFlash` to
  `IOException` mapping escaped the CAP as `6F00`. **Resolution:** status 1 now
  maps to `SystemException.NO_RESOURCE` (reason 5), validated by the bounded
  physical rerun documented under “Persistent write exhaustion” above. The
  65,535-byte arena limit protects current `uint16_t` offsets; a 77,824-byte
  trial corrupted snapshot offsets and is rejected. Safe persisted-root
  enumeration/reclamation remains open. Do not free persistent records by
  clearing heap slots alone because Java Card references encode stable handles.
- **Validation:** The physical 3.0.5 Crypto run on asymm_crypto completed
  466 calls (302 `OK`, 163 `UNSUPPORTED_PER_SPEC`, one known SHA-512 PSS
  default-salt size exception) and all previous Crypto failures except that
  valid size error passed. Security ran twice (396 `OK`, 503/504 unsupported,
  two/one `6F00` respectively). The isolated `setP`/`setPQ` plus public RSA
  4096 KeyBuilder tests passed 3/3. The narrowed 629-call prefix reproduced
  one `setP` failure. Monitor log:
  `artifacts/logs/2BED-asymm-security-prefix-serial.log`; CSVs and hashes are
  recorded in `progress.md` under `physical-asymm-crypto-security-retest`.
- **Follow-up:** Audit object/root retention and safe persistent reclamation.
  Do not classify this as an RSA implementation defect, and do not increase
  flash partitions or alter configuration without a measured layout review.

## NativeKey.clearKey byte-by-byte persistence stalled EC API calls (2026-10-29)

- **Symptom:** The focused API 3.0.5 uninitialized `ECKey.getK()` request timed
  out over VPCD after 15 seconds. The VM trace reached native dispatch index 7
  (`NativeKey.clearKey`) while preparing the test and repeatedly wrote
  persistent key-array bytes.
- **Cause:** `clearKey()` zeroed each persistent RSA/EC component by calling
  `JC_Array::setByteEntry()` for every byte. Each byte incurred an individual
  filesystem syscall; the clear did not finish within the focused API test
  deadline.
- **Viable fix:** For persistent byte arrays, validate the stored byte-array
  record and write one zero-filled record through the existing
  `write_persistent_tag()` path so transaction snapshots are preserved. Keep
  the in-memory byte loop for transient arrays. `NativeKey.clearKey()` uses
  this for both RSA and EC components.
- **Validation:** The unchanged API 3.0.5 Security CAP completed 901 calls on
  VPCD after the change: uninitialized `ECKey.getK()` was `OK`, 900 unrelated
  calls were intentionally filtered, and GPPro upload/cleanup passed. PC,
  ARM, and RISC-V `asymm_crypto` builds passed. No physical Pico was connected;
  hardware validation remains open. Final report:
  `artifacts/results/deploy/ec-key-getk-final-20261029/`.

## Persistent key component writes stalled EC API setup (2026-10-29)

- **Symptom:** The focused API 3.0.5 `copyDomainParametersFrom(null)` test
  timed out before reaching the null-source method. Dispatch tracing showed it
  stalled earlier while the applet initialized key state through native
  `NativeKey.setA` (dispatch index 29). Persistent EC component setters and
  domain copies wrote each array byte with an individual filesystem syscall.
- **Cause:** Per-byte persistent array updates made key setup/copy proportional
  to repeated filesystem syscall cost. The earlier clear-only bulk optimization
  did not cover setter writes.
- **Viable fix:** Build a full padded byte vector in memory, validate and update
  the existing persistent byte-array record with one transaction-aware
  `write_persistent_tag()` call, then update the component length. Use the same
  path for persistent RSA/EC setters and EC domain copying; retain per-byte
  updates for transient arrays. This change also centralizes bulk clearing.
- **Validation:** Unchanged API 3.0.5 Security CAP VPCD runs passed for
  uninitialized `ECKey.getField()` and `copyDomainParametersFrom(null)` (901
  calls each, selected row `OK`, 900 unrelated rows filtered, upload and
  cleanup passed). PC, ARM, and RISC-V `asymm_crypto` builds passed. No
  physical Pico was connected. Results:
  `artifacts/results/deploy/ec-key-getfield-final-20261029/` and
  `artifacts/results/deploy/ec-key-copy-domain-null-batched-20261029/`.

## VPCD endpoint can stall after a focused API timeout (2026-10-29)

- **Symptom:** After a 40-second focused EC API test timeout on `Virtual PCD 00
  01` (endpoint `127.0.0.1:35964`), the next run on the same reader timed out
  in GPPro upload after 50 seconds even though the reader was enumerated.
- **Cause:** Not established. Reader enumeration alone did not establish that
  the VPCD transport was transacting. A fresh presence probe succeeded on the
  other paired endpoint and tests then passed there.
- **Viable workaround:** Start a fresh VPCD emulator, run
  `pcsc_scan -r`, require `VpcdPresenceProbe` to report `present=true`, and
  after repeated `00 01` failures switch to its paired reader/endpoint
  `Virtual PCD 00 00` / `127.0.0.1:35963`. Do not interpret a GPPro upload
  timeout as a VM API result.
- **Validation:** On `00 00`/35963, unchanged API 3.0.5 Security CAP tests
  passed for valid P-256 domain copy, mismatched source size, and uninitialized
  source. Artifacts:
  `artifacts/results/deploy/ec-key-copy-domain-p256-reader00-20261029/` and
  `artifacts/results/deploy/ec-key-copy-domain-errors-reader00-20261029/`.

## Physical Cipher factory 6F00 not reproduced in VPCD (2026-10-29)

- **Symptom:** A prior physical API 3.0.5 Crypto report had a `6F00` for the
  `javacardx.crypto.Cipher` `Cipher()` scenario.
- **Cause:** Not established. This may be card-state or transport-specific;
  current evidence does not isolate it to native `Cipher.getInstance` code.
- **Viable next step:** Do not change native implementation based only on that
  physical row. Rerun the unchanged Crypto CAP on an identified physical Pico
  and compare its exact CAP, profile image, reader, and row against the VPCD
  result before attributing the failure.
- **Validation:** Focused unchanged API 3.0.5 Crypto CAP completed 466 VPCD
  calls: selected `Cipher()` returned object token `86` as expected, 465 other
  calls were filtered, and GPPro cleanup passed. Result:
  `artifacts/results/deploy/cipher-factory-vpcd-20261029/`.
  A second fresh-image run at 07:10 local repeated the same one-row pass and
  cleanup. Neither run establishes why the physical 6F00 occurred.

## Both VPCD endpoints timed out during CAP upload (2026-10-29)

- **Symptom:** After an earlier focused API timeout, a presence probe on the
  paired VPCD endpoint reported the reader present, but subsequent GPPro CAP
  uploads timed out on both `Virtual PCD 00 00` (`127.0.0.1:35963`) and
  `Virtual PCD 00 01` (`127.0.0.1:35964`). The native test applet was never
  installed and the targeted native methods were not invoked.
- **Cause:** Unknown. Reader enumeration and a successful presence probe did
  not establish that a later PC/SC transaction could complete.
- **Viable solution:** None confirmed for this two-endpoint failure. The prior
  workaround (fresh emulator, presence probe, switch paired endpoint) remains
  useful for a single stalled endpoint, but do not repeat an upload timeout
  and interpret it as a runtime API result. Refresh/restart the emulator
  session through its normal test harness, verify transport, then retry once.
- **Validation:** Both endpoint upload timeouts are recorded in
  `artifacts/results/deploy/signature-initial-digest-invalid-trace-20261029/`
  and `signature-initial-digest-invalid-trace-reader01-20261029/`. No physical
  Pico reader was enumerated.

## VPCD endpoint refused the emulator after its service had idled (2026-10-29)

- **Symptom:** GPPro could not connect although the Virtual PCD readers were
  listed by PC/SC. The emulator logged `ERROR: connecting to vpcd endpoint
  127.0.0.1:35964: Connection refused` and exited.
- **Cause:** The PC/SC plug-and-play service had gone idle between emulator
  runs. Reader enumeration alone did not leave the VPCD TCP backend ready. A
  background emulator command also exited with its launching shell, and two
  concurrent API clients contended for the same virtual card during diagnosis.
- **Viable solution:** Through the elevated host-access path, run
  `PICO2_HOST_USB_ACCESS=1 ./tools/env pcsc_scan -r` before each new emulator
  session. Keep the emulator attached to a persistent command session (do not
  background it from a short-lived shell), require
  `VpcdPresenceProbe ... --require-present`, and run only one API client at a
  time. Use `--test-only` for subsequent isolated cases while the CAP remains
  installed.
- **Validation:** GPPro listed the installed Crypto CAP after this sequence.
  The unchanged API 3.0.5 Crypto CAP then passed the selected RSA/SHA-256
  `Signature.setInitialDigest()` known-signature case over VPCD (465 checks
  skipped). Evidence:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/crypto-sha256-testonly.csv`.

## Signature initial-digest metadata exceeded the native IV storage (2026-10-29)

- **Symptom:** Valid RSA and ECDSA `Signature.setInitialDigest()` calls
  returned `EXC_RUNTIME`, while invalid length/block tests correctly returned
  `ILLEGAL_VALUE`.
- **Cause:** The native Signature factory allocates a 16-byte IV array, but the
  initial-digest metadata encoding required 18 bytes (128-bit byte count plus
  a redundant 16-bit state length). `storeInitialDigestState()` therefore
  raised `ArrayStoreException` for every valid state.
- **Viable fix:** Keep the full 128-bit SHA-384/512 byte count in the existing
  16-byte array and derive the state length from the digest algorithm. The
  state itself remains in `recoveryHash`; do not enlarge the IV array because
  MAC algorithms depend on its existing 16-byte size.
- **Validation:** The full unchanged upstream `setInitialDigest` group passed
  on VPCD 00 00: four RSA/ECDSA SHA-1/SHA-256 valid vectors and three invalid
  length/block cases, in a 466-call Crypto run (10 OK, eight expected
  unsupported, 448 filtered). SHA-224/384/512 and physical validation remain
  open.
- Paired-endpoint result: an isolated ECDSA/SHA-256 run timed out on 00 01,
  while isolated ECDSA/SHA-1 and SHA-256 round trips both passed on
  `Virtual PCD 00 00` / `127.0.0.1:35963` (465 calls skipped each). For these
  expensive Crypto test vectors, switch to 00 00 after a 00 01 timeout rather
  than treating the first timeout as a VM failure. Artifacts are
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/crypto-ecdsa256-reader00.csv`
  and `crypto-ecdsa-sha1-reader00.csv`.
- The earlier physical `signPreComputedHash()` ECDSA/SHA-1 and SHA-256
  `ILLEGAL_USE` results are not reproduced by the unchanged Crypto CAP on
  VPCD: both selected sign/verify round-trip cases pass on 00 00. Use a
  200-second API client deadline for those regex-filtered calls because the
  runner emits/filters preceding Crypto rows before reaching them. Results:
  `crypto-ecdsa-precomputed-sha256-testonly-reader00-retry.csv` and
  `crypto-ecdsa-precomputed-sha1-reader00.csv` in the signature test artifact
  directory.

## RSA precomputed-hash length error used the wrong reason code (2026-10-29)

- **Symptom:** The upstream API 3.0.5 Crypto CAP reported
  `CryptoException.ILLEGAL_VALUE` (`0x0001`) for a malformed precomputed hash,
  while the API contract requires `ILLEGAL_USE` (`0x0005`).
- **Cause:** Native RSA `signPreComputedHash()` and
  `verifyPreComputedHash()` classified a hash-length/digest-length mismatch as
  `ILLEGAL_VALUE`.
- **Viable fix:** Throw `CryptoException.ILLEGAL_USE` from both native RSA
  precomputed-hash entry points when the hash length differs from the selected
  message digest size.
- **Validation:** Unchanged upstream Crypto CAP VPCD test passed the malformed
  hash row with expected reason `0x0005`; 465 other calls were skipped. PC,
  ARM, and RISC-V `asymm_crypto` builds passed. No physical validation was run.
  Artifact:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/crypto-rsa-precomputed-invalid-reader00-fixed.csv`.

## SignatureMessageRecovery beginVerify mode error (2026-10-29)

- **Symptom:** The API 3.0.5 Crypto CAP's `beginVerify after init for sign`
  case did not receive the specified `CryptoException.ILLEGAL_USE`. The earlier
  physical run recorded reason `0x0004` (`INVALID_INIT`); before the fix, the
  current PC JCVM/VPCD case returned `6F00`.
- **Cause:** `beginVerify()` used the shared `requireInitialized(...,
  MODE_VERIFY)` guard. That guard reports `INVALID_INIT` for either an
  uninitialized object or a mode mismatch, but the Java Card
  `SignatureMessageRecovery.beginVerify()` contract specifically requires
  `ILLEGAL_USE` when initialized for signing.
- **Viable fix:** First call the no-mode initialization check, then report
  `ILLEGAL_USE` for a non-verify mode in `javacard_security_Signature_beginVerify`.
  This preserves `INVALID_INIT` when the object was never initialized.
- **Validation:** Unchanged upstream API 3.0.5 Crypto CAP on current PC JCVM/VPCD
  passed the selected row (`OK`; 465 other rows filtered). CAP upload and
  cleanup passed. PC, ARM, and RISC-V `asymm_crypto` builds passed. No physical
  device was available. Source commit: `be0ea69`.
- **Other diagnostic:** RSA/SHA-512 PSS on the same API CAP returned
  `CryptoException.ILLEGAL_VALUE` in VPCD. Native PSS initialization checks
  whether the key can fit the SHA-512 digest and default digest-length salt;
  insufficient key length is a plausible explanation, but the test key size
  and exact failing subcall were not established. Do not change this case
  without isolating those details.
- **Artifacts:**
  `artifacts/results/deploy/crypto-current-vpcd-20261029/mr-beginverify-wrong-mode-fixed.csv`,
  `rsa-sha512-pss.csv`, and `rsa-iso9796-params.csv`.

## SignatureMessageRecovery parameterized init must return ILLEGAL_USE (2026-10-29)

- **Symptom:** The API 3.0.5 Crypto CAP case `ALG_RSA_SHA_ISO9796_MR init with
  parameters (illegal use)` returned `CryptoException.ILLEGAL_VALUE` in VPCD.
- **Cause:** The overload `Signature.init(Key, mode, parameters, offset,
  length)` sent all RSA algorithms through the PSS parameter handler, which
  returned `ILLEGAL_VALUE` because message-recovery algorithms do not accept
  this overload. The Java Card `SignatureMessageRecovery` contract explicitly
  requires `ILLEGAL_USE` for parameterized init.
- **Viable fix:** In the native parameterized-init dispatch, detect a
  message-recovery Signature before the generic RSA/PSS handling and throw
  `CryptoException.ILLEGAL_USE`.
- **Validation:** Unchanged API 3.0.5 Crypto CAP on current PC JCVM/VPCD passed
  the selected row (`OK`; 465 other rows filtered); upload and cleanup passed.
  PC, ARM, and RISC-V `asymm_crypto` wrapper builds passed. No physical device
  was available. Source commit: `2ae55a4`.
- **Reference:** [Oracle Java Card `SignatureMessageRecovery`](https://docs.oracle.com/en/java/javacard/3.2/jcapi/api_classic/javacard/security/SignatureMessageRecovery.html).
- **Artifacts:**
  `artifacts/results/deploy/crypto-current-vpcd-20261029/mr-init-parameters-fixed.csv`.

## KeyAgreement secp192r1 VPCD failures were not repeatable (2026-10-29)

- **Symptom:** The dated physical API 3.0.5 report had `6F00` for the
  `KeyAgreement()` factory, two secp192r1 DH/DHC known-secret operations, and
  initialization with an uninitialized EC private key.
- **Investigation:** On current PC JCVM/VPCD, the factory returned `OK`, the
  uninitialized-key case returned the expected `UNINITIALIZED_KEY` (`0x0002`),
  and ECDSA/SHA-1 with secp192r1 passed. DH and DHC each returned `6F00` once;
  the runtime diagnostic identified exception ID 5 (`IOException`). A
  trace-enabled repeat passed both rows (`returned=0x0014` each). Temporary
  PC-only diagnostics around persistent byte-array reads reported no read
  failure on the passing repeat and were removed afterward.
- **Cause:** Not established. No native code change is justified by one
  non-repeatable `IOException` followed by a passing run. Consider a transient
  PC emulator/persistent-storage interaction; this is only a hypothesis.
- **Viable fix:** None confirmed. Preserve the exact-image/profile and reader
  identity when retesting on physical hardware; capture VM diagnostics if the
  failure recurs.
- **Validation:** Clean-source PC, ARM, and RISC-V `asymm_crypto` builds pass.
  VPCD result files:
  `ecdsa-sha1-secp192-known.csv`, `ec-svdp-p192-known.csv`,
  `ec-svdp-p192-traced.csv`, `keyagreement-factory.csv`, and
  `keyagreement-uninitialized.csv` under
  `artifacts/results/deploy/crypto-current-vpcd-20261029/`.

## VPCD presence probe and overlapping API runner attempts (2026-10-29)

- **Symptom:** A VPCD presence probe printed `present=false`, but a shell
  pipeline ending in `tee` still returned success. A later attempt overlapped
  the still-running runner against the same PC/SC reader, producing duplicate
  package warnings, `SCARD_W_RESET_CARD`, disconnects, and failed cleanup.
- **Cause:** The probe's nonzero status was masked by the successful `tee`
  command because the shell did not enable `pipefail`; the exec session had not
  yet completed when a second runner started. These overlapping-run artifacts
  are invalid behavioral evidence.
- **Viable procedure:** Use `set -euo pipefail` for probe pipelines, require
  `present=true` before the runner, use a fresh emulator flash image for every
  attempt, and wait until the runner exec session exits before reusing the
  reader. The clean follow-up ran both secp192r1 DH/DHC known-secret cases
  successfully (2 `OK`, return `0x0014`) and cleaned up the CAP.
- **Artifacts:** `artifacts/results/deploy/crypto-dh-vpcd-20261029/`; clean
  output is in `retry2/`.

## JCAlgTest host VPCD applet path passes after restore (2026-10-29)

- **Observation:** Current-source `tools/test-pc-vpcd-applet --passes 2` passed
  twice through the unchanged JCAlgTest v1.8.2 CAP, including GETVERSION,
  JCSystem, support probes, SHA-256 doFinal and static performance, and the
  expected unsupported one-shot factory result. Each pass reported 15/15; the
  second emulator process resumed the installed applet from the saved image.
- **Resolution:** The older TODO statement that the VPCD applet failed on its
  first applet field access is stale for current source. The host integration
  path now validates applet stateful API calls and process restart. It does not
  establish physical Pico persistence or Windows PC/SC behavior.
- **Timing/artifact:** First pass 58.449 s, second process 18.333 s, initial
  VPCD connect 23.570 s. Emulator log:
  `artifacts/logs/host-vpcd-jcalgtest-20261029-two-pass.log`.

## Full API 3.0.5 Crypto VPCD sweep is too slow for a normal session (2026-10-29)

- **Observation:** The unchanged unfiltered Crypto CAP processed 190 of 466
  calls in about 17 minutes on PC JCVM/VPCD. The processed rows were 112 `OK`
  and 78 `UNSUPPORTED_PER_SPEC`, with no failed call. The latest processed
  algorithm was `ALG_RSA_SHA_256_PKCS1`.
- **Action and cleanup:** Stopped the Java client after the observed pace
  implied several hours for all calls. The wrapper reported `CLEANUP OK` and
  removed `AlgTestApiCrypto_305.cap`; the emulator and `pcsc_scan` keepalive
  were stopped. Do not count this as full acceptance.
- **Viable approach:** Use filtered VPCD runs for focused rows and start from a
  fresh zero-length flash image per sweep. Only schedule the full unfiltered
  CAP run in a window long enough to complete all 466 calls and cleanup.
- **Artifacts:** `artifacts/results/deploy/crypto-current-vpcd-20261029/`
  (`full-crypto-current.csv`, `full-crypto-emulator.log`); raw partial client
  log at `/tmp/apitest_card_5x39_p6b/APITEST_log_1793245348258.log`.

## Local asymmetric VPCD test needs an unrestricted local socket (2026-10-29)

- **Symptom:** `./tools/env python3 tools/tests/test_asymm_crypto_vpcd.py`
  failed immediately in the restricted execution context with
  `TEST:FAIL [Errno 1] Operation not permitted`.
- **Viable workaround:** Run this local PC/VPCD test through the approved
  elevated execution path so its loopback emulator socket can be created. The
  test did not access USB or PC/SC.
- **Validation:** Elevated run passed `TEST:SUMMARY pass=99 fail=0`, including
  50 RSA Signature vectors and SHA-512 PSS precomputed sign/verify using an
  explicit zero-salt parameter. This confirms that valid native SHA-512 PSS
  path. It does not explain the separate API 3.0.5 CAP `ILLEGAL_VALUE`, whose
  generated key size/default salt constraints remain unverified.
- **Artifact:** `artifacts/logs/asymm-crypto-keypair-vpcd-20261027.log`.

## API Crypto SHA-512 PSS row uses an undersized 1024-bit test key (2026-10-29)

- **Symptom:** The API 3.0.5 Crypto CAP's
  `ALG_RSA_SHA_512_PKCS1_PSS` sign/verify sequence returned
  `CryptoException.ILLEGAL_VALUE` (`0x0001`) on current PC JCVM/VPCD.
- **Cause:** Inspection of the unchanged host client bytecode shows the row's
  case 46 maps to `K_RSA`, and `selectKeys(K_RSA)` uses
  `ApiTestCryptoKeys.rsaCrt()`, which builds a 1024-bit RSA-CRT key. Default
  PSS salt length equals the SHA-512 digest size (64 bytes). PSS needs at least
  `hLen + sLen + 2 = 130` encoded bytes, but the key provides 128. Rejecting
  this key/parameter combination with `ILLEGAL_VALUE` is correct.
- **Viable resolution:** None needed in firmware. Keep SHA-512 PSS enabled and
  retain the key-size validation. Do not modify the upstream CAP; its fixture
  cannot exercise this default-salt operation. Use explicit shorter salt or a
  larger RSA key for an affirmative operation test.
- **Validation:** Existing native PC/VPCD asymmetric suite passed 99/99,
  including SHA-512 PSS precomputed sign/verify with explicit zero salt. No
  physical device was available.
- **Evidence:** `javap -c -p` inspection of
  `AlgTest_JavaCard/deploy/client/AlgTestJClient.jar`, class
  `algtestapi.ApiTestCryptoSignature` and `algtestapi.ApiTestCryptoKeys`.

## Reuse of VPCD storage caused intermittent Crypto IOException results (2026-10-29)

- **Symptom:** After multiple Crypto CAP upload/test/cleanup cycles on one PC
  emulator flash image, uninitialized-key Signature and GCM test rows
  intermittently returned `6F00`; VPCD diagnostics reported exception ID 5
  (`IOException`). KeyAgreement DH/DHC secp192r1 rows also failed once with
  `IOException` on the reused image.
- **Cause:** The failing rows were not native algorithm failures: the same
  tests passed after starting the emulator with a fresh zero-length flash image.
  The persistent PC emulator state was contaminated or stale; the exact stored
  record involved was not identified. This does not establish a physical card
  storage defect.
- **Viable workaround:** Start a fresh PC JCVM emulator with a new zero-length
  flash image for each isolated Crypto sweep. Keep `pcsc_scan` active while
  starting VPCD, then require `VpcdPresenceProbe --require-present` before the
  client. Avoid repeating many CAP lifecycles against one emulator image.
- **Validation:** On fresh VPCD, Signature, SignatureMessageRecovery, and Cipher
  uninitialized-key rows returned expected `UNINITIALIZED_KEY`; the GCM row
  returned `NO_SUCH_ALGORITHM` (`UNSUPPORTED_PER_SPEC`), and `AEADCipher()`
  returned normally. DES 64/192 and AES 192/256 cipher known-answer rows all
  passed (five selected rows including DES CBC ISO9797-M2). Clean PC build
  passed after removing temporary diagnostics. No physical device was tested.
- **Artifacts:** `cipher-keylengths.csv`, `cipher-aead-errors.csv`,
  `cipher-aead-errors-repeat.csv`, `signature-aead-persistent-trace.csv`, and
  `signature-aead-persistent-trace.log` in
  `artifacts/results/deploy/crypto-current-vpcd-20261029/`.

## Focused API Core rows on VPCD (2026-10-29)

- **Observation:** The unchanged API 3.0.5 Core CAP contains 291 calls; an
  unfiltered run did not finish before the 300-second limit. A fresh-image
  focused rerun completed in under a minute when all but `Applet.deselect()`
  were filtered. The selected row returned `OK` (`effect=as expected`,
  diagnostic `returned=1`); this does not reproduce the physical 2026-10-28
  `WRONG_RESULT` (`-15`) and does not replace physical validation.
- **Viable filter:** `--exclude '^(?!.*deselect).*$'` leaves method descriptors
  containing `deselect` and marks other calls `SKIPPED_BY_USER`. Avoid matching
  an assumed exact delimiter spacing: a stricter negative lookahead filtered
  the target too and silently left all 291 rows skipped.
- **Artifacts:** `artifacts/results/deploy/core-deselect-vpcd-20261029/`.

## JCAlgTest RSA-512 support probe unexpectedly slow on VPCD (2026-10-29)

- **Symptom:** The official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` run on
  `asymm_crypto` reported 133.756 seconds for the `ALG_RSA LENGTH_RSA_512`
  key-pair-generation APDU. The run was interrupted before the next key size;
  its CSV is partial. The following client-side `IllegalArgumentException`
  (`apdu must be at least 2 bytes long`) resulted from that deliberate VPCD
  shutdown while the next APDU was in flight and is not a VM response.
- **Counter-evidence:** The independent native VPCD suite ran the same public
  Java Card `KeyPair.genKeyPair()` path and passed all 99/99 checks. Its
  measured RSA/RSA-CRT generation APDU times were 1.068/1.149 seconds at 512,
  1.334/3.469 seconds at 1024, and 47.488/23.334 seconds at 3072 bits.
  Therefore RSA-512 generation itself is not established as the source of the
  133.756-second result. The discrepancy may lie in the JCAlgTest workload or
  execution path; do not change RSA based only on this observation.
- **Next diagnostic:** Compare the precise JCAlgTest request and APDU timing
  against the native probe on the same fresh emulator image/profile. Capture
  the official result without interrupting an in-flight request, then run the
  exact APDU against the same applet with timing. Do not use the partial CSV as
  a profile support comparison.
- **Artifacts:** Partial official attempt in
  `artifacts/results/jcalgtest-vpcd-asymm-current-pcsc-20261029/`; complete
  native probe timing in
  `artifacts/logs/asymm-crypto-keypair-vpcd-timing-20261029.log`.

## PC VPCD does not exercise the Pico CCID GET DATA dispatcher (2026-10-29)

- **Symptom:** Running the SmartcardIO `GPGetDataApduProbe` against a plain
  `choupi --vpcd` PC emulator returned `6999` for `80 CA 00 66`. The emulator
  VPCD endpoint implements CAP/runtime APDUs but does not pass this request
  through `src/ccid_device.cpp`, where card-manager GET DATA is dispatched.
- **Viable test split:** Use `./tools/test-gp-get-data` for the shared
  CPLC/Card Recognition TLV encoder and response status. Use
  `tools/GPGetDataApduProbe.java` only with actual CCID firmware for open and
  SCP03-authenticated GET DATA checks. The separate PC VPCD GPPro install test
  remains valid for CAP LOAD/INSTALL; it does not cover GET DATA.
- **Validation:** The existing VPCD GPPro install flow passed after the
  unsuccessful GET DATA attempt was removed from it. No physical CCID reader
  was tested in this step.

## Pico reader disappears or is omitted from PC/SC after flash/reconnect (2026-10-29)

- **Symptom:** After a Pico passed an APDU probe, its PC/SC reader disappeared
  before a second run. A second Pico flashed successfully and passed finite
  serial startup, but the immediate `pcsc_scan -r` listed only VPCD readers.
- **Viable recovery:** Keep testing on another identified physical Pico rather
  than waiting for the first one. Refresh USB access with the permitted
  `PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader`, then run
  `pcsc_scan -r` again and target the exact reader name. The helper restored
  both physical readers; the second board then passed open and
  SCP03-authenticated CPLC queries. Do not infer a firmware APDU failure from
  a reader omitted by PC/SC.
- **Artifacts:** `artifacts/logs/cycle-choupi-20261029T143004Z.log`,
  `artifacts/logs/cycle-choupi-20261029T143255Z.log`, and
  `artifacts/logs/gp-cplc-physical-D92E743D5F640C0F-20261029.log`.

## Physical Core 6F00 results differ between identical-image Pico boards (2026-10-29)

- **Observation:** The current ARM firmware image was flashed to 2BED and D92
  and each passed the finite `choupi_runtime` monitor. The unchanged API 3.0.5
  Core CAP was run with `--no-scenarios` and exact-reader pinning. D92 completed
  291 calls with 272 `OK`, 11 `UNSUPPORTED_PER_SPEC`, and eight expected
  unavailable Multi-scenario rows; no `SW_ERROR`. 2BED completed the same 291
  calls twice with 198 `OK`, 74 `SW_ERROR` (`6F00`), 11
  `UNSUPPORTED_PER_SPEC`, and the same eight scenario rows. CAP cleanup passed
  on each run; GPPro could still list the ISD and persistent packages.
- **Scope:** 2BED's errors cluster in JCSystem (6), Util (19), AID (14), APDU
  (1), OwnerPIN (13), PIN (12), OwnerPINx (6), and OwnerPINxWithPredecrement
  (3). `Applet.deselect()` itself returned the expected `-19` on both boards.
  2BED had the JCAlgTest package/applet registered; D92 had that package plus
  an `F01234567890` package. The differing registry contents are a variable,
  not an established cause. No firmware change is justified yet.
- **Next diagnostic:** Establish equivalent clean persistent applet/package
  state, then rerun the exact Core command and capture per-APDU VM fault/exception
  diagnostics on 2BED. Compare dynamic package IDs and imported API dispatch
  mappings with D92. Do not classify this as transport loss: the test completed
  and received `6F00` responses.
- **Artifacts:**
  `artifacts/results/deploy/core-physical-retest-20261029/core-no-scenarios.csv`,
  `2BED-core-no-scenarios-retry.csv`, `D92-core-no-scenarios.csv`, and
  `pico1.csv` (the first parallel attempt stopped at the optional Ext CAP
  install refusal, then rolled it back).

### Follow-up: persistent applet context survived GP DELETE

- **Diagnostic:** Reproducing the failing 2BED pattern on D92 with serial
  capture showed 74 `IOException` failures at `arraylength`. Every failed read
  was a persistent array body under the Core applet's instance root; the Pico
  filesystem read returned its missing-record status. A heap-cache probe did
  not find direct cache lookups for these child tags.
- **Cause:** GP DELETE cleared the applet's persistent records and deselected
  it, but retained `selected_applet_context` and its heap. A subsequent
  reinstall with the same package/AID could reuse the already-restored object
  graph whose fields referenced the deleted array bodies.
- **Viable fix:** After a successful GP DELETE, call
  `choupi_vm_forget_selected_applet()` to clear transient lifecycle state and
  destroy the selected context/heap. Keep ordinary deselect and reset behavior
  unchanged so an installed applet still preserves its context across normal
  reselection/reset handling.
- **Validation:** `./tools/build arm` and `./tools/build riscv` passed. The
  fixed image (`6a3915f6611dca76a728b6085fda6af6671a16ca1e59ba1fef674e9ed3a4b17a`)
  was flashed to 2BED and passed finite startup monitoring. Two consecutive
  unchanged API 3.0.5 Core runs each completed 291 calls with 272 `OK`, 11
  `UNSUPPORTED_PER_SPEC`, eight expected unavailable scenarios, and zero
  `SW_ERROR`; GP DELETE/cleanup passed both times. The overlay regression suite
  passed with `TEST:SUMMARY fail=0`. D92's reflash recovery timed out; no D92
  result is attributed to the fix.
- **Artifacts:** `artifacts/results/deploy/core-physical-retest-20261029/`
  (`2BED-candidate-fix-pass1.csv`, `2BED-candidate-fix-pass2.csv`) and
  `artifacts/logs/cycle-choupi-20261029T154706Z.log`.

## Default-profile build failures in signature digest support (2026-10-29)

- **Symptom/cause:** The `current` profile's ARM build failed compiling
  `Signature.setInitialDigest()` when no RSA/ECDSA signature algorithms were
  selected, because its validation called helper functions compiled only for
  those optional providers. After guarding those calls, ARM linked with
  undefined `mbedtls_sha256_free`: the firmware CMake source condition omitted
  `sha256.c` when SHA-256 was enabled by itself.
- **Viable fix:** Compile the algorithm check only when its family is selected,
  and include SHA-256's Mbed TLS source in the firmware whenever
  `JC_SUPPORT_SHA256` is enabled. An unused local must be marked
  `[[maybe_unused]]` for the profile with both signature families disabled.
- **Validation:** `./tools/build arm` and `./tools/build riscv` passed on the
  `current` profile after both changes. The RISC-V wrapper builds its
  documented VM-core smoke image only.

## Historical issue index

Each link opens the byte-for-byte original at the recorded line. Titles retain
original terminology/status; check current summary and later fixes before reuse.

- [Physical Pico 2 USB interface disappeared after successful flash (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L1)
- [PC/SC reader suffix changes when a second Pico is present — 2026-10-27](archive/2026-10-29-pre-compaction/QKB.md#L17)
- [Asymmetric physical probe vector assertion stale after suite expansion — 2026-10-27](archive/2026-10-29-pre-compaction/QKB.md#L35)
- [Guest VM loses all Pico USB devices after D92 flash (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L49)
- [Project issue knowledge base](archive/2026-10-29-pre-compaction/QKB.md#L62)
- [RSA NOPAD reverse key/mode pair rejected as `ILLEGAL_VALUE` (2026-10-29)](archive/2026-10-29-pre-compaction/QKB.md#L64)
- [Intermittent VPCD ECDSA SHA-224/512 `6F00` during combined API filtering (2026-10-29)](archive/2026-10-29-pre-compaction/QKB.md#L85)
- [JCAlgTest 1.8.2 KeyPair RSA-3072 rows send a 4096-bit length (2026-10-28)](archive/2026-10-29-pre-compaction/QKB.md#L104)
- [Long RSA-3072 KeyPair generation exceeded default VPCD deadline (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L130)
- [VPCD command-case framing for no-input APDUs (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L145)
- [Pico USB devices absent from the guest VM despite being plugged in (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L160)
- [NativeKey ROM class identity mismatch — 2026-10-26](archive/2026-10-29-pre-compaction/QKB.md#L175)
- [jcarray test target lacked generated headers and collector stubs — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L192)
- [PC/SC stopped responding after bounded GPPro GET STATUS timeout — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L225)
- [Pico runtime USB devices absent from PC/SC after clean firmware cycle — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L247)
- [Firmware upload preserves installed CAP state; first flashed BOOTSEL board lost runtime enumeration — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L269)
- [GPPro reader state after Pico runtime re-enumeration — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L288)
- [Physical GPPro multi-package install: use `--install-only` for later applets — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L306)
- [Pico D92 PC/SC reader can enumerate while APDUs stall — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L319)
- [Host VPCD DELETE prefix erase requires privileged filesystem syscall — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L333)
- [Raw VPCD multi-package APDU setup and dynamic SELECT routing — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L350)
- [Concurrent ARM/RISC-V builds collided in the shared ROM-mask tree — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L368)
- [ROM class token differs from runtime class index — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L387)
- [Pico storage durability depends on VM transaction state — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L405)
- [JC305 CardException was reported as ISOException through RMI — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L422)
- [CryptoException reason was not preserved for Java RMI throws — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L449)
- [Fresh host VPCD card-manager state looked preinstalled — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L469)
- [Clearing the first persistent array field broke other aliases — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L492)
- [Security transient-key follow-up rows are unsupported-feature fallout — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L517)
- [SDK JCRMI client SELECT with Le=7F — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L540)
- [RMI exceptions escaped nested invocation boundary — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L563)
- [JCSystem VPCD regression findings — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L578)
- [Java Card RMI array signature and temporary allocation — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L589)
- [Duplicate deploy CAP after interrupted or reused area — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L606)
- [Persistent primitive GETFIELD skipped restored cache — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L623)
- [Chained API areas in parallel workers may block during SmartcardIO — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L646)
- [Current three-board PC/SC access blocked by stale installed helper — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L688)
- [Multi-device BOOTSEL entry and serial-targeted flash — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L705)
- [Reported BOOTSEL boards enumerate as application-mode RP2350s — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L731)
- [PC/SC ACL helper rejects multiple CHOUPI readers — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L750)
- [Host VPCD package lookup must include ROM-mask packages — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L769)
- [Dynamic applet virtual method token translated twice — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L793)
- [Preserve CAP2 image while placing two JFS2 slots — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L810)
- [BOOTSEL picotool transfer disconnect after 100% — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L832)
- [CAP3 package slots overlap RP2350 filesystem snapshots — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L855)
- [Imported interface resolution can contaminate later applet interface lookups — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L879)
- [PC VPCD storage shim is single-package — 2026-10-21 (updated 2026-10-25)](archive/2026-10-29-pre-compaction/QKB.md#L900)
- [VPCD SmartcardIO reader absent; bound PC/SC probes — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L919)
- [Util same-array overlapping copy — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L937)
- [JC305 OwnerPIN and PIN dispatch used embedded tokens — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L953)
- [ROM-to-dynamic virtual override needs inverse CAP token mapping — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L967)
- [Deploy Core IO_ERROR result was a client timeout, not a bridge defect — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L988)
- [CAP INSTALL and SELECT must resolve the package by the request/instance AID — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L1018)
- [Deploy API runner can report partial CAP install as success — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L1037)
- [GP DELETE can return 6581 after API applets write persistent state — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L1052)
- [GPPro package deletion requires removing its applet instance first — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1064)
- [Runtime-exception root failure test pulls full VM dependencies — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1078)
- [Persistent static reference could commit before GC root registration — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1092)
- [Exception object inserted before its GC root could be allocated — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1108)
- [Allocation-failure test optimized away an unused JCVMArray copy — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1123)
- [JCSystem.isAppletActive must include registered install/select callbacks — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1135)
- [JCSystem.getAID exposed an unregistered instance during install — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1154)
- [Pico reader disappears from PC/SC during second applet APDU — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1172)
- [CAP installation metadata can outlive a failed filesystem commit — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1200)
- [CAP Method component size was decoded in host byte order — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1232)
- [USB-visible Pico omitted from PC/SC reader list — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1255)
- [Pico PC/SC lifecycle verified after CDC/BOOTSEL flash cycle — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1271)
- [ROMmask source edits skipped by build cache — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L1291)
- [Native CryptoException dispatch and RSA NoPad range check — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L1305)
- [Post-SELECT PC/SC timeout repeated after 2026-10-19 reflash](archive/2026-10-29-pre-compaction/QKB.md#L1330)
- [Post-SELECT PC/SC timeout repeated after successful reflash — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1344)
- [PC/SC stops sending APDUs after SELECT — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1360)
- [Direct Pico-to-VPCD bridge requires exclusive USB interface access — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1380)
- [PC/SC Virtual PCD was listed before its endpoint was ready — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L1394)
- [JCSystem shareable lookup requires a context-switch bridge — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1413)
- [JC305 SensitiveArrays invocation used the wrong embedded method token — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1431)
- [SensitiveArrays is intentionally unsupported when capability is false — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1449)
- [JCSystem.makeGlobalArray implementation — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1460)
- [JCSystem.isAppletActive always threw NotYetImplemented — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1499)
- [JCSystem object-deletion request silently succeeded while unsupported — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1515)
- [Restricted-context USB access — complete fail-fast coverage — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1532)
- [USB passthrough and Pico visibility](archive/2026-10-29-pre-compaction/QKB.md#L1565)
- [PC/SC permission after USB re-enumeration](archive/2026-10-29-pre-compaction/QKB.md#L1576)
- [`pcsc_scan` runaway CPU / VM saturation](archive/2026-10-29-pre-compaction/QKB.md#L1592)
- [Physical JCAlgTest static SHA-256 repeat failure](archive/2026-10-29-pre-compaction/QKB.md#L1604)
- [Host/sandbox PC/SC context split — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1622)
- [Fail fast before hardware commands in the restricted context — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1647)
- [JCSystem available-memory reporting — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1666)
- [Physical SHA-256 static repeat failure — 2026-10-17 update](archive/2026-10-29-pre-compaction/QKB.md#L1693)
- [Intermittent reader recovery after MCU reboot — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1730)
- [Static SHA repeat failure mechanism — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1751)
- [Resolution: static SHA repeat heap exhaustion — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1803)
- [Interpret JCAlgTest follow-on `6F00` after unsupported prepare — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1847)
- [Correction: first residual static `6F00` is supported `setSeed` — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1870)
- [Fix supported RandomData.setSeed static failure — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1895)
- [Isolate first DES static prepare failure — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1934)
- [Fix JC305 Cipher virtual-token mismatch in static workload — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1964)
- [JSR saved-PC storage and remaining static-suite OOM — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2005)
- [Physical ALG_PERFORMANCE_STATIC heap retention — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2046)
- [Supported sweep heap exhaustion — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2103)
- [InitializedMessageDigest.OneShot unsupported factory — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2139)
- [Runtime CryptoException roots and physical follow-up — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2179)
- [RandomData.OneShot unsupported result and support-matrix audit — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2230)
- [OwnerPINx dispatch and inherited-field resolution — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2272)
- [JCAlgTest VPCD startup delay before support queries — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2307)
- [Host-access invocation latch — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2334)
- [Heap reclamation design constraints — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2350)
- [Heap-root inventory after static-cache implementation — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2386)
- [Canonical roots for serialized static references — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2422)
- [Root and typed-edge visitor API — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2443)
- [Mark-only typed heap traversal — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2466)
- [Runtime CryptoException handles are heap roots — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2491)
- [Preserve top-level reference return values as typed roots — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2511)
- [Opt-in post-APDU heap trace — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2530)
- [Persistent reference-array child identity — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2632)
- [Persistent instance fields sharing an array — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2652)
- [Array `checkcast` type-code mapping — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2670)
- [VM-generated Java runtime exception dispatch — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2706)
- [Root java.lang.Object allocation terminated the emulator — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2778)
- [VM-raised APDUException bypassed Java catch handlers — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2801)
- [Invalid transient-array clear event raised the wrong exception — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2822)
- [Heap reclamation must preserve Java Card handles — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2851)
- [JC305 DESKey interface token must be remapped — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2880)
- [Bound serial configuration when Pico USB is re-enumerating — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2904)
- [Physical JCAlgTest static run — CryptoException catch failure — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2921)
- [Correction — ROM and imported exception class views — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2949)
- [PC/SC reader disappears during physical static run — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2977)
- [Interpreter frames were left active after APDU exceptions — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L3002)
- [Collector scratch vectors exhausted RP2350 heap — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L3030)
- [PC/SC daemon hangs while Pico USB device remains present — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L3073)
- [D92 PC/SC APDU path stalled after second-package load attempt — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L3095)
- [Host VPCD timed out while updating a 20 KB persistent byte array — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L3130)
- [VM lost Pico USB passthrough during final transaction-capacity flash — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L3150)
- [Pico power-up check sees USB device but no PC/SC reader — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3168)
- [JC305 AID virtual-method token mismatch — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3223)
- [Unknown AID SELECT dispatched to active applet — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3244)
- [Pico PC/SC lists the card but Java APDU transmit stalls — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3260)
- [Physical power-up check confirms registry but APDU transmit is not delivered — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3284)
- [Transient array allocation failure terminated the VM — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3329)
- [Persistent array restoration allocation used `noexcept` constructor — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3348)
- [Persistent primitive reads leaked temporary buffers — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3363)
- [Persistent JC_Instance construction could leak its temporary tag buffer — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3379)
- [JCVMArray copy assignment lost its destination on allocation failure — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3393)
- [Physical APDU reaches applet, then stalls during post-APDU GC — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3408)
- [RP2350 BOOTSEL reinsertion detection — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3447)
- [Runtime CAP loader accepts only the configured JCAlgTest package AID — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3463)
- [Physical deploy Core lifecycle and cleanup results — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3509)
- [DELETE cleanup rejects CAP class indexes above 255 — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3545)
- [16-bit applet class indexes need distinct persistence tags — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3566)
- [`instanceof` class resolution can fail on an invalid alternate interface token — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3594)
- [JC305 Security Key interface method order mismatch — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3620)
- [Official API client reader index includes empty virtual terminals — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3654)
- [JavaCardX CAP static slots exceed the legacy tag width — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3674)
- [BOOTSEL devices are not PC/SC readers — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3694)
- [Failed scenario INSTALL can leave a partially loaded CAP — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3711)
- [Core deploy repeat reports unexpected `Applet.deselect()` value — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3732)
- [Third Pico is USB-visible but intermittently absent from PC/SC — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3759)
- [Stale dynamic package bitmap can hide a missing CAP — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3779)
- [CAP metadata update could destroy the only valid registry header — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3807)
- [CAP applet registration and filesystem install commit need reconciliation — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3831)
- [VPCD test process must exit cleanly to save its image — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3872)
- [VPCD applet harness summary count was hard-coded — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3887)
- [Parallel deploy startup can race Java reader enumeration — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3901)
- [Three-reader PC/SC access restored — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3924)
- [API runner must reject partial output after abnormal client exit — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3935)
- [Physical parallel deploy run — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3949)
- [API runner `--reader-name` did not set Java reader index — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3966)
- [Canonical JC305 extension package AIDs did not match CHOUPI ROM AIDs — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3980)
- [Three-reader deploy batch and install rollback — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L4003)
- [Host VPCD harness loopback bind denied in restricted execution — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4024)
- [JC305 MessageDigest constructor token differs from embedded ROM export — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4030)
- [VPCD deploy applet selection uses area-specific AIDs — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4050)
- [RMI export-capacity case includes existing applet references — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4062)
- [JC305 RMI names come from CAP remote metadata, not Debug — corrected 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4074)
- [Fresh-process VPCD install recovery needs the assigned package ID — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4102)
- [Restored applets lose transient RMI exports — viable workaround 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4115)
- [RMI interface-form SELECT P2 rejected before applet dispatch — fixed 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4129)
- [VPCD local socket denied in restricted execution — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4139)
- [JCDK 3.0.5 direct CAP output omits int RMI signatures — resolved 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4151)
- [RMI exported object retained through collection — validated 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4196)
- [VPCD process-restart persistence stress can collect after restore — validated 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4211)
- [GPPro card-manager lifecycle on PC VPCD — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4224)
- [Host VPCD emulator interrupted with Ctrl-C — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4242)
- [Host VPCD persistence test must resume the allocated package ID — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4255)
- [Empty transaction abort invalidated unrelated persistent caches — resolved 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4270)
- [Persistent abort cache stayed stale after flash rollback — 2026-10-22 (resolved)](archive/2026-10-29-pre-compaction/QKB.md#L4301)
- [Nested persistent array graph fixture boundaries — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4331)
- [Host VPCD idle timeout and external-install persistence — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4347)
- [JC305 interface-token remapping corrupted embedded ROM API calls — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4374)
- [Current-source Crypto API deploy outcomes and constructor mapping — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4400)
- [Host USB helper rejected by restricted execution context — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4427)
- [JC305 Signature virtual dispatch used the wrong embedded method — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4438)
- [JC305 CBC Cipher update must buffer partial blocks — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4446)
- [Physical JC305 Security follow-up rows depend on unsupported transient AES keys — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4481)
- [Transitive custom exception RMI mapping — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4497)
- [Preserve SystemException reasons in Java Card RMI — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4512)
- [Service Dispatcher returns SW=6400 only on one physical token — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4532)
- [Parallel deploy runner empty-area fallback — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4541)
- [Same-image reflash leaves the token 3 Service failures — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4547)
- [Unrestricted API 3.0.5 suite exposes matching 6F00 failures on two tokens — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4555)
- [JCAlgTest 1.8.2 cannot pass initialization in capability profiles — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4563)
- [Minimal JCAlgTest 6F00 caused by stale JC305 API class-token mappings — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4573)
- [JCAlgTest base/current support profiles on Pico 2 — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4582)
- [Selectable capability profile physical acceptance — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4620)
- [Virtual PCD VPCD endpoint refused emulator connection — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4657)
- [Symmetric crypto profile has no physical test target visible — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4675)
- [Symmetric crypto emulator vector coverage — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4690)
- [ISO9797-M1 cipher decryption retains zero padding — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4707)
- [JC305 RSA key calls use shifted interface method tokens — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4731)
- [Pico USB-visible but absent from PC/SC after asymmetric-profile flash — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4755)
- [RSA key source leaked into profiles that disable RSA — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4774)
- [Stale persistent child tags block package deletion and reinstall — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4789)
- [BOOTSEL ROM drops Pico runtime serial — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4813)
- [JCSystem persistent array body hidden by a resurrected overlay tombstone — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4828)
- [Firmware reflash does not make a used card empty — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4853)
- [Multi-board BOOTSEL selection and clean-card verification — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4875)
- [Pico leaves BOOTSEL but runtime CDC is not passed through — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4907)
- [`/deploy` runner resolves output paths from its own directory — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4931)
- [JC305 `checkcast` class/interface token collision — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4947)
- [VPCD registry restore failure must stop card service — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4973)
- [Pico reader ACL refresh and GPPro timeout failover — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4994)
- [Capability-profile physical Core/Security/Crypto follow-up — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5016)
- [Physical JCSystem BUFFER_FULL probe can leave the runtime wedged — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5043)
- [JCSystem VPCD runner must clear its registry sidecar for a fresh image — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5065)
- [JCSystem applet SELECT can fail on a dangling persistent array alias — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5079)
- [Three LEDs active while PC/SC exposes only one Pico — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5109)
- [Physical GET STATUS stalls on exact-packet CCID response — fixed 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5149)
- [Stale VPCD flash image prevents clean RMI client startup — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5194)
- [Reused PC VPCD flash copy retained stale registry sidecar — fixed 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5210)
- [D92 JCAlgTest support sweep loses PC/SC transaction on MD5 RSA OAEP — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5226)
- [Asymmetric signature profile currently adds a Java ROM-mask provider — unresolved architecture violation (2026-10-25)](archive/2026-10-29-pre-compaction/QKB.md#L5253)
- [Native Java Card API dispatch can allocate API objects directly — design finding (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5274)
- [MessageDigest native buffer must be transient — fixed (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5300)
- [Native method override manifests need full Java Card descriptors — fixed (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5309)
- [Physical SHA-256 validation after native MessageDigest dispatch (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5315)
- [Symmetric Signature MAC path migrated to native API dispatch (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5323)
- [PC emulator can remain stale after target builds (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5349)
- [CCID response buffer must fit full APDU response (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5364)
- [JCAlgTest static workloads exceeded the applet-call time guard (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5385)
- [RSA-PSS known-vector harness must distinguish explicit salt parameters (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5393)
- [JC305 ECPrivateKey interface methods need token remapping (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5400)
- [JC305 ECPublicKey interface methods also need token remapping (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5422)
- [JC305 KeyPair getter token order and qualified constructor JNI name (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5440)
- [EC subgroup order can exceed the field bit width (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5459)
- [Native RSA-CRT KeyPair keys were not usable by private Cipher/Signature (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5475)
- [CAP component framing accepted truncated or oversized lengths (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5518)
- [CAP component payload tables need internal count bounds (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5536)
- [Compact Descriptor records require seven-byte field entries (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5554)
- [CAP 2.1 Class records are variable-length and ordered by kind (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5575)
- [Descriptor `this_class_ref` must point to a Class record start (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5596)
- [CAP Descriptor type nibble grammar and padding (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5614)
- [CAP Descriptor type-offset base compatibility (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5636)
- [VPCD emulator can connect while PC/SC still reports no card (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5658)
- [ROM-mask CAP payloads are prelinked and fail standard Descriptor validation (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5675)
- [VPCD PC/SC service startup race (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5682)
- [VPCD install-recovery tests must resume from persisted registry (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5689)
- [RSA-CRT result verification needs a matching public exponent (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5696)

### Enumerate registered roots for all installed applet instances — 2026-10-30

- The firmware/PC inventory path now walks the package instance registry,
  resolves each instance's stable owner ID, and visits all registered root tags
  in that owner namespace. Missing package metadata, instance records, owner
  mappings, or root walks fail the inventory closed. This replaces the stale
  active-applet-only limitation; applet instances share the active heap used
  by the inventory tracer.
- PC VPCD multipackage coverage passed with three installed applet roots both
  before and after emulator restart (`applet_roots=3`, `trace_complete=1`,
  zero invalid references/edge failures). Logs:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261030T040017Z.log`
  (SHA-256 `9f397c883b02706990a84e532fc1ffe02a706ab89eba1debb1b7d4412e8016ab`)
  and `artifacts/logs/host-vpcd-multipackage-restart-20261030T040017Z.log`
  (SHA-256 `177355201566029f929b9fbe945c325799573a10a57ba66d0aeef3d1d5416995`).
  Suite summary: `TEST:SUMMARY pass=25 fail=0`. Physical multi-instance
  installation/root restore has not been tested. Other live contexts and
  native-held roots remain outside this enumeration; persistent record deletion
  remains disabled.

### Physical SHA-256 doFinal lost its persistent digest instance at APDU GC — fixed 2026-10-30

- On Pico 2 `2BED26ED20556535`, the unchanged installed JCAlgTest 1.8.2 applet
  previously selected normally; GETVERSION and JCSystem-info returned `9000`,
  SHA-256 setup returned `AA9000`, but doFinal returned `FF01`. Serial tracing
  showed the factory returned digest handle `0036`, the next APDU used receiver
  `0036`, then the collector reclaimed that persistent instance at APDU end.
- Cause: strict persistent-field tracing reads the serialized flash snapshot.
  A persistent object's loaded in-memory field cache can contain a newer
  reference than that snapshot; the collector was ignoring this live edge.
  The `FF01` was therefore consistent with a stale/dangling digest handle on
  the following APDU. This was not a SHA-256 arithmetic failure.
- `Heap::visitChildReferences` now first keeps strict serialized validation,
  then visits cached child handles only when the persistent field cache is
  loaded and current for the active rollback generation. A focused
  `persistent_transaction` regression proves the cached child survives and
  unrelated garbage is reclaimed. VPCD persistence passes install/commit 3/3
  and restore/GC 38/38 after process restart. On the normal symm_crypto ARM
  image, physical board 2BED was flash-verified, its finite monitor passed 18
  repeated runtime summaries, and the unchanged JCAlgTest SHA-256 prepare and
  doFinal APDUs both returned `AA9000` in three consecutive probes.
- Evidence: ARM image SHA-256
  `18a7a230adf2e1719c26e1f79ccd48f60e93ae108c4b989ade4a2a89d943604e`;
  cycle log `artifacts/logs/cycle-choupi-20261030T105252Z.log`; physical APDU
  log `artifacts/logs/sha256-dofinal-loaded-cache-2BED-20261030.log`; VPCD
  logs `artifacts/logs/vpcd-persistence-commit-20261030T105520Z.log` and
  `artifacts/logs/vpcd-persistence-restore-20261030T105520Z.log`. A recurring
  `WATCHDOG_RECOVERY ... cause=other` startup marker remains separate; all
  finite runtime checks passed.

### Installed-instance root enumeration must match the registry — fixed 2026-10-30

- A complete cursor scan could previously return zero registered roots for an
  installed applet instance and still mark the walk complete. The collector
  now validates applet and instance AID lengths, resolves the instance owner,
  and requires exactly one root whose owner, package ID, and class index match
  the package-instance registry entry. Missing, duplicate, or mismatched roots
  fail the root walk, so `collectUnreachableFrom` skips reclaiming heap slots.
- The multipackage VPCD suite passed 25/25 before and after process restart;
  all three installed roots were discovered with a complete trace and no invalid
  references or edge failures. ARM, RISC-V, and PC builds passed. On physical
  2BED `2BED26ED20556535`, a full ARM image cycle passed finite startup checks;
  after refreshing the PC/SC ACL helper, JCAlgTest SELECT, GETVERSION, and
  JCSystem-info each returned `9000`. No persistent filesystem records were
  deleted. Negative registry/root-corruption injection is still untested.

### JCArray host collector harness needs the OS FFI declarations and strict-reader stubs — fixed 2026-10-30

- `heap.cpp` now includes the Rust OS `ffi.h` and references strict persistent
  field/tag helpers. The standalone JCArray harness was picking the older
  `tools/tests/short_apdu/ffi.h` first and its filesystem stub lacked these
  declarations/definitions. Put `choupi/os/src` first in the target include path
  and extend its filesystem fixture with the new FFI and strict-reader stubs.
- `./tools/test-jcarray` now passes the existing heap lifecycle/GC suite plus
  two fail-closed checks: a root enumerator that returns false after yielding a
  partial set, and a reachable reference array with an invalid child handle.
  Both mark summaries incomplete and reclaim zero heap handles. This does not
  exercise malformed serialized fields/aliases or reboot recovery.

### Strict persistent child-read failure must stop heap reclamation — covered 2026-10-30

- The JCArray host collector fixture can mark an array persistent and inject a
  `SecurityException` from `getPersistentField_ReferenceStrict` while scanning
  its child slots. `./tools/test-jcarray` confirms the trace reports one
  `edge_load_failure`, returns incomplete, and reclaims zero handles, including
  an unrelated unrooted array. This verifies the collector's safety gate for a
  persistent read exception, but does not fuzz malformed record bytes or alias
  cycles and does not validate post-reboot recovery.

### Persistent array-alias headers reject truncated encodings — covered 2026-10-30

- The FlashMemory transaction fixture now exercises the actual
  `getPersistentField_Type` decoder against stored alias records. It rejects a
  one-byte array-alias header and a declared target-tag length that does not
  match the record payload, and rejects a correctly framed alias whose target
  is a scalar record. The full `./tools/test-persistent-transaction` suite
  passes. The fixture now implements `fs_read_inplace` and minimal unused
  Context hooks required when retaining the actual decoder section in the host
  link.
- These checks cover malformed array-alias framing only. They do not validate
  malformed object-alias chains through `getPersistentField_ReferenceStrict`,
  collector behavior on an alias-corrupt record, or recovery after restart.

### Physical static workload disconnect is not yet explained — updated 2026-10-30

- On normal `symm_crypto` firmware, two official JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` runs on 8E ended during the measured DES CBC PKCS5
  `Cipher_init()` sequence with `SCARD_E_NOT_TRANSACTED`, then
  `SCARD_E_READER_UNAVAILABLE`. The second run's reader later disappeared from
  PC/SC even though USB enumeration still showed devices; the ACL helper
  restored a reader earlier in the investigation. Preserve both reports at
  `artifacts/results/jcalgtest/performance-static-post-gc-fix-8E-20261030/`.
- A GC-diagnostic image on separate physical 2BED passed that same DES CBC
  PKCS5 `Cipher_init()` case, then DES ECB and many 3DES cases. It was still
  responding normally when the 900-second outer timeout ended during 3DES CBC
  NOPAD `Cipher_init()`; this is partial coverage only. Debug serial output
  reports passing runtime summaries. GC snapshots stabilized at 148 live / 198
  allocated arrays and 36 live / 39 allocated instances, with zero invalid
  refs, edge failures, or root failures. This makes deterministic GC/reference
  corruption at the earlier failure location less likely, but does not
  distinguish a timing-sensitive firmware issue from USB/PCSC transport loss.
- Diagnostic logs and partial report:
  `artifacts/logs/heap-gc-static-debug-2BED-20261030.log`,
  `artifacts/logs/heap-gc-static-debug-2BED-continued-20261030.log`, and
  `artifacts/results/jcalgtest/performance-static-gc-debug-2BED-20261030/`.
  Standard firmware was restored and passed both build wrappers, flash
  verification, an 18-second monitor, and PC/SC discovery (2BED).
- Next probe: rerun the exact normal image on one identified board with
  concurrent serial capture and host USB event capture, or isolate the failing
  method using client filters if available. Do not mark the official static
  suite as passing until its complete report succeeds.
- Follow-up on normal image, separate identified Pico 2 `2BED26ED20556535`:
  the official run continued for the full 3,600-second wrapper limit and
  completed DES/AES Cipher coverage (126 DES and 108 AES method entries), then
  timed out during RSA/signature cases. It produced 4,214 CSV lines / 937
  method entries and contains no `SCARD_E_*`. Concurrent 2.5 MB serial capture
  has 3,575 passing summaries and no `TEST:FAIL`, `FAULT`, or
  `WATCHDOG_RECOVERY`; concurrent udev capture has no USB add/remove event.
  PC/SC still listed 2BED after the run. This does not prove the entire suite
  passes but shows the DES CBC PKCS5 initialization failure did not reproduce
  on 2BED after about an hour of normal-image stress. The original disconnect
  remains board/state-specific or intermittent; root cause unproven.
- Separate observation: this run logged 72 `FF01` responses in DES MAC
  `Signature_*` static measurements, which JCAlgTest records as
  `CANT_BE_MEASURED`. These were applet execution errors, not reader loss; do
  not conflate them with the missing-algorithm `NO_SUCH_ALGORITHM` outcomes.
  Investigate only if these MAC signatures are intended as supported in the
  selected profile or the upstream suite expects a clean unsupported result.
- Evidence:
  `artifacts/results/jcalgtest/performance-static-normal-full-2BED-20261030/`
  (client log SHA-256
  `5b054c044d7c5a7c511906bc1498100473886aa9ce3cb6a69f310fef013c2e45`, CSV
  SHA-256 `488708254d7189dc4bdde7b3c31eea04489537a5dfaff044a56bfb8115570e46`);
  serial log SHA-256
  `0cc602228990f9919375449a9162b4aa541e37db44a4ed870322c29c69b18780`; empty
  udev log SHA-256
  `a75697edb29c2c13f19e67b1fcaa15b36e16e39fd08f9ed40f33e7f029ae5b5a`.

### Post-APDU collector has one live interpreter context — audited 2026-10-30

- `Interpretor` currently pushes one `Context` in each constructor; the hardware
  APDU entry point supplies the selected external context. Collection is called
  after interpreter execution, not while Java frames or native C++ call-local
  references are active. Its callback invokes `Context::visitRootReferences`
  and `Heap::visitRootReferences`, which visit remote-object roots, applet and
  APDU lifecycle references, APDU buffer mirrors, sensitive results, stack
  reference returns, runtime exception roots, and cached static references.
- The missing root inventory was therefore per-installed-applet roots, now
  enumerated for every registry instance, rather than several simultaneous
  interpreter contexts. `./tools/test-rmi-registry` passes
  `lifecycle_roots=6`, `sensitive_result_cross_context=1`, and session cleanup.
  Revisit this audit if the interpreter gains concurrent contexts or another
  persistent native owner. No persistent records are deleted by this change.

### Stale external profile package — refreshed 2026-10-30

- The old ignored `build/external-test/pico2-jcvm-windows-test.zip` and loose
  asymmetric expected CSV were stale. Viable refresh: run
  `./tools/package-asymm-symm-windows-test`. The packager verifies the pinned
  upstream CAP hash, runs ARM and RISC-V wrappers for both profiles, copies
  each generated ARM UF2 and expected CSV before changing profile, and writes
  per-file SHA-256s into the archive. It validates ZIP integrity and all
  checksums. Latest ZIP SHA-256:
  `26a1d6e6d34033c44a22915c8a41c644917de85fd04c5d6d3598606889708be3`.
- The generated package contains 46 `symm_crypto` and 145 `asymm_crypto`
  declared positives. Use the packaged expected CSVs; the loose ignored
  `build/external-test/expected-support-asymm_crypto.csv` remains stale.
  Windows has not yet run the included instructions. The README's comparator
  command must include the required second positional jCardSim reference CSV.
- The asymmetric physical comparison is known to be 143/145 because the
  unchanged upstream JCAlgTest CAP labels RSA/RSA-CRT 3072 key-pair probes as
  3072 but sends 4096; independent physical 3072 generation vectors pass.
  Preserve this caveat; do not alter the CAP or expected manifest to hide it.

### Physical API 3.0.5 RSA NOPAD rows pass — 2026-10-29

- The unchanged Crypto CAP completed all 466 calls on Pico 2
  `2BED26ED20556535` with `asymm_crypto`. All four `ALG_RSA_NOPAD` rows
  returned `OK`, including the parameterized illegal-value case; upload and
  cleanup stages both passed. The area totals were 302 `OK`, 163
  `UNSUPPORTED_PER_SPEC`, and one unrelated `EXC_CARDRUNTIME`.
- Evidence CSV:
  `artifacts/results/deploy/crypto-physical-retest-20261029/2BED-asymm-crypto.csv`
  (SHA-256 `28400be72b530b81d264dbad3dbd11f860ba3af6c5756c06dac792ee8e4d05b1`).
- The prior “physical retest pending” notes were stale. This does not make the
  broader deploy matrix clean; track its remaining findings separately.

### Read-only power-up check does not perform a power cycle — clarified 2026-10-30

- `tools/test-pico-powerup` checks GPPro package/applet listing, applet SELECT,
  GETVERSION, and JCSystem-info. It does not reset the card or remove USB power.
  Its completion marker is now
  `PERSISTENCE:PASS registry_applet_and_jcsystem_verified power_cycle=not_verified_by_script`.
  A separate physical reboot/cold-power procedure is required before treating
  a result as power-cycle recovery evidence.
- The available physical reader was confirmed as Pico 2
  `2BED26ED20556535` on USB bus 001 device 116 / root-hub port 2. `lsusb -t`
  exposed only the root hub, `uhubctl` was not installed, and sysfs exposed
  `power/control` autosuspend only; no controllable VBUS port was identified.
  Do not substitute USB authorization detach or PC/SC reset for power loss.
- Latest read-only check passed SELECT, GETVERSION `1.8.2_jc305`, and
  JCSystem-info (`9000`). The finite serial monitor passed seven runtime
  summaries with no `TEST:FAIL` or `FAULT`. Logs:
  `artifacts/logs/pico-powerup-persistence-20261030T145210Z.log` (SHA-256
  `4708926c9e2209409d15a6217f8043a58e473be8cff3fc9a5378fa5b6b7c4840`) and
  `artifacts/logs/persistent-inventory-latest-monitor-2BED-20261030.log` (SHA-256
  `f7a73be8232621a94faa183aabd21559748a7d829c781e0462b0e4465c06561d`).
