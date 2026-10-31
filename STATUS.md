# Project status

Updated: 2026-10-31

## Summary

CHOUPI is an experimental Java Card VM port targeting the Raspberry Pi Pico 2
(RP2350, Cortex-M33). It runs the JCAlgTest 1.8.2 JC305 applet on the PC/VPCD
path and implements a tested subset of Java Card runtime behavior. It is **not
a complete or Java Card certified VM**. Unsupported algorithms may correctly
report `NO_SUCH_ALGORITHM`; implementing every algorithm is not a project goal.
The physical deployment target is ARM Cortex-M33. ARM, RISC-V, and PC build
wrappers pass; RISC-V has not been tested on hardware. On 2026-10-27, D92
`D92E743D5F640C0F` was flashed with the combined `asymm_crypto` ARM image and
passed the finite runtime monitor, 53/53 selected RSA-signature and EC-key
storage vectors, and 71/71 symmetric crypto known-answer vectors over PC/SC.
The combined profile also passed the full 79-vector asymmetric API suite and
the complete physical JCAlgTest report (4,525 rows; 196 yes, 4,329 no, zero
other). Two expected KeyPair-3072 results remain mismatched because the
unchanged upstream CAP sends 4096 as the requested length for those rows;
direct 3072-bit RSA and RSA-CRT generation vectors pass. The profile is not
fully accepted until physical `/deploy` coverage completes and this upstream
report limitation remains explicit. Pico 2 `2BED26ED20556535` was last
confirmed running `symm_crypto` with an ISD-only GPPro listing on 2026-10-25.
Firmware images do not preload JCAlgTest, and reflashing preserves the separate
persistent CAP store; install CAPs explicitly with GPPro after confirming the
card registry is empty.

## Verified

- **Asymmetric algorithm profile parity with JCOP4 P71 (2026-10-30):** The
  current generated `asymm_crypto` manifest has 145 entries and matches all 136
  shared positive identities in the supplied target matrix. The full physical
  8E report has 4,525 rows and zero non-yes/no statuses; its generated-manifest
  comparison reports 143/145 because the unchanged upstream CAP sends 4096 for
  two RSA-3072 KeyPair queries. Independent RSA-3072 and RSA-CRT-3072 physical
  generation vectors pass. This known CAP query limitation and remaining
  private-operation hardening are separate from profile parity; see QKB.

- **Current max_crypto physical runtime and API Core check (2026-10-31):** Pico
  2 `2BED26ED20556535` was flashed with the 2,088,448-byte ARM image
  (`3331edcb99400e774ad3e7678d0626fd9ab3986bde163077226a552dbe198213`). Its
  finite runtime monitor and physical GPPro listing passed. The direct SCP03
  GET STATUS test passed with three applet instances and a short Le. Upstream
  API 3.0.5 Core completed 291 calls: 272 `OK`, 15 spec-allowed unsupported,
  and four optional Ext methods unavailable because the Ext CAP was refused
  with `6A88`; the partial Ext package, Multi CAP, and Core CAP were cleaned
  up. A final GPPro listing showed only ISD and the existing JCAlgTest applet.
  Security and Crypto from the same deploy suite still need a current-image
  rerun. The result CSV is ignored by Git at
  `artifacts/results/deploy/pico2-2BED-max_crypto-core-20261031.csv` (SHA-256
  `fdcfbb8423d30df092f2fca7a8045472ebb81feb9f70dabc9173fb8688759a6c`).

- **Current max_crypto API Security check (2026-10-31):** The same physical Pico
  completed all 901 upstream API calls: 302 `OK`, 504 `UNSUPPORTED_PER_SPEC`,
  94 `NO_MEMORY`, and one `WRONG_RESULT`. `NO_MEMORY` starts at call 460 and
  spans key families and EC accessors; one AES transient-reset state check
  returned the unexpected uninitialized value. The runner removed the Security
  CAP successfully. Treat resource exhaustion and transient reset semantics as
  open findings pending diagnosis. CSV SHA-256 is
  `c6ba9233e27584280bd5632b1ca281509fedd2cc19c08fbd4184b01179694045` under
  ignored `artifacts/results/deploy/`.

