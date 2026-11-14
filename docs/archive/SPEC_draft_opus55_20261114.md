# SPEC.md draft — product invariants for confirmation

Prepared by Claude Opus 5.5, 2026-11-14 (repository HEAD `8f14a207`).

Purpose: `SPEC.md` will hold the **product invariants**: what the system must
be, as opposed to how agents work (`AGENTS.md`) or what remains to do
(`TODO.md`). Every reviewer run checks each invariant and reports violations by
ID (`INV-xx`). Changing an invariant requires your explicit decision.

How to use this draft:
- Mark one option per item with `[x]`, or write your own on the `Other:` line.
- **A** is always the decision as it currently stands in the repository. The
  source is cited, and "(agent decision)" means no recorded user instruction
  exists for it.
- Mark `[x] Drop` to leave an item out of `SPEC.md`.

---

## 1. Platform and runtime boundary

### INV-01 Target hardware
- [x ] **A** RP2350 Pico 2, ARM Cortex-M33 only. RISC-V is out of scope (your
  decision 2026-11-14; `plan.md`).
- [ ] B ARM is primary; RISC-V must keep building warning-free (no runtime parity).
- [ ] Drop
- Other: ______

### INV-02 What runs on the card
- [x ] **A** The firmware is a native C++ Java Card VM that interprets installed
  CAP bytecode. It never runs `.java` or `.class` programs (`AGENTS.md`).
- [ ] Drop
- Other: ______

### INV-03 Where algorithms live
- [ x] **A** All Java Card API behavior and cryptography are native C++, reached
  through trusted ROM-mask dispatch. There are no Java provider/adapter
  implementations in firmware, and no created or modified CAPs to implement
  algorithms (`AGENTS.md`, `SKILL.md`; added after the 10-23 provider drift).
- [ ] Drop
- Other: ______

### INV-04 Crypto backend
- [ ] **A** Use vetted library primitives (Mbed TLS, Pico SDK SHA-256/TRNG).
  Hand-written crypto is limited to glue and Java Card semantics
  (`FIRMWARE_JCVM_ARCHITECTURE.md`; agent decision).
- [ x] B Same as A, and every new algorithm also needs known-answer tests on
  hardware before it is declared supported.
- [ ] Drop
- Other: ______

### INV-05 Second core
- [ ] **A** Single-core execution. Core 1 is unused, and TinyUSB endpoint work
  stays in one execution context (`FIRMWARE_JCVM_ARCHITECTURE.md`; agent
  decision).
- [ x] B Same as A, but using core 1 (e.g. a crypto coprocessor or a transport
  heartbeat) is allowed once designed and approved.
- [ ] Drop
- Other: ______

---

## 2. Host interface and compatibility

### INV-06 Host visibility
- [ x] **A** The Pico 2 enumerates as a standard USB CCID reader with a card
  inserted. It is usable through PC/SC on Linux and Windows without custom
  drivers (`plan.md` AC1).
- [ ] Drop
- Other: ______

### INV-07 Java Card compatibility target
- [x ] **A** The target is the Java Card Classic 3.0.5 API, accepting CAP
  2.1/2.2. This is declared as a *compatibility target*, not a claim of full
  conformance (`docs/supported-profile.md`).
- [ ] B Same as A, plus a named subset contract: the JCAlgTest-exercised API
  plus the supplied API validation suite define "supported".
- [ ] C Target full Java Card 3.0.5 conformance (long-term).
- [ ] Drop
- Other: ______

### INV-08 Honest support reporting
- [x ] **A** The card reports exactly what it implements. Unsupported algorithms
  return `NO_SUCH_ALGORITHM` and are never faked or relabelled. Support results
  must match the build profile's generated support manifest, cross-checked
  against jCardSim (`plan.md` AC4; `supported-profile.md`).
- [ ] Drop
- Other: ______

### INV-09 Known protocol exclusions
- [x ] **A** Extended-length APDUs, the T=1 waiting-time extension and logical
  channels are not implemented and are declared as exclusions
  (`supported-profile.md`).
- [ ] B Same as A, but extended-length APDUs become a required future feature.
- [ ] Drop
- Other: ______

---

