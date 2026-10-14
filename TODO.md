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

## Active: make v1.8.2 installation execute correctly

Current environment note (2026-10-14): the rebuilt ARM CHOUPI firmware was
flashed to the Pico 2 and passed its 18-second serial check with repeated
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The user reports the
reader is visible. GPPro in this execution context first returned
`SCARD_E_NO_SERVICE`; a bounded elevated retry reported no available readers,
and the latest retry after reflashing timed out after 15 seconds without reader
or APDU output. No INSTALL APDU reached the card, so applet installation remains
unverified. The host VM
target builds Java API classes and CAPs, but Cargo offline mode confirms the
local registry lacks `ipc-channel`; the online build cannot resolve
`index.crates.io`.

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
- [ ] Resolve JC 3.0.5 imported class and method tokens against the embedded
  CHOUPI exports. Validate each mapping against CAP/export metadata; do not
  treat a GP INSTALL status as proof that Java installation completed.
- [ ] Find and fix the current install-method failure using trustworthy runtime
  evidence. The last GPPro attempt that reached the card returned `6A80`; the
  most recent attempt failed before card access with `SCARD_E_NO_SERVICE`. The
  stale watchdog scratch marker is not reliable enough to identify its cause.
- [x] Track calls to the imported JC305 `Applet.register` virtual methods during
  install and reject a successful VM return if no registration call occurred.
- [x] Store the registered applet class index beside the persistent AID using a
  CRC-protected CAP metadata version, while retaining version-1 CAP readability.
- [ ] Implement `Applet.register()` and `register(byte[], short, byte)` so a
  successful Java install creates the persistent, selectable instance.
- [ ] Make failed installation atomic: remove partial instances/package state
  and return a useful status word.

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
  command methods used by JCAlgTest, including case-4 Le and `61xx` response
  handling. `getIncomingLength()` now reads the current command's Lc through
  the native bridge, `getOffsetCdata()` returns the short-APDU data offset 5,
  and `receiveBytes()` returns 0 because CCID buffers the complete short APDU
  before dispatch. `getBuffer()` binds the Java-visible array to the same native
  array used to collect response bytes. The API reports T=1, IFSC 32, IFSD 254,
  and NAD 0 from the advertised ATR and CCID descriptor. The supplied applet
  predominantly calls `setIncomingAndReceive()` and `setOutgoingAndSend()`;
  stateful `setOutgoing*`/`sendBytes*` methods, Le handling, and `61xx` chaining
  remain unfinished. Both architecture builds pass; applet-level runtime
  verification remains outstanding.
- [ ] Complete the JCAlgTest v1.8.2-used native API surface, including the
  transient array variants, `Util` helpers, `JCSystem` queries, `AID`, and
  Java exception mapping for unsupported crypto factories. The native
  `arrayFindGeneric` currently supports byte-array searches only; the API's
  other primitive-array variants remain unsupported.
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
- [ ] Run the same client through jCardSim and compare the CSV against the
  simulator and CHOUPI's declared supported-algorithm list.
- [ ] Complete hardware integration: the current ARM firmware was flashed and
  passed its finite serial runtime test, but GPPro list/install/select/process/
  delete and power-cycle persistence remain unverified. The latest bounded
  elevated GPPro run found no readers in this execution context, despite the
  user's report that the reader is visible.
- [ ] Pass AC1–AC5 and the plan's clean-flash integration sequence three times.

## Later / optional

- [ ] Add RP2350 TRNG and SHA-256 support, then additional mbedTLS algorithms,
  each with known-answer tests (plan WP8).
- [ ] Complete deferred hardening and robustness work, including malformed CAP
  and APDU fuzzing and interrupted persistent-write recovery (plan WP9).
