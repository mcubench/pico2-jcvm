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

Current environment note (2026-10-14): the latest `./tools/cycle choupi`
rebuilt ARM, RISC-V, and CHOUPI successfully, flashed the ARM firmware, and
passed the finite 18-second runtime capture with 18 repeated
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` pairs. The detailed
record is `artifacts/logs/cycle-choupi-20261014T135618Z.log`. The corrected ARM firmware was flashed;
GlobalPlatformPro CAP LOAD and INSTALL returned `9000`, the registry lists
`JCAlgTest1` as `INITIALIZED`, and SELECT by AID returned `9000` with FCI. These
steps validate install and selection, but `Applet.process()` has not yet been
validated by a JCAlgTest command. The official JCAlgTest v1.8.2 host JAR has
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

Registration implementation status: the VM captures the receiver passed to
either imported JC305 `Applet.register` method and serializes that instance's
fields under a reserved persistent applet root after a successful install
method return. CAP metadata now also records the registered class index in a
version-2 header while continuing to read version-1 headers. The v2 SELECT path
reconstructs and validates the object graph before returning FCI, but the
reference is not yet used for Java lifecycle/APDU dispatch. Installation
rollback and duplicate/invalid registration handling also remain unverified.

- [x] Parse the INSTALL [for install and make selectable] parameter field and
  pass its exact bytes in a Java byte array with offset zero and the matching
  byte length to `Applet.install(byte[], short, byte)`.
- [ ] Verify the installation-parameter byte array and length in a valid VM
  execution, including the supplied two-byte `C9 00` field.
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
  session. This has not yet been exercised against an installed applet.
- [ ] Dispatch SELECT, `select()`, `deselect()`, and `process()` through the
  interpreter for the selected applet. Ordinary `invokevirtual` now reads the
  receiver's concrete class and resolves its override through that class's CAP
  method table. The borrowed-context virtual method entry can seed receiver and
  argument locals; root short/reference/integer return values can now be
  captured without a synthetic caller frame. CCID SELECT now invokes
  `Applet.select()` and rejects a false result with `6999`; selecting another
  AID or powering off invokes `Applet.deselect()`. The APDU object/buffer is
  wired into `Applet.process()` dispatch and refreshed for each command. None
  of these lifecycle calls has been exercised with an installed applet because
  PC/SC is unavailable in the current execution context.
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
  still incomplete. Broader API coverage remains incomplete.
- [x] Implement `JCSystem.makeTransientObjectArray()` with Java-null default
  entries and a wildcard component type that accepts any reference. ARM build
  and focused factory tests pass; installed-applet use remains unverified.
- [ ] Verify that an installed applet instance and its persistent fields survive
  a power cycle, not only that its CAP blob survives.

## Card manager and host integration

- [ ] Implement and verify GP DELETE and the remaining required GET STATUS / GET
  DATA behavior, including accurate package and applet lifecycle state.
- [ ] Add a PC-build APDU transport through vpcd and golden applet coverage for
  exceptions, transient arrays, statics, APDU exchange, install parameters, and
  persistence.
- [ ] Run the matching JCAlgTest v1.8.2 client in supported-algorithms mode and
  archive its CSV and logs with hashes.
- [x] Generate a simulator reference using the official v1.8.2 client and its
  bundled jCardSim; the complete `ALG_SUPPORT_EXTENDED` CSV and run log are
  archived. Cross-comparison against a CHOUPI card result is still pending.
- [ ] Compare the CHOUPI supported-algorithms CSV against the jCardSim
  reference and CHOUPI's declared supported-algorithm list.
- [ ] Complete hardware integration: the earlier ARM firmware passed its finite
  serial runtime test, and GPPro `--debug --list` now succeeds over PC/SC on
  Linux with ISD `A000000151000000` in `OP_READY`. Windows enumeration,
  install/select/process/delete, and power-cycle persistence remain unverified.
- [ ] Pass AC1–AC5 and the plan's clean-flash integration sequence three times.

## Later / optional

- [ ] Add RP2350 TRNG and SHA-256 support, then additional mbedTLS algorithms,
  each with known-answer tests (plan WP8).
- [ ] Complete deferred hardening and robustness work, including malformed CAP
  and APDU fuzzing and interrupted persistent-write recovery (plan WP9).