- **Current max_crypto API Crypto check (2026-10-31):** The same Pico completed
  all 466 upstream calls with the documented RSA-2048 key-generation exclusion:
  263 `OK`, 161 `UNSUPPORTED_PER_SPEC`, 38 `NO_MEMORY`, three `WRONG_RESULT`,
  and one `EXC_CARDRUNTIME`. `NO_MEMORY` affected Signature, KeyAgreement,
  Cipher, and AEAD; three Signature IV known-answer checks returned unexpected
  values, and one RSA-PSS case raised `ILLEGAL_USE`. Cleanup passed, and a final
  exact-reader listing showed only ISD and the existing JCAlgTest applet/package.
  CSV SHA-256:
  `3a7859bb1f8283e1ce88578ea601aa3c7d39acd5d448a4526506a3c6e9593c10`.

- **Symmetric algorithm parity with JCOP4 P71 (2026-10-30):** The supplied
  1.7.9 reference has 41 symmetric support identities. All 41 are selected by
  `symm_crypto` and all 41 were reported on physical Pico 2
  `2BED26ED20556535`; there are zero missing or extra entries. Separate
  physical symmetric known-answer coverage passed 71/71. The exact set
  comparison and hash are in `.agent/QKB.md`. This does not infer AES-CTR
  support because the target CSV has no such row.

- **Physical `symm_crypto` support report (2026-10-30):** On Pico 2
  `2BED26ED20556535`, the unchanged JCAlgTest 1.8.2 CAP completed its full
  4,525-row `ALG_SUPPORT_EXTENDED` matrix. The generated profile comparison
  matched all 46 declared positives (zero unexpected or missing); the report
  contained 73 yes, 4,452 no, and zero other values. The full report, client
  transcript, comparator output, and post-run serial log are recorded in
  `.agent/QKB.md`. This complements the `asymm_crypto` report below.

- **Physical API 3.0.5 deploy baseline on combined `asymm_crypto` (2026-10-28):**
  D92 completed unchanged Core/Security/Crypto CAP scenarios and all temporary
  applets/packages were cleaned up. Core: 291 calls, 271 OK, 15 expected
  unsupported, four optional Ext scenarios unavailable, one existing
  `Applet.deselect()` wrong result. Security: 901 calls, 348 OK, 504 expected
  unsupported, 34 `6F00`, 12 wrong results, one runtime exception, two
  skipped. Crypto: 466 calls, 275 OK, 163 expected unsupported, 14 `6F00`,
  14 exception-reason mismatches. Constructor `6F00` and marker-interface
  wrong values need native VM diagnosis; this run is a baseline, not a passing
  acceptance result. CSV and issue details are in TODO and HANDOFF.

- **Physical `asymm_crypto` crypto and support-report validation
  (2026-10-28):** Both
  architecture wrapper builds passed, and the ARM image was flashed and
  verified on D92 `D92E743D5F640C0F`. The finite monitor passed. The physical
  PC/SC asymmetric known-answer suite passed 79/79, including RSA signatures,
  ECDSA, key agreement, and RSA/EC generation; separate 53/53 RSA-signature
  and EC-key-storage plus 71/71 symmetric KAT suites also passed. GPPro
  cleanup removed each temporary probe package. The full JCAlgTest 1.8.2
  support report returned 196 yes, 4,329 no, zero other across all 4,525 rows.
  Its only generated-manifest difference is the two upstream-mislabeled
  RSA/RSA-CRT KeyPair 3072 probes; details are at the top of TODO.md and in
  `.agent/QKB.md`.
  Logs: `artifacts/logs/asymm-crypto-pcsc-20261027T080334Z.log`,
  `artifacts/logs/symm-crypto-kat-pcsc-20261027T195919Z.log`, and
  `artifacts/logs/asymm-crypto-pcsc-20261028T191018Z.log`. Full physical
  `/deploy` acceptance, Windows validation, and RSA private-operation
  hardening remain open.

