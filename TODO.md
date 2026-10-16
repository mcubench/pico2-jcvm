# Project TODO

This tracker follows the acceptance criteria in `plan.md`. “Done” means the
behavior has evidence recorded in `progress.md`; partial results are called
out explicitly.

## Completed

- [x] Build and flash the CHOUPI image for RP2350 ARM through repository wrappers.
- [x] Provide USB CDC + CCID transport and bring up the Pico as a PC/SC reader.
- [x] Implement SCP03 authentication and enough GlobalPlatform card-manager
  behavior for GPPro listing, CAP LOAD, and registry queries.
- [x] Persist the supplied JCAlgTest v1.8.2 CAP blob across firmware reboot.
- [x] Resolve the persisted CAP through CHOUPI package lookup and return the
  applet AID/FCI on SELECT. This confirms registry/transport behavior only; it
  does not confirm that the applet constructor or `process()` has run.
- [x] Keep ARM and RISC-V wrapper builds passing for the current shared source.
- [x] Correct `Util.arrayCompare()` so the first greater source byte returns
  `1` instead of incorrectly returning `-1`; nested API commit `6dceb7e`.
- [x] Limit the default target build to two parallel jobs to avoid saturating
  all VM cores; override with `PICO_BUILD_JOBS` when desired.

## Active: validate the applet process APDUs and client results

