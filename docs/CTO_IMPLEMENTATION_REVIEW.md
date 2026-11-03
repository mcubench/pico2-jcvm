# Pico 2 Java Card VM — implementation review for technology leadership

**Evidence cutoff: 3 November 2026.** Eight-page Markdown briefing, with explicit page breaks. Based on assigned work, implementation records and selected source inspection; this review did not rebuild or retest firmware. Recommendations and judgments below are the author's synthesis, not statements attributed to an actual engineering team.

## Page 1 of 8 — What was achieved, and what it means

The project turned a legacy Java Card interpreter into an experimental, host-accessible smart-card platform on the Raspberry Pi Pico 2. It now demonstrates CAP installation, applet execution, persistent state, standard tooling access and substantial native cryptography. The remaining work is concentrated in correctness across applet boundaries, durable recovery, resource limits and repeatable acceptance.

The original assignment was narrower: load the supplied JCAlgTest 1.8.2 CAP, run its matching client, produce a truthful algorithm-support CSV and reproduce the result after power removal. Returning “unsupported” was acceptable. The starting interpreter lacked the APDU path, lifecycle, loader, exception handling and API behavior needed even for that modest outcome. Thus, the central challenge was completing a card runtime around an interpreter, rather than simply compiling existing firmware for a new processor. [Original scope and acceptance](../plan.md)

Development subsequently expanded into symmetric and asymmetric cryptography, multi-package management, transactions, garbage collection, inter-applet services, parser hardening, profile generation and Windows delivery. Those additions explain why extensive working functionality coexists with a substantial open backlog.

| Leadership question | Assessment at the cutoff |
| --- | --- |
| Does the core proposition work? | Yes. Physical Pico runs install and execute unchanged upstream applets, and complete support reports exist. |
| Is it a complete Java Card implementation? | No. It targets a tested subset of Classic 3.0.5; exclusions and incomplete context semantics remain. |
| Is this the originally proposed Rust OS port? | The active Pico implementation uses Pico SDK and native C++ services. The host emulator still uses Rust OS services. |
| Is the latest source fully accepted on hardware? | No. Several latest fixes have emulator or focused physical evidence, without a complete combined-image acceptance run. |
| Is it ready for security-sensitive production use? | The records do not establish that. Isolation, recovery and private-operation hardening remain material gates. |
| Where should the next investment go? | Close correctness and acceptance gaps before expanding algorithm or platform scope. |