- **Empty-card `symm_crypto` reflash and physical registry check (2026-10-25):**
  Rebuilt ARM and RISC-V with `./tools/cycle choupi --profile symm_crypto`
  and flashed 2BED `2BED26ED20556535`. The write verified at 100%; the finite
  18-second monitor repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. GPPro first found the persistent JCAlgTest
  applet/package from an earlier explicit installation. Deleted the applet
  AID, then its package AID, and verified the final list contains only ISD.
  Image: `artifacts/firmware/pico2-jcvm-symm_crypto-empty-card.uf2`, 1,676,800
  bytes, SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`.
  Cycle log: `artifacts/logs/cycle-choupi-20261025T084539Z.log`; final GPPro
  transcript: `artifacts/logs/gppro-symm-empty-card-2BED-20261025.log`.

- **Reader LED indication (2026-10-25):** The ARM firmware now breathes the
  Pico 2 onboard GPIO 25 LED for 1.6 seconds every 10 seconds while the CCID
  reader interface is active. Applet APDUs produce a 120 ms full-brightness
  pulse, timed by a hardware alarm so long VM execution cannot stretch it.
  ARM/RISC-V build gates and a physical 2BED flash/runtime cycle passed. The
  upstream API 3.0.5 Core CAP completed 291 physical calls and was removed;
  GPPro then listed only ISD. The firmware/log hashes and exact validation are
  in `progress.md`. The actual light waveform was not instrumented optically.

- **Current-image symmetric API coverage (2026-10-25):** On the same 2BED ARM
  image, upstream API 3.0.5 Security completed 901 calls (97 `OK`, 802
  `UNSUPPORTED_PER_SPEC`, two deliberately excluded reader-risky RSA-2048
  generation cases); Crypto completed 466 calls (161 `OK`, 305
  `UNSUPPORTED_PER_SPEC`). No unexpected status occurred. Both CAPs cleaned up
  successfully and a final GPPro listing showed only ISD. See `progress.md`.

- **JCSystem commit capacity (2026-10-25):** The native transaction suite
  passes, and the complete host VPCD JCSystem regression passes 85/85. It
  reports maximum/unused capacity `0x7FFF` outside transactions and 32755
  unused bytes after a persistent scalar update. ARM and RISC-V `symm_crypto`
  builds pass; the final ARM image is 1,676,800 bytes with SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`.
  After flashing this exact image to 2BED, upstream Core 3.0.5 passed 291
  calls (272 OK, 15 allowed unsupported, four unavailable optional Ext
  scenario calls). Its physical JCSystem subset had 47 OK and one
  spec-permitted `requestObjectDeletion()` unsupported result; the commit
  capacity probes returned `0x7FFF` outside a transaction and transaction
  commit/abort probes passed. A focused physical transaction CAP test then
  measured `32755` unused bytes after one persistent scalar update and
  confirmed abort/commit restoration. A dedicated physical CAP caught
  `TransactionException.BUFFER_FULL` (reason `3`) at full capacity and verified
  the rejected byte remained unchanged; all six assertions passed, and cleanup
  left only ISD. Focused VPCD abort/overflow cases passed. ARM and RISC-V
  `symm_crypto` builds passed; the matching ARM image was flashed to 2BED and
  passed its finite runtime monitor. A prior overflow attempt left the runtime
  unresponsive to APDUs; a full firmware reboot recovered it, while PC/SC reset
  alone did not. Persistent transaction recovery across a power cycle remains
  open. Details and recovery steps are in `progress.md` and `.agent/QKB.md`.

