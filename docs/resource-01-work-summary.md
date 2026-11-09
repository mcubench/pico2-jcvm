# RESOURCE-01: investigation, effort and lessons learned

Repository review as of **9 November 2026**, through root commit `c3e603c3`.
Five editorial pages follow, separated by explicit page breaks. Printed pagination
may vary with the Markdown renderer. This is a historical analysis; it introduces
no implementation changes and reports no newly executed runtime tests.

## Page 1 of 5 — The problem and the shape of the effort

**RESOURCE-01 is still open.** The work has established persistent orphan growth,
made experimental collection safer, and demonstrated bounded cleanup in selected
workloads. It has not established production reclamation, a reliable full
`max_crypto` JCAlgTest run, or the cause of physical resets.

The [backlog](../TODO.md#resource-01) asks for two related outcomes: stop long-run
`NO_RESOURCE` failures and establish reset causality. The distinction matters.
The original 7 November unchanged JCAlgTest 1.8.4/jc305 VPCD run reached RSA CRT
3072, then returned `F205` after 797,682 ms with persistent-reference serialization
failing from filesystem `OutOfFlash`. That is host storage-failure evidence;
it does not prove that the Pico reset or that its different storage backend has
the same limit.

A second obstacle was already visible in native JCSystem dispatch: production
object deletion was unsupported, and `requestObjectDeletion()` threw reason 6
(`F206`). In the supplied JCAlgTest cleanup sequence, that exception prevented
later RSA-cache cleanup. Thus the investigation concerned an entire lifecycle:
creation, reference replacement, cleanup requests, graph tracing, record deletion,
and filesystem reuse. Reducing one allocation could delay exhaustion without
making that lifecycle bounded.

Commit `068119b6` made this the highest priority on 8 November, 06:27–06:29 CET
(**2 recorded minutes**). Commit `8c4c323a` saved the six-stage plan at
07:07–07:09 (**2 minutes**): measure growth; complete safe persistent collection;
defer deletion until after applet execution; handle replacement and failed
construction; reduce key reuse/write costs; demonstrate long-run stability.
The actual work moved between these stages as experiments exposed safety gaps.
It did not complete them sequentially.

### How much time is supported by the records?

The following accounting sums the duration in each qualifying `progress.md`
heading, rather than inferring effort from commit timestamps.

| Explicit RESOURCE-01 records | Records | Reported minutes | Reported hours |
| --- | ---: | ---: | ---: |
| 8 November | 32 | 674 | 11 h 14 min |
| 9 November | 21 | 609 | 10 h 09 min |
| Total | 53 | **1,283** | **21 h 23 min** |

Selection rule: 8–9 November records whose `Step/experiment` begins with
`RESOURCE-01` or `resource01`, case-insensitively, including commit-reference
bookkeeping. This excludes related PERSIST-01, host-helper and release records,
as well as the 7 November precursor investigation. It is therefore a defined
subtotal, not all project effort attributable to resources.

The main recorded interval spans **36 h 19 min**, from 8 November 06:27 to
9 November 18:46 CET. That is calendar elapsed time, not continuous labor.
Some experiment windows overlap; the 141-minute full-suite record overlaps the
later telemetry and batch-control windows. Several headings also differ slightly
from their start/end arithmetic. The 21 h 23 min is a sum of reported durations,
not an independently audited person-hour total. Long client waits, builds and
compaction are included where the reports include them.

The important progression is from “resources run out” to three separable
questions: which objects become unreachable, whether deleting them preserves
all live roots, and whether the filesystem can reuse storage fast and reliably
enough for the complete workload.

<div style="page-break-after: always;"></div>

## Page 2 of 5 — Establishing what grows, 8 November morning

### Early optimizations reduced pressure but did not close the lifecycle

Between **07:25 and 08:46 CET**, the main recorded steps took **51 minutes**:
transient lifecycle regression 10, RSA component allocation 17, global inventory
12, and redundant EC initialization removal 12. Four separate one-minute
reference records documented their commits.

Commit `7525b67b` checked retained AES/RSA Cipher behavior after deselection and
reset: the focused VPCD suite passed 99 checks, including 16 transient lifecycle
checks. This established a lifecycle control before allocation changes, with
physical behavior still unverified.

Commit `508a043c` allocated only the RSA components required by each key type.
Public/plain private keys use modulus and exponent arrays; CRT keys use their
five CRT components. The narrowed component suite passed 102 checks and both
architecture wrappers built. For a 3072-bit public-plus-CRT pair, structural
arithmetic predicted **1,728 payload bytes and seven array records saved**.
That was a capacity calculation, not a measured flash saving. An exploratory
CRT sign/verify vector had failed separately; the narrower passing result did
not establish correctness of that unrelated path.

Commit `54eb08c2` removed repeated EC per-byte zero writes because backing
allocation was already value-initialized. Focused calls passed, but the first
and repeated timings were not a controlled before/after benchmark. The full
client still encountered communication failure. These optimizations were useful
pressure reductions, not evidence of bounded resource consumption.

### Measurement changed the diagnosis

Commit `04c862a9` added global persistent counts because owner-local accounting
missed package-owned graphs. A three-key probe grew from **6 records / 34 payload
bytes** to **43 / 800**. These totals include fixture and APDU overhead; they
are neither physical flash occupancy nor isolated per-key costs.

The **08:46–11:13** investigation took **147 recorded minutes**. Its complete
client attempt stopped at RSA KeyPair 1024, but the same operation on a fresh
card passed in **49.281 seconds**. Ten repeated RSA-1024 public-key constructions
left **395 global records / 10,894 payload bytes**, including **91 unreachable
tags / 3,622 bytes**. Commit `4e85e66a` records this diagnosis. This shifted the
question from “is RSA-1024 unsupported?” toward cumulative workload effects.
It did not yet prove the cause of the client's empty response.

The next sequence took **89 minutes**: distinguish RAM from persistence (6),
align PC RAM collection with firmware (52), and repeat the full client (31).
Commits `0bf69acd`, `a01808de` and `37048fb2` capture this progression. RAM heap
collection frees handles/backing; it does not erase durable body records. After
cadence parity, RSA-768 still returned **F205 after 442,532 ms**, with
`PERSIST_EXCEPTION id=20`.

This was a valuable negative result. It removed a host-versus-firmware RAM-GC
confounder and showed that improving RAM cleanup alone did not resolve durable
storage exhaustion. The failure size also moved between runs, so key size was
not a reliable standalone explanation.

**Lesson:** account separately for RAM handles, reachable durable objects,
unreachable durable records, and obsolete filesystem versions. A healthy value
in one category cannot establish that the others are bounded.

<div style="page-break-after: always;"></div>

## Page 3 of 5 — Making reclamation safe and observable, 8 November afternoon/evening

### Turning collection on exposed a correctness problem

From **12:55 to 15:56 CET**, five focused steps consumed **156 recorded minutes**:
multipackage isolation 50, root-prefix protection 42, transient-failure
reattribution 25, SELECT diagnostics 23, and registered-root protection 16.
Commits `0457498d`, `80e1f6f7`, `82c1fed2`, `a41a526f` and `60148e50` show why
“enable the existing collector” was insufficient.

A temporary post-APDU collector caused a multipackage reset/reselection failure;
the collector-disabled control passed 47/47. Initial prefix protection helped,
but later diagnostics identified erased tag **`03023900FF`**, a registered applet
instance root. That root was not represented by a currently reachable heap
object. Protecting only paths reached from the current heap could therefore
remove the durable entry needed to restore the applet.

The eventual guard excluded valid registration-root records directly. SELECT
instrumentation also showed that a later `6999` was a downstream symptom of an
unchecked failed selection. Passing repeat runs corrected an earlier suspicion
that a transient-key failure alone implicated collection.

At **18:22–18:45**, a **23-minute** reconciliation (`b58b9d9f`) recorded a more
encouraging experimental full run: **187 successful collections, 1,580 erased
tags**, reaching RSA CRT 4096 without F205. It still hit the **3,600-second suite
limit** during KeyAgreement. Temporary hooks were removed; this was incomplete
acceptance, not a production fix.

Related PERSIST-01 commits `240ef2e5` and `e7742cb4` added foreign-alias root
protection and malformed-alias refusal. Their two four-minute steps sit outside
the explicit RESOURCE-01 time subtotal. Multipackage tests subsequently passed
47/47, but passing compatibility tests did not cover every possible live root.

### Package statics explained an accounting blind spot

An **8-minute** diagnostic step (`e8edb6d5`) added exact failed-write tags; a
**5-minute** artifact refresh (`369f5de6`) prepared the corresponding image.
Neither supplied physical acceptance.

From **20:32 to 21:58**, five steps took **82 recorded minutes**: tag diagnosis
11, package-static primitive 24, active static-root tracing 7, post-APDU probe 23,
and a partial full-suite experiment 17.

Commit `8ffba182` showed eight RSA replacements growing global inventory from
**6 / 34** to **87 / 1,797**, while owner-only inventory stayed **2 / 17** with
zero unreachable records. Growth occurred under package-static prefix `023808`.
Thus an owner-only collector could report success while missing the actual
retention domain.

Commit `4e6b0b5b` added a package-static reclaimer with preflight, alias checks,
transaction refusal and retry behavior. Commit `e3e01b89` traced active static
roots; **70 records / 1,533 bytes** still qualified as unreachable. With opt-in
collection (`f0b91c71`), the eight-replacement probe stayed at **27 records /
483 bytes before cleanup**, instead of reaching 87 / 1,797.

However, scanning after every APDU was expensive and reclaimed nothing in the
first 76 rows of the next full run (`2493dff3`). That experiment was stopped.
Its induced emulator panic was not evidence of a product reset. This directly
motivated servicing explicit deletion requests after the applet clears references
and returns, as the original plan required.

**Lesson:** collection needs complete durable-root knowledge and appropriate
timing. An early scan can preserve objects that the applet has not cleared yet;
an incomplete scan can destroy objects required for restart.

<div style="page-break-after: always;"></div>

## Page 4 of 5 — From a request latch to bounded controls, overnight and 9 November morning

### Proving the mechanism in progressively stronger fixtures

The **21:58–00:11** request/reclamation sequence took **131 recorded minutes**:
request latch 71, retained-context guard 12, 64-cycle stress 7, RSA graph stress 8,
static-root preservation 7, deferred RSA batch 15, and RSA-4096 coverage 11.

Commits `2623829e` and `376334a1` introduced a PC-only test gate and a conservative
eligibility rule: another retained Context prevents collection. A request is
serviced at the post-APDU boundary, after reference clearing. This established a
usable experiment without advertising production deletion support.

The later controls tested different properties rather than merely repeating a
passing test:

| Commit | Control | Contribution to RESOURCE-01 |
| --- | --- | --- |
| `87f30150` | 64 byte-array replacements, 131/131 checks | Repeated request delivery and exact erasures |
| `bc6d3042` | Repeated RSA graph cleanup | Multiple records per key can be reclaimed |
| `c60a9d34` | Keep a package-static RSA reference live, then clear it | Collection follows reachability |
| `63f7cd17` | 32-key batch, 37/37 checks, 320 owner tags erased | Deferred cleanup handles accumulated graphs |
| `a0023fea` | Four RSA-4096 cycles, 11/11 checks | Larger key material remains reclaimable |

Physical preparation took **23 minutes** across `662aff8c`, `bb3cb62c` and
`81af6f84`. It produced a diagnostic image, RSA replacement harness and stale
helper refusal. The installed helper did not match the repository's Pico-only
allowlist; an unrelated PacketRF device shared the USB VID/PID. Physical testing
therefore remained blocked. Prepared firmware and VPCD controls are not on-board
results; the external action is recorded in [.agent/EXT_REQUESTS.md](../.agent/EXT_REQUESTS.md).

PERSIST-01 contributed additional safety evidence outside this subtotal: simulated
interrupted erases, rooted graphs, transaction rollback, and **1,000 simulated
RP2350 flash cuts** preserving live/other-owner data with retry. These strengthen
the safety argument but do not replace physical interruption tests.

### Bounded probes and full runs answered different questions

Periodic sampling (`23318302`, **38 minutes**) made long runs observable without
scanning every APDU. The **26-minute** full-run record (`7475d43e`) captured
**1,691 owner records / 43,697 bytes**, including **1,485 unreachable tags /
39,293 bytes**, before RSA-1536 returned F205 in **44.678 seconds**. The following
Ctrl-C-induced emulator failure was corrected in `3418ba9c` (**15 minutes**).
The F205 was valid; the induced panic did not prove reset causality.

Filesystem controls (`0bfc69e8`, **35 minutes**) passed normal tests, two
100,000-operation stress cases, and a 100-cycle multi-record/root workload.
A 64-cycle gated RSA probe passed 131 checks and repeatedly returned to
**13 applet records / 287 bytes**, with zero unreachable tags. This established
an isolated plateau, not a mixed-workload plateau.

An **11-minute** 512-cycle attempt timed out; making its APDU deadline configurable
took **5 minutes** (`3ffd9063`, `8bf085b9`). A smaller 16-cycle run passed;
one APDU took **30.138 seconds**, explaining why a 20-second deadline was too
short. A **14-minute** safety/build revalidation followed (`58fdf85b`).

Finally, `6d1e4f87` (**20 minutes**) recorded a clean current-profile run on
Virtual PCD 00 01: **8,607 matched rows, 15 yes, 8,592 no**. Production cleanup
still returned F206. The comparator invocation was fixed, but this lower-capability
pass did not reproduce the heavier max_crypto workload or demonstrate a storage
plateau. Endpoint and profile differences prevent treating it as a contradiction
of earlier failures.

<div style="page-break-after: always;"></div>

## Page 5 of 5 — The remaining failure and what the investigation has taught

### The focus moved from orphan removal to mixed-workload storage reuse

On 9 November, **12:41–13:55 CET**, three steps took **73 recorded minutes**:
max-profile stall capture 18 (`646b864f`), watchdog/inventory tooling 34
(`771e63c5`), and HMAC reproduction 21 (`72e6d17a`). Denser inventories found
**1,205 unreachable tags / 31,791 bytes** among **1,418 owner records**.
Some `FS_OUT_OF_FLASH` notices preceded successful EC operations: the notice is
emitted before automatic compaction, so it alone does not prove terminal failure.
The watchdog then bounded the otherwise unanswered APDU without interrupting
the emulator itself.

That APDU mapped to HMAC `TYPE_HMAC_TRANSIENT_DESELECT`. Repeating it added
**8 unreachable records / 93 bytes**; a test-gated cache clear removed exactly
those records once they became unreachable. This directly established one
persistent-churn mechanism. It did not prove that this mechanism alone caused
the historical RSA F205.

Sibling-root coverage (`c23a8a7f`, **11 minutes**, 14:10–14:21) allowed siblings
sharing one Context while preserving their roots; the integration probe passed
10/10 and erased one unreachable package record. Other retained Contexts remained
outside the supported experimental scope.

The **141-minute** full-suite-versus-isolated investigation, reported as
14:25–16:47, is the decisive remaining counterexample. The unchanged max_crypto
suite logged **36 F205/persistent-exception events despite 237 complete cleanup
calls**, and hit its 3,600-second watchdog with only 315 CSV rows. Meanwhile,
fresh-card RSA-768/896/1024 controls succeeded. A repeated RSA-768 control passed
34 checks over 16 cycles and erased **377 tags**; its two sampled inventories
were identical before cleanup. Thus isolated collection can plateau while the
mixed full suite still fails. These records were committed with `07c25870`.

Sector snapshots suggested a compaction/recovery problem: append pointers near
capacity despite low valid sizes and a nearly full 128-KiB staging sector.
Host-only telemetry took **8 minutes**, plus **1 minute** of commit bookkeeping
(`07c25870`, OS submodule `b892e4d`, reference `6ddcdfd0`). This was a hypothesis
and instrumentation step, not a repair.

The latest **162-minute** control, 16:04–18:46 (`c3e603c3`), then corrected the
suspected stuck-copyback explanation for a synthetic RSA batch. With a 180-second
APDU limit, 64 replacements passed **69 checks**, one cleanup erased **640 owner
tags**, and all **six defragmentations completed**. Compaction-triggering APDUs
took **55.068, 74.775, 91.273, 93.014, 102.237 and 101.436 seconds**. An earlier
60-second timeout had interrupted slow progress. This does not explain the full
suite's F205; it shows that this bounded compaction path can finish.

### Lessons and the remaining acceptance gap

1. **Reducing allocations is different from bounding lifetime.** RSA/EC changes
   reduce pressure; unreachable graphs still need safe reclamation.
2. **A collector is only as safe as its roots.** Registered instances, package
   statics, aliases and retained contexts require explicit coverage. The root
   deletion failure justified keeping production collection disabled.
3. **Record counts are not physical occupancy.** Low live payload can coexist
   with obsolete versions, full append regions and staging pressure.
4. **Use fresh-card controls and measured deadlines.** They separated supported
   RSA operations from cumulative failures and slow compaction from apparent hangs.
5. **A passing fixture has a limited claim.** Single-context plateaus, simulated
   cuts and current-profile support reports do not establish full max-profile or
   Pico acceptance.

The next evidence needed is a smaller mixed workload reproducing the failing
full-suite sector state, followed by a demonstrated explanation of the first
terminal allocation failure. Production enablement additionally requires complete
root/context safety, transaction and interrupted-flash acceptance, repeated
unchanged-client resource plateaus, and identified Pico hardware validation.
Physical reset causality remains a separate open question.

### Evidence and audit notes

Primary sources are [progress.md](../progress.md), the [implementation plan](resource-01-implementation-plan.md),
[TODO](../TODO.md), [QKB](../.agent/QKB.md), and Git commit contents. Representative
raw evidence: [initial accumulation](../artifacts/logs/jcalgtest-resource-accumulation-10x-rsa1024.log),
[package-static control](../artifacts/logs/resource-package-static-reclaim-vpcd-20261108.log),
[full mixed run](../artifacts/results/jcalgtest-support-vpcd-20261109T132549Z/), and
[completed batch compaction](../artifacts/logs/object-deletion-vpcd-20261109T173110Z.log).

Commit history resolves stale report fields: the early cumulative-key record
incorrectly names `23318302`, which actually adds periodic sampling; its diagnosis
is in `4e85e66a`. Later `pending` records resolve through `07c25870` and `c3e603c3`.
The latest handoff heading retains a 16:04 timestamp despite reporting later
results, so the dated experiment and commit sequence take precedence. Hashes
copied into reports were not independently recomputed in this review. Existing
uncommitted source changes were preserved and are not treated as committed fixes.
