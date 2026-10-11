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

Current environment note: physical testing is active again. The repository
cycle can flash the Pico and pass its finite serial checks. The PC/SC daemon is
running, but it has no reader because the service account cannot access the
Pico USB node; GlobalPlatformPro reports no available readers. GPPro card
operations need reader access restored before install testing can resume.
The PC build wrapper is available; Cargo still cannot fetch the uncached
`ipc-channel` dependency because crates.io DNS/network access is unavailable.

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
  evidence. The last recorded GPPro attempt returned `6A80`; the stale watchdog
  scratch marker is not reliable enough to identify its cause.
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
  eagerly decodes fields. CCID SELECT invokes this helper when version-2
  metadata supplies the class index; the branch has not yet been exercised
  against an installed applet.
- [ ] Dispatch SELECT, `select()`, `deselect()`, and `process()` through the
  interpreter for the selected applet.
- [ ] Implement Java Card APDU buffer and incoming/outgoing command methods used
  by JCAlgTest, including case-4 Le and `61xx` response handling.
- [ ] Complete the JCAlgTest v1.8.2-used native API surface, including the
  transient array variants, `Util` helpers, `JCSystem` queries, `AID`, and
  Java exception mapping for unsupported crypto factories.
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
- [ ] Re-run hardware integration when the user provides the Pico 2 for physical
  testing: GPPro list/install/select/process/delete, finite serial monitor, and
  power-cycle persistence.
- [ ] Pass AC1–AC5 and the plan's clean-flash integration sequence three times.

## Later / optional

- [ ] Add RP2350 TRNG and SHA-256 support, then additional mbedTLS algorithms,
  each with known-answer tests (plan WP8).
- [ ] Complete deferred hardening and robustness work, including malformed CAP
  and APDU fuzzing and interrupted persistent-write recovery (plan WP9).