## 3. Card lifecycle and management

### INV-10 Shipped card state
- [ x] **A** Released and acceptance firmware ships as an **empty card**: ISD
  only, no preinstalled applet. Applets are installed through GlobalPlatform
  (empty-card work 10-24/25 after the preinstalled-JCAlgTest drift; `AGENTS.md`
  acceptance rule).
- [ ] Drop
- Other: ______

### INV-11 Card management
- [ x] **A** Minimal GlobalPlatform card manager with SCP03-authenticated
  LOAD/INSTALL/DELETE, SELECT, GET DATA and GET STATUS. It is usable by unchanged
  GlobalPlatformPro (`README.md`; `plan.md` AC1/AC2).
- [ ] Drop
- Other: ______

### INV-12 Card-manager keys ⚠ decision needed
- Current state: SCP03 uses the hard-coded well-known test key `40 41 … 4F`
  (`src/scp03.cpp:22`; agent decision).
- [ x] **A** The well-known test key is acceptable. The project is declared
  educational and not for security-critical use (`HUMAN.md` warning).
- [ ] B The test key is allowed only in development builds. Release builds must
  use a per-device key (provisioning to be designed).
- [ ] C Add GlobalPlatform PUT KEY support so the user can rotate the key.
- [ ] Drop
- Other: ______

### INV-13 Persistence across power cycles
- [ x] **A** Installed packages, applet instances and persistent objects survive
  a power cycle (`plan.md` AC5). Transient (`CLEAR_ON_RESET` /
  `CLEAR_ON_DESELECT`) contents never become persistent (QKB "Durable identity").
- [ ] Drop
- Other: ______

### INV-14 Atomicity and recovery
- [x ] **A** Persistent updates are transactional: an interrupted write, commit
  or rollback leaves the previous or the new committed state, never a mix. If
  integrity cannot be proven, the store fails closed and leaves flash unchanged
  rather than guessing (MEM-01 T1 policy; QKB).
- [ ] B Same as A, plus physical power-cut testing as a release gate (not only
  simulated cuts).
- [ ] Drop
- Other: ______

### INV-15 Memory reclamation
- [x ] **A** `JCSystem.requestObjectDeletion()` must reclaim unreachable
  persistent objects, so that long runs reach a memory plateau, never
  exhaustion. Deletion is always reachability-safe: if reachability is
  incomplete, nothing is deleted. Until MEM-01 T8 and PERSIST-01 pass, it is ON
  only in the acceptance image; then it becomes the default (your decision
  11-C).
- [ ] Drop
- Other: ______

### INV-16 Allocation discipline
- [x ] **A** Repeated API use (e.g. repeated `init`, key reuse or replacement)
  must not allocate new persistent storage per call. Prefer preventing
  allocation over cleaning up after it (Opus review 11-10 → RESOURCE-02).
- [ ] Drop
- Other: ______

### INV-17 Flash wear
- [ x] **A** No explicit wear rule today.
- [ ] B Bounded writes: avoid byte-wise flash writes, batch per sector, and
  measure erase count per APDU for heavy operations (PERF-07).
- [ ] Drop
- Other: ______

---

## 4. Security model

### INV-18 Uploaded CAPs are untrusted
- [ ] **A** CAPs loaded through GlobalPlatform are untrusted executable input,
  even when authenticated. ROM-mask packages are trusted. Uploaded packages can
  never execute `impdep1`/`impdep2` or reach native dispatch directly. Method
  bodies, branches and operand fetches are bounds-checked and fail closed
  (`docs/CAP_EXECUTION_TRUST_MODEL.md`).
- [x ] B Same as A, plus a commitment to full on-card bytecode verification
  (types, linkage) as a later goal.
- [ ] Drop
- Other: ______

### INV-19 Applet isolation
- [ ] **A** Not claimed today: general firewall and multi-applet context
  semantics are incomplete (`supported-profile.md`). Shareable interface objects
  (SIO) exist (agent decision, 10-18) but are not a security boundary.
- [x ] B Make the firewall a required invariant: no applet can read or write
  another applet's objects except through SIO. Hostile multi-applet scenarios
  are acceptance tests.
- [ ] Drop
- Other: ______

