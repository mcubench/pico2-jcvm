# Product specification — invariants

Status: confirmed by the user on 2026-11-14. Source of decisions:
[docs/archive/SPEC_draft_opus55_20261114.md](docs/archive/SPEC_draft_opus55_20261114.md).

This file states **what the product must be**. How agents work is in
`AGENTS.md`; what remains to be done is in `TODO.md`.
- Only the user may change an invariant. Agents must not weaken, reinterpret
  or remove one. If a task conflicts with an invariant, stop and ask through
  `.agent/EXT_REQUESTS.md`.
- Every review checks the invariants and reports violations as `INV-xx`
  findings.
- "Known gap" lines record where the repository does not yet satisfy an
  invariant. Closing a gap is TODO work; the gap is not permission to diverge.

## 1. Platform and runtime boundary

**INV-01 Target hardware.** The target is the RP2350 Raspberry Pi Pico 2, ARM
Cortex-M33 only. Hazard3 RISC-V is out of scope: no build gate, no runtime or
physical measurement.

**INV-02 What runs on the card.** The firmware is a native C++ Java Card VM
that interprets installed CAP bytecode. It never runs `.java` or `.class`
programs as firmware.

**INV-03 Where algorithms live.** All Java Card API behavior and cryptography
are implemented in native C++ and reached through trusted ROM-mask native
dispatch. No Java provider or adapter implementations exist in firmware, and
no CAP is created or modified to implement an algorithm. Existing upstream CAPs
are used only to exercise the native implementation.

**INV-04 Crypto backend.**
- Cryptographic primitives come from vetted libraries (Mbed TLS, Pico SDK
  SHA-256/TRNG). Hand-written code is limited to glue and Java Card semantics.
- Every new algorithm needs known-answer tests that pass **on hardware** before
  it is declared supported.

**INV-05 Second core.**
- Execution is single-core today. TinyUSB endpoint work stays in one execution
  context.
- Using core 1 (e.g. as a crypto coprocessor or for transport heartbeats) is
  allowed only after a design has been written and approved by the user.

## 2. Host interface and compatibility

**INV-06 Host visibility.** The Pico 2 enumerates as a standard USB CCID reader
with a card inserted. It is usable through PC/SC on Linux and Windows without
custom drivers.

**INV-07 Java Card compatibility target.**
- The baseline target is the Java Card Classic 3.0.5 API, accepting CAP 2.1/2.2
  formats where the loader implements their components.
- In addition, the card implements a declared subset of the Java Card 3.2
  preview API (currently the post-quantum packages `javacardx.crypto.kem` and
  `javacardx.security.pqc`, and `javacard.security.NamedParameterSpec` /
  `AlgorithmParameterSpec`, for ML-KEM and ML-DSA). `docs/supported-profile.md`
  lists the exact subset.
- The loader also accepts CAP 2.3 when it can map the CAP onto the implemented
  components, including newer import versions of the 3.0.5-level ROM packages.
  Any reference to a 3.1/3.2 class or method the ROM lacks fails closed at
  installation and is never silently stubbed.
- The 3.2 preview subset adds to the 3.0.5 baseline and never replaces it. The
  3.0.5 baseline (JCAlgTest 1.8.5/jc305, INV-26) remains the acceptance
  reference.
- This is a compatibility target, not a claim of full conformance to either
  specification version.

INV-07 was extended by the user on 2026-11-26 to cover the 3.2 preview subset.

**INV-08 Honest support reporting.**
- The card reports exactly what it implements. Unsupported algorithms return
  `NO_SUCH_ALGORITHM`; results are never faked, hidden or relabelled.
- Support results must match the build profile's generated support manifest,
  cross-checked against a jCardSim reference run.

**INV-09 Known protocol exclusions.** Extended-length APDUs, the T=1
waiting-time extension and logical channels are not implemented. They are
declared as exclusions in `docs/supported-profile.md`.

## 3. Card lifecycle, management and storage

**INV-10 Shipped card state.**
- Released and acceptance firmware ships as an **empty card**: only the ISD is
  present, with no preinstalled applet.
- Applets are installed through GlobalPlatform.

**INV-11 Card management.**
- A minimal GlobalPlatform card manager supports SCP03-authenticated LOAD,
  INSTALL and DELETE, plus SELECT, GET DATA and GET STATUS.
- It is usable with unchanged GlobalPlatformPro.

**INV-12 Card-manager keys.**
- SCP03 uses the well-known test key `40 41 … 4F`.
- This is acceptable because the project is educational and must not be used
  for security-critical purposes; README and release notes must say so.

**INV-13 Persistence across power cycles.**
- Installed packages, applet instances and persistent objects survive a power
  cycle.
- Transient (`CLEAR_ON_RESET` / `CLEAR_ON_DESELECT`) contents never become
  persistent.

**INV-14 Atomicity and recovery.**
- Persistent updates are transactional: an interrupted write, commit or
  rollback leaves either the previous or the new committed state, never a mix.
- If integrity cannot be proven, the store fails closed and leaves flash
  unchanged; it never guesses.