- **Empty-card symmetric firmware flash (2026-10-25):** Built the ARM
  `symm_crypto` CHOUPI image, passed both ARM and RISC-V build gates, and
  flashed physical D92 `D92E743D5F640C0F`. Its finite monitor reported repeated
  `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, and `CYCLE:PASS`.
  GPPro `--list` afterward showed only ISD `A000000151000000`; no JCAlgTest
  package or applet is installed. Image:
  `build/arm/pico2_choupi.uf2` (1,675,776 bytes, SHA-256
  `666028ffdf62cf0038ed4e082efefdce8e3954882d3278cb7903d8a6374d51f2`).
  Cycle log: `artifacts/logs/cycle-choupi-20261025T001428Z.log`.

- **Selectable capability profile acceptance (2026-10-24):** Current (15),
  no-AES (10), and minimal (0) each passed ARM and RISC-V build wrappers;
  each ARM image passed a finite physical monitor on Pico D92
  (`D92E743D5F640C0F`). The official upstream JCAlgTest CAP was left unchanged
  (SHA-256 `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`).
  Its full current-profile report matched 15 yes / 4,510 no across 4,525 rows.
  The upstream applet returns `F103` on no-AES startup; per user direction,
  minimal was not run through JCAlgTest. Profile behavior was checked with
  `/deploy` CAPs: Core, Security, and Crypto have now run on all profiles.
  After the `checkcast` fix, Core on current and no-AES each completed 291 calls
  with 272 OK, 15 per-spec unsupported, four optional Ext calls unavailable,
  and all OwnerPINx calls passing. Current Security completed 901 calls (77
  OK, 822 unsupported, two dependent transient-key follow-up wrong-result rows);
  current Crypto completed 466 calls (22 OK, 444 unsupported); minimal Crypto
  completed 466 calls (4 OK, 462 unsupported). Its earlier `6F00` install
  failure was not reproduced after cleanup. Every completed test cleaned up;
  final GPPro list showed only the ISD. Minimal was not tested with JCAlgTest,
  per user direction. Details are in `progress.md` and `.agent/HANDOFF.md`.
  Active ROM masks are 36,574 / 36,169 / 32,491 bytes and UF2 images are
  1,658,880 / 1,656,832 / 1,633,280 bytes for current / no-AES / minimal.

- Named `base` and `current` profiles were compared on physical Pico D92
  (`D92E743D5F640C0F`) using the supplied JCAlgTest 1.8.2 JC305 CAP and the
  complete `ALG_SUPPORT_EXTENDED` operation. Both produced 4,525 valid yes/no
  rows matching their generated support sets: base 10 yes / 4,515 no, current
  15 yes / 4,510 no. Current adds AES-192, AES-256, DES, DES3-2KEY, and
  SHA-256. Both ARM/RISC-V build gates passed and both flashed ARM images
  passed finite runtime monitors. See
  `artifacts/results/jcalgtest-profile-comparison-20261024.md` for exact
  per-profile rows and runtime notes. This supersedes the earlier seven-flag
  base trial, whose disabled OwnerPINBuilder rows were exception strings.
- Duplicate JCAlgTest CAP LOAD on D92 was reproduced and now returns `6A80`
  instead of misleading `6D00`; the existing package stayed installed and its
  applet still passed SELECT, GETVERSION, and JCSystem probes. JCAlgTest's
  repeated `B0 E2` reset APDU still returns `6F00` because the applet does not
  catch the documented `SystemException.ILLEGAL_USE` from unsupported
  `JCSystem.requestObjectDeletion()`; following support-result rows remain
  valid. Details are in `.agent/QKB.md`.

- Current-source physical JC305 deploy coverage on D92/2BED: Crypto completed
  466 calls (22 OK, 444 expected unsupported, zero unexpected statuses);
  Security completed 901 calls (77 OK, 820 expected unsupported, two excluded
  RSA-2048 cases, and two follow-up transient-key results classified in QKB);
  Core completed 291 calls (272 OK, 15 expected unsupported, four Ext methods
  unavailable after optional INSTALL returned `6A88` and rollback succeeded).
  AES CBC streaming and a 50-call static `doFinal()` probe passed physically.
  Results are in `artifacts/results/deploy/crypto-cipher-inplace-final-d92-20261022.csv`,
  `artifacts/results/deploy/current-source-sec-core-post-cipher-20261022/`, and
  `artifacts/logs/aes-cipher-static-final-20261022.log`.
- PC, RP2350 ARM, and RP2350 RISC-V build wrappers pass. ARM is the physical
  target; RISC-V builds are maintained as an additional check.
- The full JCSystem/APDU probe regression passed on VPCD: 75 checks, no failures
  (`TEST:SUMMARY pass=75 fail=0 elapsed_s=276.309`). Coverage includes APDU
  handling, Java exceptions, transactions, persistent references, transient
  arrays and reset/deselect lifetimes, memory queries, and selected
  `JCSystem` APIs. Focused lifecycle checks confirm that reselecting an applet
  invokes `deselect()` and `select()` exactly once, and
  `Applet.reSelectingApplet()` is true only inside the same-AID select callback.
  Rejected selection returns `6999` and leaves the applet unavailable for APDUs;
  a later SELECT works,
  and card reset invokes deselect before the next successful select. A fresh
  VPCD emulator process also restored a committed instance field (`0x33`) and
  persistent static/array reference identities across process restart. A
  focused alias case clears one static reference, verifies GC reclaims the
  unrooted persistent array, then restores it through the remaining flash
  alias; the full restore and four-round pressure run passed 33 checks
  (`TEST:SUMMARY pass=33 fail=0`).
- A focused VPCD APDU check confirms an unknown SELECT-by-AID returns `6A82`,
  deselects the active applet, and leaves later applet APDUs unavailable. The
  card-manager path also compiles for RP2350 ARM; no physical flash was run.
- `./tools/test-vpcd-persistence` commits applet field `0x33`, exits the host
  emulator, starts a fresh process from the saved flash image, and verifies the
  restored field, static/array reference identity, object-field alias, array
  type, and lazy restoration after collection. The latest run passed 38/38
  checks with four pressure rounds, including a nested holder → `Object[]` →
  aliased byte-array graph. This validates process-restart persistence;
  physical applet installation persistence was separately verified after a
  reported five-second power-off/on on 2026-10-20 (see `TODO.md`).
- `APDU.getCurrentAPDU()` now returns the exact APDU object passed to
  `Applet.process()` and raises `SecurityException` from `Applet.select()`;
  focused VPCD checks passed both cases. This validates host VM dispatch only,
  not physical PC/SC.
- `APDU.getCurrentAPDUBuffer()` now returns that object's buffer during
  `Applet.process()` and raises `SecurityException` during `Applet.select()`;
  focused VPCD checks passed both cases.
- `APDU.waitExtension()` enforces the no-chaining `ILLEGAL_USE` rule, and the
  VPCD probe confirms its normal outgoing path. CCID/T=1 WTX signaling remains
  unsupported because the VM processes commands synchronously.
- `JCSystem.isAppletActive(AID)` follows the JC 3.0.5 registration and callback
  contract. It is false before registration, true after `Applet.register()`
  during install and select, remains true during deselect/process, and clears
  after deselect. VPCD cases 76–77 cover these callbacks; case 56 covers a
  rejected `select()` and subsequent successful selection.
- `JCSystem.getAID()` returns null before successful registration and returns
  the registered applet AID during install, after SELECT, and after a fresh
  VPCD process restores the installed instance. The focused install and restore
  probes both pass. Physical applet APDUs remain blocked by the PC/SC transmit
  issue described below.
- `JCSystem.lookupAID()` validates null and array bounds, returns the active
  applet AID only on an exact byte match, and returns null for a miss. VPCD
  cases 65–66 pass. This is limited by the current single-applet context model;
  it does not establish lookup across multiple installed applet contexts.
- Native transaction and storage recovery checks pass: injected record-write
  and snapshot-commit failures roll back cleanly, and the filesystem overlay
  test passes 1,000 randomized multi-record power-cut cases plus reset, CRC
  fallback, and tombstone restart checks. JCSystem begin/commit/abort now pass
  the full 80-check VPCD probe; abort invalidates live persistent field caches
  so subsequent reads observe restored flash snapshots. The native transaction
  suite passes. Physical transaction and power-cycle behavior remain unverified.
  An empty abort has a dedicated regression between transient-array setup and
  deselect clearing; both transient array references, contents, and clear
  events survive it. A commit whose `fs_sync` fails also invalidates caches
  when it rolls back nonempty snapshots.
- Current-source deploy Core API 3.0.5 now passes on PC VPCD after fixing empty
  transaction aborts: 291 calls completed with 272 `OK`, 11 expected
  `UNSUPPORTED_PER_SPEC`, and 8 unavailable optional Multi/Ext scenario rows;
  there were zero runtime exceptions, wrong results, or SW errors. The empty
  abort no longer advances the cache generation when no persistent records
  changed. Abort-after-write and commit-after-write still pass the focused
  transaction persistence probe (7/7).
- GET DATA (`0066`) now returns Card Recognition Data both before
  authentication, for card discovery, and through the authenticated
  card-manager path. Its exact encoding and status-word behavior pass 11 host
  unit checks. CPLC (`9F7F`) remains unavailable because the firmware has no
  manufacturing-data source. A direct real-reader probe sent both APDUs
  without selecting an applet: Card Data returned the expected TLV with `9000`,
  and CPLC returned `6A88`. A second physical probe established SCP03 S8,
  verified the card cryptogram, authenticated with C-MAC, and returned the
  same Card Recognition Data over protected GET DATA with `9000`.
- SHA-256 has known-answer and repeated-call coverage. The CCID/PCSC interface
  and physical reader have passed bounded probes in earlier sessions.
- After a physical five-second power disconnect/reconnect, the JCAlgTest
  package and applet remained installed. SELECT, GETVERSION, and JCSystem
  information succeeded, and the official 4,525-row `ALG_SUPPORT_EXTENDED`
  sweep completed and matched the declared support list (15 supported
  algorithms). Logs and CSV are recorded in the current section of `TODO.md`.
- A clean Linux GlobalPlatformPro installation was completed on the Pico on
  2026-10-20: the supplied JCAlgTest 1.8.2 JC305 CAP loaded and installed, and
  the applet was selectable with SELECT/GETVERSION/JCSystem returning `9000`.
  Windows GlobalPlatform compatibility remains unverified.
- Three physical Pico 2 readers are visible concurrently. On the installed
  image, Core and Crypto were installed as separate packages on one Pico,
  remained simultaneously selectable, and each completed its official deploy
  API run. Both packages were then removed and the registry returned to ISD
  only. The deploy runner's `--reader-name` path now resolves the Java client
  index as well as pinning GPPro to that reader.
- Current-source PC VPCD supports a fresh empty card-manager state without
  `--class-index`. GPPro 20.01.23 completed SCP03, JC305 CAP LOAD/INSTALL,
  GET STATUS, applet DELETE, package DELETE, and final ISD-only listing. This
  validates one dynamic package on the host emulator; multi-package registry
  persistence remains open. The native emulator `--install` path now passes a
  clean process-restart check: GPPro lists the applet after restart, and SELECT,
  GETVERSION (`1.8.2_jc305`), and JCSystem info return `9000`. GPPro external
  LOAD/INSTALL also passes applet restore after restart when resumed with its
  allocated package ID 56 and class index 148. Automatic registry discovery
  and multi-package persistence remain open.
- The latest three-reader deploy batch completed Crypto (466 calls), Core
  (291), and Service (124) concurrently on the installed firmware, with each
  reader assigned one area. All workers exited 0 and the main CAP cleanup
  passed. Core's optional Ext INSTALL returned `6A88` and rollback passed.
  Crypto had 2 SW_ERROR, Core had 1 wrong result and 4 optional applet-unavailable
  calls, and Service had 28 wrong results, 1 runtime exception, and 1 SW_ERROR.
  These results confirm multi-reader routing and cleanup; they do not validate
  current source because no new image was flashed. See `progress.md` and
  `.agent/HANDOFF.md` for per-reader artifacts.
- Current-source host VPCD installs and selects the deploy
  `AlgTestApiJavacardxSec_305` CAP using canonical-AID aliases for the local
  APDUUtil and SensitiveResult packages. The official 84-call JavacardxSec
  client suite passes `OK=84` through `tools/test-javacardxsec-vpcd`. The
  `SensitiveResult` assertion/reset methods now use a VM-wide typed result slot;
  same-context AID, JCSystem, and Util capture is covered. SIO context-switch
  behavior and sensitive result producers outside the tested subset remain
  open. No current-source image was flashed to hardware.
- Current-source JC305 Service CAP passes all 33 Dispatcher, 39 BasicService,
  and 124 deploy Service API catalog cases on VPCD. The 124-call run validates
  `CardRemoteObject` export/unexport and the 16-object capacity scenario against
  a VM-owned 32-slot registry whose references are included in GC root walks.
  RMI SELECT FCI supports class and interface references from a generated CAP
  2.2 fixture. Current-source VPCD now handles primitive RMI INVOKE: method-hash
  lookup, byte/boolean/short/int argument decoding, virtual dispatch, primitive
  result encoding, and unknown-method/object error responses. `Counter.add(short)`
  and `Counter.value()` pass with exact responses, including persistent field
  mutation/readback (`TEST:SUMMARY pass=6 fail=0`). Primitive-array marshalling
  and API exception responses are covered by VPCD fixtures, including preserved
  SystemException and ISOException reasons. Remote-interface returns now work
  in class and interface descriptor forms, and a follow-up INVOKE through the
  returned object passes. The bundled Oracle SDK JCRMI proxy now passes the
  unmodified SELECT (`Le=7F`), primitive INVOKE, and custom runtime/checked
  exception cases over current-source VPCD. Int[] RMI argument/result handling
  now passes VPCD with nontrivial 32-bit values, empty arrays, and null arrays.
  Runtime and checked custom exceptions pass through an extra applet-defined
  superclass with the expected nearest API exception encoding. Framework
  reasons for SystemException, CardException, CardRuntimeException,
  APDUException, PINException, TransactionException, UserException, and
  CryptoException are preserved through RMI on VPCD and physical D92. A custom
  CryptoException chain with two applet-defined classes also preserves its
  inherited reason and subclass tag. The
  official SDK 2.2.1 proxy
  identifies each as the expected framework class. ISOException dispatch now
  uses its correct embedded export token. Broader hierarchy edge cases remain
  open. Int[] RMI passed on physical D92 after the current ARM image was built
  for both architectures and flashed; its unique fixture package was removed
  afterward and JCAlgTest was confirmed intact.
  See the 2026-10-22 progress and handoff entries.
- Current-source VPCD completed the deploy Security CAP's 901 calls after
  correcting the JC305 `MessageDigest()` subclass-constructor token used by
  `invokespecial`. Official decoding reports 64 `OK`, 822 spec-allowed
  unsupported outcomes, 13 runtime exceptions, and two result mismatches. The
  two `Key.isInitialized()` mismatches depend on transient AES keys; the
  preceding transient-key setup cases return `UNSUPPORTED_PER_SPEC` because
  `KeyBuilder.buildKey` only supports persistent AES/DES keys. Treat these
  downstream cases as unavailable-feature fallout, not evidence of a VM
  defect. Unsupported algorithms remain expected; this does not claim complete
  Security API support. The current ARM and host builds pass, but no new image
  was flashed to the three physical readers.
- Heap tracing now has a guarded sweep path in firmware: it runs after APDU
  interpreter scopes return and only reclaims after a complete root walk.
  Persistent-tagged objects are conservatively pinned, global arrays are
  excluded, and stable handle slots are reused. Exception exits now unwind Java
  frames so the post-APDU collector can run. Collector marking now streams
  roots and children through stable-slot bitmaps without temporary root,
  pending, or child-reference vectors. Physical static testing passed the
  earlier 512-row OOM point and reached 544 rows without a firmware panic, then
  hit an intermittent PC/SC reader loss. After restarting `pcscd.service`, a
  focused retry selected the applet (`9000`) but the host did not send the next
  GETVERSION APDU. Concurrent serial diagnostics confirm one XfrBlock (SELECT)
  and no second XfrBlock; runtime self-checks pass and USB remains enumerated.
  The latest cycle again passed both builds and the 18-second ARM runtime
  capture; a bounded PC/SC retry reproduced SELECT `9000` followed by no
  GETVERSION and timed out (2026-10-19).
  The latest CHOUPI-specific cycle also passed after the GET DATA change:
  ARM/RISC-V build gates passed, ARM firmware flashed, and serial output
  repeatedly reported `TEST:PASS choupi_runtime` with no `FAULT`
  (`artifacts/logs/cycle-choupi-20261019T061401Z.log`).
  The latest `tools/test-pico-powerup` retries received USB ACL access, but GPPro
  and `pcsc_scan -r` exposed only Virtual PCD readers. Three
  bounded registry attempts failed before an applet APDU; power-up persistence
  is inconclusive in this run.
  The explicit reinserted-board retry also received USB ACL access but listed
  only Virtual PCD readers on all three attempts; see
  `artifacts/logs/pico-powerup-persistence-20261019T102428Z.log` (latest; the
  earlier retry is `artifacts/logs/pico-powerup-persistence-20261019T095458Z.log`).
  The VPCD bridge is installed, but it cannot claim the Pico USB interface
  while `pcscd` owns it (`libusb ... failed: -6`). The static run remains
  blocked at host reader ownership/PC/SC transport, not by an observed VM
  algorithm failure.
  A physical 50-request M1 stress plus 50-call DES3 measurement passed without
  a GC skip.
  VPCD allocation batches for arrays and objects passed. The historical
  physical static run after the DESKey mapping completed 7,785 rows, but had
  1,852 `6F00`/`CANT_BE_MEASURED` results and was not a passing matrix. The
  ROM/imported exception class-view fix is now verified on hardware: M1 returns
  `03 9000` and JCAlgTest records the expected `NO_SUCH_ALGORITHM`. The latest
  post-fix matrix remains incomplete due to the host USB/PC/SC interruption;
  the latest serial capture shows no collector allocation failure or panic;
  the reader path needs PC/SC daemon recovery before another static run.

Latest physical retry (2026-10-19): after a successful CHOUPI ARM cycle,
PC/SC listed the Pico and the probe read ATR `3B800181`, then timed out before a
SELECT response. Serial reported CCID `stage=2`, `rx_events=0`, `tasks=0`,
`xfr=0`, with repeated runtime passes. The request did not reach firmware, so
this does not test the new JCSystem behavior on hardware. See
`artifacts/logs/physical-jcsystem-active-retry-20261019.log`.

## Main gaps

1. **Applet contexts and firewall:** only one applet context is modeled. There
   is no general multi-applet registry/context switching, logical-channel
   support, or custom shareable-interface callback and ownership model. The
   current shareable lookup fallback is verified only for the default-null
   probe applet.
2. **Memory lifecycle:** basic stable-slot reclamation is enabled. The collector
   walks static/context roots and exported native RMI references; persistent
   records remain in flash and can be restored lazily. Current-source two-process
   VPCD persistence passes 7/7 checks for committed fields, static identity,
   persistent child identity, aliases, and restored array type. Broader forced-GC
   pressure coverage now includes current-source VPCD checks of static roots
   and an exported RMI child retained while 256 temporary arrays are collected;
   the child remains callable after collection. A fresh-process persistence
   test also rechecks static roots, persistent array/object aliases, and restored
   array type through four 256-array/256-object collection rounds (31/31 checks).
   Lazy flash-record restoration after its corresponding RAM object is actually
   reclaimed remains open. Persistent primitive field reads release temporary
   buffers on success and error.
3. **Physical end-to-end validation:** the latest static run reached 544 rows,
   passed the earlier collector OOM point, and then stopped when PC/SC lost the
   reader. A historical static run completed its matrix with many `6F00`
   results. Do not report either matrix as passing.
4. **Profile/API completeness:** several APIs are deliberately unsupported,
   including object deletion and integrity-sensitive arrays. Remaining runtime
   behavior needs systematic comparison against a selected Java Card profile.
5. **Card-management and persistence assurance:** storage rollback and
   randomized power-cut recovery have focused native coverage, applet
   persistence has a two-process VPCD check, and GET DATA Card Recognition Data
  has encoder/status unit coverage and direct physical open-channel and
  SCP03-authenticated APDU checks. On the Pico, authenticated applet DELETE
  left the package loaded; related-object package DELETE removed both; the
  supplied CAP was reinstalled, and both entries survived a firmware restart.
  The post-restart applet SELECT, GETVERSION, JCSystem, SHA-256, and static
  calls passed. `tools/test-pico-powerup` now checks registry persistence and
  applet execution after a boot. The physical five-second power-off/on and
  post-cycle support-sweep check passed on 2026-10-20 (see `TODO.md`). Broader
  CAP restore and general card compatibility assurance remain open.
6. **Conformance and hardening:** no full Java Card conformance suite or
   certification-grade security assessment has been completed.

## Next priorities

1. Restore Pico USB passthrough into the VM, confirm board identity, flash the
   latest ARM image, and restore PC/SC APDU access. The previous PC/SC trace
   stopped after SELECT (`9000`) without a host GETVERSION APDU; the latest
   `lsusb` showed only Linux root hubs. The streamed collector passed 544 rows
   without the earlier allocation failures, but its static matrix remains
   incomplete. The direct VPCD bridge also needs an available USB interface
   and a live VPCD endpoint.
2. Define the supported Java Card version/profile, then audit runtime APIs,
   applet lifecycle, and firewall behavior against it.
3. Expand power-cycle, CAP-validation, and host/physical regression coverage.

## Project notes

- [readme.md](readme.md): build, flash, PC/SC, install, and JCAlgTest runbook.
- [plan.md](plan.md): project scope and acceptance criteria; some early
  baseline sections are historical and predate the current implementation.
- [.agent/HANDOFF.md](.agent/HANDOFF.md): chronological session handoff and
  latest concrete work.
- [.agent/QKB.md](.agent/QKB.md): recurring issues, evidence, and known fixes.
- [progress.md](progress.md): append-only validation and commit record.