The strongest recent integration result is the complete **466-call Crypto emulator sweep**: 304 OK, 161 expected unsupported and one documented test-key incompatibility; no wrong results, transport errors or missing calls. It took 1:45:54 including upload and cleanup. That establishes useful breadth on the host backend, while leaving hardware-specific storage and transport acceptance open. [Current backlog, EMU-01](../TODO.md#emu-01)

The project's status is best described as **a demonstrated experimental platform undergoing consolidation**. Feature count alone would overstate readiness; calling it merely an unfinished port would understate the achieved integration.

<div style="page-break-after: always;"></div>

## Page 2 of 8 — How the problem was decomposed

The plan proposed two tracks: finish runtime behavior on the PC emulator, and bring up the Pico platform. Both would converge through the same real host clients. This was a sound risk-reduction strategy: a virtual PC/SC reader lets developers exercise installation and applet behavior without making each VM investigation depend on USB, flashing and device availability.

The implemented system has these main responsibility areas:

| Domain | Implemented responsibility | Important boundary |
| --- | --- | --- |
| Platform services | Pico startup, flash access, watchdog, clocks, diagnostics and device facilities | ARM is the full physical runtime target. RISC-V is a build check, not equivalent product acceptance. |
| Host transport | TinyUSB CCID, CDC diagnostics and host VPCD connection | Reader discovery, card activation and APDU execution are separate failure stages. |
| Card management | SCP03, CAP LOAD/INSTALL, SELECT, DELETE, registry and selected GET DATA/STATUS flows | Authentication does not make uploaded CAP structures safe to parse. |
| JCRE and interpreter | Applet lifecycle, bytecodes, exceptions, API dispatch, contexts and references | A compact object handle needs ownership information when crossing packages. |
| Native APIs and crypto | Framework semantics, keys, cipher/digest/signature state and algorithms | API behavior belongs in native C++; installed CAPs exercise it. |
| Persistence | Tagged objects, aliases, snapshots, transactions and INSTALL recovery | Durability spans multiple stores and publication steps. |
| Engineering infrastructure | Wrappers, generated profiles, golden tests, fuzzing and archived evidence | A selected capability is a declaration; measured behavior is its proof. |

The request path is: **host client → OS PC/SC → CCID or VPCD → card manager/JCRE → interpreter → native APIs**, with storage supporting installation and persistent objects.

A consequential departure from the plan is the platform foundation. The plan described porting the old Rust OS, including Cortex-M33 hardware memory compartments. The current ARM link instead assembles the interpreter with native Pico services; a full isolated Rust kernel port is not established. Software firewall checks should therefore not be presented as proof of hardware-enforced applet isolation. The ARM-only full-runtime target is visible in [CMakeLists.txt](../CMakeLists.txt); common filesystem behavior is documented in the [filesystem contract](FILESYSTEM_ABI.md).

Another correction concerned API implementation placement. Early crypto work used generated Java providers. A 25 October audit identified that as violating the required native boundary. Subsequent work moved digest, MAC, key and cipher behavior into native dispatch. This was architectural rework, not just algorithm expansion. The lesson is to settle implementation ownership before multiplying features across generated providers and native code. [Audit and migration records](../progress.md)

The existing architecture narrative contains historical descriptions of provider paths and older limits. This review gives newer task records and source inspection precedence over those descriptions.

<div style="page-break-after: always;"></div>

## Page 3 of 8 — How implementation unfolded

The record covers 9 October–3 November 2026. Its entries are not strictly chronological: later material appears both near the beginning and at the end. The sequence below is reconstructed from dated experiments and outcomes, rather than file order. [Implementation history](../progress.md)

| Period | Main engineering focus | What it established |
| --- | --- | --- |
| 9–11 October | Recover pinned dependencies and build tools; compile the VM; diagnose startup memory; introduce CCID, SCP03 and CAP installation | The board could execute the runtime and host tools could reach it, but persistence and dispatch were still fragile. |
| 12–16 October | Lifecycle, virtual calls, generated JNI/ROM consistency, APDU handling, host VPCD, snapshots and transactions | A real applet could follow the installation/selection/process path instead of relying on a smoke image. |
| 17–20 October | Repeated-call exhaustion, typed roots, aliases, exception translation, GC and persistent restore | Long-running use exposed object lifetime defects that startup checks could not detect. Historical cold-power evidence was obtained. |
| 21–25 October | Multi-package loading, flash layout, rollback/journaling, API coverage, RMI, support profiles and CCID packet boundaries | The project advanced from one known applet to a more general card platform, exposing stronger lifecycle requirements. |
| 26–29 October | Native API migration, RSA/EC expansion, known-answer testing, token corrections, key-write batching and CAP/APDU validation | Crypto worked through increasingly realistic API paths; errors often originated in dispatch or transport rather than mathematics. |
| 30 October–3 November | Overlay capacity and compaction, safe reclaimer groundwork, shared package state, SIO ownership, bounded progress and final acceptance sweeps | The dominant challenges became cross-feature interactions and establishing exactly what each image had proved. |

The progression was iterative. A broad client run exposed a symptom; a focused APDU, VM trace or host test narrowed it; a small fix followed; builds and available hardware checks supplied evidence. Source reviews also interrupted this loop to identify structural faults before more feature work accumulated.

The reviews were valuable. On 13 October, findings included a native ABI mismatch, JNI and ROM-mask output from different generations, and incorrect lifecycle tokens. A later review identified overlapping CAP/filesystem regions. These were foundational integration problems that could make unrelated APIs appear broken. [Early review](../artifacts/reviews/code-review-20261013.md), [layout review](../artifacts/reviews/functionality-review-20261021.md)

A milestone on 20 October followed a user-reported five-second disconnect/reconnect: the applet remained installed and a complete 4,525-row support report matched its reference identities and declared support. That is real historical AC5 evidence. It does not validate later alias, owner and reclaimer changes.

The original estimate was 22–33 person-weeks, with two engineering streams. The much shorter recorded calendar interval cannot establish equivalent labor productivity: progress records mix implementation, unattended test runtime, repeated checks and metadata updates. Actual staffing and deduplicated engineering effort were not measured here.

<div style="page-break-after: always;"></div>

## Page 4 of 8 — Runtime correctness became the core engineering task

The most persistent runtime difficulty was translating between identity systems: imported Java Card class/method tokens, embedded ROM tokens, CAP offsets, applet AIDs and context-local object handles. Small mapping errors produced broad failures.

A recent example is DES `clearKey()`. Three key sizes failed in the static performance suite. The cause was an incomplete JC305 DESKey interface remap: inherited Key methods still followed the wrong token order. Correcting the map made the exact physical measurements pass. Earlier hypotheses about persistent-write cost did not explain this defect. The implication is that API compatibility needs systematic mapping tests, including inherited methods, rather than a sequence of isolated token patches. [API-03 status](../TODO.md#api-03)

Exception handling was equally foundational. JCAlgTest depends on catching Java Card exceptions to report unsupported algorithms. Wrong exception type, incorrect unwinding or a native failure escaping into a generic `6F00` corrupts the report even when unsupported behavior is legitimate. The work therefore added handler dispatch, VM-to-Java exceptions, lifecycle cleanup and tested error paths alongside successful execution.

Memory work progressed from repairing individual leaks and repeated reconstruction to typed root traversal and stable-slot collection. Roots had to cover statics, registered applets, operand/local values, exceptions, native mirrors, RMI and result state. Reclaiming only what one subsystem could see would have endangered objects retained elsewhere.

Multi-applet support revealed an additional design distinction: **package firewall context and applet-instance ownership are different concepts**. Sibling applets from one CAP need shared package state, while selection, transient clearing, service registries and DELETE must preserve instance-specific behavior. Physical tests on two boards demonstrate sibling static sharing, transient reset/deselect behavior and reinstall scenarios. Full ownership/lifecycle acceptance remains open. [CTX-03](../TODO.md#ctx-03)

Shareable Interface Objects (SIOs) made provenance unavoidable. Two packages can both use compact handle `0x0002` for different objects. A regression now demonstrates that exact collision and verifies unequal identities through owner tags. Foreign references may sometimes be retained or compared without permitting dereference. Proxies must also become unusable after reset, DELETE and same-AID reinstall.

The current implementation has positive cross-AID calls and several physical firewall checks, but foreign-reference persistence, typed arrays, roots and lifecycle coverage remain incomplete. Rejecting a value safely can still be an API compatibility gap: ordinary foreign references have permitted storage cases that are not all implemented. [CTX-01](../TODO.md#ctx-01)

Finally, successful API output does not guarantee correct security-related result state. Recent digest, cipher and signature fixes propagated `SensitiveResult` across SIO calls and cleared stale state on failure. This shows why cross-feature tests matter: an algorithm KAT alone would miss the defect. [CTX-02](../TODO.md#ctx-02)

<div style="page-break-after: always;"></div>

## Page 5 of 8 — Persistence and capacity are the largest coupled risk

Persistence grew from a bring-up obstacle into a subsystem with snapshots, object aliases, transactions, package metadata and recovery journals. These mechanisms solve different problems:

- Copy-on-write filesystem snapshots protect publication of a new durable record set.
- Transaction before-images support rollback of grouped persistent writes.
- CAP metadata mirrors and an INSTALL journal coordinate package publication with object/registry changes.
- Reachability tracing determines which object records may safely be deleted.

None substitutes for the others. A valid filesystem can still contain an inconsistent installation, and a consistent installation can still lose a live object through an incomplete root inventory. [Persistence contract](FILESYSTEM_ABI.md), [PERSIST-01–03](../TODO.md#persist-01)

Capacity failures also had several distinct causes. Early work reduced excessive SRAM use. Later overlay formats increased usable persistent data without assuming additional flash. Version 4 uses an occupancy bitmap and compact index entries within the existing 160 KiB snapshot slots. Its nominal 96 KiB payload budget shrinks as index occupancy grows; there is also a hard **8,192-record limit**.

The 1 November diagnosis linked nine Crypto `NO_MEMORY` outcomes to nine filesystem write failures. At 8,016 records, the dynamic payload ceiling was 94,592 bytes and only 19 bytes remained. Later, writes failed at the record-count limit even with some byte capacity left. This is strong evidence of overlay exhaustion for those failures, rather than a generic crypto allocation defect. It does not explain every historical memory error. [Capacity diagnosis](../progress.md)

Package-state compaction is implemented. A physical full-index case compacted from 8,192 records to 696 records / 16,749 bytes, after which applet installation succeeded. The remaining question is sustained behavior: repeated install/run/uninstall sweeps must distinguish reclaimable records from a genuinely full live set. [STORE-01](../TODO.md#store-01)

Persistent-object reclamation is more sensitive. A bounded primitive and owner-scoped root inventory exist; a test-only physical hook erased exactly one orphan while preserving a rooted object. Normal persistent deletion remains disabled. Required proof still includes multiple owners and actual interrupted reclaimer/power-loss behavior. Simulated 1,000-cut tests establish tested model behavior, not 1,000 physical power interruptions.

The host backend further limits inference. Its `fs_sync()` is a no-op; saved images are checkpointed on normal execution paths, with a separate registry sidecar. That pair is not atomically persisted against host power loss. Host restart tests are valuable, but cannot establish Pico flash durability.

The next storage milestone should therefore be **safe, bounded lifecycle recovery**, with repeatable workload capacity measurements. Enabling deletion merely to make a long sweep pass would trade a visible resource failure for a harder-to-detect data-loss risk.

<div style="page-break-after: always;"></div>

## Page 6 of 8 — Cryptography and transport: breadth with revealing failures

Crypto expanded from truthful unsupported factories and SHA-256 into native symmetric and asymmetric operations: AES/DES modes and MACs, digests, RSA/RSA-CRT key generation and operations, ECDSA, EC agreement, message recovery and precomputed-digest paths. Profiles select subsets through generated capability data. Coverage varies by algorithm, size and API form; this is not complete Java Card crypto support. [Declared profile](supported-profile.md)

The evidence shows increasing breadth:

| Evidence | Outcome | Limit of the conclusion |
| --- | --- | --- |
| Symmetric reference comparison | 41/41 target symmetric identities matched; physical KATs 71/71 | A defined comparison set, not all algorithms or security properties |
| Historical physical `max_crypto` support report | 4,525 rows: 198 yes, 4,327 no, no other statuses | Factory/support reporting, not correctness of every operation |
| Physical JCAlgTest 1.8.4 BASIC, 3 November | 4,446 yes/no results: 148 yes, 4,298 no | A different upstream version; counts are not directly comparable to 1.8.2 modes |
| Latest full Crypto VPCD sweep | 304 OK, 161 unsupported, one explained exception across 466 calls | Host backend; current physical equivalent remains open |

These outcomes are recorded in the [completed baseline and active acceptance work](../TODO.md). Counts should always retain their version, mode, profile and image context.

Two upstream-test limitations deserve management attention. JCAlgTest 1.8.2 has RSA/RSA-CRT rows labeled 3072 that actually request 4096. The project preserves the upstream CAP and reports the mismatch instead of enabling an unwanted size. Separately, a SHA-512 PSS vector uses a 1024-bit key too small for its default salt and digest encoding: the required 130 bytes exceed a 128-byte encoded message. Correct rejection should not become a firmware “fix.”

Transport defects repeatedly resembled crypto defects. An undersized response buffer truncated signatures. Exact-packet-size CCID replies needed a terminating zero-length packet. Long operations needed cooperative USB/watchdog service and CCID extensions, while retaining finite applet deadlines.

A particularly strong physical experiment ran RSA-3072 work for approximately 118 seconds while flooding USB control requests. All 4,336 requests started during the APDU succeeded, all 85 time extensions completed, and measured native-progress calls took at most 117 microseconds. That validates a specific instrumented path under load. [ARCH-08](../TODO.md#arch-08)

Long interpreted code needed separate attention: servicing progress every 4,096 bytecodes and raising the non-install limit to 120 seconds let the exact software-AES workload finish. The complete static suite on the final combined image still needs rerunning.

Correct results also do not establish side-channel or fault resistance. Input blinding and full RSA-CRT verification remain open hardening work despite existing exponent-blinding and scratch-cleanup efforts. [HARD-01](../TODO.md#hard-01)

<div style="page-break-after: always;"></div>

## Page 7 of 8 — Problems, learning and execution quality

**Layered diagnosis was essential.** “Reader missing,” “USB visible but no PC/SC registration,” “card activation fails” and “APDU returns an exception” require different investigations. Physical access varied across sessions, and alternate identified boards provided failover. The records do not establish one common cause for every reader disappearance. Continued offline work reduced dependence on that unstable test environment. [HOST-01](../TODO.md#host-01), [issue knowledge base](../.agent/QKB.md)

The Windows story illustrates why hypotheses must be revised. Earlier `certutil` MUTE reports suggested an activation problem. Later user-provided GPPro output demonstrated T=1 activation, ATR receipt, ISD SELECT, CPLC and Card Data on two reader slots. A subsequent provider-lookup error is a different integration layer. Basic Windows communication is evidenced; complete packaged-profile workflows remain unverified. That newer evidence supersedes a blanket “Windows transport is broken” assessment. [AC-01](../TODO.md#ac-01)

**Artifact identity became part of correctness.** Early JNI/ROM mismatches and later profile-specific builds showed that a green build or old physical result says little about a different image. Firmware also once exceeded its 1 MiB reserved window by 196 bytes. Size optimization and a build-time overlap gate addressed that class of error. Linked flash usage must be assessed directly; UF2 container size is not the same measurement.

**Long sweeps tested lifetime, not merely coverage.** An isolated API could pass after reboot while a full sweep exhausted records or retained objects. Focused reproductions were useful for identifying fixes; they could not close cumulative-pressure or final combined-image gates. Performance testing was particularly effective at exposing dispatch, persistence and scheduling defects.

**The operating discipline was strong, but expensive to navigate.** Small commits, append-only experiment records, explicit failure classifications, hashes and bounded logs made many conclusions traceable. Reviews found cross-cutting problems; the QKB preserved reusable fixes; handoffs helped sessions continue. Backlog compaction mapped historical unfinished items into stable task owners without discarding evidence. [Continuity rules](../.agent/SESSION_CONTINUITY.md), [backlog migration](../.agent/archive/2026-11-01-todo-pre-compaction/MIGRATION.md)

However, `progress.md` exceeds 21,000 lines, with out-of-order records and metadata follow-ups. Handoffs and historical guides sometimes retain superseded claims. The remedy is a compact release/evidence index over the existing history: exact image, profile, upstream versions, backend, tests passed and open gates. The active TODO is currently the best owner of acceptance status.

Viewed as a human team, the work spans embedded platform engineering, language-runtime engineering, crypto/API integration, storage/recovery and test/release engineering. Those are useful responsibility boundaries for future ownership; the records do not establish actual team composition or individual productivity.

The main execution tradeoff was breadth versus closure. The project repeatedly advanced capability, then needed consolidation to repair interactions. The next phase should reward closed acceptance gates and reproducibility rather than additional supported rows.

<div style="page-break-after: always;"></div>

## Page 8 of 8 — Largest unfinished parts and recommended next phase

The following ordering is a leadership recommendation based on dependencies and consequence. It complements the active task priorities rather than redefining their acceptance criteria.

| Priority and owner | Unfinished outcome | Concrete completion evidence |
| --- | --- | --- |
| 1. Context/firewall correctness — CTX-01/03 | Complete owner-aware references, permitted foreign stores, typed compatibility, GC roots and reset/DELETE/reinstall behavior | Cross-package and same-CAP tests, including equal-handle collisions and lifecycle transitions, on the nominated physical image |
| 1. Persistent safety and bounded capacity — STORE-01, PERSIST-01/02/03 | Demonstrate repeated cleanup, cold restoration and interrupted cross-store/reclaimer recovery | Controlled physical power removal/cuts; live objects and unrelated owners preserved; consistent old/new install recovery; storage telemetry |
| 2. Current-image acceptance — API-01/03, PROFILE-01, CTX-02 | Complete Security/Crypto coverage, result-state producers and error paths after combined fixes | Method/behavior/evidence matrix; full bounded sweeps; all unexpected results explained; successful cleanup |
| 2. Release reproducibility — AC-01/02, HOST-01 | Finish Windows profile workflows and three consecutive complete integration passes | Per-run image hashes, explicit CAP installation, complete report comparisons and post-power-removal execution |
| 3. Parser and private-operation hardening — HARD-01 | Broaden malformed-CAP validation and complete selected crypto protections | Current-source threat model, negative/fuzz tests and physical parser checks; defined generated/imported-key limits |
| 3. Card-manager extensions — GP-01 | Close remaining GET DATA encodings, continuations and registry recovery cases | Exact responses across VPCD and physical CCID, authenticated/open sessions, restart and cleanup |

A practical next phase would have three milestones:

1. **Nominate a candidate image and evidence manifest.** Identify the exact source state, capability profile, CAP/client versions and available boards. Preserve unrelated worktree changes. Separate existing-image evidence from source changes that still need flashing.
2. **Close the coupled context/storage blockers.** Finish owner-aware reference paths and their root/lifecycle audit. Execute a planned physical recovery campaign before enabling persistent reclamation. Repeat install/run/delete workloads until remaining capacity limits are explicit and stable.
3. **Run release acceptance without intervening feature changes.** Complete the final static-performance suite, current API/profile runs, Windows workflows and the plan's three full integration passes. Every pass should retain image and board identity, complete results and cleanup/recovery evidence.

No defensible completion date can be derived from the logs alone. Physical availability is an external dependency; the remaining reference and recovery work crosses several subsystems. Estimate after breaking those acceptance campaigns into bounded experiments and identifying which require controlled power hardware.

Keep extended APDUs, general logical channels, a Rust rewrite, new language backends and broader platform ambitions outside this consolidation phase unless the product requirement changes. Hardware isolation and certification would require separately scoped work and evidence.

The investment decision is to finish a reliable, explicitly limited Java Card platform around the demonstrated core. Its value will increase most when a new engineer can reproduce the same behavior on a known image, and when reset, deletion, resource exhaustion and interrupted writes preserve the promised contracts.