### INV-20 Side-channel hardening
- Current state: RSA-CRT private operations use exponent blinding
  (`choupi/src/native_rsa_math.hpp`; agent decision 10-27, unrequested).
- [ x] **A** Keep it as best effort: hardening is welcome but not claimed, and
  the project is not certification-grade (`plan.md` out of scope).
- [ ] B Required: RSA/ECC private-key operations must be blinded or constant
  time, with a regression test per algorithm.
- [ ] Drop
- Other: ______

### INV-21 Key storage
- [ ] **A** No special protection: keys live in the normal persistent object
  store.
- [x ] B Future goal: a dedicated protected key memory (your Future plans item).
- [ ] Drop
- Other: ______

### INV-22 Malformed-input robustness
- [x ] **A** Malformed APDUs and CAPs must produce a controlled status word,
  never a FAULT or hang. The next valid APDU must still succeed. Parsers are
  fuzzed under sanitizers (CAPIN acceptance pattern; fuzzing was an agent
  decision 10-27).
- [ ] Drop
- Other: ______

---

## 5. Builds, identity and evidence

### INV-23 Build profiles
- [x ] **A** Algorithm support is selected at build time by profiles in
  `config/javacard-support.json` (`baseline`, `symm_crypto`, `asymm_crypto`,
  `max_crypto`, …). The generated manifest is authoritative for each image.
- [ ] Drop
- Other: ______

### INV-24 Build modes
- [ ] **A** There are three modes: `normal` (default, no debug), `acceptance`
  (object deletion ON, debug OFF) and `diagnostic` (any debug switch ON). Only
  normal and acceptance images are evidence; no debug strings appear in
  non-diagnostic images (BUILD-DBG-01; your decision 11-C).
- [ ] Drop
- Other: _Make it only to modes - normal which is objects deletion ON, debug OFF_and diagnostic (ay debug optiion ON)____

### INV-25 Firmware identity
- [x ] **A** Every image is identifiable on the card: CPLC carries the source
  commit identifier (REL-01, 11-07/11; agent decision). Every result records
  the image SHA-256, chip ID and reader name.
- [ ] Drop
- Other: ______

### INV-26 Reference test inputs
- [ x] **A** Use the unchanged supplied JCAlgTest 1.8.4/jc305 CAP with the 1.8.5
  host client, identified by JAR SHA-256. Supplied CAP/JAR bytes are never
  modified (`AGENTS.md`).
- [ ] Drop
- Other: ______

### INV-27 Acceptance definition
- [x ] **A** "Done" means physical-hardware evidence with the unchanged external
  clients on an empty-card acceptance image. Emulator (VPCD) results, builds,
  heartbeats and partial campaigns are never acceptance (`AGENTS.md`;
  `README.md`).
- [ ] B Same as A, plus independent Windows-host evidence required for release.
- [ ] Drop
- Other: ______

---

## 6. Resource envelope (numbers to confirm or set)

### INV-28 Capacity limits
- Current: a 1 MiB firmware window; 8 dynamic package slots (IDs 56–63); a
  96 KiB / 8,192-slot persistent overlay; a 2,046-byte `MessageDigest`
  cumulative input limit (`FIRMWARE_JCVM_ARCHITECTURE.md`;
  `supported-profile.md`).
- [x ] **A** Record these as the current declared limits. Changing them is a
  spec change.
- [ ] B Record them as informational only (not invariants).
- [ ] Drop
- Other: ______

### INV-29 Responsiveness
- [x ] **A** No stated bound today. Long native operations cooperate with the
  CCID time extension and the watchdog.
- [ ] B The card must never stop responding to the host: every APDU completes or
  times out within a declared bound, and BOOTSEL entry remains reachable during
  long operations (BOOTSEL-01; HOST-01).
- [ ] Drop
- Other: ______

---

## Additional invariants you want (free text)

- ______
- ______

---

When you have marked your choices, tell a session to "create SPEC.md from
SPEC_draft_opus55_20261114.md". It will:
- write `SPEC.md` with the selected statements;
- reference it from `AGENTS.md` (read at session start) and from the reviewer
  prompt;
- make "violated `INV-xx`" a reportable finding class.