Current environment note (2026-10-15): the latest `./tools/cycle choupi`
rebuilt ARM, RISC-V, and CHOUPI successfully, flashed the ARM firmware, and
passed the finite 18-second runtime capture with 18 repeated
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` pairs. The detailed
record is `artifacts/logs/cycle-choupi-20261014T135618Z.log`. The corrected ARM firmware was flashed;
GlobalPlatformPro CAP LOAD and INSTALL returned `9000`, the registry lists
`JCAlgTest1` as `INITIALIZED`, and SELECT by AID returned `9000` with FCI. These
steps validate install and selection, but `Applet.process()` has not yet been
validated on the physical card. The PC VPCD integration now restores the
persisted applet and passes ATR, SELECT, JCSystem info, and the
supported-algorithm APDU in one bounded pass. The prior
`INVOKEVIRTUAL`/`StackUnderflowException` was caused by ROM-mask generation
encoding internal virtual method references with the declaration index rather
than the class virtual-table token. The updated generator resolves the
signature against the public/package method table; its emitted APDU tokens are
`1`, `3`, `5`, and `8` for `getBuffer`, `setOutgoing`, `setOutgoingLength`, and
`sendBytes`. The pass log is
`artifacts/logs/jcalgtest-internal-virtual-table-token-fix-20261015.log`.
This is host-emulator evidence only, not a board result. The ARM wrapper build
passes with the regenerated ROM mask. No RISC-V build was run, per the current
ARM-only scope, and no flash was attempted.
This is host-emulator evidence only, not a board result. The official JCAlgTest
v1.8.2 host JAR has
been downloaded to `build/algtest-client-1.8.2/`; its bundled JC305 CAP has the
same SHA-256 as the supplied CAP. `ALG_SUPPORT_EXTENDED` completed against the
bundled jCardSim and produced
`artifacts/results/jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv` (4,595
lines). Treat this only as a reference: jCardSim itself raises
`NotImplementedException` on some algorithms and the client records those
errors. A Java `javax.smartcardio` probe compiles without extra modules, but
`pcsc_scan -r` still reports no reader after the latest flash; the user reports
that the authorized VM helper ran successfully. No APDU reached the board.
Always cap any PC/SC scan.
`tools/probe-jc-algtest` provides a finite-timeout SELECT/GETVERSION/JCSystem
APDU probe, but no APDU reached the board in this run. The ARM and RISC-V build
wrappers pass; only ARM firmware was flashed and tested. Windows reader
enumeration and applet process/power-cycle persistence remain unverified.

Current host update (2026-10-15): the release PC VPCD path now passes two fresh
JCAlgTest applet runs (`TEST:SUMMARY pass=10 fail=0`). The phase-instrumented
run measured 2.554 s connect, 0.001 s ATR, 0.231 s SELECT, 16.437 s for JCSystem
info, and 4.561 s for the supported-algorithm APDU. No physical token was used.
The official host client remains blocked here because the PC/SC service is
unavailable and the `vsmartcard-vpcd` driver is not installed. The next test
requires those host prerequisites; do not substitute the physical token.

Registration implementation status: the VM captures the receiver passed to
either imported JC305 `Applet.register` method and serializes that instance's
fields under a reserved persistent applet root after a successful install
method return. CAP metadata now also records the registered class index in a
version-2 header while continuing to read version-1 headers. The PC VPCD SELECT
path reconstructs the stored applet and runs its `select()` method; this
returned success in the latest host integration. Its APDU `process()` path is
now entered but stops on an applet field read. Installation
rollback and duplicate/invalid registration handling also remain unverified.

- [x] Parse the INSTALL [for install and make selectable] parameter field and
  pass its exact bytes in a Java byte array with offset zero and the matching
  byte length to `Applet.install(byte[], short, byte)`.
- [x] Verify the installation-parameter byte array and length in a valid VM
  execution, including the supplied two-byte `C9 00` field. The VPCD runner
  accepts `--install-params`; a PC-only VM-entry assertion verifies the Java
  local array reference, offset 0, signed `bLength` 2, array size, and both
  bytes before executing the method. A fresh JCAlgTest run then passed SELECT,
  GETVERSION, JCSystem, support, SHA-256 `doFinal`, and static SHA-256
  performance APDUs (`TEST:SUMMARY pass=11 fail=0`).
- [x] Resolve JC 3.0.5 imported class and method tokens against the embedded
  CHOUPI exports. Validate each mapping against CAP/export metadata; do not
  treat a GP INSTALL status as proof that Java installation completed. The
  corrected mapping was validated by the successful applet INSTALL and
  initialized registry record.
- [x] Use fresh install diagnostics to identify the `6A80` cause: the v1.8.2
  CAP calls JC 3.0.5 `Applet.register()` (external virtual method token 1), but
  the remap changed it to token 2, resolving `register(byte[], short, byte)`.
  The VM reported `StackUnderflowException` (id 1) at `invokevirtual` CP entry
  `0x179`, with one operand word and resolved method offset `0x305`; that method
  header has four arguments. The Applet token mapping now preserves token 1 and
  maps the later changed JC 3.0.5 tokens to the embedded API table.
- [x] Reflash and retry installation with the corrected Applet method mapping.
  ARM firmware flash passed; CAP INSTALL returned `9000`, and GPPro reports
  the applet as initialized. Current container cannot access PC/SC to continue
  process-level APDU testing.
- [x] Track calls to the imported JC305 `Applet.register` virtual methods during
  install and reject a successful VM return if no registration call occurred.
- [x] Store the registered applet class index beside the persistent AID using a
  CRC-protected CAP metadata version, while retaining version-1 CAP readability.
- [x] Implement `Applet.register()` and `register(byte[], short, byte)` so a
  successful Java install creates the persistent, selectable instance.
- [ ] Validate failed-install rollback and status words on hardware, then close
  power-loss atomicity.
  Synchronous rollback now snapshots persistent object/field writes, including
  indexed array and package-list updates, and the RP2350 overlay can erase
  transaction-created records. Failure stages now map to distinct status words
  (`6A80`, `6A82`, `6581`, `6985`, or `6F00`), with a focused host test passing.
  A host test now exercises the RP2350 overlay erase path, including collision
  probes and compressed child tags. Failure-path behavior still needs hardware
  validation; CAP metadata and filesystem state do not yet share a durable
  power-loss journal.

## Applet runtime and APDU lifecycle

- [x] Persist the registered applet object and its fields after Java install.
- [ ] Reconstruct the persisted applet object after reboot using its stored
  class index, then dispatch applet lifecycle methods. A `Context` restore
  helper now resolves the reserved root, verifies package/class identity, and
  eagerly decodes fields. CCID SELECT invokes it when version-2 metadata
  supplies the class index and retains the resulting context for the card
  session. Two fresh host VPCD emulator processes now restore the same saved
  applet and complete SELECT, GETVERSION, JCSystem info, supported-algorithm
  probes, SHA-256 doFinal, and repeated static SHA-256 calls. Power-cycle
  restoration on the RP2350 remains unverified.
- [ ] Dispatch SELECT, `select()`, `deselect()`, and `process()` through the
  interpreter for the selected applet. Ordinary `invokevirtual` now reads the
  receiver's concrete class and resolves its override through that class's CAP
  method table. The borrowed-context virtual method entry can seed receiver and
  argument locals; root short/reference/integer return values can now be
  captured without a synthetic caller frame. CCID SELECT now invokes
  `Applet.select()` and rejects a false result with `6999`; selecting another
  AID or powering off invokes `Applet.deselect()`. The APDU object/buffer is
  wired into `Applet.process()` dispatch and refreshed for each command. Host
  VPCD now restores the installed applet, and its `select()` returns true. The
  fresh applet integration pass confirms successful `process(APDU)` execution
  for JCSystem info, support probes, and SHA-256 operations. Separate callback
  tests for `deselect()`, rejected SELECT, and physical applet APDUs remain
  outstanding.
- [ ] Implement the remaining Java Card APDU buffer and incoming/outgoing
  command methods used by JCAlgTest, including response-length enforcement and
  `61xx` handling. Short cases 1, 2S, 3S, and 4S are parsed; case 2S/4S Le is
  exposed through `setOutgoing()` and `setOutgoingNoChaining()`, with zero Le
  interpreted as 256. Extended-length APDUs are rejected with `6700`.
  `getIncomingLength()` reads the current command's Lc through
  the native bridge, `getOffsetCdata()` returns the short-APDU data offset 5,
  and `receiveBytes()` returns 0 because CCID buffers the complete short APDU
  before dispatch. `getBuffer()` binds the Java-visible array to the same native
  array used to collect response bytes. The API reports T=1, IFSC 32, IFSD 254,
  and NAD 0 from the advertised ATR and CCID descriptor. The supplied applet
  predominantly calls `setIncomingAndReceive()` and `setOutgoingAndSend()`.
  Stateful `setOutgoing()`/`setOutgoingNoChaining()`, `setOutgoingLength()`,
  `sendBytes()`, `sendBytesLong()`, `setOutgoingAndSend()`, and
  `getCurrentState()` now use per-command state, append outgoing chunks, and
  enforce both the command Le and T=1 frame bound for no-chaining responses.
  The Java Card 3.0.5 `APDUUtil` CLA helpers now decode channels 0–19 and
  report command chaining, secure messaging, interindustry encoding, and CLA
  validity; the corresponding `APDU` methods read the current command's CLA.
  A standalone test passes 17 cases, and API CAP conversion plus the ARM build
  pass. The VPCD applet reaches its APDU entry and invokes the APDU path, but
  its first applet field access fails before a result can validate these
  helpers.
  The bridge fragments data responses at
  short Le and serves `GET RESPONSE` using `61xx`. Parser, response-chain, and
  Context state tests pass; the Pico cycle also passed after flashing this
  implementation. An applet-level APDU exchange and PC/SC verification remain
  outstanding; extended APDUs are rejected.
- [ ] Complete the JCAlgTest v1.8.2-used native API surface, including the
  remaining transient array variants, `Util` helpers, `JCSystem` queries,
  `AID`, and
  Java exception mapping for unsupported crypto factories. `ArrayLogic` native
  copy/fill/compare/search now support the documented byte, short, and int
  primitive types (including big-endian byte encodings for short/int search);
  `JCSystem.getPreviousContextAID()` returns null because cross-applet context
  switching is unsupported. The buffer overload of
  `JCSystem.getAvailableMemory()` now distinguishes null, invalid-range, and
  invalid-type arguments in the native implementation, with focused tests and
  an ARM build passing. The Java Card wrapper now validates those arguments
  before the native call and throws Java `NullPointerException`,
  `ArrayIndexOutOfBoundsException`, or `SystemException.ILLEGAL_VALUE`, so
  those paths use ordinary Java exception dispatch. The ARM build regenerates
  the API CAP and ROM mask successfully; applet-level catch behavior remains
  physically unverified. Native C++ exception mapping for other methods is
  still incomplete. `JCSystem.getAID()` now creates a cached framework `AID`
  object from the installed/selected instance AID; installation and restored
  contexts receive that AID from the card manager. Context byte retention is
  covered by the focused native test and ARM build. Java-level `getAID()` use
  and broader API coverage remain unverified/incomplete.
- [x] Implement `JCSystem.makeTransientObjectArray()` with Java-null default
  entries and a wildcard component type that accepts any reference. ARM build
  and focused factory tests pass; installed-applet use remains unverified.
- [ ] Verify that an installed applet instance and its persistent fields survive
  a power cycle, not only that its CAP blob survives.

## Card manager and host integration

- [x] Make PC Java API and ROM-mask generation incremental, keyed to their
  inputs. A full regeneration took 1:01 for the Java API and 5:13 for Maven;
  after adding CMake output stamps, the follow-up PC build skipped both stages.
- [ ] Implement and verify GP DELETE and the remaining required GET STATUS / GET
  DATA behavior, including accurate package and applet lifecycle state.
  GET STATUS now handles ISD (`P1=80`), applications (`40`), executable load
  files (`20`), and load files with modules (`10`), with mandatory leading AID
  search criteria, wildcard/RID matching, and basic first/next request state.
  The ISD-only query correctly ignores its AID qualifier. Its bounded TLV
  encoders, search/response builder, and next-occurrence validation pass 44
  checks, and the ARM
  images build. This is not yet exercised through GPPro or on hardware. DELETE,
  GET DATA, complete continuation behavior, and end-to-end card-manager state
  verification remain open.
- [x] Integrate the PC-build vpcd frame transport with power controls, ATR,
  applet SELECT/DESELECT, and APDU dispatch through the VM lifecycle bridge.
  The PC target builds, the frame codec passes 14 checks, and a new session
  state-machine test passes 14 checks. The bounded release VPCD applet path
  passes 11 checks on each of two fresh emulator processes, including SHA-256
  `doFinal` and the 50-call static APDU (`TEST:SUMMARY pass=22 fail=0`). The
  full official client is not yet validated against this transport; the
  current PC/SC service is unavailable and the VPCD reader driver is not
  installed.
- [ ] Add golden applet coverage for exceptions, transient arrays, statics,
  APDU exchange, install parameters, and persistence. In addition to VPCD
  session tests with fake VM callbacks, the installed JCAlgTest 1.8.2 applet
  now completes a focused real-VM host integration pass. Broader bytecode and
  persistent-state assertions still need dedicated golden tests.
- [ ] Run the matching JCAlgTest v1.8.2 client in supported-algorithms mode and
  archive its CSV and logs with hashes.
- [x] Generate a simulator reference using the official v1.8.2 client and its
  bundled jCardSim; the complete `ALG_SUPPORT_EXTENDED` CSV and run log are
  archived. Cross-comparison against a CHOUPI card result is still pending.
- [ ] Compare the CHOUPI supported-algorithms CSV against the jCardSim
  reference and CHOUPI's declared supported-algorithm list.
- [ ] Complete hardware integration: an earlier ARM image passed its finite
  runtime check, and GPPro `--debug --list` previously listed ISD
  `A000000151000000` in `OP_READY`. The later snapshot image currently emits
  repeated boot `6F00` failures; runtime exception diagnostics are now built
  into ARM but need a permitted flash/serial run to identify the cause. Current
  PC/SC access is unavailable. Windows enumeration, current-image
  install/select/process/delete, and physical power-cycle persistence remain
  unverified.
- [ ] Pass AC1–AC5 and the plan's clean-flash integration sequence three times.

## Later / optional

- [ ] Add RP2350 TRNG and SHA-256 support, then additional mbedTLS algorithms,
  each with known-answer tests (plan WP8).
- [ ] Complete deferred hardening and robustness work, including malformed CAP
  and APDU fuzzing and interrupted persistent-write recovery (plan WP9).