**INV-15 Memory reclamation.**
- `JCSystem.requestObjectDeletion()` reclaims unreachable persistent objects,
  so long runs reach a memory plateau and never exhaust storage.
- Deletion is always reachability-safe: if reachability is incomplete, nothing
  is deleted.
- Object deletion is enabled in the normal build (see INV-24).

**INV-16 Allocation discipline.**
- Repeated API use (e.g. repeated `init`, key reuse or replacement) must not
  allocate new persistent storage per call.
- Prefer preventing an allocation over cleaning up after it.

## 4. Security model

**INV-18 Uploaded CAPs are untrusted.**
- CAPs loaded through GlobalPlatform are untrusted executable input, even when
  the LOAD was authenticated. ROM-mask packages are trusted.
- Uploaded packages can never execute `impdep1`/`impdep2` or reach native
  dispatch directly.
- Method bodies, branch targets and operand fetches are bounds-checked and
  fail closed.
- **Commitment:** full on-card bytecode verification (types, constant-pool
  linkage, field and access kinds) is a project goal.
- Known gap: type and linkage verification are incomplete (see
  `docs/CAP_EXECUTION_TRUST_MODEL.md`).

**INV-19 Applet isolation (firewall).**
- No applet can read or write another applet's objects except through
  Shareable Interface Objects, as defined by the Java Card firewall.
- Hostile multi-applet scenarios are acceptance tests.
- Known gap: general firewall and multi-applet context semantics are
  incomplete today.

**INV-20 Side-channel hardening.**
- Hardening (e.g. the existing RSA-CRT exponent blinding) is welcome as best
  effort but is not claimed.
- The project is not certification-grade.

**INV-21 Key storage.**
- Keys live in the normal persistent object store today.
- Future goal: a dedicated protected key memory.

**INV-22 Malformed-input robustness.**
- Malformed APDUs and CAPs produce a controlled status word, never a `FAULT`
  or hang, and the next valid APDU still succeeds.
- Parsers are fuzzed under sanitizers.

## 5. Builds, identity and evidence

**INV-23 Build profiles.** Algorithm support is selected at build time by
profiles in `config/javacard-support.json` (`baseline`, `symm_crypto`,
`asymm_crypto`, `max_crypto`, …). The generated manifest is authoritative for
each image.

**INV-24 Build modes.** There are exactly two modes:
- **normal:** object deletion ON and every debug/trace option OFF. This is the
  default, and the only mode valid as evidence.
- **diagnostic:** any debug/trace option ON. It is never acceptance evidence.

Non-diagnostic images contain no debug strings.

`tools/build` records the mode (`mode=normal|diagnostic`) in its build
provenance. Building with `PICO2_ENABLE_OBJECT_DELETION=0` is diagnostic.

**INV-25 Firmware identity.**
- Every image is identifiable on the card: CPLC carries the source commit
  identifier.
- Every result records the image SHA-256, chip ID and reader name.

**INV-26 Reference test inputs.**
- Use the unchanged user-supplied JCAlgTest 1.8.5/jc305 CAP (user directive
  2026-11-18; SHA-256 `fa64582e…b48fb`) with the 1.8.5 host client,
  identified by its JAR SHA-256. 1.8.4/jc305 CAP results are historical.
- For the Java Card 3.2 preview subset (INV-07), also use the JCAlgTest
  1.8.6/jc32 pair (user directive 2026-11-25): CAP `AlgTest_v1.8.6_jc32.cap`
  (CAP 2.3, GETVERSION `1.8.6_jc32`) and the 1.8.6 client, both identified by
  the SHA-256 pinned in `tests/fixtures/manifest.json`. The pair is built in
  this workspace from the user's AlgTest sources
  (`tests/fixtures/external/jcalgtest/1.8.6/README.md`), not supplied
  upstream. It is run in addition to the 1.8.5/jc305 baseline, never instead
  of it. A rebuild is a new pin, and results record which pin was used.
- Supplied CAP and JAR bytes, and the bytes of a pinned build, are never
  modified.

INV-26 was extended by the user on 2026-11-26 with the 1.8.6/jc32 pair.

**INV-27 Acceptance definition.**
- "Done" means physical-hardware evidence obtained with the unchanged external
  clients on an empty-card normal image.
- Emulator (VPCD) results, builds, serial heartbeats and partial campaigns are
  never acceptance.

## 6. Resource envelope

**INV-28 Declared capacity limits.** The current declared limits are:
- a 1 MiB firmware window;
- 8 dynamic package slots (package IDs 56–63);
- a 96 KiB / 8,192-slot persistent overlay;
- streaming `MessageDigest` input up to the selected algorithm's message-length
  limit. Each call is bounded by its Java Card array and `short` arguments.
  Digest workspace is bounded independently of the input length.

The MessageDigest limit was updated by the user on 2026-11-21.

Changing any of these limits is a specification change.

## Deliberately unspecified (decided 2026-11-14)

- **Flash wear (draft INV-17):** no wear invariant for now.
- **Responsiveness bound (draft INV-29):** no stated APDU time bound for now.
  Long native operations cooperate with the CCID time extension and the
  watchdog.

Invariant numbers follow the confirmed draft; INV-17 and INV-29 are
intentionally unused.
