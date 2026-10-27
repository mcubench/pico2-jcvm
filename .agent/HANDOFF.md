# Current continuation — validate counted CAP component payloads (2026-10-27)

# Current continuation — compact CAP Descriptor bounds (2026-10-27)

# Current continuation — CAP 2.1 Class component bounds (2026-10-27)

# Current continuation — CAP 2.1 Descriptor/Class cross-reference (2026-10-27)

- Descriptor `this_class_ref` values are now checked against the Class record
  start offsets, and the descriptor class count must match the number of
  interface/class records. This is applied only to CAP 2.1 after format-aware
  Class record traversal. The prior speculative attempt failed because its
  assumed Class record sizing was wrong; the current walk was audited against
  all 48 supplied CAPs before enabling this check.
- Offline audit: all 956 internal Descriptor class refs matched record starts.
  `./tools/test-cap-parser` passes 4,156 checks under ASan/UBSan, including
  synthetic mid-record reference rejection and 100,000 randomized buffers.
  ARM/RISC-V wrappers pass. VPCD GPPro install/list passes; artifacts:
  `artifacts/results/gppro-empty-install-vpcd-20261027T040102Z/`.
- ARM UF2 2,027,008 bytes, SHA-256
  `e4d67d25c15e4c94151a71d5e0dd3b8b5b2a3f4411d157a90e3b1b6aa913f75c`;
  RISC-V smoke UF2 79,360 bytes, SHA-256
  `ef4d6410bdf99363e1833df9f8a9770cf5ccb46ada794056bea3dbf7badb1fdb`.
- Implementation commit: `8b4d6e7`; progress/QKB/TODO/handoff update follows
  separately. Physical hardware remains unavailable to this VM despite the
  user's report of plugged tokens (`pico2-pcsc-reader`: 0; `lsusb`: only VM
  root hubs and VirtualBox tablet).
- Next: verify CAP 2.1 type-descriptor grammar and nibble encodings; retain
  explicit non-validation for CAP 2.2+ layouts until version-specific parsing
  exists. Then continue interrupted persistence-write recovery testing.

# Prior continuation — CAP 2.1 Class component bounds (2026-10-27)

- Added format 2.1 Class component traversal: leading interface records,
  compact class prefixes, public/package virtual-method tables, implemented
  interfaces, and method token maps are all checked against the payload bounds.
  The parser skips newer Class formats pending their explicit versioned
  layout checks. The spec distinguishes `interface_info` and compact class
  records by the ACC_INTERFACE high bit and documents their variable tables.
- `./tools/test-cap-parser` passes 4,154 ASan/UBSan checks over 48 unchanged
  deploy CAPs plus 100,000 random buffers. Synthetic malformed count cases
  reject as expected. ARM/RISC-V `asymm_crypto` wrapper builds pass. VPCD GPPro
  LOAD/INSTALL/list passes; logs:
  `artifacts/results/gppro-empty-install-vpcd-20261027T035353Z/`.
- Rechecked after the user reported plugged tokens: `sudo -n
  /usr/local/sbin/pico2-pcsc-reader` still reports zero. `lsusb` shows only VM
  root hubs and the VirtualBox tablet. No flash/hardware testing.
- ARM UF2 2,025,984 bytes, SHA-256
  `63b5153dd469ff36fd680697449453d935ed79e294b3c5e0f4d1b2d945d7d1dc`;
  RISC-V smoke UF2 79,360 bytes, SHA-256
  `ef4d6410bdf99363e1833df9f8a9770cf5ccb46ada794056bea3dbf7badb1fdb`.
- Implementation commit: `583f272`; progress/TODO/QKB/handoff update follows
  in its own commit.
- Next: independently verify Descriptor class-reference offsets against parsed
  2.1 Class record boundaries, then add type-nibble semantic validation. For
  physical acceptance, retry only after the VM sees a Pico through host USB.
  Preserve all unrelated worktree edits.

# Prior continuation — compact CAP Descriptor bounds (2026-10-27)

- Implemented compact Descriptor structural validation. The parser now walks
  each 9-byte class descriptor prefix, its counted 2-byte interfaces,
  7-byte field descriptors and 12-byte method descriptors; checks constant
  pool type offsets; then bounds each length-prefixed type descriptor. Fixed
  the packed `field_descriptor_info.access_flags` width from two bytes to one.
- Added malformed-count tests for interface, field, and method tables plus a
  minimal synthetic Descriptor. `./tools/test-cap-parser` passes 4,147 checks
  across all 48 supplied `/deploy` CAPs and 100,000 random buffers under
  ASan/UBSan. `./tools/build arm --profile asymm_crypto` and
  `./tools/build riscv --profile asymm_crypto` pass. The host-access-gated
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-gppro-empty-install-vpcd` passes GPPro
  LOAD/INSTALL/list. Logs: `artifacts/results/gppro-empty-install-vpcd-20261027T034258Z/`.
- The user reports tokens are plugged in, but this VM still sees zero devices:
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` reported found 0; `lsusb -d
  2e8a:0009` and `lsusb -d 2e8a:000f` found no matching runtime/BOOTSEL
  hardware. No flash or hardware test was attempted.
- ARM image: 2,025,472 bytes, SHA-256
  `cee4e5b98a63dd61c95ef54610661a6b6939c56908755754e95fef584b67c465`;
  RISC-V smoke image: 79,360 bytes, SHA-256
  `ef4d6410bdf99363e1833df9f8a9770cf5ccb46ada794056bea3dbf7badb1fdb`.
- Implementation commit: `891117e`; progress/QKB/TODO/handoff metadata follows
  in its own commit.
- Next: recheck physical enumeration using the approved host-access path. If
  absent, continue offline with Java Card format/version-aware Class component
  bounds; keep semantic Descriptor type-nibble validation separate until
  encoding details are verified. Preserve all unrelated worktree changes.

# Prior continuation — validate counted CAP component payloads (2026-10-27)

- Extended the CAP parser's pre-view validation to Header AID/package-name
  lengths, Directory custom-component entries, Applet AID records, Import
  package records, Constant Pool entry counts, Method exception-handler table
  extent, Static Field array/value lengths, Reference Location tables, Export
  records, and minimum Descriptor fixed-prefix space. Class and nested
  Descriptor record walks remain incomplete; do not call all CAP structure
  validation complete.
- The ASan/UBSan corpus now passes 4,038 checks across all 48 unchanged
  `/deploy` CAPs. It mutates selected counted fields to impossible sizes,
  checks truncated/oversized outer frames, and processes 100,000 randomized
  buffers. VPCD GPPro LOAD/INSTALL/SELECT passes after the checks.
- ARM and RISC-V wrappers pass. ARM UF2 is 2,025,472 bytes, SHA-256
  `abedadff14abd16ff42ee940ef6ac149041e3a966635b7ef2664db146729306f`;
  RISC-V smoke UF2 is 79,360 bytes. The host cannot currently see plugged
  physical tokens; no hardware operation was performed.
- Implementation commit: `b5f1123`; progress/QKB/TODO metadata follow in a
  separate commit.
- Next: verify the exact JC CAP class/descriptor record walk, then continue
  interrupted persistent-write recovery and full physical acceptance. Preserve
  unrelated edits enumerated in the next handoff section.

# Current continuation — sanitizer fuzzing for the short APDU parser (2026-10-27)

- Added `tools/test-apdu-parser-fuzz`, which runs the short APDU parser under
  ASan/UBSan on 250,000 deterministic randomized byte strings. Each acceptance
  decision and decoded Lc/Le value is compared with an independent reference
  for ISO 7816 short cases 1, 2S, 3S, and 4S. Result:
  `TEST:PASS apdu_parser_fuzz inputs=250000`.
- Existing `./tools/test-short-apdu` also passed its parser and APDU-context
  regression cases. No APDU parser bug surfaced.
- Implementation commit: `00ae634`; progress/TODO metadata follow separately.
- Next: validate variable-length CAP component payload structures beyond the
  newly added outer frame check, then continue WP9 persistent-write fault
  testing. Hardware visibility remains unresolved as described below.

# Current continuation — bound CAP component parser reads (2026-10-27)

- Fixed a CAP parser out-of-bounds risk: it previously read component tag/size
  without first ensuring three bytes remained and advanced by the untrusted
  declared size without checking it fit the input. The parser now validates
  every complete component frame before constructing a view; null/nonempty
  input is rejected, while null/empty remains accepted.
- Added `tools/test-cap-parser`, a PC ASan/UBSan harness. It extracts the
  unchanged component streams from all 48 supplied `/deploy` CAP archives,
  verifies each parses, tests truncation and forged lengths at component
  boundaries, and exercises 100,000 randomized byte strings. Result:
  `TEST:PASS cap_parser_framing checks=3268 fuzz_inputs=100000 corpus_files=48`.
- Validation: ARM and RISC-V wrappers pass; the ARM UF2 is 2,023,936 bytes,
  SHA-256 `d3450ae66905730b825ff8bffcf2c1d9650080f66344c76a349832b95f1c4957`.
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-gppro-empty-install-vpcd` passed and
  explicitly installed/selected the upstream JCAlgTest applet on VPCD. The
  test uses a temporary emulator; no physical board was accessed.
- Existing warnings in imported CAP headers (`ignored-qualifiers` and
  `address-of-packed-member`) are emitted by the standalone harness build;
  ASan/UBSan reported no issue. LeakSanitizer is disabled because it aborts in
  this VM's ptrace-constrained execution environment.
- Implementation commit: `28e5e47`; progress/QKB metadata follow separately.
- Remaining: deeper component payload validation, APDU parser fuzzing, broader
  interrupted persistent-write fault injection, and unavailable physical
  acceptance. Preserve unrelated worktree changes: `AGENTS.md`,
  `src/cap_load.cpp`, `tools/test-jcsystem-transaction-physical`, dirty
  API/ROM-mask submodules, supplied test logs/lists, and target CSV.

# Current continuation — independent RSA-CRT exponent blinders (2026-10-27)

- Replaced the shared CRT exponent blinder with independent random 224-bit
  blinders for the P and Q exponentiation branches. This reduces correlation
  between the two private-operation traces. It remains exponent blinding only;
  input blinding and fault verification are still unresolved.
- Validation: `./tools/test-asymm-crypto-vpcd` passed 99/99, including CRT
  sign/verify and encrypt/decrypt. `./tools/build arm --profile asymm_crypto`
  and `./tools/build riscv --profile asymm_crypto` passed. ARM UF2 is
  2,023,936 bytes (SHA-256
  `c6398c6e2d42e7e3b86e2a0e27ca254a35b0358c6ff22af1c28d55e4a65673a9`);
  RISC-V smoke UF2 is 79,360 bytes; PC ROM mask is 524,300 bytes.
- The legacy JCAlgTest comparison remains exact for the 136 comparable
  positive identities (`python3 tools/compare-jcalgtest-legacy-target.py`).
- User reports tokens connected, but the approved PC/SC helper found zero and
  elevated `lsusb` found neither `2e8a:0009` nor BOOTSEL `2e8a:000f`. No flash
  or physical test was attempted. Restore VM USB passthrough and retry using an
  identified board.
- Implementation commit: `fd5a91d`. Progress record and QKB update follow in a
  separate metadata commit.
- Preserve unrelated worktree changes: `AGENTS.md`, `src/cap_load.cpp`,
  `tools/test-jcsystem-transaction-physical`, dirty API/ROM-mask submodules,
  four AlgTest logs, two performance measurement lists, and the supplied
  target CSV.

# Current continuation — native RSA/RSA-CRT KeyPair generation (2026-10-27)

- Added selectable per-size `ALG_RSA` and `ALG_RSA_CRT` KeyPair generation for
  512 through 3072 bits in `asymm_crypto`; 4096-bit generation remains
  disabled. Native C++ uses Mbed TLS MPI prime generation and writes N/E/D or
  N/P/Q/DP/DQ/PQ into the existing NativeKey storage. The unchanged API-only
  probe CAP checks 512/1024-bit RSA sign/verify and CRT component
  initialization, plus both 3072-bit keygen paths.
- Validation: support generator tests passed 10/10; PC/VPCD passed 99/99
  (97 asymmetric APDUs plus ATR/SELECT), with isolated RSA-3072 and
  RSA-CRT-3072 APDUs returning `9000` in 31.280s and 8.805s. PC, ARM, and
  RISC-V wrappers passed. ARM UF2: 2,016,768 bytes, SHA-256
  `29a838cc7c3654c0c6c88887f08c78233dda19ea1a777efa78cc96babccdaab8`; PC
  ROM mask: 524,300 bytes.
- Target-report audit (2026-10-27): parsed the supplied AlgTest 1.7.9 CSV and
  matched all 113 comparable supported algorithm/key identities against the
  current generated profile. Its 27 positive KeyPair-generation cases match
  RSA and RSA-CRT through 3072 bits and EC at 128, 160, 192, 224, 256, 384,
  and 521 bits. The target is an older schema, so the 1.8.2 report comparator
  cannot load it directly; this was an offline normalized identity comparison.
- Physical hardware: user reports tokens plugged in, but the authorized reader
  helper still finds zero devices; elevated `lsusb` lists only VM root hubs and
  the VirtualBox tablet, and PC/SC shows no readers. No flash or physical test
  was attempted.
- The VPCD runner uses a bounded 180-second APDU deadline so variable-time
  3072-bit key generation fits; firmware timeout policy is unchanged. Issue
  details are in `.agent/QKB.md`.
- The legacy target audit is now reproducible:
  `python3 tools/compare-jcalgtest-legacy-target.py` reports exact parity for
  136 comparable positive identities and lists the seven EC generation sizes.
- RSA-CRT private operations are now implemented in native Signature and
  Cipher dispatch. CRT keys carry P/Q/DP1/DQ1/PQ only; N is derived from P×Q
  and the product is checked before exponentiation. VPCD verifies sign/verify
  and encrypt/decrypt at 512 and 1024 bits. Full VPCD passed 99/99; PC, ARM,
  and RISC-V builds passed. ARM UF2 is 2,022,912 bytes, SHA-256
  `4b60ab143091ca404aace01569b3c5fe00b2dbb45beb5531445a28c52c236a1b`.
- CRT private operations now add a fresh 224-bit exponent blinder using
  `DP' = DP + r(P-1)` and `DQ' = DQ + r(Q-1)`. Blinded exponents are imported
  at public modulus width plus the fixed blinding width; the serialized secret
  exponent bytes are zeroized. PC/VPCD passes 99/99, and exact-source ARM and
  RISC-V wrapper builds pass. Updated ARM UF2: 2,023,936 bytes, SHA-256
  `5f50d74a348b20ea160d1a5569d8a430b8f80fba745d0db7da9d25bd39a541f5`; PC ROM
  mask: 524,300 bytes; RISC-V smoke UF2: 79,360 bytes. On RP2350 the callback
  uses Pico SDK `get_rand_32`, whose pinned SDK config enables TRNG entropy by
  default when supported by the chip. This is exponent blinding only; input
  blinding, a full side-channel audit, and a fault check with a public exponent
  remain unresolved.
- Current hardware check: the user reports tokens plugged in, but the approved
  reader helper found 0 devices and elevated `lsusb` showed only VM hubs and
  the VirtualBox tablet, with no Pico or BOOTSEL device. No flash or physical
  test was performed. Mbed TLS
  source audit confirms its normal MPI exponentiation uses a fixed-width,
  constant-time exponent path and table lookup.
- Next: restore USB passthrough so an identified board enumerates; then run
  physical asymmetric and `/deploy` acceptance. Continue offline review of
  RSA CRT input blinding and fault checks meanwhile.
- Implementation commit: `65664b3`. Preserve unrelated worktree changes listed
  below.

# Previous continuation — native 128/160-bit EC generation (2026-10-27)

- Added native SEC2 secp128r1 and secp160r1 groups shared by KeyPair, ECDSA,
  and EC agreement validation. EC `R` storage now accounts for subgroup order
  values one bit wider than the field (secp160r1 has a 161-bit order). The
  test-only CAP checks KeyPair generation and ECDSA sign/verify for 128, 160,
  192, 224, 256, 384, and 521 bits.
- Validation: PC profile build and probe CAP build passed; VPCD passed 93/93
  (91 asymmetric APDUs plus ATR/SELECT); ARM and RISC-V wrappers passed. ARM
  UF2: 2,002,944 bytes, SHA-256
  `474cb12cb6a70bf6399a8d0ab72c765f5a41ff733f9a0a95e6d2b6479bde4f90`; PC ROM
  mask: 524,300 bytes; RISC-V VM-core smoke image: 79,360 bytes.
- Physical hardware: user reports tokens plugged in, but after the builds the
  authorized reader helper found 0, `lsusb -d 2e8a:0009` found none, and PC/SC
  listed only Virtual PCD 00 00/01. No flash/test attempted.
- Remaining: exact support-report comparison, arbitrary EC domains, RSA key
  generation, private RSA hardening, and physical `/deploy` acceptance. Recheck
  reader enumeration on continuation and use any identified available board.
- Commits: native KeyPair milestone root `f17df07`, API submodule `869cb8a`,
  ROM-mask submodule `5297c51`; this small-curve step is pending commit.
- Preserve unrelated changes: `AGENTS.md`, `src/cap_load.cpp`,
  `tools/test-jcsystem-transaction-physical`, existing logs/measurement lists,
  supplied target CSV, and dirty/untracked contents in the API/ROM-mask
  submodules.

# Current continuation — native EC KeyPair generation (2026-10-27)

- Added profile-gated native `KeyPair` constructor, generation, and getters for
  EC FP. VPCD generates P-192, P-224, P-256, P-384, and P-521 keypairs and then
  passes ECDSA sign/verify. API submodule commit `869cb8a`; ROM-mask submodule
  constructor-name fix `5297c51`; root implementation commit `f17df07`.
- Validation: support generator 9/9; PC `asymm_crypto` build; VPCD 91/91 (89
  asymmetric APDUs plus ATR/SELECT); ARM and RISC-V wrappers passed. ARM full
  firmware is 1,997,824 bytes, SHA-256
  `b7e57bf46b286477d6d13e0ea840f0d0566fe47d86950eb4ec8a8477958a40c3`; PC ROM
  mask 524,300 bytes; RISC-V smoke image 79,360 bytes.
- Physical hardware: user reports tokens plugged in, but elevated authorized
  reader helper found 0, `lsusb -d 2e8a:0009` found none, and PC/SC listed only
  Virtual PCD 00 00/01. No flash/test attempted. Recheck enumeration when
  continuing, then use any available identified board.
- Remaining: implement EC KeyPair generation for reported 128/160-bit sizes;
  audit all `asymm_crypto` yes results against the target CSV, compare a full
  physical support report, complete `/deploy` hardware checks, and harden private
  RSA operations. Keep upstream CAPs unchanged and all crypto in native C++.
- Preserve unrelated worktree edits: `AGENTS.md`, `src/cap_load.cpp`,
  `tools/test-jcsystem-transaction-physical`, `tools/tests/symm_crypto_applet/`,
  supplied target CSV/logs/measurement lists, ROM-mask `capmap-free`/`target/`,
  API submodule `sdks/`, and other pre-existing submodule changes.

# Current continuation — native EC key agreement (2026-10-27)

- Added native C++ `KeyAgreement` factory and operation dispatch for six
  profile capabilities: EC SVDP-DH/DHC, plain DH/DHC, PACE-GM, and full
  uncompressed XY. The Java Card API object holds only algorithm/key state;
  all arithmetic is native. The test-only CAP uses the standard API only.
- Known-answer tests use P-256 private scalar 1 and peer point G: DH/DHC
  return SHA-1(Gx), plain variants return Gx, PACE-GM returns 2G, and XY
  returns uncompressed G. Test vectors pass. DH allows absent cofactor K;
  DHC returns `UNINITIALIZED_KEY` if K is missing. Named curves supported in
  this implementation are P-192, P-224, P-256, P-384, and P-521. Arbitrary
  curves, key generation, full target report comparison, and physical
  validation remain open.
- Validation: support generator 9/9; PC build passed; VPCD 86/86 (84 APDUs,
  ATR, SELECT), including DH without K and DHC's reason-2 missing-K result;
  ARM and RISC-V wrappers passed. ARM UF2 is 1,992,192 bytes;
  PC ROM mask is 524,300 bytes. The user reports tokens plugged in, but
  elevated `pico2-pcsc-reader` found 0, `lsusb -d 2e8a:0009` found none, and
  `pcsc_scan -r` showed only Virtual PCD readers. No hardware flash/test.
- Commits: API submodule `b6e8af6`; implementation `48eabe3`; edge tests
  `34808e6`; validation record `95470ac`.
- Preserve unrelated worktree edits: `AGENTS.md`, `src/cap_load.cpp`,
  `tools/test-jcsystem-transaction-physical`, the ROM-mask submodule, and
  supplied AlgTest logs/CSV/measurement lists. The API submodule also has
  untracked `sdks/`; preserve it.
- Next: implement EC/RSA key generation and arbitrary EC domain support as
  required by the target CSV. Then add invalid-input/cofactor regressions,
  compare a complete physical ALG_SUPPORT report, and complete `/deploy`
  physical acceptance once a Pico enumerates through the VM.

# Current continuation — native EC prime-field private key storage (2026-10-26)

- Implemented native `KeyBuilder` and `NativeKey` storage for EC FP private
  keys at enabled sizes 128, 160, 192, 224, 256, 320, 384, 512, and 521 bits.
  Added component handling for field, A, B, G, R, W, S, and cofactor; persistent,
  CLEAR_ON_RESET, and CLEAR_ON_DESELECT allocation; initialization checks,
  clear, and domain-parameter copying. No EC arithmetic/ECDSA is implemented
  yet.
- Fixed JC305 `ECPrivateKey` interface token mapping in `jc_cp.cpp`. Upstream
  class token 18 has a different inherited method order; without translation,
  `isInitialized()` called `getSize()` and falsely reported fresh keys as
  initialized. Exact export inspection is recorded in `.agent/QKB.md`.
- Validation: `python3 tools/test-javacard-support.py` passed 9/9;
  `./tools/build-pc --profile asymm_crypto` passed; after rebuilding the probe
  CAP, elevated VPCD passed 73/73 checks (71 APDUs plus ATR/SELECT); ARM and
  RISC-V wrappers passed. The user's tokens are reported plugged in, but this
  VM currently enumerates no Pico (`lsusb -d 2e8a:0009` found none; `pcsc_scan
  -r` showed only Virtual PCD 00 00/01; approved helper found 0). Do not claim
  physical validation or flash until a board is visible through the host path.
- Implementation is committed as root `db529cf`; the Java Card API submodule
  change is committed as `953fcb6`. Its `sdks/` directory remains untracked and
  must be preserved.
- Worktree includes unrelated changes; preserve them: user edits to `AGENTS.md`,
  `src/cap_load.cpp`, `tools/test-jcsystem-transaction-physical`, modified
  submodule state, the untracked ROM-mask submodule, provided CSV, and existing
  AlgTest logs/measurement lists.
- Next: when an identified PC/SC Pico reader appears, flash/test this image and
  run `tools/test-asymm-crypto-pcsc` with the exact reader name; then implement
  native ECDSA, EC agreement/generation, exact support comparison, and full
  physical `/deploy` acceptance. Preserve upstream CAP files unchanged.

# Current continuation — native RSA SHA-1 ISO9796-2 message recovery (2026-10-26)

- Added profile capability `signature_rsa_sha_iso9796_mr`, enabled only in
  `asymm_crypto`, and native `SignatureMessageRecovery` methods for algorithm
  30. Signing returns the ISO9796-2 signature and recovered-prefix length;
  verification recovers the prefix, hashes it with any suffix supplied through
  `update`/`verify`, and checks the embedded SHA-1 digest. Full and partial
  recovery plus altered-suffix rejection are covered by independent VPCD
  vectors. Existing `Signature` API method order is preserved by appending new
  methods after the upstream methods; this prevents shifts in CAP method
  tokens. The API submodule change is committed as `15df6fc`.
- Validation: support tests passed 9/9; the rebuilt test-only CAP and
  `asymm_crypto` VPCD suite passed 70/70 total checks (68 APDUs plus ATR and
  SELECT); ARM full-firmware and RISC-V VM-core wrapper builds passed. The
  first native ARM build found an unused decoder local under `-Werror`; it was
  removed and both builds then passed. Do not run the raw Python VPCD test
  against a stale probe CAP: use `./tools/build-symm-crypto-probe` before
  `tools/tests/test_asymm_crypto_vpcd.py`, or use
  `./tools/test-asymm-crypto-vpcd` when its full wrapper is suitable.
- Hardware: the user reported tokens plugged in, but this VM currently sees no
  Pico USB devices (`lsusb` only shows hubs and the VirtualBox tablet), and
  PC/SC shows only `Virtual PCD 00 00` and `Virtual PCD 00 01`. The authorized
  helper found zero CHOUPI readers. No physical flash or test was attempted;
  resume hardware validation after a Pico enumerates in the VM.
- The worktree has unrelated prior edits to `AGENTS.md`, `src/cap_load.cpp`,
  `tools/test-jcsystem-transaction-physical`, submodule state, logs, measured
  lists, and the supplied target CSV. Preserve those. Root implementation
  commit: `1b60dda`.
- Next asymmetric priority: native EC key storage and ECDSA, then EC key
  agreement/generation, exact report comparison, private RSA hardening, and
  physical `/deploy` acceptance. Keep crypto in native C++ API dispatch and
  keep upstream CAP files unchanged.

# Current continuation — native RSA ISO9796-2 scheme 1 (2026-10-26)

- Added profile capability `signature_rsa_sha_iso9796`, enabled only in
  `asymm_crypto`, and direct native Java Card `Signature` API dispatch for
  RSA/SHA-1 ISO9796-2 scheme 1. Signing matches independently generated known
  signatures. Verification covers complete-message and partial-message
  recovery, rejects changed messages, validates header/padding/trailer, and
  rejects messages too long to recover at least one byte. Precomputed-hash
  calls are rejected for this scheme.
- Validation: support generator 9/9; PC VPCD suite 65/65; ARM and RISC-V
  wrapper builds passed. Before the final precomputed-hash guard adjustment,
  D92 (`D92E743D5F640C0F`) passed the ARM runtime cycle and physical RSA
  PKCS#1/PSS/ISO9796 suite (45/45); log:
  `artifacts/logs/asymm-crypto-pcsc-20261026T193337Z.log`. After the guard
  adjustment, both architecture builds and VPCD passed again, and the image
  flashed and verified on D92. The monitor could not find a USB serial device;
  follow-up USB enumeration found no Pico devices and PC/SC listed only the two
  virtual readers. The authorized reader helper found zero physical devices.
- Board 2BED (`2BED26ED20556535`) accepted a verified image flash but its USB
  serial/PCSC interfaces disappeared before finite monitoring. Cause unknown.
  Followed session continuity guidance and completed validation on already
  enumerated failover board D92. The physical runner requires the exact PC/SC
  reader string including interface suffix `00 00`.
- Implementation commit: `50b39a7`; current validation details are recorded in
  `progress.md`. The worktree also contains unrelated pre-existing edits to
  `AGENTS.md`, `src/cap_load.cpp`, `tools/test-jcsystem-transaction-physical`,
  dependency submodule pointers, logs, measurement lists, and the provided
  target CSV; preserve them.
- Asymmetric profile remains incomplete. Next priority: native
  `SignatureMessageRecovery` support for `ALG_RSA_SHA_ISO9796_MR`, followed by
  EC key storage/ECDSA, agreement and generation, exact report comparison,
  private RSA hardening, and full `/deploy` acceptance.

# Current continuation — native RSA-PSS support and physical validation (2026-10-26)

- Added direct native Java Card `Signature` API support for RSA-PSS with
  SHA-1/224/256/384/512, default digest-length salts, and explicit salt lengths
  through the `init(key, mode, parameters, offset, length)` overload. PSS uses
  native MGF1/encoding/verification and the Pico SDK entropy source for salts.
- A test-fixture refactor briefly made old RSA PKCS#1 vectors return empty
  responses. Native tracing showed the C++ signer returned 64 bytes; restoring
  the PKCS#1 fixture's direct-return path fixed it. Additional fixture fixes
  selected correct PSS test-key sizes and distinguished default salts from
  explicit zero-salt parameters.
- Validation: support generator 9/9; PC VPCD `asymm_crypto` suite 59/59,
  including 39 RSA signature vectors. ARM and RISC-V wrapper builds passed.
  D92 (`D92E743D5F640C0F`) was flashed and finite monitor passed
  `TEST:SUMMARY pass=1 fail=0`. Physical PKCS#1/PSS suite passed 39/39; its
  temporary package cleanup passed. The ARM UF2 is 1,826,304 bytes, SHA-256
  `0fc6b186c936cd34dc460df493672f7e1868144dae3ad5112ee0ddf443a690f2`.
- Logs: `artifacts/logs/cycle-choupi-20261026T185249Z.log`,
  `artifacts/logs/asymm-crypto-pcsc-20261026T185432Z.log`, and
  `artifacts/logs/asymm-crypto-digest-vpcd-20261024.log` (VPCD log reused by
  the existing harness; SHA-256 `5d9e8f2bafb8c9929502210664bd55e2f2485cdb95ba6df7f9176232d7a7bc2f`).
- Implementation and validation record commit: `f00ab2f`.
- Asymmetric profile remains incomplete. Remaining tasks include RSA ISO9796,
  EC keys/ECDSA, key agreement/generation, exact report comparison, complete
  `/deploy` acceptance, and validation/hardening of private RSA operations.
  Continue on another enumerated board if D92 becomes unavailable; 2BED
  (`2BED26ED20556535`) was also visible during this session.

# Current continuation — physical RSA SHA-2 response sizing (2026-10-26)

- Found the physical-only cause of RSA SHA-2 signature APDUs returning `6F00`:
  `src/ccid_device.cpp` allocated a 96-byte response buffer, so an RSA-768
  signature (96 bytes) plus the two-byte APDU status exceeded the VM response
  capacity. The VM completed normally; the CCID wrapper substituted `6F00`.
- Increased APDU response storage to 1024 bytes and CCID transmit storage to
  1034 bytes. USB bulk transfers are already submitted at full size and
  packetized by the endpoint driver.
- Validation: ARM and RISC-V `asymm_crypto` builds passed. Physical
  `./tools/cycle choupi --profile asymm_crypto --chipid 2BED26ED20556535`
  flashed and verified the image; finite serial output reported
  `TEST:SUMMARY pass=1 fail=0`. The physical RSA test passed all 24 SHA-1,
  SHA-224, SHA-256, SHA-384, and SHA-512 signing/verification vectors and
  removed its temporary package: `artifacts/logs/asymm-crypto-pcsc-20261026T181526Z.log`.
- Board tested: `2BED26ED20556535`. Another reader (`D92E743D5F640C0F`) was also
  enumerated and remains available as failover. The upstream JCAlgTest CAP was
  not modified.
- Next: continue native RSA API and asymmetric-profile acceptance; the full
  asymmetric profile is not complete. Preserve all unrelated uncommitted
  changes in the worktree.

# Current continuation — native Cipher API dispatch (2026-10-26)

- Migrated Cipher factories, state, init, getters, update, and doFinal to
  direct native methods on the Java Card `Cipher` API class. Cipher objects are
  allocated from the API ROM class; AES/DES buffering, IV handling, and
  padding, plus RSA dispatch, remain in native C++. Removed generated Cipher
  provider and bridge sources and updated generator assertions.
- Validation: support-generator tests 9/9; PC `asymm_crypto` build passed;
  VPCD asymmetric suite passed 26/26 and symmetric cipher/MAC suite passed
  73/73; ARM and RISC-V wrappers passed for `asymm_crypto`. Full ARM image
  cycle passed on Pico 2 `2BED26ED20556535` (`artifacts/logs/cycle-choupi-20261026T172320Z.log`), with `TEST:SUMMARY pass=1 fail=0`. On its clean install of the unchanged upstream JCAlgTest v1.8.2 CAP, the AES CBC-NOPAD 50-call `doFinal` measurement returned `AA9000` in 3.067 seconds and DES CBC-NOPAD preparation returned `AA9000`.
- Hardware finding: a prior 50-call AES request returned `6400` at about 6.36 seconds because the interpreter applied its five-second applet execution cutoff to normal APDU calls as well as INSTALL. INSTALL keeps its five-second cap; normal applet calls now have a hard 15-second cap. A different older token returned `FF01` for AES preparation after reflash; stale persistent applet state is possible but not proven.
- API submodule commit: `d3d7e44`; root implementation commit: `6ef15a7`. Existing converter/Maven and packed-member warnings remain.
- Remaining asymmetric work: implement RSA SHA-2/PSS/ISO9796 signatures, EC
  APIs, key generation, full report comparison, then physical `/deploy`
  acceptance. The profile is not complete.

# Previous continuation — native symmetric Signature API dispatch (2026-10-26)

- Moved all 13 declared `symm_crypto` Signature MAC implementations onto
  direct native methods on the Java Card `Signature` API. The ROM-mask native
  method manifest now uses exact descriptors for both factories and all
  instance calls. Generated `SignatureProvider`, `SymmetricMacProvider`, and
  `MacNativeBridge` classes are absent; native MAC code no longer exports the
  obsolete Java bridge entry point.
- The API object stores its algorithm and operation state. MAC key access is
  revalidated at operation time, message/IV scratch is `CLEAR_ON_RESET`, and
  `sign`/`verify` reset the MAC state per the API contract. Public method
  descriptors remain unchanged. RSA Signature plus existing RSA key/cipher
  Java provider paths remain to migrate; do not build/package asymm_crypto as
  native-only until those are removed.
- Validation: support generator tests 9/9; PC `symm_crypto` ROM-mask/emulator
  build passed; VPCD regression passed 73/73 (32 symmetric cipher vectors,
  13 MAC sign vectors, and 26 MAC verify vectors); ARM and RISC-V
  `symm_crypto` builds passed. A first VPCD attempt used an 80 KiB old saved
  image despite its `empty` filename and failed on stale package metadata;
  the test now uses the clean 512 KiB current-empty image and unique output
  path. Physical testing was intentionally skipped because the user had
  unplugged the token; this is not a firmware failure.
- The implementation is committed as root `e120363`; the API state/factory
  source change is submodule commit `d0a1658`. See the latest progress record
  for artifact paths and hashes.
- The Pico was intentionally unplugged by the user. Do not wait for it or
  treat its absence as a firmware failure; continue offline with VPCD until
  physical testing is explicitly resumed.
- The current uncommitted RSA/profile changes are incomplete and include a
  generated Java provider path. Preserve them, but do not commit or treat that
  provider as acceptable firmware; the next implementation step is native RSA
  `Signature` dispatch without generated provider bytecode.
- Next: implement direct native RSA `Signature` factory/operations without
  adding Java providers, then migrate `KeyBuilder`/key and `Cipher` RSA provider
  paths. Keep the upstream JCAlgTest CAP unchanged and continue using VPCD
  while no physical Pico is attached.

# Current continuation — native MessageDigest hardware verification (2026-10-26)

- ARM and RISC-V wrappers pass for profile `current` after removing temporary factory tracing.
- D92 (`D92E743D5F640C0F`) was flashed with the clean ARM image by `./tools/cycle choupi --chipid D92E743D5F640C0F`; finite runtime passed (`TEST:SUMMARY pass=1 fail=0`, `artifacts/logs/cycle-choupi-20261026T044038Z.log`).
- On the real card, the isolated SHA-256 support APDU passed (`1500...9000`), and SHA-256 `doFinal` prepare and measurement both returned `AA9000`. The official JCAlgTest CSV recorded `ALG_SHA_256;yes`.
- The official `ALG_SUPPORT_EXTENDED` run reached 1,897 rows. The user intentionally unplugged the device, interrupting the run; do not classify the ensuing PC/SC transaction loss as a firmware defect. Partial artifacts: `artifacts/results/jcalgtest-real-d92-msgdigest-20261026/`; this is not a complete report.
- An earlier isolated support attempt returned `FF05`; it passed after the firmware was reflashed and rebooted. Root cause is unknown, and there was no MessageDigest source fix in this step.
- Focused probe switches `--sha256-support-only` and `--sha256-dofinal-only` are in `tools/JCAlgTestApduProbe.java`.
- Worktree contains unrelated pre-existing RSA/profile/API changes; preserve them. Next implementation priority remains direct native `Signature` factory/operations for MAC, then native RSA API dispatch. Do not add Java provider/adapter bytecode.

# Current continuation — descriptor-specific native method overrides (2026-10-26)

- ROM-mask native override keys now include each method's complete JCA descriptor. A direct ROM-mask probe accepted both Signature.getInstance forms and emitted two C++ overload declarations; the manifest key includes the method name in both its class/method fields and descriptor string.
- Validation: support tests 9/9; PC, ARM, and RISC-V builds passed. The PC/VPCD MessageDigest regression passed all 15 checks (TEST:SUMMARY pass=15 fail=0).
- Rommask submodule commit: dfd7f3f; root commit 713519f. Final PC, ARM, and RISC-V builds passed after the parser diagnostic was removed.
- Next: migrate symmetric MAC Signature behavior to direct native API dispatch using descriptor-qualified factory overrides; preserve symm_crypto behavior. Then move RSA signature and key/cipher implementations off Java provider classes.

# Current continuation — native MessageDigest dispatch (2026-10-26)

- Replaced generated Java digest providers with direct native C++ methods on the Java Card `MessageDigest` API class. The ROM-mask build applies configured native-method overrides, emits exported class-token metadata, and maps the API class export token to the class index used by `Heap::addInstance`.
- The API object carries algorithm, transient input-buffer reference, and input length. `doFinal` dispatches SHA-256 to the RP2350 SHA accelerator and selected other digests to mbedTLS; `reset`/`doFinal` clear transient scratch. This avoids slow persistent byte writes when the digest object is retained.
- Validation: support generator 9/9; PC build, ARM build, and RISC-V build passed on 2026-10-26. Exact PC/VPCD test passed earlier in this implementation step with `TEST:SUMMARY pass=15 fail=0`, including SHA-256 and performance calls. No physical flash/test was run in this continuation.
- Submodule commits: Java Card API `4cc1cf4`; rommask `4932ee0`. Root commit `4e71873`.
- Preserve unrelated uncommitted asymmetric/provider/profile and physical-diagnostic work. `tools/javacard-support.py` still has uncommitted RSA provider changes; they remain disallowed for accepted firmware.
- Next: continue the native-only asymmetric migration from the direct `Signature` API factory/operations and migrate RSA key/cipher and MAC provider classes without Java provider bytecode. The focused native reference-return path is now exercised by MessageDigest factory calls; use existing VPCD and real-board testing after implementation slices.

# Current continuation — JC305 RMI official client coverage (2026-10-25)

## Newest update — adapter-free Signature dispatch design (2026-10-26)

- Resumed native-only `asymm_crypto` work. Source inspection confirms the VM
  invokes API-declared native methods through `callJCNativeMethod`, and
  `Heap::addInstance(packageID, classIndex)` can allocate a ROM API class
  object directly without checking the abstract-class flag. ROM-mask JNI
  generation also handles a `REFERENCE` return by pushing `jref_t` onto the
  operand stack. This supports native `Signature.getInstance` and operations
  without Java provider subclasses in principle.
- No implementation/build has been made in this step. The current pending
  `RSASignatureProvider.java`/`SignatureProvider` changes remain prohibited;
  RSA key/cipher and symmetric MAC provider classes also need migration to
  native methods on the Java Card API classes. Preserve current uncommitted
  worktree changes and the API submodule pointer. Test ROM-mask support for a
  native factory with a reference return has not yet been built or run.
- TODO and QKB record the design direction and its unverified assumptions.
- `python3 tools/test-javacard-support.py` passed 9/9. Its current tests
  explicitly require generated `RSAKeyProvider`, `RSACipherProvider`, and
  `SymmetricMacProvider` classes, so that pass only documents the existing
  behavior; replace those expectations when native API dispatch is migrated.
- Next: build a focused ROM-mask/native factory fixture. Then migrate the
  `Signature` factory, state, RSA operations, and MAC operations as a coherent
  slice so `symm_crypto` remains intact; subsequently migrate RSA key/cipher
  APIs. Do not build/package `asymm_crypto` while generated provider classes
  remain in the ROM mask.

- Fresh local persistence validation on 2026-10-25: `./tools/test-rp2350-fs-overlay`
  passed all assertions including 1,000 randomized cuts for journal clear,
  journal creation, and registry-header publication (`TEST:SUMMARY fail=0`);
  `./tools/test-persistent-transaction` passed all assertions; and
  `./tools/test-vpcd-persistence` completed fresh-process restore plus four GC
  pressure rounds (`TEST:SUMMARY pass=38 fail=0`). Physical power-cut and
  physical persistent-alias tests remain open. Logs for the VPCD run:
  `artifacts/logs/vpcd-persistence-commit-20261025T200233Z.log` and
  `artifacts/logs/vpcd-persistence-restore-20261025T200233Z.log`.

- The `asymm_crypto` profile work has resumed under the project-wide objective.
  A source audit found the current uncommitted RSA signature attempt adds
  `tools/java-support/RSASignatureProvider.java` and generated Java factory
  dispatch into the ROM mask. That violates the user's explicit requirement
  for native C++ API dispatch without Java provider/adapter classes. Do not
  build/package the asymmetric profile from this state. Replace the adapter
  with direct native API dispatch, and audit the previously implemented RSA
  key/cipher provider code under the same rule. The native RSA signature code
  is still uncommitted and unvalidated; preserve it while determining whether
  its implementation can be reused behind direct dispatch.
- Baseline check in this continuation: `./tools/build arm` passed for profile
  `current` (15 enabled capabilities); it did not compile the RSA signature
  module and is not asymmetric-profile validation.

- The official JC305 SDK 2.2.1 `JCCardProxyFactory` probe now passes all 18
  focused VPCD checks against current source: primitive short invocation,
  byte/short/boolean/int array values and nulls, returned remote-interface
  calls, sensitive-result checks, custom runtime/checked exceptions, and nine
  framework exception classes/reasons. Final run exited 0 with
  `JCRMI_CLIENT:SUMMARY pass=18 fail=0` and emulator `VPCD:DISCONNECTED
  result=0`; client log:
  `artifacts/logs/rmi-client-official-vpcd-client-20261025T190043Z.log`.
- The RMI test wrapper now creates fresh raw 512 KiB erased flash for every
  emulator run. This avoids stale serialized package bits in the old shared
  81,920-byte scratch image. See the QKB entry for scope and evidence.
- Updated official client probe changes are host-side test code and CAP-fixture
  coverage only; Pico firmware still interprets CAP bytecode through native
  JCVM code.
- Physical module GET STATUS is now fixed and validated on D92. The 54-byte
  APDU plus CCID header exactly filled one 64-byte bulk-IN packet; the driver
  now sends a terminating ZLP. GPPro received P1=10, continued to P1=20, and
  completed `--list`; details and exact bytes are in TODO and QKB. The D92
  runtime monitor passed after the ARM flash.
- A subsequent full D92 `ALG_SUPPORT_EXTENDED` retry completed 1,393/4,525
  rows, then PC/SC failed with `SCARD_E_NOT_TRANSACTED` on the
  MD5/RSA/OAEP probe. The partial CSV is not valid acceptance evidence. The
  exact APDU is `B07526000302030900`; its current-source VPCD replay returns
  expected unsupported (`2603...9000`). A finite serial capture after the failure showed
  a board reset marker at opcode `8C`, instruction count 2,964, context
  `00020104`; reset cause is not identified. A host-side exact-replay option
  `--unsupported-rsa-oaep-md5` is ready in `tools/probe-jc-algtest`.
- The exact MD5 APDU passes current-source VPCD with fresh erased flash.
  `tools/tests/pc_vpcd_applet_test.py` also removes a stale CAP-registry sidecar
  after replacing its flash copy; reuse of the same copy path passed. The exact
  APDU replay passed ATR, SELECT, and APDU (`2603...9000`,
  `TEST:SUMMARY pass=3 fail=0`); see the correction appended to `progress.md`.
- Next: continue the remaining high-priority correctness work in TODO, starting
  by replaying the MD5 probe on an exact Pico reader with a concurrent serial
  capture when PC/SC returns. The last bounded scan found USB nodes but no
  PC/SC reader. Then continue CAP installation/filesystem
  atomicity and Service/RMI profile gaps. Do not use Windows.

# Previous continuation — Physical GET STATUS host stall (resolved below)

- Built ARM and RISC-V with repository wrappers and passed `./tools/test-gp-registry` (50/50).
- Diagnostic implementation and tracking update are committed as `0a14ba8` (`Trace physical GET STATUS response bytes`).
- Added serial capture of the module GET STATUS response bytes (up to 64 bytes). Both builds pass. Flashed the full ARM image to D92 `D92E743D5F640C0F` using `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --chipid D92E743D5F640C0F`; the latest cycle passed with `TEST:SUMMARY pass=1 fail=0` (`artifacts/logs/cycle-choupi-20261025T182059Z.log`).
- Prior image physical evidence: GPPro received `9000` for ISD/app/package status then hung after authenticated module query `84F210020A`. Firmware reported `len=54 sw=9000`, completed IN endpoint stage 7, no CCID transfer errors. The exact response data was not visible in that host attempt.
- Following diagnostic flash, PC/SC enumeration is intermittent. Java reader list sometimes shows D92 as reader 3; some attempts time out before GPPro connects and firmware sees only GetSlotStatus. The new response hex trace remains uncaptured.
- Another USB node is present but is not a PC/SC reader in current Java listing; this does not show a two-reader firmware limit. Do not use Windows for checks.
- Next: retry only when Java SmartcardIO lists D92; start the finite serial monitor concurrently with a bounded GPPro `--debug --list`; capture the retained `CHOUPI:CCID_RESPONSE_DATA` bytes. Keep physical GET STATUS unresolved until GPPro lists all categories without timeout. If PC/SC is absent, continue the offline TODO backlog.

# Previous continuation — JCSystem overlay lookup fixed; LED installed on attached Pico 2s (2026-10-25)

## Follow-up — verify GC keeps static and APDU buffer arrays (2026-10-25)

- Extended `choupi/test/jcarray_transient_test.cpp` to run the context root
  enumerator through `Heap::collectUnreachableFrom`. The cached static array
  and APDU Java buffer survive; an unrooted test array is rejected after
  collection. Result: `TEST:PASS gc_retains_static_and_apdu_java_buffer` and
  `TEST:PASS gc_reclaims_unrooted_array reclaimed=6`.
- Repaired `tools/tests/jcarray/CMakeLists.txt` to include the generated token
  map directory and added test-only stubs for unused instance persistence and
  applet-selection dependencies. Corrected a stale assertion: invalid
  transient clear events raise `SystemException` in the current implementation.
  `./tools/test-jcarray` now passes all checks.
- Next: continue collection/root coverage for instance graphs and transient
  lifecycle; physical GET STATUS stays open until PC/SC exposes a Pico reader.

## Follow-up — include cached static references in root visitor test (2026-10-25)

- Extended the focused RMI/root test to register a heap cached static-field
  reference and assert `Context::visitRootReferences` forwards it to the GC
  visitor. The test now reports `lifecycle_roots=6` and passes. This covers
  enumeration of static roots in addition to the five context/stack roots in
  the preceding commit; it does not change runtime behavior.
- Next: continue the remaining GC/persistence checks. Physical validation is
  still blocked until a Pico reader appears in PC/SC.

## Newest continuation — directly assert VM lifecycle GC roots (2026-10-25)

- Extended `tools/tests/rmi_registry/rmi_registry_test.cpp` to assert that root
  traversal visits the applet-AID object, the applet reference recorded during
  install, APDU buffer and APDU object references, and a top-level reference
  return. A primitive top-level return is confirmed not to be retained as a
  reference. `./tools/test-rmi-registry` passes with `lifecycle_roots=5`.
- This is coverage only; no runtime behavior changed. Broader GC and
  persistence-alias tests and physical power-cycle validation remain open.
- Next: continue the GC/persistence backlog in TODO; after the current USB
  enumeration issue recovers, retry physical GET STATUS on an exact reader.

## Newest continuation — move module scratch off the Pico stack (2026-10-25)

- Follow-up to `20bc9ec`: the physical GET STATUS P1=10 path had a local
  `ExecutableModuleAid[127]`, about 2.1 KiB, in addition to other locals on
  the Pico main stack. Moved it to one static scratch array because the CCID
  card-manager task processes APDUs synchronously. This is a resource-safety
  fix and a plausible explanation for physical-only behavior; it is not yet a
  confirmed fix for the GPPro stall.
- Validation after change: `./tools/test-gp-registry` 50/50;
  `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` passed.
  D92 `D92E743D5F640C0F` was reflashed with the clean image and finite runtime
  cycle passed (`TEST:SUMMARY pass=1 fail=0`, log
  `artifacts/logs/cycle-choupi-20261025T162018Z.log`).
- Physical PC/SC is still unavailable after that cycle: the approved helper
  granted access to two USB nodes; `lsusb -d 2e8a:0009` showed D92 and another
  Pico, but line-buffered `pcsc_scan -r` timed out without listing either.
  Therefore no post-fix GPPro run was possible. The previous GPPro run stalled
  on `84 F2 10 02`; the reader count is host enumeration, not a firmware
  two-reader limit.
- Next: resume physical GET STATUS testing when an exact Pico reader appears
  in PC/SC, using another identified board first if D92 remains unavailable.
  Run bounded GPPro `--list`, and capture the actual APDU response. Keep the
  selected GET DATA/GET STATUS TODO open; preserve unrelated worktree changes.

## Latest continuation — GET STATUS module lists (2026-10-25)

- User asked whether two Pico boards pulsing their LEDs means only two can be
  active as PC/SC readers. It does not: the heartbeat reports firmware reader
  idle state; PC/SC reader count is determined by USB enumeration, permissions,
  and pcscd discovery. Do not infer a two-reader firmware limit from the LEDs.
- Fixed GET STATUS `P1=10` representation in physical and PC VPCD paths: each
  executable load file has one E3 entry with all its module AIDs in repeated
  `84` TLVs. Extended BER lengths are supported. The VPCD multipackage test
  checks both packages and all three applets before/after process restart.
- Validation: `./tools/test-gp-registry` passed 50/50;
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passed 20/20;
  `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` passed.
  Physical D92 `D92E743D5F640C0F` was flashed with the instrumented image and
  passed finite runtime monitor (`TEST:PASS choupi_runtime`, summary 1/0).
  GPPro authenticated `--list` succeeded through ISD and executable load file
  categories, then stalled after `84F21002`; this is not a physical GET STATUS
  pass. Following timeout, `pcsc_scan -r` itself timed out. The approved helper
  reported two USB nodes; that count is not a maximum reader capacity.
- The final ARM/RISC-V outputs were rebuilt after removing temporary serial
  tracing, but the clean final image was not reflashed. Current physical D92
  still has the tracing build. PC/SC later stopped responding, so do not make
  more hardware claims until it recovers or another exact reader is available.
- Next: inspect the CCID/PCSC response path for the physical `P1=10` timeout,
  preferably using another identified reader under the device failover rule.
  Continue the selected GET DATA/GET STATUS TODO; keep physical result open.
  Preserve unrelated profile/RSA/AlgTest changes.

- `tools/run-deploy-parallel` defaults to one GPPro attempt, zero retry pause,
  and exposes `--gp-retries`, `--gp-pause`, and `--gp-timeout`. Zero pause is
  needed because the unchanged upstream runner sleeps after the final failed
  attempt too. This coordinator does not dynamically move a failed area to a
  different reader; failover currently requires a new targeted run. This is a
  time-loss mitigation, not a fix for the USB/PCSC transport problem.
- Next: continue the unresolved Priority 2 persistence/transport work in
  `TODO.md`. For the current dangling persistent-array alias issue, first
  inspect package deletion's static-tag cleanup and serialization consistency,
  then design a focused host regression before changing behavior. Keep
  `asymm_crypto` work and all existing unrelated changes untouched.

- The persistent array lookup failure is fixed. Legacy parent-slot child
  tombstones could become resolvable after parent recreation and mask a newer
  live body. New records use deterministic compressed tags; lookups select the
  newest live match over stale tombstones. The RP2350 overlay regression passes
  (`TEST:SUMMARY fail=0`). ARM and RISC-V builds pass.
- Physical 2BED `2BED26ED20556535` passed the transaction probe: array length
  `7FFF` before/after transaction, after firmware cycle, and after CCID reset;
  `TEST:SUMMARY pass=9 fail=0`. Physical power-off/on remains untested. Log:
  `artifacts/logs/jcsystem-transaction-physical-20261025T124715Z.log`.
- User requested LED firmware on all attached Pico 2s. The current ARM image
  (`build/arm/pico2_choupi.uf2`, 1,668,608 bytes, SHA-256
  `8fcdba1de93cf37a3e24bc7645ea99342a498538720da09d62a37091b89bd556`)
  flashed and verified on 2BED and D92 `D92E743D5F640C0F`. A third BOOTSEL
  device briefly reported chip ID `8e464f4e2bbc0a19`, but disappeared before
  the targeted wrapper could match it; user subsequently confirmed all three
  devices show the LED. At the last VM check, two runtime USB devices were
  enumerated; PC/SC listed D92 only. Do not claim a wrapper-verified third
  flash.
- LED behavior is committed as `202b072` (`df5956a` logs its commit ID).
- Persistence implementation, focused tests, QKB/TODO updates, and progress
  are committed as `8050f5b`; the commit-ID follow-up is `8f230e5`. Preserve
  unrelated capability-profile, RSA, and AlgTest changes.
- The current/no-AES/minimal `/deploy` Core, Security, and Crypto matrix has
  complete runs recorded in TODO and its cited CSV artifacts. JCAlgTest stays
  intentionally skipped on minimal, and its no-AES `F103` startup is an
  upstream applet limitation. The old unchecked TODO marker is being reconciled
  against those artifacts now.
- User confirmed LEDs on all three physical Pico 2s. Tool-verified flash writes
  completed on 2BED and D92; the third BOOTSEL identity `8e464f4e2bbc0a19`
  disappeared before the wrapper verified its write. Last USB enumeration had
  two runtime devices; do not attribute a third tool-verified flash.
- Follow-up after the user's LED confirmation: the approved PC/SC helper granted
  access to two USB nodes, while bounded `pcsc_scan -r` listed only D92 plus the
  two Virtual PCD readers. Treat the third boards' LED status as user-observed;
  no three-reader PC/SC test is currently available. Continue source work and
  use any Pico that reappears in PC/SC as the hardware failover target.
- A single-reader upstream Service deploy run was attempted on D92. GPPro
  `--install` failed `UPLOAD_NO_CONNECT` after four retries; the final report
  includes `SCARD_E_NOT_TRANSACTED` after about 201 seconds. No API calls ran.
  Result: `artifacts/results/deploy/parallel-20261025-142230/pico1.csv`.
- USB inspection found two runtime devices, serials D92 and 2BED. `lsusb -t`
  showed D92's CCID interface on `usbfs`, while 2BED's CCID interface showed
  `[none]`. After the helper granted both nodes and restarted pcscd, a separate
  bounded `pcsc_scan -r` still listed only D92. This points to per-interface
  PC/SC/USB attachment, not a firmware limit; cause of the `[none]` binding is
  unresolved.
- Next: continue the remaining TODO in priority order. The three-run
  clean-flash/power-cycle criterion and true power-off/on persistence remain
  open. Preserve the upstream CAP and unrelated working-tree changes.

## Previous continuation — Pico reader LED implemented and validated (2026-10-25)

- Added a non-blocking Pico 2 GPIO 25 PWM heartbeat: 1.6-second smooth
  brighten/fade every 10 seconds while CCID is active. Applet APDUs trigger a
  full-brightness 120 ms pulse. A hardware timer ends it even during long VM
  calls; manager traffic and reader polling are excluded. ARM/RISC-V build
  gates passed. Final image was flashed and verified on 2BED
  `2BED26ED20556535`; its bounded monitor passed. Upstream API 3.0.5 Core ran
  291 physical calls (272 OK, 11 spec-allowed unsupported, 8 optional
  `APPLET_NOT_INSTALLED`) and cleanup succeeded; final GPPro list was ISD-only.
  The LED output was not observed with an optical sensor.
- Final image `build/arm/pico2_choupi.uf2`: 1,668,608 bytes, SHA-256
  `42d0aa887aecf9e1a9f164ebdac8161edff8c773e172a99a7ac2741ceadda219`.
  Cycle log `artifacts/logs/cycle-choupi-20261025T104118Z.log`, SHA-256
  `45953aeac26bdd6826257145ef71414c3bf694fef68767ab6fbf477a00d4437f`.
  Physical Core CSV `artifacts/results/led-applet-apdu-2BED-final2-20261025.csv`,
  SHA-256 `906519e78c88195821085db2826fbf1846c63bcf0d85fe8f4d3ef46ebddf90a6`.
- Outstanding work: resume physical JCSystem state-across-power-cycle
  investigation. Uncommitted edits in `tools/JCSystemTransactionPhysicalProbe.java`
  and `tools/test-jcsystem-transaction-physical` add reset/persistence modes;
  repeated attempts after the earlier passing BUFFER_FULL test intermittently
  returned `6F00` on initial applet SELECT. Do not count those attempts as
  passes. The latest failure log is
  `artifacts/logs/jcsystem-transaction-physical-20261025T101807Z.log`.
  Preserve all unrelated asymmetric crypto dirty files.

## Previous continuation — JCSystem BUFFER_FULL verified on 2BED (2026-10-25)

- Physical 2BED `2BED26ED20556535` now passes the JCSystem exhaustion case.
  The focused probe reported `0300` (`TransactionException.BUFFER_FULL`,
  reason 3; unchanged persistent array byte), and all six physical assertions
  passed. GPPro cleanup succeeded and the final registry contains only ISD.
- The complete host VPCD JCSystem regression passed 85/85, including case 83.
  ARM and RISC-V `symm_crypto` builds passed; a full `choupi` cycle flashed
  ARM image SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22` and the
  finite runtime monitor passed. Focused VPCD cases 5 and 83 passed 4/4.
- A previous first overflow attempt returned `6F00`; PC/SC reset alone did not
  restore cleanup. Reflashing/rebooting the same firmware image reset the
  runtime and allowed applet/package deletion. Preserve the failed log and
  recovery observations in `.agent/QKB.md`.
- Artifacts: physical log
  `artifacts/logs/jcsystem-transaction-physical-20261025T095547Z.log`;
  cycle log `artifacts/logs/cycle-choupi-20261025T095236Z.log`; VPCD log
  `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261025T095818Z.log`.
- Next: verify persistent applet state across a full power cycle, then continue
  the highest-priority non-asymmetric items in `TODO.md`. `asymm_crypto`
  remains paused; preserve its dirty worktree changes.

# Previous physical validation — JCSystem commit capacity on 2BED (2026-10-25)

- Added `tools/JCSystemTransactionPhysicalProbe.java` and
  `tools/test-jcsystem-transaction-physical` to exercise the existing JCSystem
  test CAP against an exact PC/SC reader. The script checks the package AID
  before install, uses bounded GPPro/APDU commands, and deletes the fixture
  applet/package in its cleanup trap.
- Physical 2BED `2BED26ED20556535` run passed five assertions: rollback
  restored field `0x11`, commit retained `0x33`, and max/unused commit capacity
  was `32767` outside a transaction versus `32755` while a persistent scalar
  update was active. The package was removed afterward; final GPPro list showed
  only ISD. No firmware build/flash occurred.
- Log `artifacts/logs/jcsystem-transaction-physical-20261025T093140Z.log`,
  SHA-256 `98f0834d5d960df830dc755212c177a2812761cc366d91a7476a1744d9939fb3`.
  The probe CAP SHA-256 is
  `738164ec01573f5e101aca46c56e08065bbe0df8c5ab008f466e286363e31ac6`.
- Remaining: validate power-cycle recovery and catchable `BUFFER_FULL` at real
  capacity exhaustion. The earlier timed-out large-array VPCD attempt is not a
  pass. Preserve unrelated dirty asymmetric changes.

# Latest physical validation — symmetric profile JCAlgTest on 2BED (2026-10-25)

- Starting state: the 2BED `2BED26ED20556535` card had only the ISD installed
  and was running the packaged `symm_crypto` ARM image (1,676,800 bytes, SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`).
- Explicitly loaded/installed unchanged upstream CAP
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`). The
  official client selected exact reader
  `Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 01 00` (index 4), SELECT
  returned `9000`, and GETVERSION returned `1.8.2_jc305`.
- The complete 4,525-row `ALG_SUPPORT_EXTENDED` run passed: 73 `yes`, 4,452
  `no`, zero other statuses. Comparator matched all row identities and the 46
  configured capability identities, with no unexpected or missing support.
  After deleting the applet then package, GPPro listed only ISD. Firmware was
  not reflashed during this step.
- Artifacts: CSV
  `artifacts/results/jcalgtest-symm-crypto-2BED-20261025/CHOUPI_symm_crypto_2BED_ALGSUPPORT__3b_80_01_81.csv`
  (SHA-256 `19bcb6a8be3ee67c1d1656b54760654a8875396914c553137670889cb3850b0f`);
  client log SHA-256
  `daa5a8019092e88881182e8b64afcd01be37e34eaccc531040ad42f046f0ceaf`;
  comparator log SHA-256
  `679cb172d3ffcefc671eb57d27debfbba723ef081d931a50989ce4dff06cf54b`;
  post-cleanup GPPro log SHA-256
  `168bcb2ab02f8e0b23e974a906f8d0261cb5a33944c491a1d59c21f0bc900878`.
- Next: resume current TODO outside the paused `asymm_crypto` work; Windows
  PC/SC/client acceptance and the three-pass physical integration sequence are
  still open.

# Latest continuation — symmetric-only Windows package refreshed (2026-10-25)

- Added `tools/package-symm-crypto-windows-test` and fixed
  `docs/symm-crypto-windows-test.md` to prompt for the exact reader name and
  pass both physical and jCardSim CSVs to the comparator.
- `./tools/package-symm-crypto-windows-test` passed ARM and RISC-V wrapper
  builds and produced
  `build/external-test/symm-crypto-windows-test.zip` (SHA-256
  `24adae30b893acffd1faccd7455b071ca88dbcf6c0cbfb01019a95ea3675818a`). Its
  checksums and ZIP structure validate. The 4,525-row physical symmetric
  report comparison passed with 73 `yes`, 4,452 `no`, no other statuses, and
  zero mismatch against the 46 declared profile capabilities.
- Firmware image: 1,676,800 bytes, SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`.
  Windows enumeration and official client execution remain unverified; the
  archive is ready to transfer. No hardware operation was performed here.
- `asymm_crypto` remains paused. Preserve its unrelated dirty source/config
  changes. Next: deliver the package path and continue other TODO work unless
  Windows acceptance results arrive.

# Latest physical API validation — Security and Crypto on current 2BED image (2026-10-25)

- Ran the unchanged API 3.0.5 Security and Crypto CAPs/client on exact reader
  `Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 01 00`. Security completed
  901 calls: 97 `OK`, 802 `UNSUPPORTED_PER_SPEC`, and two skipped RSA-2048
  generation cases per the deploy README's reader-risk precaution. Crypto
  completed 466 calls: 161 `OK`, 305 `UNSUPPORTED_PER_SPEC`, no other statuses.
- Both CAP uploads and cleanup succeeded. Final GPPro listing shows only ISD;
  the test card remains empty. CSV SHA-256
  `7ba137393626a1d314eb17d2e842d32a76e389f9c3d19190924b388be77df2e5`;
  runner log SHA-256
  `01a4f74f714abd686893cb93878fe7b1f3f1da78fe11fdfce92e49f37deab2ab`;
  post-cleanup GPPro SHA-256
  `94a5b9f45e961b8d8ce93ba7f9bf4bff20e75d63a5805a108ba78ed0c51555e0`.
- Next: retain the no-CAP card state and continue the remaining TODO. Do not
  interpret unsupported algorithm rows as VM failures.

# Latest physical API validation — JCSystem transaction probes on 2BED (2026-10-25)

- Ran the unchanged API 3.0.5 Core CAP/client on the current `symm_crypto` ARM
  image via exact reader `Pico 2 CHOUPI CCID [JCVM CCID]
  (2BED26ED20556535) 01 00`. Core completed 291 calls: 272 `OK`, 15
  `UNSUPPORTED_PER_SPEC`, and four optional Ext scenario calls unavailable
  after expected INSTALL `6A88` and rollback. All 47 supported JCSystem rows
  passed; `requestObjectDeletion()` was the sole spec-allowed unsupported row.
- Physical capacity calls returned max/unused `32767` outside transactions;
  the begin/depth/capacity/commit probe returned `01 7f ff 00`, and begin/abort
  passed. Short and extended `getAvailableMemory` forms returned `OK`. This
  does not yet test capacity consumption after physical persistent writes or
  power-cycle recovery. The runner removed Core and Multi CAPs; Ext rollback
  succeeded. A final GPPro list contained only ISD.
- CSV SHA-256
  `c94e1435068c81e66ccd14f2e9bf339d1f13abd804009e9d2637f9f5bee63a72`;
  runner log SHA-256
  `941f40d409228e8a6fb9634c482b472d0a7097dfcd73dd3229961b975b9360c2`;
  post-cleanup GPPro log SHA-256
  `5fab190e1ac1f63c4a7a0ea97e8d00e20dc728ab519b230d13617bbad9c12cd6`.
- Next: continue the open physical transaction/power-cycle acceptance and other
  TODO priorities; leave the card ISD-only and preserve unrelated worktree
  changes.

# Latest physical validation — empty `symm_crypto` card on 2BED (2026-10-25)

- Refreshed PC/SC access after reboot and confirmed 2BED
  `2BED26ED20556535` was responsive. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle
  choupi --profile symm_crypto --chipid 2BED26ED20556535` passed ARM and RISC-V
  wrapper builds, flashed ARM with 100% verification, and passed the finite
  18-second monitor (`TEST:SUMMARY pass=1 fail=0` repeatedly). Initial GPPro
  inspection found a persistently stored JCAlgTest applet/package. Deleted the
  applet first and package second; final GPPro listing contains only ISD.
- The exact UF2 remains
  `artifacts/firmware/pico2-jcvm-symm_crypto-empty-card.uf2` (1,676,800 bytes,
  SHA-256 `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`).
  Cycle log SHA-256 `4bb4b031aecbc31ac6c9104298a44959ab04b05c0ad818ec56e304f5b1427aa6`;
  final GPPro transcript SHA-256
  `1d8135e5686ea5121fd33178dd4929075b07f91d43cb4c498ea766789487971d`.
- D92 enumerates with a card-present ATR but its GPPro APDU requests timed out;
  failover to 2BED was successful. After firmware re-enumeration, refresh the
  approved PC/SC helper and bind GPPro's optional reader as
  `--reader='exact reader name'` to avoid selecting another Pico.
- Next: continue project TODO in priority order. Keep the unrelated asymmetric
  profile worktree changes untouched.

# Latest validation — fresh-card explicit GPPro install regression (2026-10-25)

- Added `tools/test-gppro-empty-install-vpcd`, a bounded test that starts a
  genuinely blank VPCD flash image, confirms GPPro lists only ISD, explicitly
  installs the unchanged JCAlgTest 1.8.2 JC305 CAP, and confirms package
  `LOADED` plus applet `SELECTABLE`.
- Validation: `PICO2_HOST_USB_ACCESS=1
  ./tools/test-gppro-empty-install-vpcd` passed. Artifacts are under
  `artifacts/results/gppro-empty-install-vpcd-20261025T083218Z/`. This is VPCD
  evidence; physical empty-card and install behavior was previously verified
  on D92, and reflashing alone still does not remove CAPs already in persistent
  card storage.
- Script commit: `236586d`. Script SHA-256:
  `adbbbe784fef53f4111e5135ac6a00a966a68fb60a2b2e5f42149990243f449b`.
- Next: resume project TODO in priority order. Physical testing is currently
  unavailable because authorized BOOTSEL enumeration found no Pico in this VM.
  Keep the unrelated asymmetric-profile worktree changes untouched.

# Latest validation — flash current profile and verify physical boot (2026-10-25)

## Latest official current-profile JCAlgTest run — 2BED (2026-10-25)
- Confirmed 2BED `2BED26ED20556535` was running the `current` ARM image and
  the registry was ISD-only. GPPro explicitly loaded the unchanged upstream
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`).
- The exact PC/SC reader was selected by name, resolving to SmartcardIO index
  4. Official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` completed all 4,525 rows:
  15 `yes`, 4,510 `no`, zero other status values.
- Comparator: all 4,525 row identities matched jCardSim. The generated current
  manifest matched all 15 supported results (zero extra or missing). The
  reference's 1,454 additional `yes` results are unsupported capabilities in
  this firmware profile; jCardSim also reports 121 non-yes/no outcomes.
- Deleted applet `4A43416C675465737431`, then package `4A43416C6754657374`.
  Final read-only GPPro listing showed only ISD `A000000151000000`.
- Client CSV SHA-256 `9cdf7e31124f7cc9f73e4a244cbb9bc184d9027052b7b75362ab1cc08b99e78`;
  result log SHA-256 `cd4a6882fb9b6b28a948d2d4b0486575a410ceea43220437d79564ecb38d2512`;
  comparator log SHA-256 `945b10b7ea4c29aa50805bc94a4f88caf8169f6bb3de5f518d033cbc2fe1b882`.
  Final card listing:
  `artifacts/logs/gppro-current-profile-2BED-after-jcalgtest-cleanup-20261025.log`.
- Validation record commit: `da0c43ed18962b53d7b4a282629933bb72a6520b`.
- Next: AC1 Windows acceptance and three complete clean-flash/power-cycle runs
  remain open. No firmware or CAP implementation was changed in this run.

## Latest native API coverage — SensitiveResult across Context objects (2026-10-25)
- `tools/tests/rmi_registry/rmi_registry_test.cpp` now switches the active
  context between two native `Context` objects and verifies VM-wide boolean,
  short, and object results remain visible. It also verifies the shared object
  is enumerated as a GC root and `reset()` rejects stale assertions.
- Validation: `./tools/test-rmi-registry` passed with
  `RMI_REGISTRY:PASS ids=32 roots=1 session_cleanup=1 sensitive_result_cross_context=1`;
  scoped diff check passed.
- Commit: `85ab7d2b070b55f804288fb150d24babaea1726f`.
- Limitation: this is native Context-level evidence, not Java applet SIO
  dispatch. Cross-applet contexts and firewall behavior remain unimplemented.
- Next: continue current source work in TODO's priority order. Keep
  `asymm_crypto` paused; preserve unrelated staged/unstaged worktree content.

- `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile current --chipid
  2BED26ED20556535` passed both ARM and RISC-V builds, verified the ARM image
  write, and passed the finite 18-second monitor with repeated
  `TEST:SUMMARY pass=1 fail=0`. Image `build/arm/pico2_choupi.uf2` is 1,666,048
  bytes, SHA-256
  `cbd741c4f379b3738141af73b6a9a6c3b7f6c43b925a74c4f359d1bb789ac04e`.
- After the reboot, GPPro 26.06.04 `--debug --list` succeeded on exact reader
  `Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 01 00`. Only ISD
  `A000000151000000` is installed; the card remains empty. Transcript:
  `artifacts/logs/gppro-current-profile-2BED-20261025.log` (SHA-256
  `74310a82da5307c0a7104efcb92b7580566e8d99f5d3b857d20c144233796517`).
- USB showed D92 `D92E743D5F640C0F` and 2BED. ACL refresh enabled both; 2BED
  GPPro succeeded after refresh. D92 remained unresponsive to a bounded
  read-only GPPro `--list`, so no writes were attempted there. Do not treat
  enumeration alone as card health; use 2BED for the next physical test.
- Validation-record commit: `0e5cfad`. Cycle log:
  `artifacts/logs/cycle-choupi-20261025T073359Z.log` (SHA-256
  `c3996c0e39ec67f69e61237dd58427e0aaf20212f08e1e4b68a21f29c480c166`). The
  firmware cycle’s pre-flash gate covered both architectures.
- Windows workflow and true applet-state power-cycle persistence on the latest
  image remain open. Physical current-image CAP install, select, processing,
  and deletion are now verified by the `/deploy` and JCAlgTest runs above.
  Complete the requested repeatable clean-flash/power-cycle sequence after the
  external Windows pass is available.

# Previous implementation step — abort VPCD on invalid saved registry (2026-10-25)

- Fixed startup handling so a PC persistent-storage initialization error is
  latched and `starting_jcre()` refuses to start applet/card service. The VPCD
  regression now requires the expected error and verifies no VPCD connection
  occurs for corrupt or missing sidecar cases.
- Hardened the test fixture: remove a stale `.cap-registry` when creating an
  empty image, and poll emulator process exit while accepting VPCD so startup
  failures surface promptly rather than timing out.
- `./tools/build-pc` passed, including ROM-mask regeneration.
  `python3 -m py_compile tools/tests/pc_vpcd_multipackage_test.py` and scoped
  `git diff --check` passed. The gated full VPCD test passes `20/20` for invalid
  sidecar rejection and valid resume/cleanup. No physical hardware was used.
- This issue and its fix are in `.agent/QKB.md`. Logs:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261025T072710Z.log`, SHA-256
  `cc3a33fa97298ba1fb0bc51c2024e7fb7f139fb379bb8e1c3ad80a7f903dec1b`;
  restart log SHA-256
  `0aa116a45589b0fe1fab43730cedf2e47e58778df7397c2f520e6de60048a256`;
  corrupt rejection SHA-256
  `c6d142b529c8991d6b27f79b4fae38dc7764f1254bf3992d140a8133a2f842ed`;
  missing rejection SHA-256
  `41f9ae7d0e683b8a2f77f57e651871f4c4ddf501dba118751ae90c413063c086`.
- Implementation commit: `1b02284`; progress/QKB/TODO/handoff follow-up commit:
  `abe8acc`. Next: continue remaining card-manager and JCVM work in TODO; keep
  `asymm_crypto` paused.

# Previous implementation step — VPCD sidecar failure tests (2026-10-25)

- Extended `tools/test-vpcd-multipackage` with corrupted-CRC and missing-sidecar
  restart attempts against copies of the saved image. Both attempts emit their
  expected rejection before connection to the intentionally unused VPCD port;
  the valid two-package/three-instance restore and cleanup still pass.
- Python bytecode compilation and scoped whitespace check pass. The full test
  reports `TEST:SUMMARY pass=20 fail=0`. PC build from the immediately preceding
  implementation commit is unchanged; no firmware build or hardware run was
  needed for this Python-only regression extension.
- Logs:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261025T071147Z.log`, SHA-256
  `5cf2768c7b9655b2f9d11b3452bd1254997bc91f12880ec00ae86267dca0cce0`;
  restart log `artifacts/logs/host-vpcd-multipackage-restart-20261025T071147Z.log`,
  SHA-256 `c5ceb67ddddfc384a0d4cdb89448624d28e326537b68363538aa17123c2c3b3e`;
  corrupt-sidecar rejection log SHA-256
  `90549b210782ea964a950e453a4410546a43364d16936767574b11421c38e5d2`;
  missing-sidecar rejection log SHA-256
  `a2c1b3453c55e31fa83aed1f09de841fd9969d4e843701b4f4a30a6b8bc2ab99`.
- Test implementation commit: `1072e25`; progress/TODO/handoff commit:
  `5308aae`. Next: proceed with remaining card-manager and JCVM gaps in
  TODO; keep `asymm_crypto` paused.

# Previous implementation step — automatic VPCD registry sidecar (2026-10-25)

- `./tools/build-pc` passes, including ROM-mask generation. The build has
  existing converter/Maven, Rust OS, and packed-structure warnings.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage --timeout 180`
  passes `TEST:SUMMARY pass=18 fail=0`. It saves a two-package/three-instance
  registry, exits the first host process, and starts a fresh process with
  `--resume` and no explicit CAP/class restore records. Restored applets pass
  authenticated GET STATUS and SELECT, then cleanup empties the registry.
- The host now stores VPCD CAP bytes and applet/class mappings in an atomic
  `.cap-registry` sidecar adjacent to `--memory`. `--resume` validates its CRC,
  saved-image checksum, package bitmap, CAP package/applet AIDs, and duplicate
  instances. A missing or invalid sidecar is rejected when dynamic packages
  remain installed. The explicit repeatable `--restore-applet` mode remains
  available as a fallback.
- Logs:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261025T070620Z.log`, SHA-256
  `b26462a58b660dca73ea5619d4ba8724500427a910e03d1245307c980d2f33c2`;
  restart log `artifacts/logs/host-vpcd-multipackage-restart-20261025T070620Z.log`,
  SHA-256 `60ea3358a18ad638174ea4dd3be96dd6895392027eba016b0b8bcb90ccb264fc`.
- Physical multi-package power-cycle persistence and PC/SC-host GPPro testing
  remain open. This step is host-only; no hardware was used. Implementation
  commit: `3b45bd5`; progress/TODO/handoff follow-up commit: `796eb87`.
- Next: continue card-manager persistence and the remaining in-scope VM work;
  keep `asymm_crypto` paused.

# Previous implementation step — multi-package VPCD process resume (2026-10-25)

- Added repeatable `--restore-applet ID CLASS APPLET_AID INSTANCE_AID
  CAP_COMPONENTS` CLI records. Resume validates each component image, checks
  package and applet AIDs against its CAP, rejects conflicting package IDs or
  duplicate instances, reconstructs package-instance registry entries, and
  requires `--applet-aid` to identify a restored instance.
- Expanded `tools/test-vpcd-multipackage` to close and save the first emulator,
  start a fresh process, restore Core and Multi packages with three instances,
  verify authenticated GET STATUS, select every restored applet, and then run
  DELETE cleanup. It passes `TEST:SUMMARY pass=18 fail=0`.
- Build: `./tools/build-pc` passed and ROM-mask generation completed. The SDK,
  ROM-mask tools, Rust OS, and existing packed CAP structures emit legacy
  warnings; no new warning from the restore code was observed.
- Run log:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261025T064820Z.log`, SHA-256
  `9ff04323f8b1ff59e6aedf4360b12ccb4ec61dbd8f2fb3786e90d073539c8578`;
  restart log:
  `artifacts/logs/host-vpcd-multipackage-restart-20261025T064820Z.log`, SHA-256
  `e741d7d3488c759198e4ae78f7f51b7620a2c6468480a9e2cba105ca2d6deee1`.
- Remaining: automatic CAP/package/class discovery from saved PC VPCD storage
  and hardware power-cycle validation remain open. This emulator option takes
  CAP component images and instance metadata explicitly. Do not resume paused
  `asymm_crypto` work.
- Source and validation commit: `9dc0fe9`.

# Previous validation — host VPCD multi-package lifecycle (2026-10-25)

- `./tools/build-pc` passed against the current worktree. The build emitted
  existing converter, Maven, and packed-member warnings; ROM-mask generation
  completed successfully.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage --timeout 180`
  passed all 13 authenticated SCP03 lifecycle checks: two packages, three
  instances, GET STATUS, SELECT, unknown-AID rejection, instance deletion,
  refusal to delete a package with live instances, cascade deletion, and an
  empty final registry. Log:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261025T063857Z.log` (SHA-256
  `ef41bfa1d1f859dccec8fd8266dc639e4769d456aed147ad242e6ea82ad351d2`).
- The source changes being validated belong to the pre-existing worktree and
  remain uncommitted; only documentation for this validation is being recorded.
- Next: continue the open GET STATUS continuation and persisted multi-package
  process-restart work. Do not resume the paused `asymm_crypto` implementation.

# Previous implementation step — host VPCD multi-package DELETE (2026-10-25)

## Empty `symm_crypto` image delivered and card registry cleaned — 2026-10-25

- Built `symm_crypto` for ARM and RISC-V through `./tools/cycle`; both wrapper
  builds passed. The first target, BOOTSEL board 8E
  `8E464F4E2BBC0A19`, accepted a 100%-verified image write, but its runtime
  serial did not enumerate and it was absent from PC/SC. Do not count 8E as a
  runtime pass; cause is not established.
- Used the healthy failover Pico 2 2BED `2BED26ED20556535`. Its ARM image
  verified at 100%; the finite 18-second monitor repeatedly reported
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. The image was saved
  as `artifacts/firmware/pico2-jcvm-symm_crypto-empty-card.uf2`, 1,676,800
  bytes, SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`.
- GPPro showed that 2BED retained the JCAlgTest and JavacardxSec CAP packages
  after firmware upload. Deleted each listed applet before its package, then
  verified `--list` showed only ISD `A000000151000000`. This is the required
  empty card state; firmware upload alone preserves persistent applet storage.
  No JCAlgTest CAP was reinstalled, so the card remains empty for the user's
  separate Windows install test.
- Evidence: failed 8E cycle
  `artifacts/logs/cycle-choupi-20261025T061139Z.log` (SHA-256
  `7783e8bd4f5529b98b44b4f924c4ad8a68c7cac6c5f2979c3e45757ee8af03b2`);
  successful 2BED cycle
  `artifacts/logs/cycle-choupi-20261025T061403Z.log` (SHA-256
  `31e200ad94ee7a1888653f4fe4a4f88a65e88498a8d89ed9e456bb45ec837c84`);
  GPPro final listing is saved at
  `artifacts/logs/gppro-empty-symm-2BED-final-list-20261025.log`; the individual
  delete command logs are in `artifacts/logs/gppro-empty-symm-2BED-delete-*`.
- Next: share this UF2 and the existing Windows PC/SC/GPPro commands with the
  user. If another device task is requested, use 2BED as currently responsive;
  8E needs USB/runtime re-enumeration diagnosis before further use.
- Documentation, firmware artifact, and progress record commit: `fc2996c`.

## Physical PC/SC multi-package validation — 2026-10-25

- Three boards enumerated: D92 `D92E743D5F640C0F`, 2BED
  `2BED26ED20556535`, and reserve BOOTSEL board `8E464F4E2BBC0A19`.
- Built/flashed the current ARM CHOUPI image to D92 and 2BED with the required
  wrapper. Both cycles passed ARM/RISC-V builds, verified the flash, and showed
  repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` in the
  finite monitor. Image SHA-256:
  `cbd741c4f379b3738141af73b6a9a6c3b7f6c43b925a74c4f359d1bb789ac04e`.
- D92 was visible to PC/SC but GPPro `--list` timed out, including after reader
  helper refresh. No CAP writes were attempted on D92. Failover to 2BED worked:
  GPPro listed its pre-test JCAlgTest and JavacardxSec packages, then the
  official API 3.0.5 Core and Multi CAPs were loaded. Multi applet A and B were
  installed/listed/selected; the second instance required `--install-only`
  because another `--install` repeats LOAD and receives `6A80`.
- Core API runner on 2BED completed 291 calls: 272 `OK`, 15
  `UNSUPPORTED_PER_SPEC`, and four optional Ext `APPLET_NOT_INSTALLED`. The
  four MultiSelectable methods report unsupported because logical-channel
  opening returns `6D00`; ordinary AID SELECT for both Multi instances passed.
- Cleanup deleted both Multi instances, the Core instance, and both packages.
  Final GPPro `--list` exactly matched the pre-test state: ISD, JCAlgTest, and
  JavacardxSec only. Reserve 8E remained BOOTSEL and was not used.
- Evidence: cycles
  `artifacts/logs/cycle-choupi-20261025T054429Z.log` (D92, SHA-256
  `704355714f3dc157fc200dd2dcbbe9c3939ea126e4f60d6c1ba68af4b35cc974`) and
  `artifacts/logs/cycle-choupi-20261025T054813Z.log` (2BED, SHA-256
  `76625cc9f51df6df784a7cf8edbd154b78b4c4c06cb6ce12336edcbc3964d6de`);
  power-up probe `artifacts/logs/pico-powerup-persistence-20261025T055009Z.log`;
  runner CSV `artifacts/results/deploy/physical-multipackage-2BED-20261025/core-with-multi.csv`;
  cleanup listing `artifacts/logs/gppro-final-list-2BED-multipackage-20261025.log`.
- Next: close automatic package/class discovery and multi-package process
  restart reconstruction in host VPCD; keep GET DATA and multi-entry
  continuation open. Physical multi-package operation works on 2BED, but
  physical power-cycle reconstruction was not tested.

- Host VPCD DELETE used to panic because namespace prefix erasure called the
  host filesystem directly outside privileged syscall context. It now uses
  `FsErasePrefix`; the syscall checks namespace write permission and calls
  `FileSystem::erase_prefix`.
- The raw authenticated SCP03 multi-package test passes 13/13. It loads the
  upstream Core and Multi CAPs, installs three applets, checks GET STATUS and
  dynamic SELECT, deletes one instance, confirms package DELETE is refused
  while an instance remains, cascades deletion of Multi, and deletes Core so
  the registry ends empty.
- `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` pass.
  Regression command:
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage --timeout 180`.
  Log: `artifacts/logs/host-vpcd-multipackage-apdu-20261025T050904Z.log`,
  SHA-256 `8123442ca2a86d4d1cdc24d77a73d8b0312c3207c3404c58a4836d81ef0ef829`.
- Hardware status: despite the report that tokens are in BOOTSEL, authorized
  host-access `lsusb -d 2e8a:000f` and `picotool info -a` found no device in
  this VM. No physical flash was attempted. Recheck after USB passthrough
  becomes visible; PC/SC GPPro integration remains open.
- Multi-package process-restart reconstruction and package/class discovery
  remain open. Preserve unrelated asymmetric-profile changes in the worktree.
- Source commit: `c6a5f60`; OS submodule commit: `5c07f44`.

# Previous implementation step — host VPCD multi-package registry (2026-10-25)

## Follow-up — raw authenticated multi-package VPCD test (2026-10-25)

- Dynamic SELECT routing is now registry-based. The first test exposed that
  `Session` only routed the initially configured AID; it now passes any valid
  SELECT AID to the installed-instance restore callback and returns `6A82` for
  unknown AIDs. Removed the now-unneeded initial AID from the session object.
- Added `tools/test-vpcd-multipackage` and its Python harness. It speaks raw
  VPCD, selects ISD, authenticates SCP03 with repository test keys, loads the
  unchanged upstream API 3.0.5 Core and Multi CAPs, installs three applets
  across two packages, checks GET STATUS paging, verifies unknown-AID rejection,
  and selects all three instances.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. The test passed `TEST:SUMMARY pass=9 fail=0`.
  Log: `artifacts/logs/host-vpcd-multipackage-apdu-20261025.log`, SHA-256
  `9fbf1ba9ea90b4a36a781c75614fcc84badb9b4625aa177897c14a2e597db9d9`.
- PC/SC GPPro integration and physical CCID are still unverified. Although the
  user reported BOOTSEL tokens connected, host-access `lsusb -d 2e8a:000f`
  returned no device and `picotool info -a` reported none accessible; no board
  was flashed. Recheck USB passthrough only after VM enumeration changes.
- The earlier 30/30 single-package/restart VPCD test remains evidence for that
  separate path. Multi-package process-restart reconstruction and PC/SC GPPro
  GET STATUS remain open. Existing user changes for asymmetric crypto stay
  untouched.
- Source commit: `e05fec0`. Follow-up metadata commit will record the final SHA.

- `choupi/src/main_pc.cpp` now mirrors the device package store with an
  in-process registry keyed by dynamic package ID. It supports multiple
  package AIDs/CAP images and multiple installed applet instances per package.
  Package lookup, CAP retrieval, instance lookup/removal, and GET STATUS
  enumeration now use the registry.
- `CapLoadSession` exposes its resolved package ID. The VPCD applet restore
  callback uses that ID instead of the CLI's initial package ID, which fixed a
  SELECT `6F00` caused by restoring a newly allocated package under ID 63.
- GET STATUS now builds entries across all loaded host packages and applet
  instances, with `6310` continuation when a page fills. The PCSC/GPPro
  multi-package integration check remains unverified: this VM's PC/SC has no
  `Virtual PCD 00 00` reader registered, so GPPro stopped before any CAP
  upload. Do not claim multi-package end-to-end success yet.
- Validation: `./tools/build-pc`, `./tools/build arm`, `./tools/build riscv`
  passed. Final raw VPCD regression:
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-pc-vpcd-applet --passes 2
  --apdu-timeout 60 --log
  artifacts/logs/host-vpcd-multipackage-registry-20261025.log` passed 30/30.
  Pass 1 took 57.868 s; pass 2 took 19.117 s. The same test at its default
  20-second APDU bound timed out during GETVERSION; the bounded retry passed.
- Host package registry is process-local. Reconstructing several dynamic CAP
  images across emulator process restarts remains open. The firmware's native
  package store was not changed, and no Pico was flashed because this is a
  PC/VPCD-only change.
- Next: enable or provide a registered PC/SC VPCD reader (or add a raw
  authenticated GP command harness), then validate two packages and two
  instances through GPPro GET STATUS, pagination, SELECT, and DELETE. Continue
  to preserve the unrelated asymmetric-profile worktree changes.

# Latest implementation step — JCSystem transaction commit capacity (2026-10-25)

- Implemented `getMaxCommitCapacity()` / `getUnusedCommitCapacity()` with the
  Java Card short ceiling `0x7FFF`. Capacity accounts the pre-write snapshot
  payload for existing persistent records, encoded tag, and journal metadata;
  newly created records charge only their erase journal. Repeated writes to one
  tag do not double-charge. Overflow raises the VM `TransactionException` with
  reason `BUFFER_FULL` before mutating the rejected record.
- Validation passed: native persistent-transaction suite (`fail=0`), full host
  VPCD JCSystem applet regression (`84/84`), PC build, and ARM/RISC-V
  `symm_crypto` builds. VPCD verified max and unused capacity `0x7FFF` outside
  a transaction and active unused capacity `32755` after one persistent scalar
  write.
- Final physical validation remains incomplete. The cycle rebuilt both
  architectures, but D92 `D92E743D5F640C0F` did not enter BOOTSEL after the
  reset-interface and CDC fallback attempts. The subsequent authorized host
  enumeration showed only Linux root hubs, and picotool found no accessible
  BOOTSEL board. No final image was flashed in this attempt. When a Pico is
  passed through to the VM again, verify identity and rerun
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile symm_crypto
  --chipid D92E743D5F640C0F` with host access. PC/SC transaction behavior has
  not been validated; bounded large-array overflow probe previously timed out,
  so Java-level catchability at actual capacity exhaustion remains open.
- Current card storage/registry is unknown. Do not issue package writes until
  PC/SC responds and GET STATUS can be read. Preserve existing card data.
- Final ARM image built: `build/arm/pico2_choupi.uf2`, 1,676,800 bytes,
  SHA-256 `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`.
  VPCD log: `artifacts/logs/jcsystem-transaction-capacity-final-vpcd-20261025.log`.
  Failed final flash attempt:
  `artifacts/logs/cycle-choupi-20261025T034755Z.log`.
- Source/docs commit: `78115dc`. Unrelated asymmetric-profile changes remain
  in the worktree; do not stage them as part of the transaction-capacity step.


# Latest physical task — `symm_crypto` flash, explicit install, static benchmark

## Latest physical API validation — current-profile Core deploy run (2026-10-25)
- On Pico 2 `2BED26ED20556535`, the unchanged API 3.0.5 Core area completed
  291 calls: 272 `OK`, 15 `UNSUPPORTED_PER_SPEC`, and four Ext scenario rows
  unavailable after the expected Ext CAP INSTALL refusal (`6A88`).
- JCSystem subset: 37 `OK`; one spec-permitted `requestObjectDeletion()`
  `ILLEGAL_USE`. No JCSystem wrong-result/runtime-error rows.
- The runner deleted the installed Core and Multi CAPs. Final GPPro listing
  showed only ISD, leaving the card empty. No firmware source or CAP was edited.
- CSV: `artifacts/results/deploy/current-2BED-20261025/core.csv` (SHA-256
  `edfb7f6827b1051c837e468cd3db45db67f1fb88d00e7d9aca6d9a9f8c8afc9e`).
  Final card listing: `artifacts/logs/gppro-current-profile-2BED-after-core-20261025.log`
  (SHA-256 `d32b48744874e072169c2e981ba62ea900919fe3726cdf0dc3ac180c93deab6`).
- Security completed 901 calls: 77 `OK`, 822 `UNSUPPORTED_PER_SPEC`, and two
  `WRONG_RESULT` follow-ups after unsupported transient-key setup. The known
  classification is documented in `.agent/QKB.md`; no transient-key clearing
  defect was reproduced.
- Crypto completed 466 calls: 22 `OK`, 444 `UNSUPPORTED_PER_SPEC`, no
  unexpected results. The upstream-documented RSA-2048 keygen exclusion was
  used. GPPro cleanup listing after both areas showed only ISD.
- Security CSV SHA-256 `96c3ee05fdda77179075fa809f4be65cdc4bf8825fa6dd3cd829d8c5fd3eece4`;
  Crypto CSV SHA-256 `447d8bdc81fb57c0d8b713d32973451befc391e1155dd28e12d2968465c3aafc`.
  Final listing log: `artifacts/logs/gppro-current-profile-2BED-after-security-crypto-20261025.log`
  (SHA-256 `168bcb2ab02f8e0b23e974a906f8d0261cb5a33944c491a1d59c21f0bc900878`).
- Security/Crypto validation record commit: `7c7fd11917e22246cdd026c9e82509ab9a555c25`.
- Validation record commit: `9c652bfb317f4145bb89abf634eaba99f8e66d20`.
- Current-profile `JavacardxSec` passed 84/84; Service passed 124/124. Both
  cleanup operations passed. `Javacardx`, `JavacardxExt`, `JavacardxInt`, and
  Biometry each returned INSTALL `6A88` for missing imports and were rolled
  back. The final physical GPPro listing showed only ISD.
- Artifacts: `javacardxsec.csv` SHA-256
  `01dde4a24b966199ebd9e6a8b067684b6dc8bb7e7c4dccc308e645d4e39c406a`;
  `service.csv` SHA-256
  `cb337f40cedbda61a0bd88c38f0cad509e68184731852dab8b4c261157fada97`;
  `optional-areas.csv` SHA-256
  `22b2683699e41b585c1ce770da73d09a4609ee55abf4befd9b42c60a5d5abe3c`;
  final listing log SHA-256
  `168bcb2ab02f8e0b23e974a906f8d0261cb5a33944c491a1d59c21f0bc900878`.
- Service/optional-area validation record commit: `2450cc5e66f09fd75e3e09cf754423637b9c9a2e`.
- Next: inspect current physical API coverage against the remaining TODO and
  fix the highest-priority reproducible JCVM failure. Windows visibility and
  true physical power-cycle persistence remain open. Keep `asymm_crypto` paused.

## Current continuation — D92 multi-package transaction-probe attempt (2026-10-25)

- Rebuilt ARM and RISC-V with `symm_crypto`; both wrappers passed. Flashed ARM
  to D92 `D92E743D5F640C0F` using the required host-access cycle. Image
  write/verify passed and the 18-second serial monitor repeatedly reported
  `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`.
- D92 already had the explicitly installed upstream JCAlgTest CAP. To avoid
  replacing it, built a transaction-probe CAP with a distinct package AID
  `F01234567890` and applet AID `F0123456789001`. GPPro completed LOAD but did
  not return from INSTALL after about one minute; the command was interrupted.
  Do not claim the probe applet is installed or deleted. The previous JCAlgTest
  package was not intentionally removed.
- After a second full flash/reboot recovery, serial runtime still passed, but
  bounded GPPro list/raw APDU and SmartcardIO probes continued to time out.
  PC/SC listed the reader and recognized an inserted card with ATR `3B800181`.
  CCID monitoring showed repeated GetSlotStatus requests; refreshing ACLs and
  restarting pcscd via the authorized helper did not restore APDU access. No
  BOOTSEL failover board was visible to picotool. See `.agent/QKB.md` issue
  “D92 PC/SC APDU path stalled after second-package load attempt”.
- This does not establish a passing physical transaction test or a clean
  package registry. Preserve current persistent card data. Before another
  physical package write, restore working PC/SC access and inspect GET STATUS.
  Continue offline with native JCSystem/transaction work; do not modify
  upstream CAPs or add Java crypto/provider code.
- Image SHA-256:
  `666028ffdf62cf0038ed4e082efefdce8e3954882d3278cb7903d8a6374d51f2`.
  Cycle logs: `artifacts/logs/cycle-choupi-20261025T025305Z.log` and
  `artifacts/logs/cycle-choupi-20261025T025904Z.log`. Transaction-probe CAP
  SHA-256 `d12675f53b2ec96657cd51d0ea8392735a6aeb3a32383caf1911e50562d2addc`.

- Both `./tools/build arm --profile symm_crypto` and
  `./tools/build riscv --profile symm_crypto` passed. The ARM image is
  `build/arm/pico2_choupi.uf2`, 1,675,776 bytes, SHA-256
  `666028ffdf62cf0038ed4e082efefdce8e3954882d3278cb7903d8a6374d51f2`.
- The repository cycle flashed identified Pico 2 D92
  `D92E743D5F640C0F`; it passed the 18-second monitor with repeated
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0` lines. The initial
  BOOTSEL selector attempt failed because the reset interface was absent; the
  cycle recovered using the board's unique CDC serial and 1200-baud fallback,
  then completed a verified 100% flash. Log:
  `artifacts/logs/cycle-choupi-20261025T013632Z.log`.
- Refreshed PC/SC and used GPPro to delete the old JCAlgTest applet and package.
  `--list` then showed only ISD `A000000151000000`. GPPro explicitly loaded and
  installed the unchanged upstream CAP (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`); final
  listing shows package `4A43416C6754657374` `LOADED` and applet
  `4A43416C675465737431` `SELECTABLE`. This confirms no CAP is embedded in the
  image. Current card state: applet installed by the explicit request.
  Evidence: `artifacts/logs/gppro-empty-install-D92-20261025.log`.
- The physical official `ALG_PERFORMANCE_STATIC` run on that image used a
  3,600-second timeout and exited 124 at the configured limit while the client
  was measuring DES MAC. Its partial CSV has 3,127 rows / 675 methods: 540
  `NO_SUCH_ALGORITHM`, 67 `CANT_BE_MEASURED`, 68 measured outcomes. There were
  no unexpected SWs, `6F00`, Java exceptions, `TEST:FAIL`, or `FAULT`. SHA-256
  completed at 3.48 ms/op (`doFinal`), 4.08 (`update`), and 5.67 (`reset`), at
  256 bytes / 250 iterations. Secure random measured 1.74 ms/op (`generateData`)
  and 2.79 (`setSeed`). Measured DES `Cipher_init` cases were 39.0, 47.8, and
  56.0 ms/op for DES / 2-key 3DES / 3-key 3DES. AES `Cipher_init` was 59.4–60.9,
  68.2–68.8, and 76.7–77.5 ms/op for 128/192/256-bit keys. Partial log:
  `artifacts/logs/jcalgtest-symm-crypto-static-D92-20261025-final.log`; CSV:
  `artifacts/results/jcalgtest-symm-crypto-static-D92-20261025-final/CHOUPI_symm_crypto_D92____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED_1792892534007__3b_80_01_81.csv`.
- After the timeout, the physical SELECT, GETVERSION, and JCSystem info probe
  passed (`9000` for all three APDUs), confirming the applet/card remained
  responsive. Log: `artifacts/logs/jcalgtest-post-static-timeout-D92-20261025.log`.
- Current card state remains JCAlgTest explicitly installed. The full
  `ALG_PERFORMANCE_STATIC` suite is not complete: there is no JCAlgTest option
  to filter a subset, and the full RSA/method matrix exceeded one hour. Do not
  call this a complete run. The focused SHA-256 result is sensible; no source
  fix is indicated by this measurement. The earlier completed full static run
  recorded in TODO remains historical evidence for its earlier image.
- The working tree still has unrelated asymmetric-prototype and other
  pre-existing dirty files. Preserve them; stage only the progress, handoff,
  and TODO record for this hardware step. Windows testing remains out of scope.

# Latest physical firmware and explicit CAP installation — 2026-10-25

- The user said Pico 2 tokens were available in BOOTSEL. In this VM only one
  device was visible (`2e8a:0009`), runtime serial D92
  `D92E743D5F640C0F`. The repository cycle automatically requested BOOTSEL;
  it flashed and verified the `symm_crypto` ARM image at 100%.
- ARM and RISC-V `symm_crypto` builds passed. The 18-second physical monitor
  repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`; cycle result was `CYCLE:PASS`.
- GPPro `--list` immediately after the flash showed only ISD
  `A000000151000000`. This proves the image did not preload JCAlgTest. It also
  confirms the earlier apparent preinstallation was persistent CAP storage
  from a previous explicit install; reflashing firmware does not erase that
  store.
- As requested, then explicitly installed the unchanged upstream
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` using GPPro. SHA-256:
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  GPPro now lists package `4A43416C6754657374` as `LOADED` and applet
  `4A43416C675465737431` as `SELECTABLE`. SELECT, GETVERSION
  (`1.8.2_jc305`), and JCSystem info probes all returned `9000`.
- The official client then completed `ALG_SUPPORT_EXTENDED` over the physical
  reader: 4,525 matched result rows, 73 `yes`, 4,452 `no`, and no other CHOUPI
  statuses. Comparison with the archived jCardSim report and generated
  `symm_crypto` manifest passed: 46 expected/actual capability identities,
  zero unexpected and zero missing. Unsupported algorithms remain expected.
- Support artifacts: `artifacts/results/jcalgtest-symm-crypto-D92-20261025/CHOUPI_symm_crypto_D92_ALGSUPPORT__3b_80_01_81.csv`
  (SHA-256 `31a530948b195a5ce1a6f57d391b826a10ff8b51b92b0495c199b3cfefacfd1e`),
  run log `artifacts/logs/jcalgtest-symm-crypto-D92-20261025.log`, comparator
  log `artifacts/logs/jcalgtest-symm-crypto-compare-D92-20261025.log`.
- **Current card state:** JCAlgTest is installed because the explicit install
  just succeeded. To return this card to empty, delete applet AID
  `4A43416C675465737431` first, then package AID `4A43416C6754657374`, and
  confirm `gp --list` shows only ISD.
- UF2: `build/arm/pico2_choupi.uf2`, 1,675,776 bytes, SHA-256
  `666028ffdf62cf0038ed4e082efefdce8e3954882d3278cb7903d8a6374d51f2`.
  Cycle log: `artifacts/logs/cycle-choupi-20261025T002953Z.log`. Final GPPro
  and APDU evidence: `artifacts/logs/gppro-explicit-jcalgtest-D92-20261025.log`.
- No source/CAP files changed. Previous dirty asymmetric-prototype files were
  preserved. Next action: continue the native JCVM backlog and keep the
  physical registry state explicit. User instruction excludes Windows checks
  and Java-side crypto providers/adapters.

## Selectable-profile API matrix and OwnerPINx cross-profile verification — 2026-10-25

- The user said tokens were in BOOTSEL. VM USB discovery showed only the
  identified D92 board, which had already returned to runtime after prior work.
  Repository cycle wrappers automatically re-entered BOOTSEL and flashed it.
- Current profile Core after the checkcast fix: 291 calls, 272 OK, 15
  `UNSUPPORTED_PER_SPEC`, four optional Ext `APPLET_NOT_INSTALLED`; all
  OwnerPINx rows pass. Cleanup passed. Current Security: 901 calls, 77 OK,
  822 unsupported, two dependent transient-key `Key.isInitialized()` wrong
  results (setup key types unsupported). Current Crypto: 466 calls, 22 OK, 444
  unsupported. Both cleanup stages passed.
- No-AES profile Core after the fix: 291 calls, 272 OK, 15 unsupported, four
  optional Ext calls unavailable; all OwnerPINx rows pass. Cleanup passed.
  Both architecture builds passed and the ARM cycle monitor passed.
- Minimal profile was flashed with both architecture gates passing. Its Crypto
  CAP uploaded, ran all 466 calls (4 OK, 462 `UNSUPPORTED_PER_SPEC`), and
  cleaned up. The earlier `6F00` INSTALL failure did not reproduce.
- GPPro was checked after test cleanup; only ISD `A000000151000000` is present.
  The card is empty of test applets/packages. D92 is currently running `no_aes`.
- Evidence CSVs: `artifacts/results/deploy/current-D92-core-ownerpinx-fixed-20261025.csv`,
  `current-D92-security-20261025.csv`, `current-D92-crypto-20261025.csv`,
  `minimal-D92-crypto-20261025.csv`,
  `no-aes-D92-core-ownerpinx-fixed-20261025.csv`. Cycle logs include
  `artifacts/logs/cycle-choupi-20261024T235250Z.log` (current) and
  `artifacts/logs/cycle-choupi-20261025T000109Z.log` (no-AES).
- Next: continue the remaining API/runtime backlog. JCAlgTest 1.8.2 is not to be
  run on minimal, and its no-AES eager-startup `F103` is an upstream CAP/profile
  incompatibility. Preserve the empty physical registry; firmware flash alone
  does not uninstall applets.

## OwnerPINx `checkcast` regression fixed and physically verified — 2026-10-25

- The user reported Pico tokens in BOOTSEL. Rebuilt and flashed the identified
  D92 `symm_crypto` board after the VM-side fix. ARM and RISC-V builds passed;
  the ARM write and verify reached 100%, then the finite monitor repeatedly
  reported `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, and
  `CYCLE:PASS`.
- Root cause was namespace ambiguity in `checkcast`: JC305 imported class and
  interface export tokens can share a byte value. The old code resolved a
  class candidate successfully even though it was unrelated, and did not try
  the valid OwnerPINx interface candidate. `Bytecodes::docheck` now resolves
  both independently and checks the interface if the distinct class candidate
  does not match. Source commit: `fa15bfa`. No Java provider or CAP changes.
- PC/VPCD full regression passed 84/84. On physical D92, the unchanged API 3.0.5
  Core CAP completed 291 calls: 272 OK, 15 `UNSUPPORTED_PER_SPEC`, and four
  unavailable optional Ext calls. All OwnerPINx cases pass; the previous 17
  runtime errors and two wrong results are gone. Ext INSTALL refusal `6A88`
  was rolled back. The runner removed its Core/Multi CAPs; final GPPro `--list`
  contains only ISD `A000000151000000`. This card is empty of test applets.
- Artifacts: `artifacts/logs/jcsystem-ownerpinx-profile-fixed-full-vpcd-20261025.log`,
  `artifacts/logs/cycle-choupi-20261024T232303Z.log`, and
  `artifacts/results/deploy/symm_crypto-D92-ownerpinx-fixed-20261025.csv`.
  Image SHA-256: `3add9fdaac80f697e694d68ea1510952b3e9b156f676c966f3a763e875323fbf`.
- Next: proceed with the prioritized native JCVM work / remaining TODO items.
  Keep the physical registry empty unless a specific CAP test is requested;
  firmware reflashing does not erase the persistent card filesystem.

## Latest physical run — empty `symm_crypto` image and OwnerPINx reproduction (2026-10-25)

- User said the Pico tokens were available in BOOTSEL. The VM initially saw
  only Pico D92 `D92E743D5F640C0F` in runtime CCID mode. The authorized wrapper
  requested BOOTSEL through the CDC 1200-baud fallback after picotool could not
  find a reset interface; BOOTSEL appeared after 3 seconds.
- Both `./tools/build arm --profile symm_crypto` and the RISC-V wrapper passed.
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile symm_crypto --chipid
  D92E743D5F640C0F` flashed and verified the 1,675,264-byte image, then its
  finite monitor passed repeatedly. SHA-256:
  `ff043cac0a8a5fc4770aa8783232e7da61a37fd90d01573f990137bfbbbbdbf4`.
- After re-running `sudo -n /usr/local/sbin/pico2-pcsc-reader`, PC/SC exposed
  the reader. GPPro `--list` showed only ISD `A000000151000000`; the physical
  registry is empty. Firmware does not preload JCAlgTest. Future applet tests
  must use explicit `gp --install` with the unchanged upstream CAP.
- The physical upstream API 3.0.5 Core rerun uploaded the Core CAP, completed
  291 calls, and cleaned up successfully. Counts: 253 OK, 15
  `UNSUPPORTED_PER_SPEC`, 17 `EXC_RUNTIME`, 2 `WRONG_RESULT`, and 4 optional
  Ext `APPLET_NOT_INSTALLED` calls because Ext INSTALL returned `6A88` and its
  rollback succeeded. All OwnerPINx variants comprise the 17 runtime errors
  and two wrong results; normal OwnerPIN rows pass. This reproduces the prior
  failure on a freshly flashed image. No source change has been made to address
  it yet.
- The runner needs SmartcardIO's full exact reader string including the serial
  and slot: `Pico 2 CHOUPI CCID [JCVM CCID] (D92E743D5F640C0F) 00 00`. The shorter
  `pcsc_scan` display name is not accepted by its exact-name resolver. Refresh
  PC/SC ACL after each USB re-enumeration before resolving readers.
- After Core runner cleanup, GPPro `--list` with the full reader string again
  showed only ISD `A000000151000000`; no test CAP remains installed.
- Evidence: `artifacts/logs/cycle-choupi-20261024T225805Z.log` and
  `artifacts/results/deploy/symm_crypto-D92-ownerpinx-repeat-20261025.csv`.
- Next: trace `invokeinterface` method mapping for OwnerPINx and its parent
  interfaces in the C++ JCVM. Do not add Java providers/adapters or modify
  upstream CAPs. Rebuild both architectures before any next flash; preserve
  the empty D92 registry and use another identified board if D92 disappears.

## Architecture layering analysis — 2026-10-24

- Extended `FIRMWARE_JCVM_ARCHITECTURE.md` to 4,217 words, distinguishing
  hardware, HAL, native OS services, JCRE/card management, JCVM execution,
  native APIs/crypto, and installed applets. Added ten source-backed layering
  findings, a desired dependency flow, and staged refactoring criteria.
- Corrected the boot description: Pico consumes the PC-generated ROM mask,
  whose selected entry is the empty `Starter.runVPCD()`. Java `.class` files
  remain build intermediates; the embedded executable representation is
  Java Card CAP components. Existing interpreted API providers are documented
  as debt against the required native C++ API boundary.
- Recorded existing CAP metadata mirrors and the INSTALL recovery journal;
  did not claim cross-store recovery is wholly absent. Key open boundaries:
  duplicated CCID/VPCD card policy, board-owned storage semantics, VM-dependent
  `fs_sync`, scattered service ABI/platform hooks, and host storage mismatch.
- Validation was documentation/source inspection only: all 16 local Markdown
  links resolve, ten findings are present, code fences balance, and document
  whitespace checks pass. No build, flash, USB/PCSC access, or new runtime
  measurement occurred. Existing uncommitted implementation changes remain
  outside this documentation step.
- Next implementation should establish explicit owners/contracts, share card
  session policy across transports, coordinate durable storage, and move API
  behavior into native dispatch while preserving upstream CAPs and recovery
  mechanisms. See the document for ordering and acceptance conditions.

## RSA cipher providers and physical vectors — 2026-10-24

- Added profile-gated RSA `ALG_RSA_PKCS1`, `ALG_RSA_NOPAD`, and SHA-1 OAEP
  ciphers. Java provider performs Java Card key/mode validation and buffering;
  native bridge performs modular exponentiation and PKCS#1/OAEP encoding.
  Added Mbed TLS bignum sources and Pico millisecond clock hook only when RSA
  cipher capabilities are selected.
- Fixed JC305 RSA public/private and CRT method-token remapping. Before this,
  RSA key setters could dispatch to getters and fail with `6F00`.
- Validation: support generator 8/8; probe build; VPCD `TEST:SUMMARY pass=20
  fail=0`; ARM and RISC-V builds passed; ARM cycle and finite serial monitor
  passed on both `D92E743D5F640C0F` and `2BED26ED20556535`. Probe CAP install
  on 2BED succeeded and physical tests passed 18/18. D92 install returned
  `6581` (memory failure) after LOAD, so its JCAlgTest package was preserved.
- Physical vectors are recorded in `build/symm-crypto-probe/asymm-physical-vectors.tsv`;
  VPCD log: `artifacts/logs/asymm-crypto-digest-vpcd-20261024.log`.
- Remaining asymm profile scope: RSA SHA-1/SHA-2 PKCS#1/PSS/ISO9796 signatures,
  EC key types and ECDSA, key agreement, key generation, expected full-report
  comparator, and complete target `/deploy` acceptance. Target support CSV SHA:
  `a335e371a3f4b1bed8c74aac4b3cbe353b623f37a778e94fd14a52d00ee23dcb`.
- Next: implement RSA signatures, starting with PKCS#1 and PSS digest variants.
  Keep the user-provided target CSV unchanged and untracked.

## Crypto constructor dispatch follow-up — 2026-10-22

- Added package and class scoped constructor-token remaps for JC305
  `javacard.security.Signature` and `javacardx.crypto.Cipher` based on the
  already validated `MessageDigest` pattern. Both ARM and RISC-V wrappers
  passed. ARM image flashed and verified on Pico `D92E743D5F640C0F`; its
  bounded monitor passed eight consecutive runtime checks.
- A first post-flash run through the parallel scheduler auto-selected reader
  `2BED26ED20556535`, which still had the old firmware; do not use that run as
  validation of the new mapping. After ACL refresh, pinned the exact D92 reader
  and completed the 466-call Crypto area with upload and cleanup successful.
- The fresh image fixes `Cipher()` (`SW_ERROR` row disappeared), but
  `Signature()` remains `6F00`. Remaining crypto rows: AES CBC-NOPAD init with
  IV throws `CryptoException.ILLEGAL_VALUE`; empty `doFinal` returns 0 where
  illegal use was expected. Updated CSV:
  `artifacts/results/deploy/crypto-ctor-remap-d92-20261022.csv`.
- Three boards remain USB connected; PC/SC count varied (3 after ACL helper,
  then 2 and later 1 visible to SmartcardIO). The parallel dry run requiring
  three readers did not start tests. Preserve the third board and avoid
  `pcsc_scan`.
- `.git` remains read-only, so no commit was created. The initial missed host
  escalation produced a `sudo` no-new-privileges error; all following USB work
  used the authorized elevated host path.
- Next: inspect the Signature CAP's imported class and constructor tokens to
  explain why the same mapping pattern has not resolved it. Then investigate
  valid IV parameter handling and the empty `doFinal` expected exception.

## Current-source multi-reader deploy triage — 2026-10-22

- Current ARM firmware was flashed to all three physical Pico 2 boards; the two
  additional boards passed `FLASH:PASS` and finite runtime monitors. All three
  were USB visible, but only D92 and 2BED appeared as SmartcardIO readers after
  the authorized ACL helper was refreshed. Third USB serial 8E464F4E2BBC0A19
  remained absent from PC/SC.
- Two-reader deploy run completed Core, Security, Crypto, and Service worker
  execution and cleanup. Core Ext optional applet INSTALL returned `6A88`; its
  rollback passed, but this made the runner exit 1. Core 272 OK / 15 unsupported
  / 4 Ext applet unavailable; Security 77 OK / 820 unsupported / 2 skipped / 2
  transient-key fallout; Crypto 18 OK / 444 unsupported / four unresolved rows;
  Service 124 OK. CSVs and logs are in
  `artifacts/results/deploy/current-source-two-reader-20261022/`.
- QKB now tracks the four Crypto anomalies: no-arg `Signature()` and `Cipher()`
  returned `6F00`, AES CBC-NOPAD IV init returned `CryptoException.ILLEGAL_VALUE`,
  and empty `doFinal` returned 0 where the test expected illegal use. Trace the
  CAP setup and virtual-token dispatch before deciding whether these are VM
  bugs. Keep transient-key follow-up statuses classified as unsupported fallout.
- The official run-generated already-measured list was moved under the ignored
  artifact directory. Builds passed before physical flashing. `.git` remains
  read-only; no commit was created.
- Next: inspect the exact Crypto CAP test setup/method invocation and compare
  with virtual-token mapping and ROM API methods; then fix and validate a narrow
  issue if the evidence identifies one. Afterward restore three-reader PC/SC
  enumeration and rerun coverage if available. Do not run `pcsc_scan`.

## Build-wrapper lock prevents shared ROM-mask races — 2026-10-25

- A simultaneous ARM/RISC-V build attempt raced on the shared
  `build/choupi-pc` ROM-mask directory; one LTO link failed with a bus error.
  The final sequential builds passed and the validated ARM image was flashed
  to D92.
- `tools/build` now serializes complete wrapper invocations through
  `build/.tools-build.lock` using `flock`. A concurrent post-change validation
  showed one architecture waiting while the other held the lock; both
  `symm_crypto` builds then exited 0. This changes host build orchestration
  only, not firmware bits; no second physical flash was performed.
- See `.agent/QKB.md` for the observed failure and repeatable workaround.
- Next: trace a native-only API dispatch path for the remaining asymmetric
  algorithms; preserve the dirty provider experiment and do not extend it until
  it matches the user's architecture rule. Keep D92's registry empty.

## Empty `symm_crypto` card confirmed on Pico D92 — 2026-10-25

- User reported Pico tokens available in BOOTSEL. Host discovery showed only
  physical D92 `D92E743D5F640C0F`, initially in runtime CCID mode; the cycle
  wrapper entered BOOTSEL using its CDC 1200-baud recovery path.
- Both architecture build gates passed sequentially. ARM and RISC-V
  `symm_crypto` builds passed; the first concurrent attempt was invalidated by
  contention in the shared ROM-mask build tree, and a later sequential cycle
  reran the gates before flashing.
- Flashed and verified the ARM CHOUPI image on D92. Its 18-second monitor
  reported repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`; cycle returned `CYCLE:PASS`. One
  `WATCHDOG_RECOVERY cause=other` marker appeared once and did not recur.
- Refreshed PC/SC ACL and ran GlobalPlatformPro `--list` pinned to D92. The
  only listed registry entry is ISD `A000000151000000`; the JCAlgTest package
  and applet are absent. The built firmware does not preload the JCAlgTest
  CAP; any test applet must be installed explicitly after flashing.
- Image: `build/arm/pico2_choupi.uf2`, 1,675,776 bytes, SHA-256
  `666028ffdf62cf0038ed4e082efefdce8e3954882d3278cb7903d8a6374d51f2`.
  Log: `artifacts/logs/cycle-choupi-20261025T001428Z.log`.
- Next: preserve the empty physical registry. Continue native JCVM backlog
  separately; do not extend the dirty Java-side RSA provider experiment,
  which conflicts with the user's native-C++/CAP-only architecture rule.

# Handoff

## Empty-card state clarification — 2026-10-24

- The `symm_crypto` ARM UF2 (`build/arm/pico2_choupi.uf2`) builds successfully
  with 46 selected capabilities. It contains no preinstalled JCAlgTest CAP.
- Physical Pico flash updates firmware but preserve the separate persistent
  filesystem/CAP store. The D92 board still has JCAlgTest because the earlier
  task explicitly installed it with GPPro; this persisted state was mistaken
  for applet code baked into the firmware. Do not claim that re-flashing makes
  an already-used card empty.
- Ran `PICO2_HOST_USB_ACCESS=1 ./tools/test-gppro-empty-install-vpcd` with a
  fresh zero-length flash image. GPPro's pre-install `--list` contained only
  ISD `A000000151000000`; explicit `--install` of the unchanged upstream
  JC305 CAP succeeded, and the package/app were then loaded/selectable. Full
  evidence is in
  `artifacts/results/gppro-empty-install-vpcd-20261024T212248Z/`.
- The BOOTSEL/CCID device was not visible inside this VM during this session:
  `lsusb -d 2e8a:000f` returned no device and the authorized PC/SC ACL helper
  found zero `2e8a:0009` devices. The physical old package could not be deleted
  now. When that reader returns, delete applet AID
  `4A43416C675465737431` first, then package AID `4A43416C6754657374`, and
  verify `gp --list` shows only the ISD before the next explicit install.
- Current ARM image SHA-256 is
  `ff043cac0a8a5fc4770aa8783232e7da61a37fd90d01573f990137bfbbbbdbf4`
  (1,675,264 bytes). ARM and RISC-V `symm_crypto` build wrappers passed.
- Asymmetric implementation remains paused as previously requested.
- Updated `docs/symm-crypto-windows-test.md` and
  `docs/windows-asymm-symm-test.md` so the Windows path first removes the known
  JCAlgTest applet/package, verifies no app/package entries remain, and only
  then explicitly uploads the upstream CAP. Refreshed
  `build/external-test/symm-crypto-windows-test.zip` with the current UF2 and
  updated instructions; `unzip -t` and all member hashes pass.
- Physical cleanup is still pending because no Pico USB/PCSC device appeared
  to this VM. The archive workflow prevents mistaking a persisted CAP for
  firmware contents and prevents running the requested explicit install on an
  already-populated card.

## Runtime format correction — 2026-10-24

- Correction to the previous handoff note: the host `.class` outputs are
  intermediate only. The Java Card converter emits JCA, then the ROM-mask
  generator converts each JCA package to CAP components before embedding it.
  The Pico interprets Java Card CAP bytecode, not standard JVM bytecode. The
  PC build has 170 intermediate `.class` files, 20 JCA files, and zero `.cap`
  files written to disk; CAP components are generated in memory by
  `JCAConverter.converter()`.
- Therefore the build representation itself is not a JVM bytecode runtime
  problem. The user's implementation constraint still applies: crypto
  algorithms belong in native C++ API dispatch, not Java provider/adapter
  method bodies or edited upstream applet CAPs.
- The root TODO's earlier Priority 0 claim was removed and replaced with this
  verified format description. Do not remove the ROM-mask API packages as a
  response to the earlier mistaken interpretation.

## Multi-board testing rule — 2026-10-24

- Added to `SKILL.md` and `.agent/SESSION_CONTINUITY.md`: when multiple
  identified Pico 2 boards are available, continue on another board if one is
  unavailable or unresponsive. Record board identity, image, per-device result,
  and keep each failed board's evidence. Do not make a single board failure
  stop the full test sequence.

## Native instance-method dispatch — 2026-10-24

- The ROM-mask generator now includes a `jref_t` receiver in generated C++
  declarations and passes it for non-static native methods. Its source commit
  is `7704b0b` in `choupi/javacard-sdk/rommask`.
- `NativeImplementation.selectingApplet()` accepts the new receiver ABI.
  Mbed TLS platform alternatives are selected for the Pico target, avoiding
  POSIX time/zeroize dependencies when RSA is disabled or enabled.
- Validation: support-generator tests 8/8; asymmetric ARM and RISC-V builds
  passed; physical ARM cycle passed on `2BED26ED20556535` with repeated
  `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, and
  `CYCLE:PASS`. Log: `artifacts/logs/cycle-choupi-20261024T163247Z.log`.
- The uncommitted RSA Signature Java provider/bridge experiment was removed.
  Signature flags are not in the current profile configuration, and these
  algorithms remain unsupported. Existing interpreted Java crypto providers
  are still a known architecture violation; migrate the implementations to
  direct native C++ API dispatch. The new receiver ABI is groundwork for that.
- Next: inspect native class/object field APIs and implement an end-to-end
  direct native key operation, beginning with RSA key setters/getters, without
  adding Java provider methods or editing upstream CAPs.

## Post-flash PC/SC availability — 2026-10-24

- The asymmetric ARM image was flashed to Pico `2BED26ED20556535` and its
  finite monitor passed. The subsequent upstream Crypto deploy run could not
  start because SmartcardIO found no physical reader.
- The ACL helper found two USB nodes and granted access; `lsusb` saw two
  `2e8a:0009` devices, while bounded `pcsc_scan -r` timed out with no output.
  `pcscd.service` was active/running. Cause is unknown. The QKB now records this
  distinction; do not retry APDUs until exact PC/SC reader names are visible.
- Continue implementation offline. The next code task is direct native RSA key
  API dispatch using the newly supported instance receiver; the active RSA
  cipher and key providers still contain interpreted Java method bodies and
  must be migrated before they meet the user's native-only requirement.

## Asymmetric crypto profile — SHA provider groundwork — 2026-10-24

- User request: create `asymm_crypto` matching asymmetric support in
  `docs/NXP_JCOP4_J3R180_P71_ALGSUPPORT__3b_fa_18_00_ff_10_00_4a_54_61_78_43_6f_72_65_56_31_(provided_by_PetrS).csv`
  (SHA-256 `a335e371a3f4b1bed8c74aac4b3cbe353b623f37a778e94fd14a52d00ee23dcb`).
- Implemented profile plumbing and a bounded buffered Java Card digest provider
  for SHA-1, SHA-224, SHA-384, and SHA-512 using Mbed TLS; existing SHA-256
  stays on its current accelerator. Added the Mbed TLS native module to PC,
  ARM, and RISC-V source selection, plus a VPCD KAT runner.
- `python3 tools/test-javacard-support.py` passes 8 tests. `./tools/build arm
  --profile asymm_crypto` and `./tools/build riscv --profile asymm_crypto`
  passed. The local VPCD digest run passed 10 KATs (SHA-1/SHA-224/SHA-256/
  SHA-384/SHA-512, messages `abc` and 128 bytes):
  `artifacts/logs/asymm-crypto-digest-vpcd-20261024.log`.
- The first loopback VPCD attempt needed elevated socket access; with that it
  passed. The empty-input case was removed because the constructed short APDU
  was rejected with `6700`; empty-message digest behavior remains untested.
- **Do not flash this interim profile yet.** `asymm_crypto` currently combines
  `symm_crypto` and the SHA digest prerequisites only. RSA/EC key storage,
  RSA cipher/signatures, ECDSA, key agreement, key generation, and the exact
  target support comparator are still unimplemented.
- Next: add RSA/EC key builders and methods, then profile-gated Mbed TLS RSA,
  ECDSA and ECDH providers; cover them with VPCD known-answer tests before the
  full ARM/RISC-V builds and identified physical board acceptance.

## Asymmetric crypto profile — RSA key storage — 2026-10-24

- Added selectable RSA public, private, and CRT key-builder lengths for 512,
  736, 768, 896, 1024, 1280, 1536, 1984, 2048, 3072, and 4096 bits, plus
  reset/deselect transient private variants. `RSAKeyProvider` stores key
  components and implements initialization, clear, size/type, and getters/setters.
- Validation: support generator tests pass (8 tests); VPCD probe passed 10
  digest vectors and five RSA key-storage vectors; ARM and RISC-V builds pass
  under `asymm_crypto`. This validates key object storage only, not RSA crypto.
- The VPCD log remains `artifacts/logs/asymm-crypto-digest-vpcd-20261024.log`.
- Next: implement profile-gated RSA cipher and Signature providers with known
  answer vectors; then EC key storage, ECDSA, key agreement, and generation.
  The profile is not ready for physical flashing yet.

## Symmetric crypto physical acceptance and M1 fix — 2026-10-24

- Corrected `Cipher` ISO9797-M1 decryption. Java Card preserves zero padding
  for M1 because the original length cannot be recovered. The implementation
  had stripped trailing zeros; the emulator KAT was also updated to expect the
  padded result and allocate enough output space.
- `./tools/test-symm-crypto-vpcd` passes 71 cipher/MAC vectors
  (`TEST:SUMMARY pass=73 fail=0`, with ATR and SELECT). ARM and RISC-V wrappers
  pass on profile `symm_crypto`.
- Current ARM image SHA-256:
  `f7c145df97671400f8657d1cc05d39284c01d8329300146797e8ce6e6d87c467`
  (1,670,656 bytes). D92 `D92E743D5F640C0F` and 2BED
  `2BED26ED20556535` received this image and passed their finite cycle monitors.
  8E `8E464F4E2BBC0A19` flash verification passed, but its USB serial/CCID did
  not re-enumerate into the VM afterward; the cycle returned an error before
  monitor validation. Final PC/SC scan lists D92 and 2BED only.
- Corrected physical KAT on 2BED: 71/71 APDUs pass. The official API 3.0.5
  Crypto area on the same board completed 466 calls, with 158 OK, 305
  `UNSUPPORTED_PER_SPEC`, and three unrelated Signature `SW=6F00` cases; no
  `WRONG_RESULT` rows remain. Before the fix, 8E reported four `WRONG_RESULT`
  rows for AES/DES CBC/ECB M1 decrypt. The post-fix 2BED run removes all four.
- Logs and CSVs:
  `artifacts/logs/symm-crypto-kat-vpcd-20261024.log`,
  `artifacts/logs/symm-crypto-kat-pcsc-20261024T120000Z.log`,
  `artifacts/logs/symm-crypto-deploy-crypto-8E-20261024.log`,
  `artifacts/logs/symm-crypto-deploy-crypto-2BED-20261024.log`,
  `artifacts/results/symm-crypto-deploy-crypto-8E-20261024.csv`, and
  `artifacts/results/symm-crypto-deploy-crypto-2BED-20261024.csv`.
- Physical probe utilities now support exact reader selection for both the
  known-answer test and JCSystem memory probe. The 2BED and 8E available
  persistent-memory readings before CAP tests were 42,437 and 52,042 bytes;
  D92 previously had 23 bytes and was not used for another applet install.
- Next: reconnect 8E into the VM if board-specific validation is desired. The
  corrected implementation is validated on 2BED; do not interpret the three
  Signature errors as symmetric crypto failures. Latest host check on
  2026-10-24 saw only two CHOUPI CCID readers (D92 and 2BED), no BOOTSEL USB
  device, and no 8E reader; `lsusb` agreed. The recent insertion notice did not
  result in a BOOTSEL device visible inside the VM.

## Symmetric cryptography profile — 2026-10-24

- The user-provided target is in
  `docs/NXP_JCOP4_P71_NoECC_ALGSUPPORT__3b_ea_00_00_81_31_fe_45_00_31_c1_73_c8_40_00_00_90_00_7a.csv`.
  It remains unmodified (SHA-256
  `8fa3593c5f6a196e9ed5eb88a69bfac528c5acbcc933f5e4fdae36e3bf8d0943`).
- Implemented `symm_crypto`: AES/DES CBC and ECB padding variants, DES/AES
  symmetric MACs including MAC3 and AES-CMAC, transient AES/DES key storage,
  generated API dispatch, and profile-conditioned native modules. API sources
  are committed in submodule `d0e7fc6`; root implementation commit is
  `2adca1c`.
- `python3 tools/test-javacard-support.py` passed 7 tests. ARM, RISC-V, and PC
  wrappers passed with `--profile symm_crypto`. The bounded in-process PC VPCD
  smoke passed 15/15 checks, including installation/selection, JCSystem info,
  SHA-256 doFinal/static performance, and the existing AES-CBC-NOPAD support
  query. Log: `artifacts/logs/symm-crypto-vpcd-applet-final-20261024.log`.
- No physical test was possible at the last check: the authorized
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper reported zero visible
  Pico CCID devices. Do not claim this profile was flashed or tested on real
  cards. The emulator smoke also did not exercise known-answer vectors for
  the newly added AES/DES cipher modes or MAC algorithms.
- Next: when a physical reader becomes visible, run the identified board
  through the normal approved host-access path; record its serial, flash the
  full ARM image only after both architecture builds pass, capture finite
  runtime output, then run Crypto CAP KATs and the `/deploy` Crypto area. If
  no board is visible, add or run targeted emulator vectors before claiming
  correctness. Preserve the upstream JCAlgTest CAP unchanged; its SHA-256 is
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
- Current ARM image: 1,670,656 bytes, SHA-256
  `86f203a09fffbbf795739b2b019baaf93f6a86219317d5803e521498ddbf1ca9`.
  PC ROM mask: 524,300 bytes, SHA-256
  `b8af1c28c384278a2442f5cd9e9e02c8cc1ef2d69b8977e9baaf14315d4d46e5`.

## Symmetric crypto emulator known-answer follow-up — 2026-10-24

- Added `tools/build-symm-crypto-probe`, a Java Card probe applet, and
  `tools/test-symm-crypto-vpcd`. The runner checks all 16 enabled cipher
  algorithms in both directions, 13 MAC outputs, and valid/modified MAC
  verification. Reference outputs come from Python `cryptography`.
- Result: all 71 crypto vectors passed; the VPCD test reports
  `TEST:SUMMARY pass=73 fail=0` including ATR and SELECT. Padded decrypt cases
  use a 3-byte output array and pass. Log:
  `artifacts/logs/symm-crypto-kat-vpcd-20261024.log`.
- Physical checks still show no Pico reader (`found 0`), so these results are
  emulator-only. When a board is available, rebuild ARM/RISC-V, flash ARM via
  the approved path, capture the finite monitor, run the probe on the physical
  card, and run `/deploy` Crypto. The upstream JCAlgTest CAP remains unchanged.
- Current root commit for the implementation is `2adca1c`; its API submodule
  commit is `d0e7fc6`. The KAT harness commit will be recorded in progress.

## Selectable capability profiles acceptance — 2026-10-24

- Follow the user's clarified constraint: do not alter the upstream JCAlgTest
  CAP. `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` remains SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  Minimal is intentionally not tested with JCAlgTest; use `/deploy` CAPs across
  profiles.
- Current/no-AES/minimal (15/10/0) ARM and RISC-V wrapper builds passed, and
  all three physical ARM cycles and finite monitors passed on Pico D92
  (`D92E743D5F640C0F`). Current's full upstream JCAlgTest report passed the
  generated support comparison: 4,525 rows, 15 yes, 4,510 no. No-AES's
  unchanged JCAlgTest fails startup with `F103` because it eagerly requests
  crypto disabled by that profile.
- `/deploy` Core, Security, and Crypto ran on all three profiles; later runs
  after the OwnerPINx cast fix are documented in the root TODO and progress.
  Results and per-run artifacts are in
  `artifacts/results/capability-profiles-acceptance-20261024/`. Core's
  current/no-AES OwnerPINx rows have 17 runtime exceptions and 2 wrong results;
  minimal's disabled OwnerPINx variants return 8 `ILLEGAL_USE` exceptions.
  Ext scenario INSTALL returned `6A88` with rollback on all profiles.
- Do not install more CAPs on D92: physical memory probe found only 23
  persistent bytes free after the stateful deploy suite. Later installs
  returned `6F00`; this cannot be distinguished from storage exhaustion.
  D92 was restored to current firmware and the unchanged JCAlgTest applet was
  confirmed selectable. Use a fresh card for further stateful deploy work.
- Profile outputs: active ROM masks 36,574 / 36,169 / 32,491 bytes; UF2 images
  1,658,880 / 1,656,832 / 1,633,280 bytes (current / no-AES / minimal).
  Exact image hashes, build/module inspection, and comparator artifacts are
  recorded under the profile acceptance artifact directory and `progress.md`.
- Code changes awaiting this acceptance commit: dynamic ISOException ROM offset
  for profile-dependent ROM layouts; optional JCSYSTEM probe flag; corrected
  base-profile config test. The original CAP and submodule trees are untouched.
- The `/deploy` Core/Security/Crypto profile matrix is complete. The original
  minimal Crypto INSTALL `6F00` was storage-limited and later did not reproduce
  after cleanup; the successful 466-call rerun and cleanup are recorded in the
  root TODO. No-AES JCAlgTest startup and minimal JCAlgTest are excluded as
  described above.
- Acceptance/source changes were committed as `36c8abf`; Git writes required
  elevated host access because `.git` is read-only in the default context.

## JCAlgTest base/current physical profile comparison — 2026-10-24

- Completed the requested `base` profile and compared it against `current` on
  the same Pico 2 D92 (`D92E743D5F640C0F`). The complete JCAlgTest 1.8.2
  `ALG_SUPPORT_EXTENDED` reports both contain 4,525 rows and match generated
  expected-support CSVs: base 10 yes / 4,515 no; current 15 yes / 4,510 no.
  Current adds AES-192, AES-256, DES, DES3-2KEY, and SHA-256.
- Base includes all three OwnerPINBuilder variants because the official probe
  does not catch the documented SystemException for disabled variants; those
  rows otherwise contain exception text instead of strict yes/no values.
- Current and base ARM/RISC-V build gates passed. Both ARM images were flashed
  and their finite runtime monitors passed. Base UF2 size: 1,652,736 bytes;
  current size was not preserved. A clean CAP LOAD+INSTALL was proven on the
  initial base trial; the same installed applet remained selectable and
  passed GETVERSION/JCSystem probes after final base/current reflashes. A new
  clean install on current was not completed because GPPro DELETE returned
  `6985` while the applet was active.
- Duplicate package LOAD now returns `6A80`; physical verification confirmed
  the existing JCAlgTest package remains available. Each full test also sees
  4,525 reset-control `B0 E2` APDUs returning `6F00`: the applet doesn't catch
  `SystemException.ILLEGAL_USE` from unsupported object deletion. This does
  not alter the subsequent support rows.
- Full report and checksums:
  `artifacts/results/jcalgtest-profile-comparison-20261024.md` and the two
  result directories `artifacts/results/jcalgtest-profile-base10-20261024/`
  and `artifacts/results/jcalgtest-profile-current-20261024/`. Ignored
  artifacts remain untracked per `.gitignore`.
- Next: if continuing capability-profile work, verify package installation
  after a clean current-profile install when the card can be safely reset;
  otherwise prioritize the next item in root `TODO.md`. Do not interpret the
  repeated `B0 E2` status as an algorithm-support result.
- Commit for this step: `744bceb`; the separate follow-up commit records this
  final handoff/progress SHA. Pre-existing nested submodule artifacts remain
  untouched.

## Framework exception reason preservation in RMI — 2026-10-23

- The RMI exception mapper recognizes embedded framework export tokens for
  CardRuntimeException, APDUException, CardException, PINException,
  ISOException, SystemException, TransactionException, and UserException. It
  emits each protocol type with the 16-bit reason from instance field slot 0.
- Fixed the ISOException throw dispatch token: embedded export token 18 is
  CardException; ISOException is token 21. JC305 imported token numbering is
  separately remapped and must not be confused with these embedded exports.
- Validation passed: ARM and RISC-V wrappers; VPCD 35/35 class and 3/3
  interface APDUs; official SDK 2.2.1 JCRMI client recognized all seven tested
  framework exception classes; physical Pico D92 returned exact reasons for
  all seven, cleaned up the fixture, and preserved JCAlgTest.
- Physical cycle log: `artifacts/logs/cycle-choupi-20261023T085610Z.log`.
  It initially could not locate the reset interface, automatically entered
  BOOTSEL through CDC, then flashed and verified the full CHOUPI image. Monitor
  ended with repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`.
- Test logs: `artifacts/logs/rmi-array-invoke-vpcd-20261023T085319Z.log`,
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T085319Z.log`,
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T085436Z.log`,
  `artifacts/logs/rmi-array-physical-20261023T085737Z.log`.
- Next: cover remaining framework exception families and custom subclasses,
  or move to the next open Priority 3 RMI profile gap in `TODO.md`. Do not
  reopen expected unsupported algorithm results.
- Exception mapping, fixtures, and progress were committed as `5d3aabc`; the
  deeper hierarchy fixture was committed as `f755e9b`; progress SHA references
  were updated in `d9bb5ab`. Other pre-existing worktree changes remain
  unstaged.

## CryptoException reason preservation over RMI — 2026-10-23

- Added separate `javacard.security` package recognition for CryptoException
  (ROM-mask class offset 233), preserving its 16-bit reason in RMI responses.
- Extended the converter fixture and physical probe with
  `CryptoException.throwIt(0x1234)`. CAP method hash is `ec5c`.
- VPCD passed 36/36 class-form and 3/3 interface-form checks. The official
  SDK 2.2.1 proxy decoded the exception as `javacard.security.CryptoException`.
  ARM and RISC-V builds passed; D92 was reflashed and its runtime monitor
  passed. Physical APDU returned `823012349000`; fixture was removed and
  JCAlgTest remained installed.
- Logs:
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T090126Z.log`,
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T090250Z.log`,
  `artifacts/logs/rmi-array-physical-20261023T090545Z.log`, and
  `artifacts/logs/cycle-choupi-20261023T090418Z.log`.
- Next: cover remaining security-package exception subclasses or move to
  another open Priority 3 RMI profile gap. The VPCD C++ build emits an existing
  `-Waddress-of-packed-member` warning from `jc_cap_export.hpp`; ARM/RISC-V
  wrappers are clean. Investigate this separately before requiring a warning-
  free PC build.
- The CryptoException mapping and its progress entry are in `5d3aabc`; its
  later hierarchy extension is in `f755e9b`.

## CryptoException subclass traversal — 2026-10-23

- Added `CustomCryptoException extends CryptoException` to the CAP fixture.
  It now extends an applet-defined `CustomCryptoExceptionBase`, also extending
  CryptoException. Its method hash is `8a9e`; exact RMI response is
  `833012349000`, preserving both subclass tag and inherited reason after two
  applet-defined classes.
- VPCD passed 37/37 class-form and 3/3 interface-form APDUs. The official SDK
  client decoded the response as its `CryptoExceptionSubclass` wrapper. The
  physical D92 probe returned `833012349000`; temporary package cleanup and
  JCAlgTest preservation passed. No firmware change was needed after the
  preceding CryptoException build/flash.
- Logs:
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T090847Z.log`,
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T091002Z.log`,
  and `artifacts/logs/rmi-array-physical-20261023T091115Z.log`.
- Next: cover more security/framework exception subclasses and RMI profile
  boundaries; the current test establishes one cross-package hierarchy of
  depth two before the API parent.

## RMI int[] malformed payload bounds — 2026-10-23

- Added two malformed `echoInts(int[])` requests: length 5 with one int of data,
  and declared length 254 with one int of data. Both return protocol error
  `9900039000`.
- VPCD passed 39/39 class-form APDUs and 3/3 interface-form APDUs; valid calls
  after the malformed inputs still passed. Physical D92 returned the same
  errors, passed subsequent array and exception calls, removed the fixture,
  and preserved JCAlgTest. No firmware was changed or reflashed.
- Logs: `artifacts/logs/rmi-array-invoke-vpcd-20261023T102107Z.log`,
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T102107Z.log`, and
  `artifacts/logs/rmi-array-physical-20261023T102244Z.log`.
- Next: continue broader RMI profile coverage; this validates declared-length
  checks for int[] but not all malformed signature/array types.

## JC305 RMI int-array VPCD coverage — 2026-10-23

- Added `echoInts(int[])` and `short sumInts(int[])` to the CAP 2.2 fixture and
  changed its builder to call the bundled JC305 Converter directly with `-i`;
  the bundled Ant task does not expose that flag. Converter-generated JCA has
  both int[] methods, but its direct CAP output omitted their records. Running
  bundled `com.sun.javacard.jcasm.cap.Main` on the JCA produces all 18 remote
  method records. The builder checks hashes `a336`/`bb6c` and signatures `dd`/
  `d4`; the inspector scans every remote class.
- `./tools/test-rmi-arrays-vpcd` passed through elevated execution because the
  VPCD harness needs loopback bind permission: 23/23 class-form and 3/3
  interface-form checks against the revised fixture. The restricted attempt failed before startup with the
  known socket `Operation not permitted` error.
- ARM and PC build stages passed. The VPCD array suite passes 28/28 class-form
  checks and 3/3 interface-form checks. Int[] coverage includes 32-bit value
  round-trip, empty and null arrays, and a short result computed from int[].
  No RISC-V build, physical flash, or device test occurred.
- The guarded VM int[] RMI path is validated on VPCD and physical D92. The
  current ARM image passed its bounded runtime monitor after the ARM/RISC-V
  build gate. Physical SELECT, sum, echo, empty, and null int[] calls passed;
  fixture cleanup and the post-cleanup JCAlgTest probe also passed. Log:
  `artifacts/logs/rmi-array-physical-20261023T041218Z.log`. Broader custom
  exception hierarchy and complete API reason-code capture remain open.

## Physical AES and SHA-256 static validation — 2026-10-22

- Resolved the embedded API dispatch bug: `bc_invokeinterface()` now remaps
  imported interface method tokens only when the current package is a dynamic
  JC305-compatible CAP. CHOUPI's resident ROM-mask API calls keep their native
  method order. JC305 AESKey class/interface/method token mappings remain
  package scoped.
- `./tools/build arm` and `./tools/build riscv` passed sequentially. Flashed
  only Pico serial `D92E743D5F640C0F`; the other two reported BOOTSEL devices
  were untouched. An 8-second monitor showed repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. It also emitted a
  `WATCHDOG_RECOVERY ... cause=other` diagnostic, seen in earlier clean starts;
  this does not establish a watchdog reset.
- Physical AES-128 CBC-NOPAD static APDUs passed: prepare, five baselines, and
  50-call measure all returned `AA9000`. The official client reports
  `Cipher_doFinal()` 8.58 ms/op and `Cipher.init()` 53.16 ms/op, each over 250
  invocations. SHA-256 static reported `doFinal()` 4.04 ms/op and `update()`
  4.38 ms/op, also over 250 invocations. A final AES 50-call probe passed
  after the long official run.
- Official `ALG_PERFORMANCE_STATIC` used the exact physical reader and a fresh
  output directory. Its 900-second timeout stopped it during RSA CRT public
  variants after 538 methods had started. The partial run had 17 measured
  methods and 520 expected `NO_SUCH_ALGORITHM` results; no performance APDU
  error or VM exception appeared. GPPro's CPLC info query returned `6A88`, as
  expected for unavailable data.
- Logs: `artifacts/logs/jcalgtest-performance-static-after-aes-remap-20261022.log`,
  `artifacts/logs/jcalgtest-aes-cipher-static-physical-20261022.log`,
  `artifacts/logs/jcalgtest-sha256-static-physical-20261022.log`,
  `artifacts/logs/jcalgtest-aes-cipher-static-post-full-run-physical-20261022.log`.
  Results: `artifacts/results/physical-performance-static-aes-remap-20261022/`.
- Remaining: the full client operation cannot complete within 900 seconds;
  focused supported-path behavior is validated. `.git` is read-only, so no
  commit was made. The existing uncommitted tree was preserved.
- Next: continue with the next open acceptance item in `TODO.md` after checking
  current priorities. Keep the two untouched BOOTSEL boards available.

## VPCD clean-save and process restart — 2026-10-22

- Three physical Pico 2 boards were reported reinserted in BOOTSEL. They remain
  untouched; no flash, board PC/SC access, or hardware test was attempted.
- Fixed the host VPCD idle deadline: the transport loop had not passed its
  configured timeout to `read_message()`, leaving a blocking receive. It now
  uses `poll()` with the time remaining since the last APDU; control frames do
  not reset the deadline. A bounded 2-second idle run exited with
  `VPCD:IDLE_TIMEOUT seconds=2`, `VPCD:DISCONNECTED result=0`, and saved a
  524,288-byte image.
- Native `--install` lifecycle passed across a clean emulator process restart.
  GPPro listed the applet and package in both processes. The JCAlgTest probe
  passed SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem info before and after
  restart, all with `9000`. Logs are under
  `artifacts/results/gp-vpcd-cli-install-restart-20261022/`; final timeout log:
  `artifacts/logs/vpcd-idle-timeout-final-20261022.log`.
- GPPro external CAP LOAD/INSTALL also passed across restart after correcting
  the resume fixture: LOAD allocated package ID 56, not the placeholder 63.
  With `--package-id 56 --class-index 148`, the saved record restored and
  SELECT, GETVERSION, and JCSystem passed. The earlier `RESTORE_READ_FAIL` was
  caused by the mismatched package ID, not lost applet data.
- Validation: `./tools/test-vpcd-protocol` passed 18/18; VPCD session 21/21,
  GP DELETE 26/26, GET STATUS registry 47/47, GET DATA 11/11; `./tools/build-pc`
  passed. A final bounded idle run again exited 0 after 2 seconds and saved
  524,288 bytes. No ARM/RISC-V build, physical device operation, or flash was
  run.
- Remaining gap: PC VPCD resume still requires explicit package/class metadata;
  automatic multi-package registry discovery remains open. `.git` is read-only;
  no commit was created.

## Nested persistent reference graph coverage — 2026-10-22

- Added transaction-probe case 82 for a persistent object whose `Object[]`
  field contains two aliases to one byte array. It verifies restored identity,
  visible mutation through the alias, and value preservation after four
  GC-pressure rounds.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed install 3/3
  and fresh-process restore plus four pressure rounds 38/38. `./tools/build arm`
  and the PC build passed. No flash or RISC-V build was run.
- An initial fixture using `Object[][]` was rejected by the JCDK 3.0.5 CAP
  converter as an unsupported multidimensional array. A follow-up one-dimensional
  array-of-arrays fixture reached runtime but failed installation with
  `SecurityException` at `AASTORE`; cause remains unisolated. The passing
  holder-field graph avoids that unresolved write shape.
- A physical reader preflight found no Pico PC/SC readers. Resume physical
  multi-reader work only after the devices enumerate through the authorized
  host-access path. Do not flash under the ARM-only constraint.
- Evidence and hashes are in the newest matching `progress.md` entry. `.git`
  remains read-only; no commit was created.

## Current-source Core deploy and transaction rollback cache fix — 2026-10-22

- Root cause: the Core abort probe starts and aborts an empty transaction. The
  rollback path advanced the global persistent-cache generation even when no
  flash snapshots existed. That forced live persistent instance caches to
  reload, discarded runtime state, and made later Util/AID/PIN checks fail.
  The stable persistent-array body-tag change was not causal: a PC VPCD A/B
  run with that change temporarily reverted had identical row statuses.
- Fix: advance `persistent_write_rollback_generation` only when rollback had
  snapshots to restore. An empty abort leaves live caches intact; a rollback
  after actual writes still advances the generation so cached persistent
  instance fields reload. A failed commit that rolls back snapshots after
  `fs_sync` failure advances it by the same rule.
- Validation: ARM and PC builds pass. Current-source API 3.0.5 Core VPCD now
  completes 291 calls with `OK=272, UNSUPPORTED_PER_SPEC=11,
  APPLET_NOT_INSTALLED=8`; there are zero `EXC_RUNTIME`, `WRONG_RESULT`, or
  `SW_ERROR` rows. The focused persistent transaction probe passed 7/7 across
  begin/commit, abort-after-write, read-after-abort, commit-after-write, and
  read-after-commit.
- Validation is host VPCD only; no physical board was flashed or queried, and
  RISC-V was not built. The 8 unavailable rows need the separate Multi/Ext
  scenario applets. Exact logs, output hashes, and the A/B files are in the
  latest corresponding `progress.md` entry. `.git` remains read-only, so no
  commit was created.
- Added transaction probe case 79: after creating `CLEAR_ON_RESET` and
  `CLEAR_ON_DESELECT` arrays with known values, abort an empty transaction,
  then verify both array references, values, and clear-event types before the
  deselect lifecycle case. Focused cases 30/79/31 pass 5/5 assertions.
- Re-ran `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` after the
  rollback-generation change. Install passed 3/3; fresh-process restore and
  four GC-pressure rounds passed 33/33. Logs are
  `artifacts/logs/vpcd-persistence-commit-20261022T073040Z.log` and
  `artifacts/logs/vpcd-persistence-restore-20261022T073040Z.log`.
- Failed-commit rollback also advances the generation when it restored
  snapshots after `fs_sync` failed. The native persistent-transaction suite
  passes, including injected snapshot-commit failure and rollback.
- Next: keep deploy Core in current-source PC regression coverage; proceed to
  the next open priority in `TODO.md`/`STATUS.md`.

## Persistent array alias survives GC reclamation — 2026-10-22

- Fixed a persistence identity bug: a persistent array's data used to share the
  tag of its first reference field, so clearing that field replaced the array
  record and broke any remaining aliases. Arrays now store their body under a
  stable tag derived by appending reserved index `0xffff`; reference fields
  store aliases to that body. Instance-field serialization uses the same path.
- Added probe cases 80/81: read and clear static alias A, then read alias B.
  The emulator trace confirms case 80 reclaimed one array
  (`arrays=7/8 reclaimed_arrays=1`); case 81 then restored it from flash and
  returned `01`.
- `./tools/build arm` passed. Elevated
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed install 3/3
  and restore/stress 33/33, including four rounds of 256 temporary arrays and
  256 objects. The final run's restore client log SHA-256 is
  `7d8ce80bb26891618c8a2ac96330aecdff213605e55e9148000928bb2c6cadc5`; its
  emulator trace SHA-256 is
  `6705cc897a446bc6cf37807a5cb43e84fdcc02d65791cfac6e160078469dc445`.
- No physical board was used, no flash occurred, and RISC-V was not built.
  `.git` metadata is read-only, so no commit was created. Next: continue with
  persistent object graph / nested-array alias cases, then return to the RMI
  `int[]` converter metadata gap.

## Deploy JavacardxSec regression after persistence change — 2026-10-22

- Re-ran the official deploy `JavacardxSec` API 3.0.5 client on current-source
  PC VPCD after changing persistent array identity. `PICO2_HOST_USB_ACCESS=1
  ./tools/test-javacardxsec-vpcd` passed all 84 calls with status `OK`.
- CSV SHA-256:
  `244b42beadbe2594e461a4410fc1656c60f040f9e62930a00cab7e54e04b2566`;
  emulator log SHA-256:
  `0edf8bc475a511a6bb7073359573da4f85dfdd62b89f436494f644454e78621f`.
- This validates one official deploy applet on current VPCD; it is not physical
  hardware validation. Next: run the current-source Core deploy applet after
  the persistence change, including a tally of scenario rows unavailable
  because VPCD supports one dynamic package per process.

## RMI array regression and bundled JCRMI client — 2026-10-22

- Re-ran `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-arrays-vpcd` using elevated
  local VPCD/PCSC access: ARM and PC builds passed; array/protocol checks passed
  22/22 and interface-reference SELECT passed 3/3. It does not cover `int[]`;
  the JCDK converter still omits int-array remote signatures.
- Re-ran `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-client-vpcd`: the bundled
  Oracle 2.2.1 proxy selected with `Le=7F`, `add(short)` returned 3,
  SensitiveResult returned true, and runtime/checked exception responses were
  decoded. The checked subtype surfaced as `UndeclaredThrowableException`,
  consistent with the response encoding's nearest exported exception class.
- Security cases 470/472 are follow-up `Key.isInitialized()` checks whose
  transient AES key setup already returned `UNSUPPORTED_PER_SPEC`. There is no
  VM defect reproduction; keep these classified as unsupported-feature fallout
  rather than adding transient cryptographic key support solely for decoder
  counts. QKB and STATUS have the evidence and classification.
- No physical Pico was used, no USB access to the boards occurred, and no
  RISC-V build or flash was performed. See the newest 2026-10-22
  `progress.md` entries for artifacts and hashes.
- Next: close the converter-generated `int[]` metadata gap if a valid fixture
  can be produced, then broaden exception hierarchy and API reason coverage.

## Exported RMI reference survives collection under allocation pressure — 2026-10-22

- Extended the CAP 2.2 RMI fixture with `Counter.allocationPressure()`, which
  allocates and drops 256 byte arrays of 64 bytes each. In
  `tools/test-rmi-arrays-vpcd`, the sequence returns an exported child, runs the
  pressure APDU with `PICO2_HEAP_GC=1`, then invokes the child.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-arrays-vpcd` passed ARM and PC
  builds, 23/23 class-form array/protocol checks, and 3/3 interface-reference
  checks. The emulator log records `arrays=5/261 reclaimed_arrays=256` with
  `complete=1`; the subsequent child invocation returned `8100009000`.
- The first attempt expected an extra RMI short length byte (`8102FF809000`);
  observed and correct encoding is `81FF809000`. The fixture expectation was
  corrected before the passing run.
- This proves the exported child remains a GC root across an actual
  APDU-boundary collection after 256 temporary arrays. Persistent flash recovery
  under concurrent reclamation and additional applet shapes remain open. No
  physical token, USB board operation, RISC-V build, or flash was used.
- Next: continue with persistent-record recovery under GC pressure, then return
  to the outstanding converter-generated `int[]` RMI fixture metadata gap.

## Restored persistent graph survives repeated collections — 2026-10-22

- Updated `tools/test-vpcd-persistence` to run with `PICO2_HEAP_GC=1` after
  restoring the applet, followed by four configurable GC-pressure rounds
  (`PICO2_VPCD_PERSISTENCE_GC_ROUNDS`, default 4). Each round allocates 256
  arrays and 256 objects, then checks static roots, persistent array/object
  aliases, and restored array type.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed commit/install
  (3/3) and process-restart restore/stress (31/31). Trace confirms complete
  root walks reclaimed 256 arrays and 256 instances per pressure batch. See
  the 2026-10-22T060544Z `progress.md` entry and artifacts for hashes.
- This validates restored persistent references under pressure while they are
  rooted. It does not prove lazy restoration after a persistent object itself
  has been reclaimed; that remains open. No hardware, RISC-V build, or flash was
  used. The PC build retains pre-existing Rust, Java source-level, and packed
  member warnings.
- Next: inspect and test whether unrooted persistent objects can be safely
  reclaimed while their flash records remain available for later restoration.

## Resume validation and Security result classification — 2026-10-22

- `./tools/test-rmi-arrays-vpcd` was rerun through the elevated host-access
  route. ARM and PC builds passed; its primitive-array/protocol suite passed
  22/22 and the interface-reference SELECT case passed 3/3. Exact artifacts
  and hashes are in the newest matching `progress.md` entry. This test does
  not cover `int[]`; the converter still omits those remote methods from CAP
  metadata.
- Triaged the two current-source Security mismatches (cases 470 and 472 in
  `security-full-vpcd-fixed-decoded.txt`). Both are `Key.isInitialized()`
  checks after transient AES key setup. The immediately preceding setup cases
  return `UNSUPPORTED_PER_SPEC`: `KeyBuilder.buildKey` explicitly supports
  persistent AES/DES only, so no transient key exists for the follow-up
  assertions. Do not implement this solely to turn the deploy count green;
  keep it classified as unsupported-feature fallout unless a VM behavior
  mismatch is reproduced with a supported key.
- No real Pico, USB reader, or RISC-V build was used. Keep ARM-only/no-flash
  scope. Next: resolve the RMI `int[]` fixture/converter metadata gap or work
  on exported-reference GC pressure coverage.

## GC allocation batches and persistent transaction abort fix — 2026-10-22

- The transaction probe now accepts `--gc-pressure-rounds N`. Each round issues
  256 short-lived byte-array allocations, 256 short-lived object allocations,
  then checks static root identity, persistent array-child identity, object
  field alias/mutation, and restored array type. Focused current-source VPCD
  validation passed 54/54 checks over 8 rounds (9.822 s). This collects at APDU
  boundaries and verifies the reference graph remains valid; it does not yet
  force collection while exported RMI refs are held.
- Fixed persistent-cache coherence after abort: `FlashMemory_Handler` advances
  a rollback generation, and each live persistent instance reloads its field
  cache on next access. PC, ARM, and RISC-V builds passed. Ordered transaction
  cases 1–7 passed 10/10, full JCSystem VPCD suite passed 80/80, and native
  `./tools/test-persistent-transaction` passed all checks.
- Fixed the test runner to reselect the applet after case 74 intentionally
  deselects it; case 75 onward now runs in the proper selected state.
- GC stress passed 54/54 over eight rounds with 256 arrays and 256 objects per
  round, checking static-root identity, persistent array child and object-field
  aliases, and restored array type. Exported RMI roots under pressure remain
  open.
- Physical transaction/power-cycle validation was not run and no token was
  flashed. Full and focused transcripts plus hashes are in the latest
  `progress.md` entry. Commit unavailable because `.git` metadata is read-only.

## VPCD persistence package-ID fix — 2026-10-22

- The two-process VPCD persistence test initially committed the probe but failed
  SELECT after restart (`6F00`): the fresh process was told package 63, while
  CAP LOAD had allocated package 56. The install marker now includes the
  assigned ID, and `tools/test-vpcd-persistence` passes it into restore.
- Current-source run passed install/commit 3/3 and fresh-process restore 7/7,
  including committed field `0x33`, static root identity, persistent array
  child identity, object-field alias mutation, and restored array type. This is
  process-restart VPCD validation, not physical power-cycle validation.
- `STATUS.md` now reflects the current collector: persistent records remain in
  flash and are lazily restored; they are not globally pinned in RAM. Next
  priority is allocation-pressure testing while static and exported references
  are held, then multi-package persistence coverage.
- Logs are in `artifacts/logs/` with timestamp `20261022T052745Z`; hashes and
  exact commands are in the newest matching record in `progress.md`.
- No physical token was used, no firmware flashed, and no RISC-V build ran.
  Commit unavailable because `.git` metadata is read-only in this environment.

## SensitiveResult initial implementation and three-reader run — 2026-10-22

- Replaced `SensitiveResult` assertion/reset stubs with native bridge calls and
  a typed result slot. Current result producers include AID comparisons,
  `JCSystem.getAID`/`lookupAID`, and the listed `Util` copy/fill/compare and
  `setShort` operations. Applet entrypoints clear the slot. Object results are
  included in GC roots.
- Corrected the storage model after reviewing the API contract: the slot is
  VM-wide because it must survive Java Card context switches, including SIO
  calls. ARM and PC builds passed after this correction. The official SDK RMI
  fixture confirms same-context object, boolean, and short assertions, mismatch
  rejection, and reset/unassigned behavior. The full current-source
  `JavacardxSec` deploy suite now passes all 84 calls over PC/SC VPCD. SIO
  context-switch coverage and other sensitive API result producers remain open.
- Added `tools/test-javacardxsec-vpcd`. It activates a VPCD endpoint with a
  bounded SmartcardIO presence query, confirms the virtual card is present
  after connection, runs the official deploy client, and requires exactly 84
  OK calls. This avoids the previous `Connection refused` and no-applet
  startup failures. Invoke it with `PICO2_HOST_USB_ACCESS=1` through the
  elevated host-access path.
- The refreshed host access setup exposed three physical readers (Java indices
  3–5). A live parallel run used Security, JavacardxSec, and JavacardxExt:
  Security completed 901 calls (63 OK, 820 expected unsupported, 13 runtime
  exceptions, 2 wrong results, 1 SW error) and cleaned up; JavacardxSec and
  JavacardxExt each returned INSTALL `6A88` and rolled back. These results use
  existing firmware, not the current source. No token was flashed.
- Results: `artifacts/results/deploy/parallel-20261022-051734/`. The physical
  Security ATR was `3B800181`. ARM and PC build logs are archived as
  `artifacts/logs/build-arm-sensitive-result-vm-wide-20261022.log` and
  `artifacts/logs/build-pc-sensitive-result-vm-wide-20261022.log`.
- Full current-source `JavacardxSec` CSV and run logs are in
  `artifacts/results/javacardxsec-vpcd-20261022T033243Z/`; the official suite
  completed 84/84 `OK`. The repeatable entry point is
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-javacardxsec-vpcd` using the elevated
  host-access path.
- Next: add a focused VPCD case for result visibility across an SIO context
  switch, then complete remaining sensitive API producers. Keep the no-flash
  gate and ARM-only scope in force.
- Commit: not created; `.git` metadata is read-only in this environment.

## GPPro card-manager lifecycle complete on PC VPCD — 2026-10-22

- The previous checkpoint's missing PC callback is now implemented. Host VPCD
  routes authenticated SCP03 manager APDUs to GET STATUS, GET DATA, CAP
  LOAD/INSTALL, and DELETE. Default empty-AID ISD selection remains covered.
- `./tools/build-pc` passed; `./tools/test-vpcd-session` passed 21/21;
  `./tools/test-gp-delete` passed 26/26; `./tools/test-gp-registry` passed
  47/47. `./tools/build arm` passed before the final PC-only resume metadata
  change. PC build reports existing Rust and packed-member warnings.
- GPPro 20.01.23 on `Virtual PCD 00 00` successfully completed `--list`,
  package DELETE, reinstall of `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`, and
  another list. After process restart using the scratch flash image and explicit
  applet/class resume metadata, GPPro listed both the package as loaded and
  applet `4A43416C675465737431` as selectable. Logs and scratch image are under
  `artifacts/results/gp-lifecycle-vpcd-20261022/`; log hashes are recorded in
  root `progress.md`.
- A first post-restart check found GET STATUS omitted the applet because the
  host registry record was process-local. Resume now reconstructs this
  supported single-instance record from `--applet-aid` and `--class-index`.
  This does not implement persistent registry enumeration or multiple PC VPCD
  instances/packages.
- No physical token was used or flashed. No RISC-V build was run. VPCD
  `waitForCardPresent` can report false before the GP client connects; use the
  emulator's `VPCD:CONNECTED` marker followed by the official GPPro exchange.
- Next: address remaining GET DATA and continuation coverage, then implement
  complete persistent registry enumeration and extend PC VPCD storage beyond
  one dynamic package/instance. Physical CCID confirmation remains separate.
- Commit not created because `.git` metadata is read-only in this environment.


## GPPro GET DATA profile inspection — 2026-10-22

- Ran GPPro `--info --debug` against current-source host VPCD. It exited 0.
  Card Recognition Data `80CA0066` returned the expected 26-byte CRD TLV and
  `9000`, advertising GlobalPlatform and SCP03 i=10. CPLC `9F7F`, IIN `0042`,
  CIN `0045`, Card Capabilities `0067`, and Key Info Template `00E0` each
  returned `6A88`, which GPPro accepts as unavailable.
- No extra card-specific values should be fabricated. Remaining profile tests
  include pagination across multiple packages and modules. Authenticated GPPro
  first/next pairs (`P2=02/03`) now pass for application, package, and module
  categories with one result each; next returns empty `9000`. An isolated next
  request without a first returns `6985`. PC VPCD currently stores one dynamic
  package and instance.
- Log: `artifacts/results/gp-lifecycle-vpcd-20261022/gppro-info.log`, hash in
  `progress.md`.
- `./tools/test-gp-get-data` passes 11/11, including exact CRD TLV bytes and
  `6A88` for unavailable CPLC/unknown objects.
- Shutdown note: Ctrl-C caused an unresolved host emulator panic at
  `src/arch/host/emulator.rs:602`; use a test harness that sends SIGTERM to the
  emulator child and waits instead. See `.agent/QKB.md`.
- GET STATUS sequence log:
  `artifacts/results/gp-lifecycle-vpcd-20261022/gppro-get-status-pagination.log`,
  SHA-256 `59db893aa6321b40a1ffeb7c272c6c1a503dc5e3e229c33ba66583a860681e81`.

## Three-reader retest and stock SDK SELECT fix — 2026-10-22

- The refreshed helper exposed all three physical readers. A parallel JC305
  Core/Crypto/Service run completed in
  `artifacts/results/deploy/parallel-20261022-050033/`. Crypto completed 466
  calls, Core 291, and Service 124. CAP uploads and required cleanup passed;
  Core's optional Ext package INSTALL returned `6A88` and its rollback passed.
  The old firmware still reports Crypto constructor errors and Service API
  mismatches; these do not represent current source. Post-run `pcsc_scan -r`
  still listed all three Pico readers.
- Fixed SDK RMI SELECT with `Le=7F`: `is_select_for_applet()` previously only
  accepted `Le=00`, so RMIService did not recognize the SDK's valid SELECT and
  the applet returned `6D00`. It now validates the APDU shape without
  constraining Le's value. The probe no longer rewrites Le.
- Validation: `./tools/build arm` and `./tools/build-pc` passed;
  `./tools/test-rmi-client-vpcd` passed with the original SDK SELECT, returning
  interface FCI and `9000`, `Counter.add(short)=3`, and both custom exception
  cases. `git diff --check` passed. No RISC-V build or flash was run. The
  checked exception is exposed as `UndeclaredThrowableException`, since the
  interface declares only the concrete subclass while JCRMI reports its
  exported parent.
- Artifacts: `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T030617Z.log`,
  `artifacts/logs/build-pc-rmi-client-20261022T030617Z.log`, and the three
  per-reader CSV/log pairs in the deploy result directory above.
- Next: implement and exercise `SensitiveResult` plus native result capture;
  this is a fully failing current-source deploy API area. Continue under ARM
  build and VPCD validation; do not flash without satisfying the repository
  architecture build gate.

## Official SDK JCRMI proxy probe and three-reader physical coverage — 2026-10-22

- Three physical readers were exercised concurrently with the JC305 deploy
  runner. The most recent batch is
  `artifacts/results/deploy/parallel-20261022-044709/`: Crypto (466 calls) on
  D92E743D5F640C0F, Core (291 calls) on E0E14525E64AF6ED, and Service (124
  calls) on 5CFC5328F5BDE437. Uploads and primary cleanup passed. Optional Core
  Ext INSTALL returned `6A88` and rollback passed, making the coordinator
  return 1; each worker returned 0. Results exercise existing card firmware,
  not current source. No firmware was flashed.
- Added `tools/test-rmi-client-vpcd` and an accessor harness using the bundled
  Oracle JCRMI 2.2.1 client. With adapter handling for the client's status-first
  response format and SELECT Le, its proxy invoked `add(short)` and decoded
  runtime and checked subclass responses. The checked subclass is surfaced as
  `UndeclaredThrowableException`, because the client method declares only the
  custom exception while the protocol provides its closest API parent type.
- Open issue: stock SDK SELECT Le `7F` gets `6D00`; changing only its Le to
  `00` yields the expected interface FCI. Cause is unknown and recorded in
  `.agent/QKB.md`. This is not full unmodified-client interoperability.
- Validation: `./tools/test-rmi-client-vpcd` passes after host build and fixture
  generation. It runs host VPCD only; no physical APDU or flash. Logs:
  `artifacts/logs/build-pc-rmi-client-20261022T025456Z.log` and
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T025456Z.log`.
- Next: investigate SELECT Le handling, then rerun the SDK probe without its
  Le rewrite. Preserve the three-reader runner for physical deploy areas;
  current-source flashing remains under the ARM-only/no-flash constraint.

## Three-reader JC305 run after reader setup refresh — 2026-10-22

- Used all three visible readers concurrently with
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas
  Core,Crypto,Service --min-devices 3 --max-devices 3`. Routing: Crypto on
  D92E743D5F640C0F, Core on E0E14525E64AF6ED, Service on 5CFC5328F5BDE437.
- Crypto: 466 calls (11 OK, 434 `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`,
  three `EXC_CARDRUNTIME`, two `SW_ERROR`); upload and cleanup passed. Core:
  291 calls (271 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional applets not
  installed, one `WRONG_RESULT`); primary upload and cleanup passed. Service:
  124 calls (94 OK, 28 `WRONG_RESULT`, one `SW_ERROR`, one `EXC_RUNTIME`);
  upload and cleanup passed.
- Optional Core Ext INSTALL returned `6A88` (missing imported data) and the
  runner successfully rolled back its partial CAP. The coordinator therefore
  returned 1 although all worker processes returned 0 and all required cleanup
  completed. No firmware was flashed; this run exercised existing board images.
- Results: `artifacts/results/deploy/parallel-20261022-044709/pico1.csv` through
  `pico3.csv`, with matching per-reader `.log` files. Next: continue current
  source VPCD work. These physical results do not validate current-source code.

## RMI checked and runtime exception subclasses — 2026-10-22

- Extended the remote CAP fixture with `CustomCheckedException extends
  Exception` and a method that throws it. Together with the existing
  `CustomRuntimeException`, this checks mapping of custom subclasses through
  two Java `Throwable` branches.
- `./tools/test-rmi-arrays-vpcd` passes ARM and host builds, fixture generation,
  full VPCD invocation (`TEST:SUMMARY pass=22 fail=0`), and interface
  references (`TEST:SUMMARY pass=3 fail=0`). Exact subclass responses are
  `830900009000` for RuntimeException and `830500009000` for Exception.
- Logs: `artifacts/logs/rmi-array-invoke-vpcd-20261022T024418Z.log`,
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T024418Z.log`,
  `artifacts/logs/build-arm-rmi-arrays-20261022T024418Z.log`, and
  `artifacts/logs/build-pc-rmi-arrays-20261022T024418Z.log`. CAP SHA-256
  `cdbb5be061b06d7b61338cc28f2f797947714dbc9ca23a21a8e70454007e66ed`; load
  code SHA-256
  `e3ef992a2ed5db38c468db02e74787f774666f24866e13b39644ce4d0e8b196a`.
- No physical APDU, flash, or RISC-V build was run. External JCRMI client
  interoperability and API exception reason coverage remain open. The three
  physical readers remain available for future parallel runs, subject to the
  ARM-only/no-flash gate.
- `.git` metadata is read-only, so no commit was created. Next: set up an
  external JCRMI client exchange against VPCD and exercise its exception
  decoding.

## RMI custom exception subclass responses — 2026-10-22

- Uncaught Java exception objects now retain their compact heap reference when
  they cross the remote invocation boundary. RMI maps the actual exception
  class through its superclass chain to the nearest exported Java Card API
  exception and emits CAP 2.2 tag `0x83` for subclasses, with reason zero.
- Added `CustomRuntimeException extends RuntimeException` to the CAP 2.2
  fixture. `./tools/test-rmi-arrays-vpcd` passes ARM and host builds, fixture
  generation, the full VPCD invocation suite (`TEST:SUMMARY pass=21 fail=0`),
  and interface references (`TEST:SUMMARY pass=3 fail=0`). The exact custom
  exception response is `830900009000`; prior API exception responses remain
  passing.
- This source change was validated with VPCD only. No hardware was flashed and
  no physical APDU was run. External JCRMI client interoperability and broader
  custom hierarchy/reason-code coverage remain open.
- Artifacts: `artifacts/logs/rmi-array-invoke-vpcd-20261022T024228Z.log`,
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T024228Z.log`,
  `artifacts/logs/build-arm-rmi-arrays-20261022T024228Z.log`,
  `artifacts/logs/build-pc-rmi-arrays-20261022T024228Z.log`. Fixture CAP SHA-256
  `0289e897fbeb1d6dbcee223a4f6ddd51b6ae562a53cea86067338c8156d42398`;
  load code SHA-256
  `e9f4b3cd291feb670714e6b215257638981f3fd75e41a57de8261d00f9a15373`.
- `.git` metadata is read-only, so no commit was created. Next: test custom
  exception behavior with an external JCRMI client and expand exception
  hierarchy/reason coverage. Keep ARM-only/no-flash constraints in effect.

## RMI remote-reference returns — 2026-10-22

- Added CAP 2.2 remote-interface return decoding and remote reference response
  encoding in the SELECT-selected class or interface form. Returned exported
  objects are retained for the current session. The VPCD fixture's `Counter`
  returns a second `CounterImpl`; the client successfully invokes `value()`
  through returned object ID `0002`.
- `./tools/test-rmi-arrays-vpcd` passes ARM and PC builds, with VPCD summaries
  `pass=20 fail=0` for class references and the existing cases, and
  `pass=3 fail=0` for interface references. Exact return descriptors and
  logs are in the latest `progress.md` entry. VPCD needed elevated execution
  because local socket creation in the restricted sandbox returned `EPERM`.
- No physical hardware, USB, RISC-V build, or flash was used; ARM-only/no-flash
  scope remains. `.git` is read-only, so no commit was created.
- Next: support custom Java exception subclass responses, then test against an
  external JCRMI client. Full API exception reason coverage and the known
  JCSystem regression findings remain open.
- Follow-up investigation: the existing JCDK 3.0.5 converter accepted `-i`
  but omitted int[] RMI methods from generated remote metadata. The fixture was
  restored to its passing baseline. Keep int[] end-to-end coverage open; details
  and evidence are in the latest progress/QKB entries.

## RMI primitive arrays and API exception responses — 2026-10-22

- Current-source VPCD fixture exercises primitive-array marshalling and
  unhandled RMI exceptions. Implemented CAP 2.2 API exception types as `0x82`
  responses with their reason field; preserves SystemException reason,
  APDUException's ILLEGAL_USE reason, and ISOException status reason. Nested
  INVOKE bounds exception-handler search at its entry frame while still
  allowing handlers inside the invoked method. This prevents uncaught remote
  exceptions from escaping into the Java RMI service's own frames.
- `./tools/test-rmi-arrays-vpcd` passes ARM and PC builds and reports
  `TEST:SUMMARY pass=18 fail=0`. Exact exception responses: array bounds
  `820200009000`, null `820800009000`, SystemException.ILLEGAL_VALUE
  `822500019000`, and ISOException `82236A829000`; locally caught array bounds
  returns `81559000`. Fixture CAP SHA-256
  `972ce3969de0353d45395c63f7682b2fd2b845bfa8748d2375ce0b6b4fb201bb`, load
  code SHA-256 `f3f8cf42cdb6ec449979aeac7f1b467360c49ddaf21660a4b79b2fb1d8863617`.
- JCSystem targeted regressions passed cases 19, 20, 21, 22, and 24. The full
  applet test stopped at case 5 (abort persistence expected `0x11`, got
  `0x22`); a separate selected run of case 27 also failed because its APDU
  exception sentinel remained `0x7F`. These findings need independent
  diagnosis. No RISC-V build or physical flash was run; all three readers are
  available but the ARM-only/no-flash build gate remains.
- Next: add remote-reference return marshalling, then custom exception
  subclass responses and external JCRMI client interoperability. Reason capture
  is not complete for every Java Card API exception.
- Artifacts: `artifacts/logs/rmi-array-invoke-vpcd-20261022T021751Z.log` and
  `artifacts/results/deploy/parallel-20261022-040147/`.

## Three-reader deploy confirmation — 2026-10-22

- The refreshed PC/SC setup exposed three physical Pico readers at Java
  indices 3–5: D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437.
  `tools/run-deploy-parallel --api 305 --min-devices 3 --max-devices 3`
  completed Security, Crypto, Core, and Service in parallel on the already
  installed firmware. All supported-area cleanup passed. Optional
  Javacardx-family/Biometry installs returned `6A88` and were rolled back.
- Results in `artifacts/results/deploy/parallel-20261022-040147/`: Security
  901 calls (63 OK, 820 per-spec unsupported, 13 runtime exceptions, two
  skips, two wrong results, one SW_ERROR); Crypto 466 (11 OK, 434 per-spec
  unsupported, 16 runtime exceptions, three card-runtime exceptions, two
  SW_ERROR); Core 291 (270 OK, 15 per-spec unsupported, four optional
  applets unavailable, two wrong results); Service 124 (94 OK, 28 wrong
  results, one runtime exception, one SW_ERROR).
- No firmware was flashed; no current-source physical validation is claimed.
  The ARM-only/no-flash build gate remains active. Continue RMI exception
  response handling and reference returns with current-source VPCD tests.
- `.git` metadata is read-only, so no commit could be created. `progress.md`
  records this run.

## RMI primitive-array INVOKE support — 2026-10-22

- Added CAP 2.2 remote signature decoding for the Java Card RMI primitive
  array codes and marshalled boolean[], byte[], and short[] inputs by value
  into JCRE global arrays. Array and null-array results are copied into the
  response by value. Temporary global-array slots are released after INVOKE
  and reused so repeated calls do not exhaust stable reference handles.
- Added `tools/build-rmi-array-fixture` and
  `tools/test-rmi-arrays-vpcd`. The converter-built fixture exercises byte and
  short echoes, boolean mutation, null and zero-length arrays, eight array
  parameters in one method, malformed input length, invalid boolean values,
  plus primitive add/readback regression.
- `./tools/test-rmi-arrays-vpcd` passed ARM and PC builds and current-source
  VPCD with `TEST:SUMMARY pass=13 fail=0`. Exact array responses include
  `810211229000`, `8100079000`, `8100249000`, `81021234FFFE9000`,
  `810201009000`, `81FFFF9000`, and `81009000`. Invalid requests return
  `9900039000`. The PC build emitted 49 warnings from the bundled Rust
  `javacard-os` crate; ARM produced `BUILD:PASS`. No RISC-V build or physical
  flash was run under the ARM-only constraint.
- Int-array parsing/output is guarded by `JCVM_INT_SUPPORTED` but not yet
  covered by the CAP fixture. Next implement RMI remote-reference return and
  API exception response encoding, then test an external JCRMI client.
- Artifacts: fixture CAP SHA-256
  `5f7c5301002fa8d2f8a73d9837a3bed230737042bd982e16b79b9b01b756479f`;
  load code SHA-256
  `f8d760eba9e1a8cbe6368847bf897ff2b421e1de4c1d0d7c92bf025b4be0cfa`;
  VPCD log `artifacts/logs/rmi-array-invoke-vpcd-20261022T015928Z.log`.

## Three-reader deploy retest and stale CAP cleanup — 2026-10-22

- User refreshed the USB/PCSC helper; the parallel runner found all three
  physical readers: D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437.
- Ran the API 3.0.5 deploy suite with
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --min-devices 3 --max-devices 3`. All workers returned 0, but uploads found
  stale Security and JavacardxInt CAPs on two readers. Removed only those
  deploy test CAPs with GPPro `--uninstall` pinned to those readers. Re-ran
  Security successfully: 901 calls (63 OK, 822 per-spec unsupported, 13
  runtime exceptions, two wrong results, one SW_ERROR), with cleanup OK.
  JavacardxInt then returned `6A88` at INSTALL because its imported package is
  unavailable; rollback succeeded. This is expected optional-package behavior.
- The main run also completed Crypto (466 calls: 11 OK, 434 per-spec
  unsupported, 16 runtime exceptions, three card-runtime exceptions, two
  SW_ERROR), Core (291: 271 OK, 15 per-spec unsupported, four optional applets
  unavailable, one wrong result), and Service (124: 94 OK, 28 wrong results,
  one runtime exception, one SW_ERROR). Core's one wrong result is
  `Applet.deselect()`. Remaining optional area INSTALL refusals were `6A88`
  with rollback. These results are from firmware already on the devices, not
  the current source; nothing was flashed.
- Results: `artifacts/results/deploy/parallel-20261022-034350/`. The two
  reader-specific retries are `pico1-security-rerun-clean.csv` and
  `pico3-javacardxint-rerun-clean.csv`; each omits the first local invocation
  that stopped before card access because the direct runner's default CAP path
  was not set. The original CSVs are retained too.
- Historical next action at the time of that physical run was current-source
  VPCD validation. The RMI array work is recorded above; continue with remote
  references, exception responses, and the physical Core findings when current
  source can be validated without violating the ARM-only build gate.

## Three physical readers confirmed; RMI short INVOKE fixed — 2026-10-22

## Three physical readers confirmed; RMI short INVOKE fixed — 2026-10-22

- After the user refreshed the PC/SC helper, the parallel-runner dry run mapped
  three physical readers to Java SmartcardIO indices 3–5:
  D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. A follow-up
  `pcsc_scan -r` still listed all three after testing.
- Ran one independent JC305 area per reader with
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --areas Crypto,Core,Service --min-devices 3 --max-devices 3`. All workers
  exited 0 and cleaned up. Crypto completed 466 calls (11 OK, 434
  per-spec unsupported, 16 runtime exceptions, 3 card-runtime exceptions,
  2 SW_ERROR). Core completed 291 (271 OK, 15 per-spec unsupported, 4
  optional applet unavailable, 1 wrong result); optional Ext INSTALL returned
  `6A88` and rollback passed. Service completed 124 (94 OK, 28 wrong results,
  1 runtime exception, 1 SW_ERROR). These are results from the firmware
  already on the cards; no firmware was flashed.
- Focused current-source VPCD test exposed persistent primitive GETFIELD
  bypassing the restored field cache. Changed byte/short/int getters to load
  via `JC_Instance::getFields()` before reading values. A converter-generated
  CAP 2.2 RMI fixture now passes SELECT, `Counter.add(short)` and
  `Counter.value()`; both invoke responses are exactly `8100039000` after
  adding 3 to the persistent counter.
- Validation: `./tools/build arm`, `./tools/build riscv`, and
  `./tools/build-pc` passed. The clean focused VPCD harness reports
  `TEST:SUMMARY pass=6 fail=0`, including exact errors for unknown method
  (`9900029000`) and object ID (`9900019000`). No physical flash or physical
  APDU tested the new source. Artifacts:
  `artifacts/results/deploy/parallel-20261022-032759/`,
  CSV SHA-256 values in `progress.md`, and
  `artifacts/logs/rmi-invoke-primitives-errors-20261022.log`.
- An earlier run at `parallel-20261022-031638/` was interrupted while workers
  were testing chained areas; it is incomplete and should not be cited as a
  completed run. The one-area-per-reader run above completed normally.
- Next: continue RMI argument/result marshalling beyond byte/boolean/short/int
  primitives, and add negative protocol cases. Keep hardware firmware state
  unchanged until an explicit target/source validation run is chosen.
- `.git` metadata is read-only; no commit was created. Progress and the
  persistence cause/fix are recorded below and in `.agent/QKB.md`.

## Current RMI SELECT implementation and three-reader run — 2026-10-22

- Verified the PC/SC helper refreshed three physical Pico readers;
  `pcsc_scan -r` lists IDs D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437. The parallel JC305 deploy run used all three readers.
  All workers exited 0; the coordinator returned 1 for six optional `6A88`
  INSTALL stages, each rolled back. Results are from existing firmware. Logs
  are in `artifacts/results/deploy/parallel-20261022-024604/`; no firmware was
  flashed.
- RMI SELECT now emits class-form and interface-form FCI references from the
  converter-generated CAP 2.2 metadata. Restored applets re-register the RMI
  service's initial object in the transient export registry. VPCD and CCID
  SELECT matching accepts P2 `0x00` and `0x10`. Exact fixture FCI tests pass
  for both formats.
- Validation: `./tools/test-rmi-cap-metadata`; `./tools/build arm`
  (`BUILD:PASS`, image end `0x100c794c`); `./tools/build-pc`
  (`ROMMASK:PASS`); `tools/test-pc-vpcd-applet` class-form and interface-form
  SELECT both report `TEST:SUMMARY pass=2 fail=0`. Artifacts:
  `artifacts/logs/rmi-select-class-vpcd-20261022.log`,
  `artifacts/logs/rmi-select-interfaces-vpcd-20261022.log`. External JCRMI
  client interoperability and INVOKE remain unverified.
- Next: implement RMI INVOKE framing, remote method hash lookup, primitive
  argument decoding, virtual dispatch, and result/error response encoding; add
  a focused VPCD test using `Counter.add(short)` and `Counter.value()`. Preserve
  the existing successful 33 Dispatcher / 39 BasicService / 124 deploy-Service
  cases. Keep ARM-only and do not flash under the current two-architecture
  flash gate.
- `.git` metadata is read-only; no commit was created. Progress is appended to
  `progress.md`; causes/fixes are in `.agent/QKB.md`.

## Latest CAP 2.2 remote metadata fixture validation — 2026-10-22

- User confirmed the PC/SC helper ran. Elevated host access refreshed three
  reader nodes and `pcsc_scan -r` listed physical readers D92E743D5F640C0F,
  E0E14525E64AF6ED, and 5CFC5328F5BDE437 (alongside two empty virtual readers).
- Generated a remote-enabled CAP 2.2 fixture with the bundled JCDK 3.0.5
  converter. Extended Class component parsing for the signature-pool prefix
  and remote interface names, with component-size validation for malformed
  signature-pool lengths. The unit test checks exact Header/Class component
  bytes from the emitted CAP, including the package name, signature pool,
  remote interface/class names, hashes, offsets, tokens, and interface refs.
- `./tools/test-rmi-cap-metadata` passes:
  `RMI_CAP_METADATA:PASS package_name=bounded signature_pool=4
  remote_methods=2 interface=Counter class=CounterImpl`.
  Added `remote_ref_with_class` and `remote_ref_with_interfaces` encoders and
  known-answer tests, including reserved-ID and output-capacity rejection.
  `./tools/build arm` passes; image end `0x100c7368`. No RISC-V build, PC
  build, physical APDU, or flash was run. The CAP has not yet been loaded or
  executed; this is component/descriptor-format validation, not RMI
  interoperability. The encoders are not wired into `RMIService` yet.
- Fixture archive SHA-256:
  `5ef8e4587a9445bf5bc80b25b36496261c49e18e5d18e87039254d53a5eb114c`
  at `build/rmi-cap22-fixture/cap/rmi/fixture/javacard/fixture.cap`.
- Next: wire parsed metadata into RMI SELECT FCI/reference encoding and run
  the generated CAP in current-source VPCD. Keep ARM-only/no-flash scope.

## Latest three-reader JC305 deploy run — 2026-10-22

- `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --min-devices 3 --max-devices 3` used three live physical Pico readers.
  Worker 1 ran Security (901 calls: 63 OK, 820 per-spec unsupported, 13
  runtime exceptions, two wrong results, two skipped, one SW_ERROR). Worker 2
  ran Crypto (466: 11 OK, 434 per-spec unsupported, 16 runtime exceptions,
  three card-runtime exceptions, two SW_ERROR), Service (124: 94 OK, 28 wrong
  results, one runtime exception, one SW_ERROR), and optional JavacardxSec.
  Worker 3 ran Core (291: 272 OK, 15 per-spec unsupported, four optional
  scenarios unavailable) and optional Javacardx/JavacardxExt/JavacardxInt/
  Biometry. Optional installs returned `6A88`; rollback passed. All workers
  exited 0; coordinator exited 1 because six optional install stages failed.
- Results are from firmware already on the boards, not current source; no
  firmware was flashed. Artifacts: `artifacts/results/deploy/parallel-20261022-023306/`.
- Next: integrate the RMI descriptor encoders through a native bridge and
  process SELECT FCI, then test the converter-generated CAP in current-source
  VPCD.

## Latest CAP 2.2 Header package-name parsing — 2026-10-22

- Added bounds-checked accessors for the optional package-name field after the
  AID in CAP 2.2+ Header components. They validate the declared component size,
  package AID length, and complete name length before exposing the name. CAP
  2.1 reports no package-name field.
- `./tools/test-rmi-cap-metadata` passes synthetic cases for valid CAP 2.2
  names, absent CAP 2.1 metadata, a missing length byte, and truncated name
  data; output: `RMI_CAP_METADATA:PASS package_name=bounded methods=1
  class_name=Impl interfaces=1`. `./tools/build arm` passes. No RISC-V build,
  PC build, physical APDU, or flash was run.
- The first truncated-name assertion expected a missing length-byte pointer;
  review showed the pointer was correctly valid because that byte remained in
  the declared component. The test now checks that the incomplete name is
  rejected. No CAP 2.2 remote-enabled fixture is present or validated yet.
- Next: determine whether the supplied capmap-free/SDK CAP tooling can build a
  small remote-enabled CAP 2.2 fixture; then parse remote interface metadata
  and signature pool using a real emitted component. Preserve ARM-only and
  no-flash scope.

## Latest full JC305 deployment across three physical Picos — 2026-10-22

- Refreshed host USB permissions with the documented helper. `pcsc_scan -r`
  listed three physical readers: D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437 (Java indices 3–5, after two virtual readers).
- Ran `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --min-devices 3 --max-devices 3`. Security (901 calls), Crypto (466),
  Service (124), and Core (291) ran and their main CAP cleanup passed. The
  expected unsupported algorithm results are not defects. Security had one
  SW_ERROR, 13 runtime exceptions, and two wrong results; Crypto had two
  SW_ERROR, 16 runtime exceptions, and three card-runtime exceptions; Service
  had one SW_ERROR, one runtime exception, and 28 wrong results; Core had one
  wrong result (`Applet.deselect()`). These outcomes describe the firmware
  already on the cards, not current source.
- Optional Core Ext and JavacardxSec, Javacardx, JavacardxExt, JavacardxInt,
  and Biometry INSTALLs returned `6A88`; every partial LOAD was rolled back.
  This reports referenced optional packages unavailable on the card and no
  test method calls ran for those areas. Worker processes all exited 0; the
  coordinator returned 1 because these optional INSTALL refusals are counted
  as failed stages. No firmware was flashed.
- Artifacts: `artifacts/results/deploy/parallel-20261022-021536/`; CSV SHA-256:
  pico1 `422ca434884bde410d68982d2037d85bedeb2d91bbc633559b00425731849cfe`,
  pico2 `e74fea8db046a76e8f36cb47aff697f47b136498debe7963fa8642dcaf4c6e20`,
  pico3 `08764a6544ed6ce84398552a792845e2d3178404f37e18797549eb8c071a3247`.
- Next: correlate physical wrong/error calls against current-source VPCD tests
  before changing code, then continue RMI support with a valid CAP 2.2+
  fixture. Preserve ARM-only/no-flash scope.

## Latest install-journal creation cut coverage — 2026-10-22

- Extended the RP2350 fake-flash overlay test to stage the install intent
  journal and applet persistent-state record in one transaction. Across 1,000
  deterministic pseudo-random cuts during snapshot commit, each simulated
  reboot recovered either neither record or both byte-exact records; no torn
  mixed state was accepted.
- `./tools/test-rp2350-fs-overlay` passed with
  `install_journal_and_applet_state_recover_atomically_after_1000_cuts` and
  `TEST:SUMMARY fail=0`. This is failure injection against fake flash, not a
  physical power cut. No hardware APDUs or flash were used.
- Priority 2 install atomicity now has simulated coverage for transaction
  creation, CAP instance-header publication, and journal clear, each with
  1,000 cut locations. Physical power-cut characterization remains open.
- Next: continue the broader install recovery edge audit, then return to
  Service RMI using a genuine CAP 2.2+ fixture. Preserve ARM-only/no-flash
  scope.

## Latest cross-store CAP install recovery coverage — 2026-10-22

- Extended the RP2350 fake-flash overlay test to exercise the real install
  ordering across both stores: establish a durable filesystem journal, publish
  applet instance metadata in the CAP copy-on-write header, then idempotently
  recover/clear the journal. It injects 1,000 deterministic pseudo-random cuts
  during CAP header publication. After each cut the journal must remain
  byte-identical; the CAP header may be the prior empty state or the fully
  published state. Recovery must finish publication, clear the journal, and
  preserve exact applet/instance AIDs and class index across another reboot.
- `./tools/test-rp2350-fs-overlay` passes with
  `install_registry_and_filesystem_journal_recover_after_1000_header_cuts` and
  `TEST:SUMMARY fail=0`. The case uses fake flash and the actual `cap_store`
  header implementation plus RP2350 filesystem overlay; it is not a physical
  power-cut test.
- Next: audit remaining install crash boundaries, then continue the current
  Service RMI implementation using a real CAP 2.2+ fixture if one can be
  generated from bundled SDK tooling. Preserve ARM-only/no-flash scope.

## Latest filesystem cut coverage for install-journal cleanup — 2026-10-22

- Extended `tools/tests/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp` with the
  actual lifecycle journal tag (`02 FF 38 03`). The harness commits a journal,
  stages transactional erase, then injects 1,000 deterministic randomized
  power cuts during the new snapshot write. Each simulated reboot must recover
  either the previous intact journal bytes or the committed absent state.
- `./tools/test-rp2350-fs-overlay` passes all cases, including
  `install_journal_clear_transaction_commits_tombstone` and
  `install_journal_clear_1000_power_cuts_recover_old_or_cleared_state`.
  The preceding VPCD sync-failure restart test, transaction suite, ARM build,
  and host build also pass. This is fake-flash overlay coverage, not a physical
  power cut.
- The combined CAP registry/filesystem cut fixture is now recorded at the top
  of this handoff.

## Latest install-journal cleanup recovery work — 2026-10-22

- Added a PC-only one-shot `fs_sync()` failure hook for the journal-clear
  transaction. It is armed only after the install journal has been committed
  and the CAP instance registry entry has been published. The existing
  transaction commit failure path rolls the erase back, leaving the journal
  for restart recovery.
- Added `--verify-install-journal-clear-restart` to
  `tools/test-pc-vpcd-applet`. It injects the failure, starts a fresh emulator
  process, confirms the journal is recovered idempotently, then runs SELECT,
  GETVERSION, JCSystem, SHA-256 support/doFinal/static, and unsupported-algorithm
  smoke APDUs. Result: `TEST:SUMMARY pass=17 fail=0`. The first test attempt
  exposed that the runner must use the dynamically assigned package ID from
  CAP LOAD; this is now read from the injection marker.
- `./tools/test-persistent-transaction`, `./tools/build arm`, and
  `./tools/build-pc` pass. Host build retains existing Rust and packed-member
  warnings. No RISC-V build, physical APDU, or flash was performed.
- Remaining atomicity gap: fake-flash injection does not replace physical
  power-cut characterization.
- Artifacts: `artifacts/logs/install-journal-clear-sync-restart-20261022-retry.log`,
  `artifacts/logs/build-arm-install-journal-clear-recovery.log`, and
  `artifacts/logs/build-pc-install-journal-clear-recovery.log`.

## Latest three-reader physical deploy run — 2026-10-22

- After the PC/SC helper refreshed access, elevated `pcsc_scan -r` listed all
  three physical Pico readers: D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437 (plus two virtual readers).
- Ran `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --areas Core,Crypto,Service --min-devices 3 --max-devices 3` through the
  elevated host USB path. The runner pinned Crypto to D92, Core to E0E1, and
  Service to 5CFC. It completed in about 59 seconds. Crypto completed 466
  calls and cleanup; Service completed 124 calls and cleanup. Core completed
  291 calls and cleanup; optional Ext INSTALL returned `6A88`, was rolled back,
  and is the only reported failed stage. All worker processes exited 0. This
  tests installed board firmware, not current source. No firmware was flashed.
- Artifacts: `artifacts/results/deploy/parallel-20261022-020215/`. CSV SHA-256:
  Crypto `afc1948e5741f4d7d466e724fc2980fc15f800acc4288d98dff4be81881d35f5`,
  Core `62ee65f3180cc7f2f52d80182cf5ec93046ea038324589f194094ba4d50ff92e`,
  Service `ee08d099e0c47a46e8e20ba76ebb2ac8ee33300748e223efe36bb1594ed13fcb`.
- Next: continue current-source RMI using a genuine CAP 2.2+ remote-enabled
  fixture. Keep ARM-only/no-flash scope.

## Latest work — 2026-10-22

- After the user ran the reader permission helper, elevated `pcsc_scan -r`
  found all three physical readers (D92E743D5F640C0F, E0E14525E64AF6ED,
  5CFC5328F5BDE437) alongside two virtual readers. The parallel deploy runner
  assigned Crypto, Core, and Service to one physical Pico each.
- The 466 Crypto, 291 Core, and 124 Service calls completed; all worker
  processes exited 0 and all three CAP cleanup stages passed. Core's optional
  Ext INSTALL returned `6A88`; rollback succeeded and the coordinator returned
  1 because that optional stage is counted as a failed stage. Results describe
  existing board firmware, not current source; no image was flashed.
- Artifacts: `artifacts/results/deploy/parallel-20261022-014958/`. CSV hashes
  are recorded in `progress.md`.
- The prior claim that RMI names must come from `Debug.cap` was incorrect and
  is superseded in `.agent/QKB.md` and `TODO.md`. CAP format 2.2+ stores remote
  method/class metadata in the Class component. The supplied JC305 Service CAP
  is format 2.1, so it is not a fixture for that path. A first class-tail
  parser and synthetic test exist, but CAP 2.2 signature-pool/interface/header
  parsing is incomplete and no remote-enabled fixture has been validated.
- The PC/SC helper was refreshed in this session; elevated `pcsc_scan -r`
  currently lists three physical readers: D92E743D5F640C0F,
  E0E14525E64AF6ED, and 5CFC5328F5BDE437. No physical APDUs or flashing were
  performed in this session.

## Next concrete action

- Obtain or generate a CAP 2.2+ remote-enabled fixture and validate complete
  Header/Class parsing (including signature pool, remote interface names, and
  package name) before wiring metadata into RMI SELECT. Preserve ARM-only and
  no-flash scope.

## Latest work — 2026-10-22

- Ran the three physical deploy workers after the PC/SC helper granted all
  readers. Crypto (466 calls), Core (291 calls), and Service (124 calls) all
  completed and cleaned up. Core's optional Ext INSTALL returned `6A88`, was
  rolled back, and is the sole reason the coordinator exited 1. No firmware
  was flashed. Artifacts: `artifacts/results/deploy/parallel-20261022-012453/`.
- Current-source VPCD completed all 124 Service deploy API calls after using
  the Service CAP's own AID (`4A43416C675465737441706953766331`): decoder
  `SERVICE_API:SUMMARY total=124 pass=124 fail=0`. An earlier attempt used the
  Security AID and failed before APDUs; that was a test invocation error.
- Added a 32-entry per-applet exported-remote-object registry. Java
  `CardRemoteObject.export/unexport` call VM natives; IDs remain stable through
  re-export, registry entries are GC roots, and lookup rejects unexported
  objects. The Service API's 16-object capacity call initially returned
  `NO_RESOURCE` with a 16-slot table because existing applet references also
  consume slots; it passes with 32. `./tools/build arm` and `./tools/build-pc`
  pass. No RISC-V build or flash was performed.
- Added explicit selection references to the registry. Unexported objects that
  were not published are removed immediately; a marked session reference stays
  resolvable through that selection and is removed on deselect. A focused host
  test covers ID reuse/capacity, root enumeration, session lookup, and cleanup.
  `RMIService.beginSelectionSession()` activates its pending INS and marks its
  initial object, but nothing calls the helper until `processCommand()` is
  implemented. A direct collector survival test is still missing.
- Remaining: RMI SELECT FCI/reference encoding, wiring the session helper to
  SELECT, remote method hashing/mapping, INVOKE argument/result/exception
  marshalling, and VM virtual dispatch. `RMIService.processCommand()` remains
  a stub. ARM/host builds and the 124-call Service VPCD matrix pass. No
  RISC-V build or flash occurred.

## Next concrete action

- Implement RMI SELECT recognition and response generation in
  `RMIService.processCommand()`, including initial object export/reference ID,
  session retention, and the configured invoke INS. First establish how the
  selected reference descriptor gets the required proxy type metadata from a
  standard loaded CAP, then add a focused VPCD SELECT test. Keep the ARM-only,
  no-flash gate.

## Latest work — 2026-10-22

- The PC/SC helper exposed three physical readers. Elevated `pcsc_scan -r`
  confirmed D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. A parallel
  Core/Crypto/Service run completed all three worker tests and cleanup. Core's
  optional Ext CAP INSTALL returned `6A88`; its rollback succeeded. The
  coordinator exited 1 because that optional INSTALL refusal is counted as a
  failed stage. Results are firmware already on the boards; no flash occurred.
  Artifacts: `artifacts/results/deploy/parallel-20261022-004140/`.
- Fixed a current-source `MessageDigest()` superclass constructor failure.
  JC305 imported constructor token 1 now maps to embedded ROM export token 2
  for `invokespecial`; the isolated APDU passes, and the full Security area
  transport completes all 901 cases. Official decoding reports 64 `OK`, 822
  spec-allowed unsupported, 13 runtime exceptions, and two result mismatches.
  ARM and host builds pass. No RISC-V build or device flash was performed.
- The host VPCD logs and decoder output are listed in the newest `progress.md`
  entries. `.git` remains read-only, so no commit was created.

## Next concrete action

- Continue the Service RMI gap against current-source VPCD: implement the
  complete select/invoke flow in `RMIService`, persistent invoke INS handling,
  and exported object-reference lifecycle in `CardRemoteObject`. Preserve the
  existing BasicService and Dispatcher passing cases. Do not flash under the
  ARM-only scope and two-architecture repository flash gate.

## Current-source VPCD Security deploy CAP smoke test — 2026-10-21

- Installed the deploy `AlgTestApiSecurity_305` component image
  (`build/deploy-security-305-load-code.bin`) through the host VPCD emulator,
  selected applet AID `4A43416C675465737441706953656331`, and received `9000`.
  After correcting the test summary counter, `--select-only` reports
  `TEST:SUMMARY pass=2 fail=0` (ATR and SELECT; it skips the JCAlgTest-specific
  APDU suite).
- Artifacts: `artifacts/results/deploy/security-select-vpcd-current-accurate/FLASH_MEMORY`
  and `artifacts/logs/deploy-security-select-vpcd-current-accurate.log`.
- This confirms current-source install/selection of the Security CAP but does
  not compare its full 901-call API output with the physical results. The
  deploy client talks through SmartcardIO; the VM has PC/SC Virtual PCD readers
  but no active VPCD relay daemon in this execution context.

## Remaining deploy areas covered on three physical readers — 2026-10-21

- Ran Security, Javacardx, JavacardxSec, JavacardxExt, JavacardxInt, and
  Biometry API 3.0.5 using one area per Pico. Security on D92 completed 901
  calls (820 allowed unsupported, 63 OK, 13 EXC_RUNTIME, 2 skipped, 2
  WRONG_RESULT, 1 SW_ERROR) and cleaned up. The other five areas were refused
  at install and rolled back. Javacardx, JavacardxSec, JavacardxInt, and
  Biometry returned `6A88`. JavacardxExt on D92 returned `6F00` twice; the
  other readers returned `6A88`. Stale dynamic-package bitmap state on old
  firmware is a plausible explanation, not a confirmed diagnosis.
- Results: `artifacts/results/deploy/parallel-20261021-223749/` (Security,
  Javacardx, JavacardxSec), `parallel-20261021-224015/` (JavacardxExt,
  JavacardxInt, Biometry), and `parallel-20261021-224114/` (JavacardxExt
  repeat). Core/Crypto/Service results are in `parallel-20261021-223127/`.
- All deploy areas have now been exercised on the already-installed board
  firmware. This does not verify current source changes; no firmware was
  flashed.

## Three physical Pico 2 deploy areas rerun in parallel — 2026-10-21

- User confirmed the PC/SC installer helper had run and all three readers were
  visible. Ran `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --areas Core,Crypto,Service --min-devices 3 --max-devices 3` through the
  elevated host USB path. No firmware was flashed.
- All three workers exited 0 and cleaned their installed CAPs. Crypto on
  D92E743D5F640C0F completed 466 calls (11 OK; unsupported and exceptions
  preserved). Core on E0E14525E64AF6ED completed 291 calls (272 OK, 15
  spec-allowed unsupported, four expected uninstalled optional Ext calls).
  Optional Ext INSTALL returned `6A88`; its uploaded CAP was rolled back.
  Service on 5CFC5328F5BDE437 completed 124 calls (94 OK; 28 existing
  mismatches, one SW_ERROR, one EXC_RUNTIME). The coordinator reports the
  optional Ext stage as failed even though Core tests and rollback passed.
- Artifacts: `artifacts/results/deploy/parallel-20261021-223127/`;
  CSV SHA-256: Pico 1 `a0deba0720def10cb05f650d23bf7d39cb6981fcdc6ed4ce8b3935b042a524fa`,
  Pico 2 `31182bb12643543bc2ec8f2246ed2e5f652fde9b8c7ab85113b55d053c540600`,
  Pico 3 `e6a59e244f9d94839e0af51ed331d74b43f5673f80966c828279e41115d21c06`.

## CAP install process-restart recovery test — 2026-10-21

- Extended `tools/tests/pc_vpcd_applet_test.py` with
  `--verify-install-process-restart`. The first VPCD process injects a skipped
  registry publication and leaves the committed journal. It skips the first
  applet restore, closes the VPCD connection, and exits. The second process
  opens the saved flash image and must recover the journal before applet SELECT.
  The test reads the allocated package and class IDs from install output.
- Fixed graceful test shutdown: after closing the VPCD connection, wait up to
  five seconds for normal emulator exit so `starting_jcre()` can save the flash
  image. Send signals only if graceful exit times out. A prior immediate
  SIGINT caused intermittent loss of the saved restart state.
- Validation: `./tools/build arm` and `./tools/build-pc` pass; host VPCD
  process-restart test passes with `VPCD:INSTALL_JOURNAL_RECOVERED package=57`,
  applet SELECT `9000`, and corrected `TEST:SUMMARY pass=17 fail=0` (15 checks
  in the fresh-process applet pass plus two pre-restart checks). The earlier
  same-process run executed 15 checks; its old summary counter printed 11.
  `./tools/test-persistent-transaction` passes (`TEST:SUMMARY fail=0`). Host
  Rust warnings are pre-existing. No physical USB operation or firmware flash
  was performed.
- Also ran `--verify-install-after-publication-restart`: the first process
  published the registry entry but left the journal; the fresh process
  recognized the existing matching entry, cleared the journal, and passed
  SELECT plus all 15 checks. Thus both missing-registry and already-
  published-registry replay cases pass across process restart.
- Also ran `--verify-install-metadata-error-restart`: the CAP instance store
  returned an error after filesystem commit (INSTALL stage 8 / `6581`), then a
  fresh process replayed the journal and passed SELECT plus all 15 checks.
  The outer emulator exits 0 despite this internal INSTALL failure; the test
  asserts the explicit error marker and status word.
- Remaining: add simulated reset/power cuts at filesystem commit, registry
  publication, and journal cleanup. Install atomicity remains incomplete until
  those are covered.
- Artifacts: `artifacts/results/deploy/install-journal-process-restart-v5/FLASH_MEMORY`,
  `artifacts/logs/pc-vpcd-install-journal-process-restart-v5.log`, and
  `artifacts/logs/pc-vpcd-install-journal-inprocess-final.log`; published-entry
  restart artifacts: `artifacts/results/deploy/install-journal-published-restart/FLASH_MEMORY`
  and `artifacts/logs/pc-vpcd-install-journal-published-restart.log`; metadata
  error recovery: `artifacts/results/deploy/install-journal-metadata-error-restart-retry/FLASH_MEMORY`
  and `artifacts/logs/pc-vpcd-install-journal-metadata-error-restart-retry.log`.

## Install journal added to CAP install flow — 2026-10-21

- Added opaque persistent-record read/write APIs to `FlashMemory_Handler`; they
  use the existing transaction snapshot/rollback path. Unit coverage confirms
  record rollback, commit/readback, commit-sync failure preserving the old
  record, and transactional erase/cleanup.
- `CapLoadSession::installAndMakeSelectable()` now writes a CRC-protected
  install intent containing package/applet/instance AIDs and class index in
  the same filesystem transaction as applet persistent state. It commits that
  state before publishing the applet instance in the COW CAP registry, then
  clears the intent in a second filesystem transaction. `restoreInstalled()`
  validates and replays pending intents idempotently before restoring applets;
  mismatched or duplicate registry data is not overwritten.
- `./tools/test-persistent-transaction` passes, including five install-journal
  record cases. ARM build passes; host build passes with its existing Rust/C++
  warnings. A fresh host VPCD JCAlgTest 1.8.2 install reached
  `VPCD:APPLET_INSTALLED class-index=148`; its APDU regression passed
  (`TEST:SUMMARY pass=11 fail=0`). Artifacts:
  `artifacts/results/deploy/install-journal-vpcd-current/FLASH_MEMORY` and
  `artifacts/logs/pc-vpcd-install-journal-current.log`.
- This closes the ordinary commit-order window by leaving a durable replay
  record before CAP registry publication. Full process/power-cut injection at
  each install/recovery boundary has not yet been added, so treat cross-store
  recovery as incomplete. No RISC-V build or hardware flash was run.
- Next: add deterministic failure injection for CAP registry publication and
  journal cleanup, restart VPCD from the saved image, and assert the same
  installed applet is restored without duplicate registration. Keep physical
  validation gated by the ARM-only instruction and repository flash gate.

## Current three-reader validation and CAP-import fix — 2026-10-21

- Refreshed the approved PC/SC helper; it granted three USB nodes
  (`/dev/bus/usb/001/050`, `/048`, `/052`). `pcsc_scan -r` listed all three
  CHOUPI readers: D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437.
- Ran one bounded area per reader with
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas
  Core,Crypto,Service --min-devices 3 --max-devices 3`. All three workers
  exited 0 and cleanup passed. Crypto: 466 calls (11 OK; expected
  unsupported/results preserved). Core: 291 calls (272 OK, 15
  `UNSUPPORTED_PER_SPEC`, four optional Ext calls not installed); Ext INSTALL
  returned `6A88`, and rollback passed. Service: 124 calls (94 OK; existing
  non-Core mismatches preserved). Artifacts:
  `artifacts/results/deploy/parallel-20261021-current/`.
- The Core CSV contains 38 JCSystem rows: 37 `OK`, and the profile-allowed
  `requestObjectDeletion()` result `UNSUPPORTED_PER_SPEC` (`ILLEGAL_USE`).
  Transient/global arrays, available-memory forms, transaction begin/commit/
  abort/depth, AID lookup, and active-applet checks passed in this run.
- Added a stale dynamic-package bitmap guard to
  `FlashMemory_Handler::isPackageExist()`: dynamic package IDs 56–63 require a
  nonempty stored CAP as well as an enabled bitmap bit. Persistent-transaction
  unit tests cover stale bit/present CAP/clear bit, and install-status tests
  cover missing-import stage 10/`6A88`. `./tools/test-persistent-transaction`,
  `./tools/test-install-status`, ARM build, host build, and the 11-case host
  VPCD applet test passed. The VPCD missing-import case returns `6A88`.
- The current physical batch used firmware predating this source fix; it
  validates the three-reader runner only. No RISC-V build or flash was done.
  The source fix is not hardware-verified because the repository flash gate
  requires both architecture builds, while the active scope is ARM-only.
- Next: continue the highest-priority offline correctness item: inject a final
  filesystem-sync failure through the CAP install path and prove persistent
  rollback consistency. Do not flash until the existing architecture gate is
  met or explicitly changed.

## CAP registry header power-cut protection — 2026-10-21

- `src/cap_store.cpp` now alternates v4 CAP registry headers between the slot's
  primary metadata sector and a dedicated mirror sector per slot in the unused
  flash gap. Boot validates both and selects the newest generation; v3 primary
  headers remain readable. CAP data offset and slot sizing are unchanged.
- Extended `./tools/test-rp2350-fs-overlay` to simulate a cut after erasing the
  inactive mirror/primary destination. Both directions preserve the prior
  installed/empty registry and CAP data. A constructed v3 header fixture also
  confirms old package metadata remains readable before it is upgraded. All
  filesystem overlay and 1,000 randomized filesystem power-cut checks pass.
  `./tools/build arm`, `./tools/build-pc`, and the VPCD JCAlgTest 1.8.2 applet
  regression pass (`TEST:SUMMARY pass=11 fail=0`). Host build retains
  pre-existing Rust warnings. The glibc overlay-test build uses `mallinfo2()`
  on supported hosts and no longer emits the prior deprecation warning.
- No RISC-V build or flash was run. Cross-store atomicity is still incomplete:
  an unexpected power cut between CAP instance-header publication and the
  filesystem transaction's final commit can leave those stores inconsistent.
  Next add a durable install journal and inject failures through the complete
  `CapLoadSession` install path. Current ARM-only scope conflicts with the
  repository's two-architecture flash gate, so do not flash this change.

## Latest reader refresh and interrupted parallel rerun — 2026-10-21

- `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial
  5CFC5328F5BDE437` completed Pico 3's ARM flash verification and bounded
  runtime capture. The first subsequent `pcsc_scan -r` saw only Pico 1 and
  Pico 2; running the approved `sudo -n /usr/local/sbin/pico2-pcsc-reader`
  granted all three USB nodes, after which `pcsc_scan -r` listed all three
  physical readers.
- A parallel API 3.0.5 rerun started all three workers in
  `artifacts/results/deploy/parallel-20261021-213747/`. Pico 2 Crypto (466
  calls) completed and cleaned up. Pico 3 Core (291 calls) completed and
  cleaned up; optional Ext INSTALL returned `6A88` and rollback passed. Pico 2
  JavacardxSec INSTALL also returned `6A88` and rollback passed. Pico 1
  Security and Pico 2 Service stalled while waiting in the official Java
  client; Pico 3 stalled during Javacardx upload. The coordinator was
  interrupted after roughly three minutes, and no child worker remained.
  These artifacts are incomplete and do not replace the successful complete
  sweep at `parallel-20261021-211910/`.
- Before retrying, investigate the SmartcardIO/GPPro waits under concurrent
  load. Prefer one exact-reader worker at a time first; check card registry
  state and clean any partial Security, Service, or Javacardx package before
  another deployment. Keep the known Core reset fix and expected unsupported
  algorithm results out of scope for this investigation.

## Sequential recovery after interrupted workers — 2026-10-21

- Inspected GP registries after the interruption. Pico 1 had only the JCAlgTest
  fixture plus the partial `AlgTestApiSecurity_305` package; Pico 2 had the
  partial `AlgTestApiService_305` package; Pico 3 was clean. Removed only the
  Security and Service test CAPs from Pico 1/2. JCAlgTest remained installed
  on Pico 1.
- Reran Security alone on Pico 1 using exact-reader selection. It completed
  901 calls in about two minutes and cleaned up the CAP: 822
  `UNSUPPORTED_PER_SPEC`, 63 `OK`, 13 `EXC_RUNTIME`, 2 `WRONG_RESULT`, and 1
  `SW_ERROR` (one CARD_ERROR). Reran Service alone on Pico 2; it completed 124
  calls and cleaned up: 94 `OK`, 28 `WRONG_RESULT`, 1 `SW_ERROR`, and 1
  `EXC_RUNTIME`. These reproduce the known area results, while showing both
  clients finish sequentially.
- Artifacts: `artifacts/results/deploy/pico1-security-sequential-20261021.csv`
  SHA-256 `9d33c4e46a62f3578c846012ffae44260474b1c46d5ef88a6b5b9772618d8759`;
  `artifacts/results/deploy/pico2-service-sequential-20261021.csv` SHA-256
  `11f30e8e62522c13b5cc8a0de07ff5d820232be877fb918fed0d85f935d5c162`.
- At the time of this checkpoint, sequential Security and Service both
  completed. Subsequent two-reader and one-area-per-board three-reader runs
  also completed (see next section), so reader concurrency itself is working.
  The remaining question is which later area in the interrupted multi-area
  worker chain stalled. Use one area per worker for predictable parallel
  batches, and investigate chained areas only when needed.

## Bounded concurrent validation on two and three readers — 2026-10-21

- Two-worker `Security,Service` run at
  `artifacts/results/deploy/parallel-20261021-214449/` completed with both
  worker exit codes 0. Security completed 901 calls with the same outcomes as
  the sequential rerun; Service completed 124 with the same outcomes. Both
  CAP cleanups passed.
- Then ran one area per reader (`Crypto`, `Core`, `Service`) on all three
  readers at `artifacts/results/deploy/parallel-20261021-214712/`. All three
  workers completed and exited 0. Crypto completed 466 calls and cleanup;
  Service completed 124 and cleanup; Core completed 291 calls, with Core and
  Multi cleanup. Optional Ext INSTALL returned expected `6A88` and partial
  LOAD rollback passed. The coordinator itself returned 1 only because this
  optional Ext INSTALL is reported as a failed stage.
- Final `pcsc_scan -r` lists all three serials (D92E, E0E, 5CFC). These
  bounded runs establish that concurrent two- and three-reader execution
  works when each worker is given one area. The incomplete prior run involved
  several sequential areas per worker, so isolate which chained area caused
  the wait before diagnosing a general concurrency problem. Use one area per
  worker for predictable parallel runs; retain the existing complete sweep as
  additional evidence.
- Three-reader CSV hashes: Pico 1 Crypto
  `a8de9f60a92f743c68fa6d83f6b40e5a589baa00df5bc1b8605f8be29369b471`, Pico 2
  Core `3f406b78e69cfabaaefb23b0f24b8e984a248ae31f8b5416185ac838d099cbcc`,
  Pico 3 Service
  `1fc8ec6747a67708929a43e9847f1f3024fa6600cd788e09fc0c099cb40aca3b`.

## CCID reset semantics and focused three-reader retest — 2026-10-21

- The PC/SC helper now exposes all three CHOUPI Picos and two empty Virtual
  PCD terminals. A focused physical run of Security (Pico 1), Crypto (Pico 2),
  and Core (Pico 3) completed all tests and cleaned each installed CAP. The
  only failed stage was the known optional Ext INSTALL refusal (`6A88`); its
  partial LOAD rollback passed. Artifacts are under
  `artifacts/results/deploy/parallel-20261021-210341/`.
- Core again returned one wrong `Applet.deselect()` result. Client disassembly
  confirms the row is an applet counter check: `select()` increments transient
  slot 0, `deselect()` increments slot 1, and the probe checks slot 1 equals
  slot 0 minus one. This run recorded 239 selects and 98 deselects. It is not a
  VM `void` return-value error.
- Fixed reset-path omissions in `src/ccid_device.cpp`: `IccPowerOn` and USB bus
  reset now drop selected applet state and clear reset-transient arrays;
  `IccPowerOn` also resets SCP state and aborts partial CAP transfer state
  before restoring installed package metadata. `./tools/build arm`,
  `./tools/build-pc`, and the host VPCD applet test pass
  (`TEST:SUMMARY pass=11 fail=0`, log
  `artifacts/results/deploy/vpcd-reset-lifecycle-20261021.log`). The physical
  source change was flashed to Pico 3 with `tools/cycle choupi --serial
  5CFC5328F5BDE437`. Both architecture builds passed and the bounded boot
  capture passed. Two subsequent physical Core API runs each reported
  `select() returned=-19` and `deselect() returned=-19`, completed all 291 calls
  with 272 OK/15 allowed unsupported/4 optional Ext calls not installed, and
  cleaned up Core and Multi CAPs. No Core `WRONG_RESULT`, `SW_ERROR`, or
  `EXC_*` remained. Results:
  `artifacts/results/deploy/pico3-core-resetfix-20261021.csv` and
  `artifacts/results/deploy/pico3-core-resetfix-repeat-20261021.csv`.
- After the flash the third reader temporarily disappeared from PC/SC while
  all three remained in `lsusb`; the approved
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` refreshed the ACL and all three
  readers returned. Do not erase, alter OTP, or change boot security.
- Flashed the same ARM image to Pico 1 (`D92E743D5F640C0F`) and Pico 2
  (`E0E14525E64AF6ED`); both 18-second cycle checks passed. The complete
  parallel API 3.0.5 sweep then finished on all three cards at
  `artifacts/results/deploy/parallel-20261021-211910/`. Core reported 272 OK,
  15 allowed unsupported, four Ext calls unavailable because its import is
  absent, and no wrong results/runtime/status errors. Its 37 JCSystem calls
  passed and one was spec-allowed unsupported. Core/Multi, Crypto, and Service
  physical CAP cleanup passed. Javacardx/JavacardxExt/JavacardxInt/Biometry and
  JavacardxSec CAP INSTALLs were refused with `6A88`; rollback removed each
  partial LOAD. Security/Crypto/Service still contain non-Core API mismatches;
  preserve results and do not turn expected algorithm absence into work.
- Full-run CSV hashes: Pico 1 `a7565906e8618c8bb164e406b8af2a99856d0aa392b83b11a95d5e3f8cb4cc91`, Pico 2 `da135c19e1de741037e69dc45453766846e28a33fa4796aa31657de040e46cb7`, Pico 3 `22a393d6c1d4643af35240b6eccf71167abc23c4447515401f4da470eae50a1d`.
- The power-up helper initially found no JCAlgTest fixture on Pico 3 or Pico 1.
  Installed the repository-built JCAlgTest 1.8.2 CAP on Pico 1 and reran the
  bounded probe: package/applet registry, SELECT, GETVERSION `1.8.2_jc305`,
  and JCSystem info all passed. That fixture remains installed on Pico 1 for
  follow-up persistence checks. Pico 3 remains USB-visible but intermittently
  absent from PC/SC despite ACL refresh; see the latest QKB entry.
- Next: continue on Core/JCSystem and remaining applet/runtime priorities;
  use all three matched devices for parallel runs. Preserve all existing
  uncommitted work. Git commits remain unavailable because `.git` metadata is
  read-only.

## Three-reader deploy sweep and partial-scenario rollback — 2026-10-21

- PC/SC now lists three CHOUPI readers: `D92E743D5F640C0F`,
  `E0E14525E64AF6ED`, and `5CFC5328F5BDE437`; two empty Virtual PCD terminals
  precede them. `tools/run-deploy-parallel --dry-run --api 305` found all three
  and assigned Security to Pico 1, Crypto/JavaCardxSec/Service to Pico 2, and
  Core plus the remaining areas to Pico 3.
- The physical parallel run is archived at
  `artifacts/results/deploy/parallel-20261021-204343/`. Security (901 calls),
  Crypto (466), Service (124), and Core (291) executed. Core had 272 OK, 15
  allowed unsupported, and four optional Ext calls not run. JavacardxSec,
  Javacardx, JavacardxExt, JavacardxInt, and Biometry uploads were refused with
  `6A88` because imported packages are absent. Ordinary failed uploads were
  removed by the runner. Do not count `UNSUPPORTED_PER_SPEC` algorithms as
  failures.
- The first parallel Core run exposed that `upload_scenarios()` did not roll
  back a partial Ext LOAD after INSTALL refusal. Manually removed that Ext
  package from Pico 3, factored a common partial-install rollback helper, and
  reran Core on Pico 3. `UPLOAD_EXT_ROLLBACK OK` was recorded and GPPro then
  showed only the ISD. Focused CSV:
  `artifacts/results/deploy/pico3-core-scenario-rollback-20261021.csv`.
- A repeat Core run again produced one `WRONG_RESULT` for
  `Applet.deselect()`; later analysis confirmed this is a callback counter
  mismatch, not a VM `void` return value. Test CSVs:
  `artifacts/results/deploy/pico3-core-scenario-rollback-20261021.csv` and
  `artifacts/results/deploy/pico3-core-repeat-20261021.csv`.
- No firmware was flashed. ARM build passed; host build passed with existing
  Rust/C++ warnings; RISC-V was not built due the active ARM-only constraint.
  The persistent transaction test needed its local FFI stub updated to declare
  and implement the already-used `path_static16`; it then passed. The FS overlay
  suite passed. A best-effort removal of the CAP instance record was added
  after failed persistent-transaction commit, but final-sync failure has not
  yet been injected through the full CAP install path.
- Next: analyze `Applet.deselect()`; implement a focused final-sync injection
  test for CAP install rollback; continue improving deploy API/package support.
  `tools/run-deploy-parallel --api 305` is ready for repeat physical sweeps.
- Git commits remain unavailable because the execution environment exposes
  `.git` metadata read-only. Preserve all existing worktree changes.

## Three CHOUPI boards flashed; PC/SC helper refresh pending — 2026-10-21

- All three known Pico 2 serials are now visible in the VM as CHOUPI CCID
  devices: `D92E743D5F640C0F`, `E0E14525E64AF6ED`, and
  `5CFC5328F5BDE437`. The last board briefly disappeared after its first
  cycle, then reattached; the latest `lsusb -d 2e8a:` showed three
  `2e8a:0009` devices. Its 18-second cycle log passed.
- `tools/flash`, `tools/monitor`, and `tools/cycle` now accept
  `--serial SERIAL`; `tools/flash` also requests picotool flash verification.
  Serial selection was tested on the active `E0E14525E64AF6ED` board from
  CHOUPI application mode: the picotool reset interface was absent, the
  targeted CDC 1200-baud fallback entered BOOTSEL in 2 seconds, the flash
  verified at 100%, and the 18-second CHOUPI cycle passed. Both ARM and RISC-V
  builds passed in each cycle.
- `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --dry-run` safely found
  one PC/SC Pico and reported that two more readers are needed. The approved
  installed helper still rejects `found 3`; source helper supports up to
  three, but cannot be installed autonomously under the current sudo rule.
- **Operator action required:** run `sudo ./tools/install-pcsc-sudoers`, then
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`, then `pcsc_scan -r`. Expected:
  three Pico readers plus the two empty Virtual PCD entries. After that, run
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305` through the
  elevated host-access path to start three independent API workers.
- No physical API calls ran in this step because the system pcscd ACLs are not
  yet granted for the two added nodes. CHOUPI runtime checks passed on both
  added devices. Git commits remain unavailable because `.git/index` is
  read-only in the current execution context.

## Serial-targeted three-board setup — 2026-10-21

- Confirmed `tools/flash` could not safely distinguish two BOOTSEL Picos. Added
  `--serial SERIAL` to `tools/flash`, `tools/monitor`, and `tools/cycle`; the
  wrappers now use picotool's serial filter and the matching stable
  `/dev/serial/by-id/` path. The CDC 1200-baud reset from the repository's
  recovery flow successfully moved both added boards into BOOTSEL. Shell
  syntax and CLI help checks pass.
- ARM and RISC-V builds both pass. Flashed the current ARM CHOUPI image to
  `E0E14525E64AF6ED`; its finite 18-second capture repeatedly reported
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. Flashed the
  current ARM CHOUPI image to `5CFC5328F5BDE437` via
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial 5CFC5328F5BDE437`;
  its archived cycle log passes. An earlier `cycle arm` on that third board
  flashed only the smoke image, which was subsequently replaced by CHOUPI.
- Current live inventory is degraded: USB sees only the original
  `D92E743D5F640C0F` and second CHOUPI `E0E14525E64AF6ED` as `2e8a:0009`. The
  third board appeared briefly as `2e8a:000f` after its successful cycle, then
  its USB node disappeared from the VM. `pcsc_scan -r` still lists only the
  original reader because the installed helper rejects multiple USB matches.
- Updated helper source `tools/pico2-pcsc-reader-root` accepts one to three
  matching devices, but the installed helper must be refreshed by the
  operator: `sudo ./tools/install-pcsc-sudoers`, followed by
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`. Then verify reader count with
  `pcsc_scan -r`. Physical API parallel runs await this helper update and
  third-board USB reattachment. No APDU/API tests were run against these boards
  in this step.
- The platform build gate is satisfied for flashing: both ARM and RISC-V
  wrappers passed. Commits could not be created because git reports `.git`
  index read-only in this execution context.

## Three-board state reconciliation — 2026-10-21

- The user reported the two added Pico 2 boards in BOOTSEL, but the VM's live
  host-access USB inventory shows three RP2350s in application mode at bus 1,
  addresses 28, 31, and 32. `picotool info` found no BOOTSEL devices and
  identified all three as having USB serial connections. `lsusb` reports
  `2e8a:0009` for all three; two are CHOUPI CCID and one is generic Pico
  firmware. Do not treat these observations as a successful BOOTSEL attach.
- A targeted `picotool reboot --usb --force --ser E0E14525E64AF6ED` failed
  with `Unable to locate reset interface on the device`; no flash or other
  state change was made. The third device was not targeted.
- The installed PC/SC helper is still the single-reader version and rejects
  three matching USB devices (`found 3`). The repository helper has already
  been revised for one to three matching devices, but deployment requires the
  operator to run `sudo ./tools/install-pcsc-sudoers`, then the approved
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`. Until then, PC/SC lists only
  the original physical reader. A dry run of `tools/run-deploy-parallel`
  resolved that reader correctly and made no card writes.
- Next: confirm BOOTSEL devices are actually passed through to this VM
  (`lsusb -d 2e8a:` should show `2e8a:000f`; `picotool info` should list them),
  then boot CHOUPI firmware on them when the build/flash gate allows. To enable
  parallel PC/SC API tests for the two existing CHOUPI boards, install the
  updated helper and rerun it. No firmware was flashed in this step.

## ROM package import resolution — 2026-10-21

## ROM package import resolution — 2026-10-21

- Updated `Import_Handler::getPackageIndex()` to use compatible package
  versions (same AID and major version, imported minor no greater than
  resident). Updated the PC VPCD `isPackageExist()` path so ROM-mask CAPs are
  visible to import lookup even though they are outside the mutable package
  bitmap.
- ARM and host builds pass. Current-source Core 3.0.5 VPCD install/SELECT
  passes (`11/11`). JXExt's `B0835102010000` still returns `6F00`: the CAP
  imports `javacardx.external` AID `A0000000620203` version 1.0, which is not
  present in the generated ROM mask. See the new TODO and QKB entries; don’t
  call this APDU fixed. The next VM task is to validate imports at INSTALL and
  decide whether the API package should be added to the ROM profile.
- Physical status: three RP2350 devices are visible to the VM, but only one
  exposes a CHOUPI PC/SC reader; the other two are in BOOTSEL. No flash occurred
  because the current ARM-only constraint and both-architecture flash gate
  conflict. No real-device firmware validation for these source changes.
- Commit not created: `.git` metadata is read-only in this environment.

## Three Pico 2s connected — PC/SC access update pending — 2026-10-21

- Rebooted both BOOTSEL devices directly to their existing applications with
  `picotool reboot --application`, without flashing. Serial
  `E0E14525E64AF6ED` came up as a second CHOUPI CCID device; serial
  `5CFC5328F5BDE437` came up as generic Pico firmware with no CCID interface.
  Together with original CHOUPI `D92E743D5F640C0F`, USB sees two CHOUPI CCID
  devices plus one generic Pico.
- `pcscd` has an ACL on only the original USB node. The currently installed
  approved helper refuses multiple CHOUPI devices; restarting pcscd therefore
  still lists just the original reader. Updated `tools/pico2-pcsc-reader-root`
  to apply ACLs to one to three matching nodes and restart once. Syntax check
  passes, but the updated helper is not installed yet.
- **Next user command:** `sudo ./tools/install-pcsc-sudoers` deploys the revised
  helper under the existing no-argument rule. After that, run
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`; I can then verify both readers
  and launch a two-worker deploy test. The generic Pico requires CHOUPI firmware
  before it can become reader three.
- No firmware was flashed. ARM and host builds passed for the package-import
  fix; RISC-V was intentionally not built due the ARM-only constraint, so the
  repository's both-architecture flash gate still blocks flashing the third
  board.
- Commit not created: `.git` metadata is read-only in this environment.

## Three-device test preparation and wide static tags — 2026-10-21

- Added `--reader-name` support to the deploy API runner. It resolves the
  official Java client's one-based index using the exact SmartcardIO name and
  pins GlobalPlatformPro to that same device. Added `tools/run-deploy-parallel`
  to distribute API areas over up to three physical Picos with separate result
  CSVs and logs. It requires all three live PC/SC readers by default and skips
  the known RSA-2048 keygen reader-drop case. Usage: elevated
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305`.
- Current enumeration: one active CHOUPI reader
  (`D92E743D5F640C0F`) plus two RP2350 BOOTSEL devices. The runner correctly
  refused to start until three PC/SC readers are available. `pcsc_scan -r`
  still lists two empty Virtual PCD readers first; exact name resolution
  handles the Java terminal index safely.
- Fixed JavaCardX CAP INSTALL failure: its StaticField component contains 164
  slots and reaches byte offset 326, beyond the old 8-bit tag. New 16-bit
  big-endian tag roots are used above offset 255; legacy tags remain unchanged
  below that. ARM and host builds passed. Current-source VPCD install/SELECT
  passed (`11/11`, `9000`). No Pico was flashed.
- Artifacts: `artifacts/results/deploy/javacardx-vpcd-select-only-wide-static-20261021.log`
  (SHA-256 `57a7070ce1451dbee1458d4fbd51167389c7c4929a98b27049f44eb68026fb40`).
  Built ARM UF2 SHA-256: `3c4c08d1a8f16bf0f261b059a0fbc7680f63a841dcd28e8555842b125864b8bf`.
- Next: when all three Picos run CHOUPI and PC/SC exposes them, run the
  three-worker deploy coverage, then continue resolving Javacardx and other
  install/call failures. Flashing remains blocked by the ARM-only constraint
  versus the repository's both-architecture flash gate.
- Commit not created: `.git` metadata is read-only in this environment.

## Single physical Pico Crypto API run — 2026-10-21

- Continued the deploy API sequence on the same named physical Pico reader
  (terminal index 3; ATR `3B800181`). Crypto 3.0.5 upload, 466-call test, and
  cleanup all completed successfully. The documented `genKeyPair` RSA-2048
  case was excluded because some readers disconnect on that operation.
- Counts: 444 `UNSUPPORTED_PER_SPEC`, 14 `OK`, six `EXC_CARDRUNTIME`, and two
  `SW_ERROR`. The two generic `6F00` rows were `javacard.security.Signature()`
  and `javacardx.crypto.Cipher()`; six ILLEGAL_VALUE (`0x0002`) results came
  from Cipher update/doFinal or IV/invalid-input cases. These are from the
  firmware already on the board and need source/VPCD comparison before being
  called implementation defects. Unsupported algorithm outcomes remain
  expected per the user's instruction.
- CSV: `artifacts/results/deploy/pico2-single-crypto-reader3-20261021.csv`
  (SHA-256 `1a9766f365ae0d47c370d884017779c2f1ad2f4c2d8baec9120f0b38a7aa4ccf`).
  Next deploy stage is `JavacardxExt`, then continue the remaining relevant
  deploy areas. No source change, ARM build, or flash occurred in this step.

## Single physical Pico Security API run — 2026-10-21

- The physical reader was `Pico 2 CHOUPI CCID [JCAlgTest CCID]
  (D92E743D5F640C0F) 00 00`, ATR `3B800181`. `gp --list` confirmed the board
  responds and has the Core/Multi/Ext and Javacardx applets. The Security 3.0.5
  deploy CAP was uploaded, tested, and removed successfully with
  `AlgTest_JavaCard/deploy/run_api_test.sh`.
- First launch mistakenly passed client reader index 1. The Java client sees
  both empty Virtual PCD terminals before the Pico, so it waited on an empty
  reader; that run was interrupted. Removed its installed `AlgTestApiSecurity_305.cap`
  explicitly, then reran using client index 3 and the exact Pico reader name.
- The successful physical run completed all 901 calls: 822
  `UNSUPPORTED_PER_SPEC`, 43 `OK`, 15 `SW_ERROR`, 13 `EXC_RUNTIME`, and 8
  `WRONG_RESULT`. CAP cleanup passed. These results are from the firmware
  already on the board; no flash occurred in this session, so they do not
  validate the current unflashed source build. The earlier no-RISC-V constraint
  and both-architecture flash gate remain in force.
- CSV: `artifacts/results/deploy/pico2-single-security-reader3-20261021.csv`
  (SHA-256 `49032c715eace1fe830e3c42da83603d9f1db83b51deda100c37b6556af86bee`).
  It shows failures across KeyBuilder/Key/DESKey/AESKey methods and the
  MessageDigest constructor. Treat these as a physical baseline for the
  currently flashed image; compare with current-source VPCD probes before
  assigning code fixes. The interrupted index-1 attempt is recorded separately
  at `artifacts/results/deploy/pico2-single-security-20261021.csv`.
- Next: inspect the exact failed call rows against current source and focused
  VPCD results, then continue with Core/JCSystem priorities. Keep the same
  explicit reader pinning; when both Virtual PCD slots are listed first, pass
  `--reader-index 3` to the official API client.

## Deploy Core dynamic-receiver token fix — 2026-10-21

- The physical Core 3.0.5 runner, with Multi and Ext scenarios, initially
  reported 256 OK, 19 allowed unsupported, 14 runtime exceptions, and two
  status errors. All CAP uploads and cleanup passed after flashing the then
  current source; the earlier populated DELETE `6581` no longer reproduced.
- Fixed `Class_Handler::getMethodOffset` for a dynamic receiver: keep the
  importing CAP's virtual token until resolution crosses into the embedded
  framework superclass, where it is remapped once. The old path remapped
  before dynamic lookup and again at the superclass boundary. Preserve the
  unrelated, pre-existing interface-resolution hunk in the same source file.
- ARM and RISC-V builds passed. The initial `./tools/build-pc` run failed on
  an intermediate misplaced edit; it was corrected and both firmware builds
  passed. `./tools/cycle choupi` reached 100% but picotool exited with a USB
  connection error. A subsequent BOOTSEL `picotool verify` matched the exact
  built UF2 (SHA-256 `552c2b5a79c3669a24ed70b627ab8c935a3fb404d004c90b20b10cbe32f039a6`),
  and the verified image rebooted with seven serial `TEST:PASS` lines and no
  failure markers.
- The post-fix physical Core runner reported 267 OK, 19 allowed unsupported,
  three `SW_ERROR`, and two `EXC_RUNTIME`; all nine Applet method failures
  cleared. Remaining failures are OwnerPIN validated-flag operations and two
  Shareable calls. Core, Multi, and Ext cleanup passed. The card currently
  has Security, Crypto, and Service installed; Core was removed by the runner.
- Continue with Security/Crypto/other deploy CAP probes. Fix OwnerPIN and
  Shareable separately, then rerun Core and verify restart. Results are in
  `artifacts/results/deploy/pico2-deploy-core-{,tokenfix-}20261021.csv`.

## Physical JFS2 migration verified with CAP2 preserved — 2026-10-21

- With the Pico reinserted in BOOTSEL (`2e8a:000f`), flashed the revised ARM
  `pico2_choupi.uf2` using `picotool load -v` without auto-reboot; flash
  verification passed. Rebooted to the application and captured a finite
  30-second serial log: 30 `TEST:PASS choupi_runtime`, 30 matching summaries,
  no `FAULT` or `TEST:FAIL`. The one-time migration print was missed by CDC
  attach timing, so the flash readback is the migration proof.
- A verified post-migration full-flash image has SHA-256
  `51a303f6f00da7611737b4e2a7b174ec59e7d793b6efbcb25002daab0adf6391`.
  JFS2 slot 0 at `0x188000` validates at generation 11748, with 2,264 records,
  28,852 overlay bytes, and a 51,492-byte payload identical to the old
  generation-11747 snapshot. CAP3/old-JFS2 region `0x100000..0x188000` and
  CAP2 extent `0x1c0000..0x1c8000` match the pre-flash backup byte-for-byte.
  JFS2 slot 1 remains unused; physical second-slot writes were not tested.
- Booted again from the migrated state: an eight-second serial log had eight
  runtime passes and summaries, with no failure markers. Physical GPPro
  `--list` then returned the same four selectable applets and four loaded
  packages. The repository's `test-pico-powerup` wrapper exited 1 because it
  expects a different, hard-coded JCAlgTest AID; its GPPro listing itself
  succeeded. This is a fixture mismatch, not a migration failure.
- Card is running the revised firmware. Keep the pre-flash backup and
  post-migration image under `artifacts/backups/`. CAP2 bytes are preserved
  but CAP2 package ID 20 is not converted into CAP3 or made active in the VM.
  Do not flash old CAP2-writing firmware without considering its 256 KiB erase
  range, which overlaps the new JFS2 slot 1.

## CAP2-preserving flash layout revision — 2026-10-21

- After BOOTSEL reattachment, a verified full flash read showed the previous
  interrupted transfer had written the new ARM firmware, while every storage
  byte in `0x100000..0x200000` remained identical to the pre-flash backup.
  The application enumerated as `2e8a:0009` but the CAP2 fail-closed guard
  stopped USB servicing, and the bounded serial monitor failed while
  configuring `/dev/ttyACM0`; no runtime `TEST:PASS` was observed.
- Revised `src/flash_layout.hpp`: CAP3 remains at `0x100000..0x188000`;
  JFS2 slots are now `0x188000..0x1b0000` and `0x1c8000..0x1f0000`,
  preserving CAP2's maximum header/code extent `0x1c0000..0x1c8000`.
  Removed the CAP2 startup guard. The shared-flash suite passes migration,
  second-slot sync, and CAP2-byte-preservation tests; ARM and RISC-V builds
  passed. Hardware validation of this revision awaits another BOOTSEL
  reattachment. The user has been asked to do that.
- Keep the pre-flash image at
  `artifacts/backups/flash-before-layout-migration-20261021.bin` and the
  verified post-transfer image at
  `artifacts/backups/flash-after-interrupted-layout-flash-20261021.bin`.
  The old JFS2/CAP3/CAP2 contents were byte-identical in both. On reconnect,
  flash the revised ARM image only after confirming the device, then inspect
  bounded serial output and read back CAP2 bytes and new JFS2 headers.
- CAP2 bytes are preserved physically, but CAP2 package ID 20 is not converted
  to the CAP3 format or active in the current VM. Returning to old firmware
  that writes CAP2 may erase the second new JFS2 slot; keep the backup.

## Physical flash-layout test interrupted by USB detach — 2026-10-21

- Connected BOOTSEL Pico was visible as `2e8a:000f`. Full flash backup
  `artifacts/backups/flash-before-layout-migration-20261021.bin` passed
  `picotool save -a -v`, size 2,097,152 bytes, SHA-256
  `408ba9dff6edf6924830d6ae9e9860a1aa7fef40fe6976ebe9a2ee736e9bc4b6`.
  Old JFS2 generations 11747 and 11746 validated. Four CAP3 headers and a
  valid, distinct CAP2 package 20 image were found. The current firmware
  intentionally fails closed on that CAP2 marker, so a successful migration
  test cannot proceed without a data-preserving decision for CAP2.
- `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` built ARM and RISC-V, then
  `picotool` reported 100% transfer but aborted with `picoboot::connection_error`
  before confirming reboot. The wrapper's recovery exhausted its bounded
  attempts and authorized `lsusb -d 2e8a:` found no Pico. No serial output or
  post-flash contents were verified. See
  `artifacts/logs/cycle-choupi-20261021T091042Z.log` and the new QKB entry.
- The user has been asked to reconnect in BOOTSEL. After it reappears, first
  compare flash with the backup, especially `0x100000..0x200000`, then capture
  bounded serial output. Keep the CAP2 image and backup intact. Do not claim
  hardware migration passed based on the transfer progress display.

## Flash layout migration implementation — 2026-10-21

- Shared layout now keeps all eight CAP3 slots at `0x100000..0x188000` and
  moves JFS2 to `0x188000..0x200000` in two `0x3c000` slots. A valid old JFS2
  snapshot is copied through RAM into the first new slot at boot. Incomplete
  migration can retry without erasing the old snapshot. See
  `docs/flash-layout-migration.md` for recovery boundaries.
- `./tools/test-rp2350-fs-overlay` now uses the real CAP3 store on one fake
  flash image and passes coexistence, migration, and power-cut cases. ARM and
  RISC-V builds passed; ARM was rebuilt after the final source change. A
  pre-existing host-only `mallinfo()` deprecation warning remains in the test
  build. Read-only, authorized `lsusb -d 2e8a:` found no Pico (exit 1), so no
  flash or physical serial validation was possible.
- CAP2 data at `0x1c0000` is not automatically converted; detection stops
  initialization before JFS2 could reuse it. A corrupted old JFS2 snapshot
  cannot be reconstructed and also stops initialization when recoverable
  storage markers remain. Back up any valuable card flash before deploying.
- The worktree still contains extensive unrelated edits, including the
  unfinished Shareable dispatch changes. Only the filesystem-related portion
  of `src/choupi_pico2.cpp` belongs to this step; preserve its other edits.

## Current functionality review update — 2026-10-21

- Added `review_20261021.md`, comparing the 2026-10-19 review with current
  source, progress evidence, and six offline test wrappers. Distinct instance
  AID restore and SELECT `61xx` selection retention are implemented, with
  firmware/end-to-end coverage still open. CAP/filesystem transaction recovery
  and SCP03 production key provisioning remain unfinished.
- **New source-confirmed blocker:** CAP3 slots occupy flash
  `0x100000..0x188000`, overlapping both filesystem snapshot slots
  `0x100000..0x180000`. The two stores can erase each other's records. The
  current 2 MiB flash budget cannot fit the declared CAP and FS capacities
  above a reserved 1 MiB firmware region. Do not use the current layout for
  further physical CAP/FS writes; fix and validate the shared map and plan
  migration first. See the new QKB entry and review for exact evidence.
- The review changed documentation only. Offline VPCD/session, short-APDU,
  GP DELETE/registry, persistent-transaction, and filesystem-overlay wrappers
  exited 0. No build gate, flash, USB/PCSC, or physical APDU ran. Existing
  uncommitted source work, including unresolved Shareable dispatch, remains
  untouched.

## Current task — deploy Core Shareable dispatch handoff — 2026-10-21

- Work was requested to test the deploy API applets on the physical Pico and
  fix any failures. The authorized USB check found no device:
  `PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader` reported
  `found 0`; gated `lsusb -d 2e8a:0003` and `lsusb -d 2e8a:0009` showed none.
  Nothing was flashed or sent to hardware.
- The deploy Core 3.0.5 CAP loaded, installed and selected through host VPCD;
  SELECT returned `9000`. Its Shareable probe APDU `B083210000` still returns
  `04 9000`, so the Core Shareable row remains broken and the task is
  incomplete.
- Runtime work in this checkout adds a Java callback wrapper for
  `getAppletShareableInterfaceObject()` (self-AID only), exposes the current
  registered/restored applet reference to that wrapper, and fixes interface
  resolution so each implementation interface is resolved from the concrete
  class package. Host build `./tools/build-pc` passed after those changes.
  `./tools/build arm` passed before the final wrapper/resolver source state, so
  it must be rerun before any flash. No RISC-V build or physical validation
  occurred.
- Diagnosis: interface lookup reused a `ConstantPool_Handler` after resolving
  an imported framework interface. That mutates the resolver's package; the
  next internal interface reference was then looked up under package 0 rather
  than the applet package. A diagnostic run showed the subsequent
  `invokeinterface` raised `SecurityException` (enum 29). The resolver fix is
  in `choupi/src/jc_handlers/jc_class.cpp`, but it has not fixed the end-to-end
  APDU yet. Next, add a narrow temporary trace to confirm whether the fixed
  lookup matches the applet's internal interface; if it does, trace the
  callback result and interface method invocation to identify the remaining
  exception. Remove temporary diagnostics after capture.
- Temporary `INVOKEVIRTUAL` debug logging was removed during handoff. Preserve
  unrelated dirty worktree changes. The resolver, callback bridge and wrapper
  are uncommitted because the deploy Shareable APDU still fails; do not treat
  them as validated or commit them until the focused APDU passes and both
  required architecture builds are current.
- Evidence: `/tmp/deploy-shareable-vpcd-diag.log` (dispatch trace) and
  `/tmp/deploy-shareable-vpcd-final.log` (post-fix failure). Temporary logs
  may be unavailable after cleanup/reboot; rerun the bounded VPCD reproduction
  if missing. The PC VPCD shim is single-package/single-instance and cannot
  validate concurrent package coexistence; see the QKB entry below and the
  earlier multi-package notes.
- Session digests reviewed for this handoff were redacted operational
  checkpoints only. They are not promoted and are not canonical project
  knowledge; technical state above comes from the checkout and current test
  evidence.

## Current task — deploy CAP smoke tests and UserException fix — 2026-10-21

- The authorized BOOTSEL visibility check found no physical Pico in the VM:
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` reported `found 0`, and gated
  `lsusb -d 2e8a:` was empty. No flash, physical LOAD/INSTALL, or hardware APDU
  was attempted.
- Fixed `Bytecodes::docheck()` so a failed speculative JC305 interface-token
  lookup cannot replace a valid class reference with `SecurityException`.
  The cause and workaround are recorded in `.agent/QKB.md`.
- Implementation commit: `58e92e6` (`Fix imported class checks for deploy Core`).
- Validation: ARM build passed (`build/arm/pico2_choupi.uf2`, image end
  `0x100c5c7c`); host build passed. VPCD case 78 passed, including generic
  `catch (Exception)`, `instanceof UserException`, and reason `0x1234`. The
  actual deploy Core 3.0.5 CAP loaded/installed/selected and its UserException
  APDU returned `000000019000`.
- Isolated 3.0.5 deploy CAP load/install/select results: Core, Security, Crypto,
  JavacardxExt, JavacardxSec, Service, and Biometry selected with `9000`.
  Javacardx and JavacardxInt were refused at install with `6A80`, matching the
  README's missing framework-package and `int` limitations. These were separate
  host emulator runs, not concurrent package retention tests or physical API
  client runs.
- The two applets in `AlgTestApiMulti_305.cap` were each independently
  installed and selected through VPCD: AID `4A43416C675465737441706D41` and
  AID `4A43416C675465737441706D42` both returned `9000`. Each used a fresh host
  emulator image, so this still does not verify both instances coexisting.
- Multi smoke-test record commit: `8dcd7a1`.
- Host regressions: GP registry 47/47, install-status 10/10, GP DELETE 26/26.
  Deploy inventory listed all nine areas. The full transaction VPCD run passed
  cases 1–12 but was interrupted while case 13 waited; case 13 passed alone in
  59.686 seconds. Investigate the slow cumulative run separately.
- Build-pc has been restored to the default non-debug configuration. No
  RISC-V build was run. Next: when the Pico enumerates, flash ARM and run the
  official deploy Core scenarios; verify both Multi instances, upload a second
  package concurrently, check GET STATUS/DELETE/reinstall, then continue the
  README areas with the official client.

## Multi-applet physical retest — 2026-10-21

- The requested Pico 2 BOOTSEL check was run through the authorized host USB
  path. `PICO2_HOST_USB_ACCESS=1 sudo -n
  /usr/local/sbin/pico2-pcsc-reader` reported `found 0`, and gated
  `PICO2_HOST_USB_ACCESS=1 lsusb -d 2e8a:` returned no device. No flash,
  GlobalPlatform command, or physical applet test was possible.
- Offline regressions passed: GP registry 47/47, install status 10 checks,
  and GP DELETE 26/26. Deploy API 3.0.5 asset listing found all nine areas.
  The runner's three stream-fd permission messages did not prevent listing.
- Source inspection confirmed the PC VPCD shim in `choupi/src/main_pc.cpp`
  persists one package and one applet instance only. It cannot validate
  multiple-package CAP loading through the emulator. Existing hardware notes
  show the Multi scenario's two applet instances were previously selectable,
  but the newly generalized multi-package path still needs a fresh physical
  LOAD/INSTALL/SELECT/GET STATUS/DELETE cycle.
- Next: when Pico USB enumeration returns, build the ARM target under the
  repository build gate, flash, and run the deploy Core stage with scenarios.
  Verify both Multi AIDs, then upload a second distinct package while the
  first remains installed; check selection, GET STATUS, instance deletion,
  package deletion, and reinstall. Continue the README areas after Core.

## Physical and deploy retest checkpoint — 2026-10-21

- The latest gated PC/SC helper invocation reported `found 0`; no Pico 2 was
  visible to the VM, so nothing was flashed and no physical API CAP was
  uploaded.
- `AlgTest_JavaCard/deploy/run_api_test.sh --list --api 305` found the GPPro
  jar, client, and all nine listed API areas. Focused current regressions pass:
  GP registry 47/47, install-status 10/10, and GP DELETE 26/26.
- A bounded host VPCD JCAlgTest run selected the applet successfully but timed
  out on GETVERSION at 20 seconds and 60 seconds. The test input files
  `build/algtest-v182-empty-flash.bin` and
  `build/algtest-v182-load-code.bin` are dated 2026-10-14, while the PC
  emulator and ROM mask were rebuilt on 2026-10-21. A second run used the
  exact provided `AlgTest_v1.8.2_jc305.cap` concatenated into fresh load code
  and a freshly erased 512 KiB image; SELECT passed but GETVERSION still timed
  out at 30 seconds (connect took 12.368 seconds). This is an unresolved host
  VPCD method-execution failure, not evidence of a physical regression.
- Deploy API 3.0.5 asset listing passed for all nine areas; host GP registry,
  install-status, and DELETE regressions passed 47/47, 10/10, and 26/26. No
  deploy CAP was uploaded. Once hardware is visible, run Core first; its runner
  loads Multi and creates both applets with AIDs ending in `41` and `42`.
- Commit `f964152` includes pre-existing `JC_Instance` diagnostic changes
  along with the intended file-mode repair due an index refresh during the
  staged-mode operation. They are preserved in HEAD; inspect before future
  edits to that file.
- Next: diagnose the host VPCD GETVERSION timeout only if emulator testing is
  needed; restore Pico USB visibility. Once available, load deploy Multi and Ext,
  verify both Multi applets, then continue the README stages in order.

## Distinct persistence keys for high CAP class indexes — 2026-10-21

- Extended persistent applet-field tags to preserve the full 16-bit class
  index for classes above 255. Legacy classes keep their five-byte tags. New
  wide tags use six-byte roots; high-index reads retry the old low-byte key so
  records from older firmware remain accessible. DELETE clears wide and
  legacy records.
- Validation passed: `./tools/build-pc` (including Java Card OS Rust build),
  `./tools/build arm` (image end `0x100c5c3c`),
  `./tools/test-persistent-transaction`, `./tools/test-rp2350-fs-overlay`
  (30 checks), and `./tools/test-gp-delete` (26 checks). The overlay test
  confirms class 1 and 257 tags remain distinct and wide array tags compress.
- `./tools/check-vm-syntax` still cannot complete because the standalone host
  check lacks `mbedtls/aes.h`; the ARM and PC builds compile the changed VM
  sources. No RISC-V build was run.
- Physical follow-up is still open. The VM host cannot see the BOOTSEL Pico
  (`sudo -n /usr/local/sbin/pico2-pcsc-reader` found 0; gated `lsusb` empty).
  When it is accessible, flash according to the required build gate, verify
  high-index persistence and populated DELETE/reinstall, then resume the
  deploy README stages including Multi and Ext.
- Files changed for this step: `src/cap_load.cpp`, `src/choupi_pico2.cpp`,
  `choupi/src/jc_handlers/flashmemory.{cpp,hpp}`,
  `choupi/src/jc_types/jc_instance.cpp`, `choupi/os/src/{ffi.h,ffi.rs,filename.rs}`,
  `tools/check-vm-syntax`, and the persistence/overlay tests.

## Deploy DELETE cleanup follow-up — high CAP class indexes — 2026-10-21

- Source review found the physical deploy cleanup failure path: the applet
  cleanup helper rejected stored 16-bit CAP class indexes above 255 and
  returned `6581`. CHOUPI's existing `path_applet_field` ABI stores the class
  component as one byte, so cleanup now makes the same low-byte conversion as
  the runtime. A persistent-transaction regression deletes a field built from
  class index `0x1234`.
- ARM build passed (`build/arm/pico2_choupi.uf2`, image end `0x100c57b0`);
  persistent-transaction regression and GP DELETE regression passed (26/26
  DELETE checks).
- The Pico is not currently enumerated in the VM. The allowed PC/SC helper
  reported zero readers, and gated `lsusb -d 2e8a:` was empty. Do not claim a
  physical DELETE fix yet. When it returns, flash the clean ARM image, retry
  deletion of a populated Core CAP, verify GET STATUS and reinstall, then
  resume the deploy README stages.
- The persistent tag format still has only an 8-bit class component. Check for
  same-package key collisions when class indexes differ by 256; keep that
  encoding issue separate from the immediate DELETE `6581` fix.
- Code/test files in this step: `src/cap_load.cpp`,
  `tools/tests/persistent_transaction/filesystem_stub.cpp`, and
  `tools/tests/persistent_transaction/persistent_transaction_test.cpp`.

## Physical deploy API follow-up — inherited lifecycle dispatch — 2026-10-21

- Fixed `Class_Handler::doGetPublicMethodOffset()` to translate a dynamic CAP's
  imported virtual token when superclass lookup crosses into the embedded
  framework package. Before the fix, selecting the deploy Ext applet returned
  `6999`; with the fix, its inherited `Applet.select()` returned `9000` on the
  physical Pico. Core API 3.0.5 also reports inherited `Applet.select()` and
  `deselect()` as OK.
- ARM build passed; the flashed diagnostic firmware's bounded serial capture
  reported repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`.
  The source was rebuilt after removing temporary diagnostics, but that clean
  image could not be flashed because the Pico then disappeared from the VM USB
  bus (`lsusb -d 2e8a:` empty; PC/SC helper found no device). Reflash the current
  `build/arm/pico2_choupi.uf2` when USB access returns before hardware tests.
- The deploy Core client completed 291 calls on reader index 3. JCSystem,
  Util (including overlap), APDU, AID, and ordinary OwnerPIN/PIN methods passed.
  Remaining observed results: `getValidatedFlag` twice `SW=6f00`,
  `setValidatedFlag` twice `EXC_RUNTIME`, three `UserException.throwIt`
  `EXC_RUNTIME`, one Shareable wrong result, plus four scenario failures
  because Multi and Ext were not installed during that run. Eleven
  `UNSUPPORTED_PER_SPEC` rows are expected.
- Prior physical checks on this card state confirmed Multi applets A and B
  independently selectable. The documented pre-clean removed Multi and Ext
  successfully; cleanup of Core, Security, Crypto, and Service applets or
  packages returned `6581`. The last observed registry contained Core,
  Security, Crypto, and Service. Do not retry those deletes blindly; diagnose
  the persistent-data cleanup failure first.
- Result files: `artifacts/results/deploy/pico2-selectfix_APISUPPORT_3b800181.csv`
  and `artifacts/results/deploy/pico2-dispatch-fix_APISUPPORT_3b800181.csv`;
  the pre-clean GPPro transcript is
  `artifacts/logs/deploy-preclean-305-20261021.log`.
- Next: restore USB/PCSC discovery, flash the clean current ARM build, retest
  Ext SELECT and the Core API; reinstall Multi and Ext scenario CAPs and test
  both Multi applets. Continue Security, Crypto, Javacardx, Service, and
  Biometry in README order after addressing cleanup `6581`.

## Arbitrary package AIDs — loader and registry integration — 2026-10-20

## Arbitrary package AIDs — loader and registry integration — 2026-10-20

- Generalized runtime CAP loading beyond JCAlgTest. The loader validates the
  package AID declared in each CAP, rejects duplicate loaded package AIDs,
  allocates an available dynamic package ID (56–63), and stores the CAP in its
  indexed flash slot. The VM's package lookup now searches all dynamic slots.
- CAP slots store up to 40 installed applet-instance records with applet AID,
  instance AID, and class index. Multiple applets can be installed from one
  package and distinct packages can use their own AIDs. SELECT restores the
  matching instance; GET STATUS lists dynamic packages, declared applet
  modules, and installed instances; DELETE restores the requested target and
  removes every recorded applet's persistent fields for package deletion.
- Validation: `./tools/build arm` passed after the package DELETE status fix
  (image end `0x100c56fc`); `./tools/build-pc` passed before that last small
  shared-source guard. Focused GP
  registry (47/47), DELETE (26/26), and install-status (10/10) regressions
  passed. `./tools/test-pc-vpcd-applet` did not complete: the restricted run
  could not create its loopback socket; the host-access retry timed out before
  VPCD connection. No physical flash or API CAP run was performed. The ARM
  final image ends at `0x100c56fc`, below the CAP-slot area beginning at
  `0x10100000`.
- Implementation commit: `e20944e` (`Support arbitrary CAP package AIDs`).
- Package DELETE without the related-object flag returns `6985` when its slot
  has any recorded instances, even if another applet was selected most recently.
- Remaining validation: install two distinct package AIDs through GPPro/API
  CAP flow, SELECT each applet, inspect GET STATUS, then delete one instance
  and package while confirming the other remains selectable. Retry the deploy
  README Core stage on the physical card after confirming its PC/SC reader.

## Multi-package CAP storage checkpoint — 2026-10-20

## Multi-package CAP support — storage layer checkpoint — 2026-10-20

- Began implementing the agreed multi-package plan. `src/cap_store.cpp` now
  provides eight fixed flash slots, each with a 4 KiB metadata sector and
  capacity for a 64 KiB CAP payload. Slot headers carry package AID, package
  ID, code length/CRC, and the current single-instance metadata. The region is
  relative flash `0x100000..0x188000` and is compile-time checked against the
  configured flash size; the ARM image ends at `0x100bb400`.
- Added package-indexed storage calls for lookup, writing, deletion, data and
  package-AID access, and installed-instance metadata. Legacy entry points
  still map to package ID 63 so existing card-manager behavior remains
  buildable during migration.
- Validation: `./tools/build arm` passed; `BUILD:PASS arch=arm` and
  `BUILD:PASS target=choupi`. No RISC-V build or hardware flash was run. The
  repository's current ARM-only scope and hardware build gate were respected.
- This is only the storage foundation: the CAP loader still accepts one
  configured package AID, allocates package ID 20 at its call boundary, and
  persists only one applet instance. Next generalize package-ID allocation
  and loader validation, then integrate package-indexed lookup into the VM.
- Storage step committed as `9c7b9ae`; preserve all unrelated dirty worktree
  changes.

## Deploy-folder Java Card API coverage attempt — blocked before CAP LOAD — 2026-10-20

- Read `AlgTest_JavaCard/deploy/README.md`: it recommends staged API coverage
  starting with Core, uses ordinary API coverage CAPs plus GPPro, and detects
  the supported API version by trying Core CAPs from 3.0.5 down to 2.2.1.
  It warns that full runs create persistent objects and can use about 2,600
  APDUs; later Crypto stage should exclude RSA-2048 generation.
- ARM CHOUPI firmware had already been flashed. Bounded serial monitoring
  passed (`TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, no `FAULT`).
  PC/SC listed the Pico and GPPro `--info` connected to the named physical
  reader over T=1.
- Ran only the README's first Core stage with `--api auto`. All five supplied
  `AlgTestApiCore` CAP versions returned `6D00` at `INSTALL [for load]`; no
  LOAD blocks were sent, so no API methods were exercised. This is the known
  configured-package-AID restriction, not an API-version compatibility
  result. Post-run GPPro `--list` still showed the original JCAlgTest package
  and applet and no API coverage CAPs.
- Current implementation cannot run the deploy README's stages without a
  generalized runtime CAP package/storage path. See `.agent/QKB.md` and
  `TODO.md`. Resume by implementing support for ordinary CAP package AIDs,
  then rerun Core first and continue stage-by-stage only after it loads.
- Evidence: serial `artifacts/logs/api-coverage-arm-monitor-20261020.log`;
  GPPro preflight `artifacts/logs/api-coverage-gppro-info-20261020.log`;
  runner transcript and CSV `artifacts/logs/api-coverage-pico2-core-auto-20261020.log`
  and `artifacts/results/api-coverage-pico2-core-auto-20261020.csv`;
  final registry `artifacts/logs/api-coverage-gppro-registry-after-20261020.log`.

## Current integration run — pass one awaiting power-cycle repeat — 2026-10-20

- Both `./tools/build arm` and `./tools/build riscv` passed. The subsequent
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` flashed the ARM image and
  passed the 18-second serial gate (`TEST:PASS choupi_runtime`,
  `TEST:SUMMARY pass=1 fail=0`, no `FAULT`). The capture also printed one
  `CHOUPI:WATCHDOG_RECOVERY cause=other` diagnostic; runtime checks continued
  passing and the cycle wrapper returned 0.
- After the flash, GPPro listed the ISD and the previously installed applet.
  For a clean run, the applet instance and package were deleted (`9000` each),
  the empty registry was confirmed, and the supplied v1.8.2 JC305 CAP was
  loaded and installed (`9000` on every LOAD block and final INSTALL).
- `tools/test-pico-powerup` passed: package `LOADED`, applet `SELECTABLE`,
  SELECT/GETVERSION/JCSystem `9000`, version `1.8.2_jc305`. The official
  `ALG_SUPPORT_EXTENDED` client completed with status 0. The comparison passed
  all 4,525 rows: 15 `yes`, 4,510 `no`, zero other CHOUPI statuses, and all
  declared support identities matched. CSV SHA-256:
  `8d0c4acd6469d24246e467f1402c5230854f8154e6ea900107817cfa89bbd6a6`.
- Pass one is not counted complete until the board has a true power removal,
  the post-cycle probe passes, and the support sweep is repeated. I requested
  a five-second unplug/reconnect from the user. Do not substitute firmware
  reboot for power removal. The full three-pass sequence remains open.
- Artifacts: build logs `artifacts/logs/build-arm-three-pass-integration-20261020.log`
  and `artifacts/logs/build-riscv-three-pass-integration-20261020.log`; cycle
  `artifacts/logs/cycle-choupi-20261020T140230Z.log`; GPPro logs with prefix
  `artifacts/logs/gppro-three-pass-run1-`; probe
  `artifacts/logs/pico-powerup-persistence-20261020T140519Z.log`; client,
  serial, comparison and CSV under `artifacts/logs/three-pass-run1-*` and
  `artifacts/results/three-pass-run1-20261020/`.

## Latest integration result — clean GlobalPlatform install on Pico — 2026-10-20

- `pcsc_scan -r` listed the physical reader alongside both Virtual PCD readers.
  GPPro 26.06.04 `--list` connected over T=1 and reported ISD
  `A000000151000000 (OP_READY)`.
- A package-level DELETE initially returned `6985` while the applet instance
  remained installed. Deleting the instance AID `4A43416C675465737431` returned
  `9000`; deleting package AID `4A43416C6754657374` then returned `9000`.
  A fresh `--list` confirmed the test package/applet were absent.
- GPPro `--install AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` completed all
  LOAD blocks with `9000` and final INSTALL returned `9000`. CAP SHA-256:
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
- Post-install `tools/test-pico-powerup` confirmed package `LOADED`, applet
  `SELECTABLE`, SELECT `9000`, GETVERSION `1.8.2_jc305`, and JCSystem info
  `9000`. This completes the Linux install/select portion of AC1 and AC2.
  Windows reader/GPPro compatibility remains unverified; the three-run clean
  flash integration sequence is still pending.
- Transcripts: `artifacts/logs/gppro-clean-install-list-before-20261020.log`,
  `artifacts/logs/gppro-clean-install-delete-20261020.log` (first package delete
  attempt, `6985`), `artifacts/logs/gppro-clean-install-delete-applet-20261020.log`,
  `artifacts/logs/gppro-clean-install-delete-package-20261020.log`,
  `artifacts/logs/gppro-clean-install-list-empty-20261020.log`,
  `artifacts/logs/gppro-clean-install-load-install-20261020.log`, and
  `artifacts/logs/pico-powerup-persistence-20261020T135329Z.log`.

## Latest hardware result — AC5 physical power-cycle persistence — 2026-10-20

- The user disconnected and reconnected the Pico after about five seconds.
  Host access was restored through `sudo -n /usr/local/sbin/pico2-pcsc-reader`;
  PC/SC identified the same reader, `Pico 2 CHOUPI CCID [JCAlgTest CCID]
  (D92E743D5F640C0F) 00 00`.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup '<reader>'` passed.
  GPPro confirmed the JCAlgTest package (`LOADED`) and applet (`SELECTABLE`);
  SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem info returned `9000`.
- A fresh official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` run completed with
  status 0 after the power cycle. Comparison passed all 4,525 rows and declared
  support identities: 15 `yes`, 4,510 `no`, zero other CHOUPI statuses, and no
  missing or unexpected declared algorithms. Unsupported algorithms remain
  expected. CSV SHA-256:
  `b2b0d4d279ace0b2f663b4a79779afcce65c5fc9e0030768b635f4aac759ce8b`.
- Evidence: probe `artifacts/logs/pico-powerup-persistence-20261020T132635Z.log`;
  client `artifacts/logs/jcalgtest-alg-support-after-powercycle-20261020.log`;
  serial `artifacts/logs/monitor-alg-support-after-powercycle-20261020.log`;
  comparison `artifacts/logs/jcalgtest-support-compare-after-powercycle-20261020.log`;
  CSV in `artifacts/results/physical-alg-support-after-powercycle-20261020/`.
- AC5's persistence/reproducibility criterion is complete. AC2 is now complete;
  AC1 still needs Windows compatibility evidence, and the plan's three-run
  clean-flash integration sequence remains open; see `TODO.md`.

## Previous checkpoint — refresh AC4 on physical Pico — 2026-10-20

- Rebuilt ARM and RISC-V, then flashed the CHOUPI ARM image with the gated
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi`. Both architecture builds
  passed and the 18-second runtime capture repeatedly reported
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`, with no `FAULT`.
- Physical PC/SC discovery listed `Pico 2 CHOUPI CCID [JCAlgTest CCID]
  (D92E743D5F640C0F) 00 00`. The official JCAlgTest 1.8.2
  `ALG_SUPPORT_EXTENDED` client completed the 4,525-row matrix after the
  OwnerPINX fix. Results: 15 `yes`, 4,510 `no`, zero other CHOUPI statuses.
- The comparator passed all row identities and the 15-entry declared support
  list. jCardSim's 1,454 additional `yes` rows are capability differences;
  expected unsupported algorithms are not failures.
- Evidence: `artifacts/results/physical-alg-support-ownerpinx-20261020/`;
  client log `artifacts/logs/jcalgtest-alg-support-ownerpinx-20261020.log`;
  serial `artifacts/logs/monitor-alg-support-ownerpinx-20261020.log`;
  comparison `artifacts/logs/jcalgtest-support-compare-ownerpinx-20261020.log`.
  CSV SHA-256: `4877cd4ab8205ae41721d5717bf46328a9a910805e0fc5655f60e7af235495ea`.
- At this checkpoint AC5 had only a pre-cycle baseline. It has since been
  completed; see the latest hardware result at the top of this handoff.
  AC1/AC2 still need a current clean GP install transcript and Windows
  compatibility evidence. See current checklist in `TODO.md`.
- An experiment to install a separate AID-scoped JCSystem persistence CAP was
  rejected before LOAD with `INSTALL [for load]` SW `6D00`. `CapLoadSession`
  intentionally accepts only the configured JCAlgTest package AID, so a
  separate package cannot be installed on this firmware. No LOAD block was
  sent and the existing JCAlgTest applet remained selectable. See QKB and
  `artifacts/logs/gppro-install-jcsystem-persistence-probe-20261020.log`.

## Current implementation backlog — 2026-10-20

- The prioritized implementation and integration backlog is consolidated in
  root [`TODO.md`](../TODO.md). The historical checklist below its current
  section predates the latest successful physical static run.
- Highest-priority open items are: repeat the clean GP install flow and
  complete AC1–AC5 three consecutive times; and close the known CAP-slot /
  filesystem atomicity risk. AC4 and AC5 now have current physical evidence.
- The latest physical `ALG_PERFORMANCE_STATIC` run passed at 2,262 transfers.
  Expected unsupported-algorithm results are not implementation bugs.

## Current device insertion — BOOTSEL mass storage detected — 2026-10-20

- The user reports reinserting the Pico in BOOTSEL. Host enumeration confirms
  `2e8a:000f` (`Raspberry Pi RP2350 Boot`); `picotool info -a` reports
  `boot type: bootsel`. The BOOTSEL product ID here is `000f` (the earlier
  `0003` expectation was incorrect for this RP2350).
- The host recognized the same board serial `D92E743D5F640C0F`, created USB
  disk `/dev/sdb` (DISKSEQ 106), and automatically mounted the `RP2350` FAT
  volume at `/run/media/claude/RP2350`. `INFO_UF2.TXT` identifies UF2 Bootloader
  v1.1 / Raspberry Pi RP2350.
- This verifies host recognition after the reported reinsertion. No additional
  unplug/replug cycle was performed or timed in this check. BOOTSEL presents a
  mass-storage device, so the PC/SC reader is expected only after firmware
  boots.
- Next: if another physical cycle is requested, observe udev remove/add events
  and confirm the serial identity and BOOTSEL disk reappear; do not infer
  reinsertion latency from this snapshot.

## Current hardware state — APDU and GC pass on Pico — 2026-10-20

- Although the operator reported BOOTSEL insertion, current USB enumeration is
  `2e8a:0009` (CHOUPI CCID), not the RP2350 BOOTSEL ID. The authorized ACL
  helper and `pcsc_scan -r` currently list `Pico 2 CHOUPI CCID`.
- Root cause was mark-vector size becoming stale during GC: persistent
  reference reads can lazily restore objects and extend the stable heap tables.
  The collector now grows the relevant bitmap before marking those handles and
  snapshots each pass's slot count so newly restored slots are visited on the
  next pass. It also handles lazy restores reusing initially empty slots.
- Both builds pass. On hardware, SELECT, GETVERSION (`1.8.2_jc305`), and the
  JCSystem info APDU returned `9000`; serial showed heap growth from 7 arrays / 6
  instances to 27 / 17, completed GC, and returned the responses. The bounded
  power-up persistence probe passed package/applet registry and the same APDU
  checks (`TEST:PASS jc_algtest_persistence_probe`). Log:
  `artifacts/logs/pico-powerup-persistence-20261020T063339Z.log`.
- Current USB ID is `2e8a:0009` and PC/SC lists `Pico 2 CHOUPI CCID`; this is
  firmware mode, not BOOTSEL. Cycle log:
  `artifacts/logs/cycle-choupi-20261020T063106Z.log`.
- The first performance-only attempt and stalled captures are committed as
  `71c164b`; the dynamic-bitmap correction and physical confirmation are
  pending the next commit.
- Next: run the requested official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC`
  against this exact reader with concurrent serial capture, then review the
  output and collector runtime.

## Latest hardware check — CHOUPI runtime passes, physical APDU blocked — 2026-10-20

- Reflashed from BOOTSEL with `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi`.
  Both ARM and RISC-V builds passed; the ARM image flashed and the 18-second
  capture repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`.
- The bounded power-up test confirmed the JCAlgTest package/applet in GPPro but
  failed SELECT with `SCARD_E_NOT_TRANSACTED`. Diagnostic firmware then showed
  repeated `GetSlotStatus` (`0x65`) polling, followed by `IccPowerOn` (`0x62`)
  and the SELECT XfrBlock. The applet lookup/restore/selection path completed
  with internal status `9000`; however, the bounded Java probe timed out before
  printing an APDU response or GETVERSION. PC/SC discovery later listed only
  virtual readers, and the latest scan timed out without output, while `lsusb`
  still sees `2e8a:0009`. Thus physical SELECT reaches firmware, but response
  delivery to SmartcardIO and persistent applet behavior are unverified.
- The temporary CCID diagnostic reports unknown-command count and a 12-command
  rolling history. No unknown commands or endpoint errors were seen. The
  PC/SC daemon journal is not readable by this user.
- Logs: `artifacts/logs/cycle-choupi-20261020T055535Z.log` for the diagnostic
  firmware cycle; the select/version probe and concurrent 20-second serial
  trace were inspected live but not archived. Earlier power-up attempts remain
  in logs ending `041200Z`, `041334Z`, `041650Z`, and `041802Z`.
- Next: restore PC/SC service discovery, then run a longer bounded probe with
  concurrent serial capture to verify the CCID IN response and GETVERSION.

## Test limitation — runtime exception root injection needs a full heap target — 2026-10-20

- A direct `Heap::addRuntimeException()` allocator-injection case was attempted
  in the small array-allocation executable, but it pulled `Heap::addInstance()`
  and unresolved `JC_Instance`, CAP, and filesystem symbols into that target.
  The attempted case was removed; the executable again passes all nine current
  checks. The static-root reserve failure remains directly covered.
- The runtime exception-root ordering fix still passes ARM/RISC-V builds and
  the host VM build. Direct low-memory failure injection needs a dedicated
  dependency-complete heap integration target; avoid adding production test
  hooks just for the lightweight regression.
- Next: continue auditing heap root and ownership mutations for failure after
  persistent or VM state changes.

## Latest regression — inject static-root reservation allocation failure — 2026-10-20

- The focused allocation test now overrides scalar `operator new` to force
  `Heap::reserveStaticReferenceRoot()` to fail. It verifies the failed reserve
  leaves no root entry, a later reserve and registration succeed, and reserving
  an existing key does not allocate.
- Validation: `./tools/test-jcvmarray-allocation` passes all nine checks with a
  warning-free build. No firmware rebuild or hardware operation was needed;
  only the host regression changed.
- Test commit: `45581f9`.
- Next: continue auditing heap root-vector mutations for operations that can
  fail after persistent or heap state changes.

## Latest static-root hardening — reserve before persistent reference write — 2026-10-20

- `Heap::reserveStaticReferenceRoot(package, field)` now reserves an additional
  GC-root slot when the key is new. `Static_Handler::setPersistentRef()` calls
  it before writing the serialized reference, so allocation failure cannot
  leave flash naming a transient handle that was never registered as a root.
- Validation: ARM and RISC-V builds passed. `./tools/test-persistent-transaction`
  passed with `TEST:SUMMARY fail=0`. The two-process VPCD persistence workflow
  passed install/commit 3/3 and fresh restore 7/7, including static reference
  identity, persistent array aliases, and committed instance data. The host
  rebuild emitted existing converter/Maven and packed-CAP warnings. No board
  flash or physical APDU was run. Direct allocation injection at the root
  reservation boundary is directly covered by the focused host allocation
  regression.
- Source commit: `9154aa8`.
- Evidence: `artifacts/logs/vpcd-persistence-commit-20261020T035837Z.log` and
  `artifacts/logs/vpcd-persistence-restore-20261020T035837Z.log`.
- Next: continue auditing heap root-vector mutations and add direct low-memory
  coverage where it can be done without disrupting the shared worktree.

## Latest heap hardening — reserve runtime exception roots before allocation — 2026-10-20

- `Heap::addRuntimeException()` now reserves space in its root vector before
  adding the Java exception object. A root-table allocation failure therefore
  cannot occur after the heap has been mutated.
- Validation: ARM and RISC-V wrapper builds passed. `./tools/build-pc` passed;
  its wrapper regenerated APIs/ROM mask and emitted existing converter,
  Maven/network, and packed-CAP warnings. The bounded VPCD applet run connected
  but stopped during persisted-app restoration with no APDU result, so runtime
  exception behavior remains unverified. No hardware operation was run.
- Source commit: `f311958`.
- Next: add direct allocation-failure coverage around runtime exception-root
  insertion if a focused heap test seam can be added without disturbing the
  shared worktree; otherwise continue auditing heap mutation boundaries.

## Latest regression — clean up JCVMArray storage when element copies throw — 2026-10-19

- The allocation regression now uses a throwing element assignment to exercise
  failures after backing allocation. Copy construction releases the partial
  array; copy assignment releases its staged replacement and preserves the
  destination's old contents. The test counts exact array deallocations.
- Validation: `./tools/test-jcvmarray-allocation` passed all six cases with a
  warning-free rebuild. No firmware build or hardware operation was needed;
  this changes only the host regression.
- Test commit: `6285edf`.
- Next: continue the bounded runtime memory/API audit, preserving the extensive
  unrelated worktree changes.

## Latest regression — inject JCVMArray copy-constructor allocation failure — 2026-10-19

- `tools/test-jcvmarray-allocation` now forces allocation failure in mutable
  `JCVMArray` copy construction and verifies `std::bad_alloc` propagates. The
  successful copy path observes its size and first element; without that use,
  the compiler optimized the copy and its allocation away, so the injection
  never triggered. The QKB records this test-design caveat.
- Validation: the focused test passes all four cases: constructor failure,
  copy-constructor failure, copy-assignment preservation, and persistent-array
  constructor failure. ARM build was intentionally skipped because only the
  host test changed. No hardware operation was needed.
- Test commit: `b8545a9`.
- Next: continue the bounded runtime memory/API audit. Preserve unrelated dirty
  files and do not run USB/PCSC unless the required elevated host-access path is
  available from the first attempt.

## Latest allocator fix — preserve JCVMArray on failed copy — 2026-10-19

- Mutable JCVMArray copy construction and assignment now stage backing storage
  in a local unique_ptr. Assignment only replaces the original after allocation
  and copying succeed; a failed assignment preserves the destination's original
  length and elements.
- ./tools/test-jcvmarray-allocation passed all three injected allocation checks:
  constructor failure propagation, copy-assignment preservation, and persistent
  JC_Array constructor failure propagation. ./tools/build arm passed. No
  hardware flash was run.
- Commit: `79c2a79c0fd42e21f154bf5e973bca9e7f497d83`.
- Next: continue the runtime memory/API audit; physical PC/SC APDU delivery
  remains blocked by the known host transmit stall.


## Latest exception-safety fix — persistent JC_Instance construction — 2026-10-19

- The persistent JC_Instance constructor now holds its overflow tag and field
  array in local unique_ptrs until all throwing setup completes. A failure
  resolving field metadata or allocating fields can no longer leak the tag
  buffer from a partially constructed object.
- Validation: ./tools/build arm passed. PICO2_HOST_USB_ACCESS=1
  ./tools/test-vpcd-persistence passed install/commit 3/3 and fresh restore 7/7.
  The constructor's own allocation failure was not directly injected. The host
  rebuild emitted existing Rust/C++ and CAP-header warnings. No hardware flash
  or physical APDU ran.
- Commits: source `1ac2955fbb901c13158f94481d0bfd1497c9ad92`; validation/docs
  `b2947afa6cdb494dcdc17b758df567ae9d0d5f00`.
- Next: continue the runtime memory/API audit; physical APDU delivery remains
  blocked by the known PC/SC transmit issue.


## Latest memory fix — release persistent field read buffers — 2026-10-19

- getDataFromTag() returned an owning raw pointer. Persistent byte/short/int
  getters did not free it, including on read or validation errors. It now returns
  std::unique_ptr<uint8_t[]>; the shared getter path releases the buffer on all
  exits.
- ./tools/test-persistent-transaction passes. Its allocation tracker verifies
  exactly one new[] and delete[] for a valid byte read and for injected
  fs_read() failure (TEST:SUMMARY fail=0). ./tools/build arm passed. The
  wrapper now suppresses existing qualifier/packed-member header warnings.
  Follow-up VPCD persistence passed install/commit 3/3 and fresh-process restore
  7/7, including persistent array aliases and the committed applet field. No
  physical flash or APDU test was run.
- Commit: `01f7eba75fc1afc53dbabcecb68cf4cdf5e307ed`.
- Validation record commit: `71c6811c33e15f3672d31b2fb37c2efcea123785`.
- Next: continue runtime memory/API audit; physical APDU delivery remains blocked
  by the known PC/SC transmit stall.


## Latest hardening — allow persistent array restore allocation to fail cleanly — 2026-10-19

- Removed `noexcept` from the persistent `JC_Array` constructor, which allocates
  a tagged backing buffer with `new[]`. Restore and APDU caller boundaries catch
  failures; previously this constructor could invoke `std::terminate` first.
- Extended `tools/test-jcvmarray-allocation` to force `new[]` failure in both
  `JCVMArray` and the persistent `JC_Array` constructor. Both propagation checks
  pass. ARM build passed. The test wrapper uses `-Wall -Wextra` and suppresses two
  pre-existing warnings in included CAP/API headers; no hardware test was run.
- The test initially failed to compile/link until the OS FFI include path and
  `jref_t.cpp` were added.
- Commit: `a64a9d72b7387c4a15797930dd638b418762daab`.
- Regression commit: `292802da16f02c12915b87381eb0a18d52bb8301`.
- Next: continue the runtime/API audit. Full firmware restore/APDU behavior under
  forced exhaustion and transient-factory exception mapping through the VM remain
  open.


## Latest implementation — map transient allocation failure — 2026-10-19

- `JCVMArray` backing allocation could throw inside a `noexcept` constructor,
  terminating the VM. The constructor now propagates allocation failure, and
  the primitive/reference transient array factories translate `std::bad_alloc`
  to `SystemException.NO_TRANSIENT_SPACE` (reason 2). Invalid clear events now
  set `SystemException.ILLEGAL_VALUE` (reason 1).
- Validation: `./tools/build arm` passed. The restricted VPCD attempt returned
  `EPERM`; the required host-access retry passed probe cases 28 and 29,
  `TEST:SUMMARY pass=4 fail=0`. No Pico flash was run; the build gate for flashing
  requires both architectures, while current work scope is ARM-only. The new
  `./tools/test-jcvmarray-allocation` regression deterministically forces a
  backing `new[]` failure and passes `allocation_failure_propagates`; full-VM
  exception mapping under forced exhaustion remains untested. Regression commit:
  `61cdcdedf3068dd8126b8b2da54c2b958ce8121a`.
- Commit: `da50522af16913d8b7dc1351c805b7ed15b1d66e`.
- Progress finalization commit: `e260313a2e4dc48c618300961cfc50634beb8318`.
- Next: continue the selected Java Card runtime/API audit. Physical PC/SC applet
  APDUs remain blocked by the documented host transmit stall.


## Latest correction — JCSystem.getAID before registration — 2026-10-19

- `JCSystem.getAID()` previously created an AID object from the INSTALL
  instance AID before the applet had called `Applet.register()`. The native
  bridge now returns null until registration succeeds, and continues returning
  the AID for a restored installed instance.
- The focused VPCD install check passed (`TEST:SUMMARY pass=3 fail=0`), confirming
  null before registration and the registered AID during install. A fresh
  emulator process restoring the saved applet passed the same check
  (`pass=3 fail=0`). The CHOUPI cycle passed ARM/RISC-V build gates, flashed the
  ARM image, and passed its 18-second runtime capture.
- Logs: `artifacts/logs/jcsystem-get-aid-registration-vpcd-20261019.log`,
  `artifacts/logs/jcsystem-get-aid-registration-restore-vpcd-20261019.log`, and
  `artifacts/logs/cycle-choupi-20261019T193502Z.log`.
- Next: continue JCSystem/API contract audit; hardware APDU testing still needs
  the PC/SC transmit path to deliver an XfrBlock.

## Latest correction — JCSystem.isAppletActive callback contract — 2026-10-19

- The previous lifecycle fix incorrectly made `JCSystem.isAppletActive()`
  false during `install()` and `select()`. JC 3.0.5 specifies true during both
  callbacks after successful registration. The native query now allows the
  install window after registration, and the selected context is marked active
  before invoking `select()`; failed selection clears it.
- Focused VPCD cases 76, 77, and rejected-selection case 56 passed (`3/3` each).
  ARM and RISC-V builds passed. `./tools/cycle choupi` passed both build gates,
  flashed the ARM image, and captured repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0` output.
- Physical package listing succeeded, but the Java APDU probe timed out before
  SELECT reached firmware (`xfr=0`); see the Pico transport QKB entry. Physical
  applet behavior remains unverified.
- Next: continue API/runtime audit offline; do not repeat physical probes until
  the host PC/SC/CCID transmit stall is diagnosed.

## Latest continuation — flash and verify the reinserted Pico — 2026-10-19

- The reinserted BOOTSEL board accepted `build/arm/pico2_choupi.uf2`;
  `FLASH:PASS` was recorded. ARM/RISC-V gates completed because the cycle
  reached its flash stage. A separate 18-second monitor capture then recorded
  repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`.
- The authorized PC/SC helper and bounded reader scan listed the Pico. GPPro
  confirmed JCAlgTest package `4A43416C6754657374` and selectable applet
  `4A43416C675465737431`. The one-attempt power-up probe connected and read ATR
  `3B800181`, but SELECT timed out after 15 seconds. Serial counters remained
  `rx_events=0`, `tasks=0`, `xfr=0`; no applet APDU reached firmware.
- Evidence: `artifacts/logs/pico-powerup-persistence-20261019T191503Z.log`
  and `artifacts/logs/monitor-apdu-probe-after-timeout-20261019.log`.
  The first `cycle choupi` log ends after `FLASH:PASS` because its serial stage
  was interrupted; use the separate bounded monitor as runtime evidence.
- Next: stop repeating physical probes until an XfrBlock reaches firmware.
  Continue the Java Card API/runtime audit and VPCD checks offline; physical
  applet execution and true power-cycle persistence are still open.

## Latest implementation — track whether the applet is active — 2026-10-19

- `JCSystem.isAppletActive(AID)` previously returned true for the registered AID
  during install because it checked identity/registration but not selection
  lifecycle. `Context` now records active state; the VM sets it after successful
  `select()` and clears it after `deselect()` or reset. Case 76 verifies false
  during install and true in `process()`; case 77 verifies false in `select()`,
  true in `deselect()`, and true in `process()` (`TEST:SUMMARY pass=3 fail=0`).
  Implementation commit: `da6a6e6`; callback regression commit: `e1ebcfe`.
- `./tools/build-pc` and ARM/RISC-V build gates passed. `cycle arm` passed the
  smoke test; `cycle choupi` flashed the changed ARM VM and passed its 18-second
  runtime capture with repeated `TEST:PASS` and no `FAULT`.
- After the flash, PC/SC listed the Pico and the probe read ATR `3B800181`, but
  timed out before SELECT response. The serial CCID state stayed at `xfr=0`, so
  that attempt did not deliver an XfrBlock to firmware. See
  `artifacts/logs/physical-jcsystem-active-retry-20261019.log` and QKB.
- Next: continue the JCSystem/API audit on VPCD; physical consecutive APDUs remain
  blocked by intermittent PC/SC/USB transport loss. No RISC-V flash was run.

## Latest continuation — validate applet restore metadata and Pico reader — 2026-10-19

- `CapLoadSession::restoreInstalled()` now requires a valid persisted class
  index before setting `installed_`; missing or invalid metadata can no longer
  produce a falsely installed applet. Commit `169c131`.
- `./tools/build arm` passed (`pico2_choupi` image end `0x100bb520`).
  `./tools/test-vpcd-persistence` passed install/commit 3/3 and fresh-process
  restore 7/7. Logs use timestamp `20261019T182907Z`.
- The physical Pico reader appeared after the authorized ACL helper; GPPro
  listed the JCAlgTest package and selectable applet. ATR and SELECT succeeded,
  but GETVERSION failed with `SCARD_E_NOT_TRANSACTED`; retries then lost the
  reader from PC/SC while `lsusb` still saw `2e8a:0009` and the finite serial
  monitor showed repeated `TEST:PASS choupi_runtime`. This is not a successful
  applet APDU or a true power-cycle persistence check. See the new `.agent/QKB.md`
  entry and `pico-powerup-persistence-20261019T183156Z.log`.
- No flash or RISC-V build was attempted under the ARM-only scope. Next:
  diagnose post-SELECT PC/SC loss when reader access is available; continue
  applet/JCSystem runtime work offline meanwhile.

## Latest persistence review — CAP and filesystem commit boundary — 2026-10-19

- Source inspection found that INSTALL writes the separate CAP slot's
  installed AID/class metadata before the persistent filesystem transaction's
  final `fs_sync()`. If sync fails, filesystem snapshots roll back but the CAP
  header does not. The physical failure has not been reproduced.
- Recorded in `.agent/QKB.md` and `TODO.md`. Next: add fault injection for the
  final sync and metadata write, then design startup reconciliation across the
  two flash stores.
- A bounded `timeout 8s pcsc_scan -r` listed only Virtual PCD 00 00/00 01; no
  physical APDU ran. Existing host tests and ARM build for SELECT `61xx` passed.
- Commit: `cc292ef`.

## Latest implementation — SELECT 61xx continuation — 2026-10-19

- CCID and VPCD now keep an applet selected when its SELECT APDU returns
  `61xx`. The next GET RESPONSE APDU can reach the applet. A shared status
  helper treats `9000` and `61xx` as successful selection outcomes.
- `./tools/test-vpcd-session` passed 18 checks, including `6103` followed by
  `00 C0 00 00 03`; `./tools/build arm` passed. No physical flash or APDU was
  attempted.
- The earlier distinct instance-AID restore change is committed as `b3da80e`;
  its finalized progress/handoff metadata is `e0602ff`.
- Commit: `86ffde9`.
- Next: continue the API/runtime audit and retry physical power-up when the
  Pico is visible in PC/SC.

## Latest implementation — distinct instance AID restore — 2026-10-19

- Removed the incorrect requirement that a persisted instance AID equal the
  CAP Applet component AID. The GlobalPlatform INSTALL command records these
  AIDs separately and permits distinct values.
- A temporary PC VPCD CLI experiment used instance AID
  `A00000015150455253495354` while the probe CAP registers
  `4A43416C675465737431`. Install/commit passed 3/3; a fresh emulator process
  selected the instance and restored persisted references and field `0x33`,
  passing 7/7. The test CLI edits were removed because `main_pc.cpp` has
  extensive unrelated pre-existing changes and could not be staged separately.
- The retained same-AID regression `./tools/test-vpcd-persistence` also passed
  install/commit 3/3 and process-restart restore 7/7.
- `./tools/build arm` passed. No flash was attempted. Note: the PC emulator
  restores VM state from its configured AID but does not invoke the firmware
  `CapLoadSession::restoreInstalled()` metadata reader; that exact reader is
  compile-checked and awaits physical power-cycle verification.
- Commit: `b3da80e`.
- Next: continue the runtime/card-manager review, starting with SELECT
  response handling for `61xx`.

## Latest implementation — null-safe Java Card AID comparisons — 2026-10-19

- Java Card `AID.equals(byte[],short,byte)` and
  `AID.partialEquals(byte[],short,byte)` now return false for null arrays, as
  required by their JC 3.0.5 contract. Added VPCD applet case 75 to verify null
  handling plus valid full-AID and prefix matches.
- Validation: `./tools/build-pc` passed API/ROM-mask generation and PC emulator
  build; `PICO2_HOST_USB_ACCESS=1 ./tools/test-jcsystem-transaction-applet
  --case 75 --log artifacts/logs/aid-null-comparison-20261019.log` passed 3/3;
  `./tools/build arm` passed. The ROM-mask step printed a hostname socket
  permission warning and existing Maven/converter warnings but completed.
- Commits: nested API `f3e41e0`; root test/TODO `c9fc0eb`.
- Continue the API/runtime audit. The focused VPCD test covers the behavior;
  physical APDU validation remains unavailable while PC/SC omits the Pico.

## Latest review handoff — functionality implementation guide — 2026-10-19

- Added `review_20261019.md` with concrete code paths, implementation steps,
  regression cases, and completion criteria for distinct instance-AID restore,
  `61xx` SELECT response handling, and CAP/filesystem power-loss recovery.
  The embedded SCP03 test key is recorded as deferred hardening.
- This was a documentation-only step. No source code, firmware build, flash, or
  hardware test was performed. The extensive pre-existing worktree changes
  remain untouched.
- Next: implement the distinct instance-AID restore fix as the smallest
  independent step, then the SELECT `61xx` fix. Treat durable CAP/filesystem
  recovery as a separate storage-design change with power-cut tests.

## Latest continuation — AC4 support comparison and Pico PC/SC diagnosis — 2026-10-19

- Added `tools/compare-jcalgtest-support.py` and a reviewed 15-entry support
  declaration. The comparator validates matching applet versions, all result
  row identities/statuses, and agreement between CHOUPI `yes` rows and the
  declared API support set. Archived comparison passed: 4,525 matched rows,
  15 expected/observed support rows, 4,510 `no`, and zero CHOUPI error rows.
  This CSV predates the OwnerPINX implementation, so a fresh physical support
  sweep remains required to close AC4. Comparator commit: `4863b56`.
- Validation against the current worktree also passed `./tools/build arm` and
  the host two-process persistence regression (install 3/3, restore 7/7). The
  first persistence invocation in the restricted shell returned `EPERM`; the
  host-access retry passed. Current source changes are extensive and
  pre-existing; preserve them and stage only files belonging to a focused step.
- Reinserted Pico power-up check: USB access was granted for
  `/dev/bus/usb/001/004`, but GPPro listed only Virtual PCD readers. USB
  descriptors advertise the CCID interface and libccid has the VID/PID match.
  `pcscd.socket` is active with normal idle auto-exit service state; its
  foreground daemon cannot bind the system-owned socket as the current user.
  No applet APDU or physical persistence check ran. See the QKB entry and
  `artifacts/logs/pico-powerup-persistence-20261019T121536Z.log`.
- The physical PC/SC issue has not yielded a new recovery. Avoid unbounded
  `pcsc_scan` and do not launch another pcscd instance. Continue implementation
  and VPCD verification; rerun physical checks after the named Pico reader
  returns.

## Latest continuation — verify persistent graph after VPCD restart — 2026-10-19

- Expanded `tools/test-vpcd-persistence` to check cases 14–17 and 58 after a
  fresh emulator process restores the saved flash image. The assertions cover
  static reference identity, persistent array child identity, object-field
  aliasing and mutation, array type restoration, and committed instance field
  `0x33`.
- Validation: install/commit process passed 3/3 checks; restore process passed
  7/7, with all five persistence APDUs returning expected data and `9000`.
  This remains host process-restart evidence, not physical power-cycle proof.
- Commit: `636fbfc`.
- Logs: `artifacts/logs/vpcd-persistence-restore-20261019T121017Z.log` and
  `artifacts/logs/vpcd-persistence-restore-emulator-20261019T121017Z.log`;
  full build and other phase logs share timestamp `20261019T121017Z`.
- Next: continue AC4 support-result verification and outstanding runtime work;
  physical AC5 still depends on the Pico returning to PC/SC.

## Latest continuation — automate VPCD applet persistence restart — 2026-10-19

- Added `--memory` to the JCSystem applet VPCD runner and a
  `tools/test-vpcd-persistence` wrapper. It builds the host emulator and probe,
  commits the persistent instance field (`0x33`) in a first emulator process,
  then restores that flash image in a fresh process and verifies the field.
- Validation passed: first process committed `0x33` (`339000`); the fresh
  process restored the registered applet and returned `339000` for case 58.
  This is a host process-restart check, not a physical Pico power cycle.
- Logs: `artifacts/logs/build-pc-vpcd-persistence-20261019T120347Z.log`,
  `artifacts/logs/vpcd-persistence-commit-20261019T120347Z.log`,
  `artifacts/logs/vpcd-persistence-restore-20261019T120347Z.log`, and the two
  emulator logs with `install-emulator` / `restore-emulator` in their names.
- Next: continue remaining runtime/card-manager work. Retry physical AC5 only
  after PC/SC exposes the Pico reader; current power-up check sees Virtual PCD
  readers only.

## Latest continuation — reject unknown AID SELECT — 2026-10-19

- CCID and VPCD now identify well-formed short SELECT-by-AID APDUs before
  applet dispatch. Unknown AIDs deselect the active applet and return `6A82`;
  they no longer reach `Applet.process()`.
- Validation: VPCD session unit test passed 16 checks; focused applet VPCD
  case 74 passed (unknown AID returned `6A82`, next applet APDU returned
  `6999`). PC emulator and RP2350 ARM builds passed. No flash or physical APDU
  was attempted.
- Artifacts: `artifacts/logs/test-vpcd-session-unknown-aid-select-20261019.log`,
  `artifacts/logs/unknown-aid-select-vpcd-20261019.log`,
  `artifacts/logs/build-pc-unknown-aid-select-20261019.log`, and
  `artifacts/logs/build-arm-unknown-aid-select-20261019.log`.
- Physical power-up remains unverified: the latest bounded test saw USB but
  PC/SC exposed only Virtual PCD readers. See
  `artifacts/logs/pico-powerup-persistence-20261019T111724Z.log`.
- Next: continue the runtime/API audit in TODO.md; retry physical persistence
  after the Pico reader appears in PC/SC.

## Latest implementation — Applet.selectingApplet and CAP Method bounds — 2026-10-19

- Implemented `Applet.selectingApplet()` across Java API, VM, and native bridge.
  On a successful applet SELECT, the VM now invokes `Applet.process()` with the
  original SELECT APDU while the selecting flag is true. It is true during
  `select()`, false during `deselect()`, and tested across reset, rejected
  selection, and re-selection.
- Fixed `jc_cap_method_component::methods()` to decode the component size in
  network byte order and exclude the handler-count byte and exception table.
  This resolved a reproducible install failure for the probe CAP's 4,102-byte
  Method component (previously exposed as 1,552 bytes).
- Commits: API `bf323aa`; root implementation/tests `694b408`.
- Validation: PC release build, probe CAP build, ARM M33 build, and VPCD
  session test passed. Full applet regression passed 75 checks in 276.309 s.
  No RISC-V build, physical flash, or physical APDU was attempted.
- Physical power-up retry `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup`
  acquired USB access for `/dev/bus/usb/001/004`, but GPPro again saw only
  `Virtual PCD 00 00` and `Virtual PCD 00 01` through all three retries. See
  `artifacts/logs/pico-powerup-persistence-20261019T111724Z.log`; persistence
  remains unverified.
- Next: continue the Java Card API/runtime audit from TODO.md. Retry physical
  persistence after `pcscd.service` exposes the named Pico reader.

## Latest implementation — Applet.register installation semantics — 2026-10-19

- Implemented native `Applet.register()` overloads. They are legal only during
  the install entry point, reject duplicate successful registration, validate
  explicit AID length and array bounds, and require the explicit AID to match
  the instance AID from INSTALL. Registration now comes from the API native
  bridge instead of matching hard-coded bytecode import tokens.
- API commit: `5cd9aec`; root implementation/tests: `3b9e6de`.
- Validation: `./tools/build-pc`, `./tools/build-jcsystem-transaction-probe`,
  and `./tools/build arm` passed. VPCD cases 67–70 passed for matching AID
  (`6/6`, 16.536 s); a mismatched explicit AID produced reason 4 and fallback
  install passed (`3/3`, 11.097 s). No RISC-V build or physical flash was run.
- Physical Pico power-up validation remains blocked because USB enumeration
  succeeds but PC/SC exposes only Virtual PCD readers. Its latest evidence is
  in `artifacts/logs/pico-powerup-persistence-20261019T105115Z.log`.
- Next: continue the applet lifecycle and Java Card API audit from TODO.md.
  Inspect `Applet.selectingApplet()` and card-manager SELECT/PROCESS ordering;
  maintain the ARM-only target and preserve all other dirty worktree changes.

## Latest continuation — retry Pico power-up persistence — 2026-10-19

- Re-ran `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` after reinsertion.
  The access helper granted `/dev/bus/usb/001/004`; `lsusb` confirmed Pico
  `2e8a:0009`, but GPPro and a bounded `pcsc_scan -r` listed only the two
  Virtual PCD readers. The check exited after three bounded retries before
  package or applet APDUs, so physical persistence remains unverified.
- To resume, restart `pcscd.service` interactively, run the authorized
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper, and confirm the Pico
  reader appears before rerunning the power-up test.
- Evidence: `artifacts/logs/pico-powerup-persistence-20261019T105115Z.log`.
- Next implementation task remains the in-progress `Applet.register()`
  registration semantics work; inspect the worktree before staging or editing
  because many unrelated changes from prior sessions are present.

## Latest continuation — implement JCSystem.lookupAID — 2026-10-19

- `JCSystem.lookupAID()` now validates null and byte-array ranges, returns the
  active applet AID on an exact match, and returns null for a miss. This is
  limited to the active applet because the runtime still models one applet
  context. VPCD cases 65–66 pass (`TEST:SUMMARY pass=4 fail=0 elapsed_s=24.373`).
- The first VPCD run failed with `StackUnderflowException` at `invokevirtual`.
  Root cause: JC305 AID method tokens differ from the embedded API. Added
  mapping `{0, 4, 2, 1, 3, 5}` so JC305 `getBytes()` token 3 resolves to embedded
  token 1 instead of four-argument `partialEquals()`. The nested API commit is
  `aa4c7e0`; the runtime and VPCD test commit is `5f995a0`.
- `./tools/build arm`, `./tools/build-jcsystem-transaction-probe`, and
  `./tools/build-pc` passed. A default restricted VPCD run returned
  `Operation not permitted`; the host-context retry passed. The first retry
  after adding the mapping used a stale PC emulator and still failed; rebuild
  the host VM before VPCD verification.
- The explicit `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` retry
  granted USB ACL access but GPPro listed only the two Virtual PCD readers in
  all three attempts. No applet APDU was sent, so power-up persistence remains
  inconclusive. Log: `artifacts/logs/pico-powerup-persistence-20261019T095458Z.log`.
- No RISC-V build or physical flash was attempted.
- Next: continue the JCSystem/context API audit; prioritize behavior that can
  be implemented correctly within the single-context runtime, and keep
  multi-applet lookup/firewall work open rather than claiming it verified.

## Latest continuation — implement APDU.waitExtension state checks — 2026-10-19

- Removed the remaining `APDU.waitExtension()` stub. It now returns for a
  normal outgoing transfer, throws `APDUException.ILLEGAL_USE` after
  `setOutgoingNoChaining()`, and maps a negative APDU error state to
  `APDUException.IO_ERROR`. The internal native bridge reads the per-command
  no-chaining flag from `Context`.
- The API does not perform T=1 WTX: the current CCID transport dispatches a
  command synchronously and has no asynchronous time-extension message path.
  This remains a transport limitation to resolve if long-running applets need
  reader timeout extensions.
- `./tools/build arm` and `./tools/build-jcsystem-transaction-probe` passed.
  VPCD cases 63–64 passed (`TEST:SUMMARY pass=4 fail=0 elapsed_s=14.110`). No
  RISC-V build or flash was attempted.
- Java API change committed in the nested API repository as `edfa4cf`.
- Continue the APDU/profile audit; next inspect callback lifecycle, applet
  firewall behavior, and remaining JCSystem native contracts.

## Latest continuation — gate APDU current buffer access — 2026-10-19

- `APDU.getCurrentAPDUBuffer()` now delegates through the active APDU object,
  inheriting the `Applet.process()` scope check from `getCurrentAPDU()`. The
  probe verifies buffer identity during `process()` and `SecurityException`
  during `select()`.
- `./tools/build arm` and `./tools/build-jcsystem-transaction-probe` passed.
  VPCD cases 59–62 passed: `TEST:SUMMARY pass=6 fail=0 elapsed_s=33.705`.
- Java API change committed in the nested API repository as `2ec9efa`.
- The requested physical `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup`
  run acquired USB ACL access to `/dev/bus/usb/001/004`, but GPPro timed out
  discovering the package/applet registry in all three attempts. See
  `artifacts/logs/pico-powerup-persistence-20261019T092025Z.log`; this does not
  establish whether the persistent applet survived power-up.
- No RISC-V build or flash was attempted. Continue the APDU API audit and retry
  physical persistence once the Pico reader is visible in PC/SC.

## Latest continuation — implement APDU.getCurrentAPDU — 2026-10-19

- Implemented `APDU.getCurrentAPDU()` through a native bridge to the current
  context's APDU object. A scoped process flag makes the reference available
  only while `Applet.process()` executes; calls during `select()` raise
  `SecurityException`. The nested Java Card API change is committed as
  `43f9877` in `choupi/javacard-sdk/javacard-api`.
- ARM build passed with `./tools/build arm`. API CAP/ROM-mask generation
  completed without converter errors; existing export/int-support and
  packed-member warnings remain. `./tools/build-jcsystem-transaction-probe`
  passed, and VPCD cases 59 and 60 passed (`TEST:SUMMARY pass=4 fail=0
  elapsed_s=11.280`) for exact object identity during `process()` and security
  rejection during `select()`.
- Physical PC/SC validation remains unavailable: USB `2e8a:0009` was present,
  but pcscd exposed only Virtual PCD readers. No flash was attempted.
- Next: continue the APDU Java Card profile audit and broaden focused golden
  coverage; keep the physical test open until the Pico reader enumerates.

## Latest continuation — make the Pico power-up check tolerate reader re-enumeration — 2026-10-19

- Updated `tools/test-pico-powerup` with independently bounded retries for
  GPPro registry discovery and the SELECT/GETVERSION/JCSystem probe. Each
  retry refreshes the authorized USB ACL; attempts and command output are
  retained in the timestamped transcript. Attempt count and timeouts are
  validated and bounded.
- `bash -n tools/test-pico-powerup` and `git diff --check --
  tools/test-pico-powerup` passed. Running the updated test on the reinserted
  Pico granted ACL access to `/dev/bus/usb/001/004`, but all three GPPro
  attempts saw only `Virtual PCD 00 00` and `Virtual PCD 00 01`. A bounded
  `pcsc_scan -r` also listed only the virtual readers. The test stopped before
  an applet APDU.
- Log: `artifacts/logs/pico-powerup-persistence-20261019T085227Z.log`.
  This does not prove a power-cycle persistence failure; the PC/SC reader was
  unavailable. Do not use RISC-V. Continue offline implementation while the
  physical PC/SC path is unavailable; retry the power-up check after reader
  enumeration recovers.

## Latest continuation — verify DELETE and persistence on the Pico — 2026-10-19

- Rebuilt ARM/RISC-V, flashed the CHOUPI ARM image, and captured repeated
  `TEST:PASS choupi_runtime` with no `FAULT` in
  `artifacts/logs/cycle-choupi-20261019T064423Z.log`.
- GPPro initially listed JCAlgTest 1.8.2. Authenticated instance DELETE
  removed the selectable applet while leaving the package loaded. Authenticated
  `--force --delete <package AID>` removed the package and applet; a subsequent
  list showed only the ISD. The exact supplied v1.8.2 CAP reinstalled
  successfully. CLI operation output is in
  `artifacts/logs/gp-delete-physical-20261019.log`.
- Reflashed/restarted CHOUPI through
  `artifacts/logs/cycle-choupi-20261019T065052Z.log`. After the restart,
  GPPro again listed the package as `LOADED` and applet as `SELECTABLE`.
  `tools/probe-jc-algtest` selected it, read `1.8.2_jc305`, passed JCSystem,
  repeated SHA-256 doFinal, and the static SHA-256 workload. A single retained
  `WATCHDOG_RECOVERY ... marker=494E5354` was reported; runtime self-checks
  continued to pass and no `FAULT` appeared.
- Added and ran `tools/test-pico-powerup`. It checks GPPro registry state and
  applet SELECT/GETVERSION/JCSystem, saving
  `artifacts/logs/pico-powerup-persistence-20261019T065455Z.log`.
- Limitation: `picotool reboot` could not access the composite device reset
  interface. The repository cycle used its CDC/BOOTSEL recovery and reflashed
  the same image, which verifies persistence across restart/reflash, not a
  true removal and restoration of USB power.
- Next: use `tools/test-pico-powerup` after a real power-off/on when available;
  retain broader CAP restore validation as open.

## Latest continuation — test GP DELETE target resolution offline — 2026-10-19

- Extracted the pre-mutation target decision from `CapLoadSession::deleteObject()`
  into `gp::resolve_delete_target()` and wired the production method through
  it.
- `./tools/test-gp-delete` passed 26 checks, including unknown AID (`6A88`),
  package dependency rejection (`6985`), related-object package deletion, and
  instance deletion. `./tools/build arm` and `./tools/build riscv` both passed.
- This verifies command-to-target policy and transaction sequencing through
  shared code, but not real persistent records, CAP-slot erase, authenticated
  CCID dispatch, or restart restoration. No card APDU/flash was attempted
  because the Pico is in BOOTSEL.
- Next: connect the session lifecycle to a host storage/CAP-store test backend
  and exercise delete, GET STATUS, and restore across a fresh session.

## Latest continuation — exercise GP DELETE transaction sequencing offline — 2026-10-19

- Extracted the persistent transaction portion of `CapLoadSession::deleteObject()`
  into `gp::commit_delete_state()`. The production call site uses a backend
  adapter wired to `FlashMemory_Handler` and `fs_sync`; host tests use a fake
  backend to verify ordering and errors without Pico flash APIs.
- `./tools/test-gp-delete` passed 21 checks, including instance/package
  deletion sequencing, start failure, applet/static clear failure, sync
  failure, commit failure, and marker exceptions. `./tools/build arm` and
  `./tools/build riscv` both passed.
- The test does not exercise actual persistent records, CAP-slot deletion,
  authenticated CCID dispatch, or restart restoration. The device is in
  BOOTSEL, so no card APDU or flash operation was attempted.
- Next: add a storage-backed `CapLoadSession` test seam to cover markers,
  GET STATUS after actual deletion, CAP deletion, and restore after restart.

## Latest continuation — verify SCP03-authenticated GET DATA on the Pico — 2026-10-19

- Extended `tools/probe-gp-get-data` to establish SCP03 S8, verify the card
  cryptogram, authenticate with C-MAC, and send protected GET DATA (`0066`).
- On the physical Pico, both unprotected discovery and SCP03-authenticated
  GET DATA returned the expected Card Recognition Data with `9000`; absent
  CPLC still returned `6A88`. Full transcript:
  `artifacts/logs/gp-get-data-physical-20261019.log`.
- This probe does not select an applet or mutate card content. The next open
  lifecycle task is authenticated GP DELETE with GET STATUS and restore checks.

## Latest continuation — cover post-delete GET STATUS representations — 2026-10-19

- Extended the GP registry host checks to assert that an instance-deleted view
  returns no applet entry while retaining the loaded package entry, and that a
  package-deleted view returns no package entry.
- These checks validate the GET STATUS response builder for the expected
  lifecycle states. They do not call `CapLoadSession::deleteObject()` or prove
  persistence across restart.
- `./tools/test-gp-registry` passed 47 checks.
- Next: add an offline seam around persistent lifecycle/CAP storage so the
  actual authenticated DELETE dispatch and restart restoration can be tested.

## Latest continuation — verify GET DATA directly on the Pico — 2026-10-19

- Added `tools/probe-gp-get-data`, a bounded PC/SC probe that sends only
  `80CA006600` and `80CA9F7F00` on the basic channel, without selecting an
  applet. It checks the exact Card Recognition Data bytes and the unavailable
  CPLC status.
- `sudo -n /usr/local/sbin/pico2-pcsc-reader` granted access to
  `/dev/bus/usb/001/078`. The physical reader was
  `Pico 2 CHOUPI CCID [JCAlgTest CCID] (D92E743D5F640C0F) 00 00`, ATR
  `3B800181`. Both APDUs passed:
  `80CA006600` → `6618731606072A864886FC6B01640B06092A864886FC6B0403109000`;
  `80CA9F7F00` → `6A88`.
- The probe log is `artifacts/logs/gp-get-data-physical-20261019.log`. No
  applet SELECT was sent. The SCP03-authenticated GET DATA route is covered in
  the latest continuation above; GP DELETE lifecycle remains open.
- The parser and registry response builder have isolated host tests; the
  authenticated APDU dispatch and `CapLoadSession` persistent deletion remain
  untested end to end.

## Latest continuation — add truthful GlobalPlatform Card Data GET DATA — 2026-10-19

- Added `src/gp_get_data.hpp` with a Card Recognition Data (`0066`) TLV for the
  implemented GlobalPlatform recognition OID and SCP03 S8 OID. The response
  does not claim a CPLC or unique chip identity; `9F7F` and unknown tags remain
  `6A88` because there is no manufacturing-data source.
- Added `./tools/test-gp-get-data`; all 11 checks passed, including exact TLV
  bytes, success/not-found status words, unavailable CPLC, unknown-tag output
  clearing, and insufficient response capacity. `./tools/build arm` passed and
  rebuilt the CHOUPI firmware; `./tools/build riscv` passed.
- GET DATA `0066` is dispatched before authentication for card discovery and
  through the authenticated card-manager path. No physical or emulator APDU
  test was run, so transport and GlobalPlatformPro parsing remain unverified.
- `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed both target build gates,
  flashed the ARM CHOUPI firmware, and captured an 18-second serial run with
  repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`; no
  `FAULT` occurred. Log: `artifacts/logs/cycle-choupi-20261019T061401Z.log`.
- Next: create an offline authenticated `E4 DELETE` regression that exercises
  the actual card-manager lifecycle, including unknown AID, instance deletion,
  package dependency handling, and GET STATUS / restore after restart. Keep
  CPLC unavailable until a real manufacturing-data source exists.

## Latest continuation — confirm transaction rollback and power-cut coverage — 2026-10-19

- Re-ran `./tools/test-persistent-transaction`: all 29 checks passed, including
  rollback after an injected record-write failure and a failed filesystem
  snapshot commit. Existing ignored-qualifier and packed-member warnings remain;
  the test command exited 0.
- Re-ran `./tools/test-rp2350-fs-overlay`: all 27 checks passed, including
  1,000 randomized multi-record power-cut mutation points, CRC fallback,
  transaction deferral, reset restoration, and tombstone persistence. The host
  test emitted the existing `mallinfo()` deprecation warning and exited 0.
- This confirms storage-layer failure/recovery coverage; it does not prove an
  authenticated DELETE APDU or Java-level power-cycle behavior. Updated
  `STATUS.md` and `TODO.md` to remove storage-layer tests from the open list
  while retaining those end-to-end gaps.
- No physical token or APDU path was used. Next: build an offline regression
  for authenticated card-manager DELETE semantics, including unknown AID,
  applet deletion, package dependency handling, and post-delete GET STATUS / fresh
  process restore. GET DATA still needs truthful, profile-specific data.

## Latest continuation — restore committed applet state across emulator process restart — 2026-10-19

- `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed ARM and RISC-V builds,
  flashed the ARM image from BOOTSEL, and captured repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` with no `FAULT`.
  Log: `artifacts/logs/cycle-choupi-20261019T035222Z.log`.
- The authorized ACL helper found `/dev/bus/usb/001/073` and PC/SC listed the
  Pico reader. SELECT returned `9000`, but the bounded probe again timed out
  before GETVERSION (15-second wrapper, exit 124). No further physical APDUs
  were attempted.
- Improved the JCSystem VPCD test runner so closing its socket lets the
  emulator return through its normal save path. Added `--restore-existing` to
  reuse saved memory and launch without reinstalling the applet. First process
  committed instance value `0x33`; the new 512 KiB flash image was then used by
  a fresh emulator process. Static reference identity, persistent array child
  identity, persistent object-array alias/mutation, restored array checkcast,
  and the committed field all passed. Summary:
  `TEST:SUMMARY pass=7 fail=0 elapsed_s=9.520`.
- The first restore attempt reused case 7, whose existing expected value is
  `0x11` after transaction abort; actual restored value `0x33` showed commit
  persistence worked. Added dedicated case 58 expecting `0x33`; the rerun
  passed. Log: `artifacts/logs/jcsystem-restore-second-process-vpcd-20261019.log`.
- Next: continue GP DELETE/GET DATA and transaction failure/power-loss checks
  through VPCD. Do not retry the physical static workload until PC/SC sends an
  APDU after SELECT.

## Latest continuation — verify lifecycle callbacks across card reset — 2026-10-18

- Added case 57 to read the probe applet's callback counters, send VPCD card
  reset, SELECT again, and require one `deselect()` plus one `select()`.
- Focused case 57 passed (`TEST:SUMMARY pass=3 fail=0`, 16.801 s). Then cases
  55/56/57 passed together in one session (`TEST:SUMMARY pass=5 fail=0`,
  21.503 s): ordinary reselect counts `01 00`→`02 01`; rejected SELECT and a
  process APDU while unselected both returned `6999`, and a subsequent SELECT
  succeeded; reset/reselect counts advanced `04 02`→`05 03`.
- Log: `artifacts/logs/jcsystem-applet-lifecycle-callbacks-vpcd-20261018.log`.
  Physical testing was not used. Continue the profile/lifecycle audit on VPCD;
  the PC/SC host path still stops after SELECT.

## Latest continuation — verify rejected applet selection cleanup — 2026-10-18

- Extended the VPCD transaction probe with a test-only switch that makes the
  applet reject its next `select()` callback. The driver checks the SELECT
  failure status (`6999`), confirms a process APDU is not dispatched while the
  applet is unselected, then verifies a subsequent SELECT succeeds.
- `./tools/build-jcsystem-transaction-probe` passed. The combined cases 55/56
  passed in one session: callback counts advanced `01 00` to `02 01`, rejected
  SELECT and the subsequent process APDU returned `6999`, and the next SELECT
  returned `9000`. Summary: `TEST:SUMMARY pass=4 fail=0 elapsed_s=19.811`.
  Log: `artifacts/logs/jcsystem-applet-lifecycle-vpcd-20261018.log`.
- No hardware was used for this step. Continue with the Java Card profile and
  lifecycle audit on VPCD; physical consecutive APDUs remain blocked by the
  host PC/SC stall after SELECT.

## Latest continuation — verify applet callbacks and retry physical reader — 2026-10-18

- `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed: ARM and RISC-V builds
  succeeded, ARM CHOUPI flashed, and the 18-second serial capture reported
  repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` with no
  `FAULT`. Log: `artifacts/logs/cycle-choupi-20261018T194931Z.log`.
- Refreshed the authorized USB ACL. PC/SC listed both Virtual PCD readers and
  `Pico 2 CHOUPI CCID`; the bounded JCAlgTest probe selected the applet and got
  `9000`, then timed out without GETVERSION. No further physical APDUs were
  attempted; this matches the recorded post-SELECT host transport failure.
- Added VPCD case 55 to measure applet callback counters around a same-AID
  reselect. Focused test passed: initial counts `01 00`, then `02 01`; both
  `select()` and `deselect()` ran once. CAP build passed. The first sandboxed
  emulator run failed with `EPERM`; rerunning the local probe with host access
  passed in 13.627 seconds. Log:
  `artifacts/logs/jcsystem-applet-callback-vpcd-20261018.log`.
- Next: continue the Java Card profile/runtime audit on VPCD. Broader callback
  checks still need applet selection rejection, power-off/reset behavior, and
  multi-applet context support. Do not rerun physical APDUs until PC/SC can
  transmit the APDU after SELECT.

## Latest continuation — isolate PC/SC failure after SELECT — 2026-10-18

- The operator restarted `pcscd.service`. The authorized helper granted access
  to `/dev/bus/usb/001/068`; `lsusb` showed `2e8a:0009`, and the daemon was
  active. PC/SC briefly enumerated the physical reader alongside both Virtual
  PCD readers.
- `./tools/probe-jc-algtest --des-cipher-prepare` repeatedly selected
  JCAlgTest successfully (`9000`) and then timed out before GETVERSION. One
  run returned `SCARD_E_NOT_TRANSACTED`; another bounded run expired after
  eight seconds.
- A concurrent serial capture shows repeated GetSlotStatus requests, then
  exactly one XfrBlock: SELECT AID, 16 bytes, `select=9000`, `xfr=1`. No second
  XfrBlock/GETVERSION reached the card. Runtime self-checks continued to pass;
  there was no `FAULT` or applet failure. USB was still enumerated after the
  probe. This isolates the current failure to the host PC/SC transaction path
  after the first response, not the streamed collector or algorithm code.
- Evidence: `artifacts/logs/monitor-pcsc-probe-concurrent-20261018.log` and
  `artifacts/logs/monitor-pcsc-after-restart-20261018.log`. The focused probe
  itself was run in the terminal and its raw output was not archived. The
  monitor wrapper returned 137 after its 18-second capture because `cat` did
  not terminate promptly; the complete captured serial output is present.
- Next: use a host-side PC/SC diagnostic/recovery that does not run an
  unbounded `pcsc_scan`; then confirm SELECT + GETVERSION + JCSystem with one
  reader session before retrying the static suite. If host transport remains
  unavailable, continue the applet and JCSystem audit independently.
- Tried `tools/bridge-pico-ccid-vpcd` as an alternate host path. Initially the
  virtual endpoint refused connections. A bounded Java SmartcardIO status
  query showed both Virtual PCD readers with no card and caused TCP listeners
  on ports 35963/35964 to appear. The bridge then failed to claim CCID
  interface 2 (`libusb ... failed: -6`), consistent with the PC/SC service
  owning that USB interface. Do not run both paths against the same interface.
- Next access needed for this route: release or exclude the Pico interface
  from the system PC/SC daemon while preserving the Virtual PCD reader; then
  start the direct bridge and target `Virtual PCD 00 00`. This requires a host
  PC/SC configuration/service change and was not performed here.

## Latest continuation — isolate the remaining PC/SC interruption — 2026-10-18

- After `677d4e3`, the USB ID remained present as `2e8a:0009`; `pcscd.service`
  was active and the authorized ACL helper returned `PCSC:ACCESS_GRANTED`.
  Still, `timeout 5s pcsc_scan -r` hung without output and Java sometimes
  listed only the two Virtual PCD readers.
- Serial capture after a failed probe recorded SELECT and a firmware `9000`,
  but no GETVERSION APDU. An earlier recovery probe after the static run did
  pass SELECT, GETVERSION, JCSystem info, and DES prepare (`AA9000`). This
  intermittent reader/daemon state remains separate from the collector: the
  static capture showed no allocator failure or panic through 544 rows.
- Evidence: `artifacts/logs/pcsc-host-state-gc-stream-roots-20261018.log`,
  `artifacts/logs/monitor-gc-stream-roots-recovery-probe-20261018.log`, and
  `artifacts/logs/physical-gc-stream-roots-recovery-final-20261018.log`.
- **Blocked action:** restart `pcscd.service`. The supplied repository
  `AGENTS.md` permits autonomous sudo only for
  `/usr/local/sbin/pico2-pcsc-reader`, so I cannot run `sudo systemctl restart`
  myself. Operator command: `sudo systemctl restart pcscd.service`; then run
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` and `timeout 5s pcsc_scan -r`.
  Resume hardware testing once the Pico appears in the PC/SC list.

## Latest continuation — stream collector roots and remove traversal buffers — 2026-10-18

- Replaced dynamic GC worklist traversal with repeated stable-handle bitmap
  scans. Root and child references stream directly from `Context`/`Heap` into
  the marks, eliminating the large temporary root vector, pending queue, and
  child-reference vector. Mark states are 0 unreachable, 1 pending, and 2
  scanned; collection still sweeps only after a complete walk.
- ARM, RISC-V, and PC builds passed. The final `./tools/cycle choupi` passed
  on the physical Pico 2 with `TEST:SUMMARY pass=1 fail=0`.
- The physical JCAlgTest 1.8.2 static run reached 544 CSV rows, beyond the
  previous collector OOM point at 512 rows, without an allocation failure,
  panic, or GC-skip marker in the serial capture. It stopped when PC/SC returned
  `SCARD_E_READER_UNAVAILABLE` during AES-192. A recovery probe after the first
  interruption passed SELECT, GETVERSION, JCSystem info, and DES prepare
  (`AA9000`). A later probe showed SELECT reached firmware but GETVERSION was
  absent from serial; the USB ACL helper intermittently found zero devices or
  Java could not enumerate the Pico reader. This looks like a separate
  USB/PCSC passthrough issue. Do not report the static matrix as complete.
- The earlier fixed-capacity worklist attempt was not viable: it failed at a
  1,642-byte allocation. Removing the root vector was necessary after the
  bitmap-only walk still hit a 2,048-byte root-list allocation at 528 rows.
- Relevant evidence: `artifacts/logs/build-{arm,riscv,pc}-gc-stream-roots-final-20261018.log`,
  `artifacts/logs/cycle-choupi-gc-stream-roots-final-20261018.log`,
  `artifacts/logs/monitor-static-gc-stream-roots-20261018.log`,
  `artifacts/logs/jcalgtest-static-gc-stream-roots-20261018.log`, and the
  partial result directory
  `artifacts/results/physical-perf-static-gc-stream-roots-20261018/`.
- Next: diagnose the intermittent host USB/PCSC loss. Compare host USB
  enumeration, PC/SC reader visibility, and the firmware's CCID receive
  counters at the exact failed APDU. Then rerun static testing and resume the
  JCSystem/context audit. Keep expected `NO_SUCH_ALGORITHM` rows out of the bug
  count.

## Latest continuation — add project status overview — 2026-10-18

- Added root `STATUS.md` as the concise current-state summary, with tested
  capabilities, explicit limitations, current physical-test blockers, next
  priorities, and links to the runbook, plan, handoff, QKB, and progress log.
- Checked the summary against the latest VPCD result (51/51), current build
  wrappers, and physical-validation findings recorded in this handoff/QKB. No
  builds, tests, or hardware actions were needed for this documentation step.
- Next: use `STATUS.md` as the high-level overview and keep it current when
  major capabilities or validation status changes.

## Latest continuation — define single-applet shareable lookup fallback — 2026-10-18

- Replaced `getAppletShareableInterfaceObject()`'s `NotYetImplemented` VM abort
  with `null` for the current single-applet/default-Applet behavior. Added case
  48 covering null AID, an absent server AID, and the probe applet inheriting
  Applet's default method.
- PC build and probe CAP build passed. Focused VPCD case passed
  `TEST:SUMMARY pass=3 fail=0 elapsed_s=7.688`; ARM and RISC-V builds passed.
  Full suite passed `TEST:SUMMARY pass=51 fail=0 elapsed_s=138.095`. No
  physical device was accessed.
- Limitation: custom shareable-object callbacks require a multi-applet registry
  and applet context-switch bridge, which this VM does not have. The fallback
  is verified only for the default-null probe applet.
- SensitiveArrays is committed as `d220fdb`; this step is committed as
  `b34ed04`.
- Next: audit the remaining framework/runtime API gaps, especially applet
  context and shareable-object dispatch.

## Latest continuation — define unsupported SensitiveArrays behavior — 2026-10-18

- Replaced the remaining `NotYetImplemented` behavior for integrity-sensitive
  array APIs with their documented unsupported-feature outcomes. The runtime
  reports the capability as false; valid creation calls raise
  `SystemException.ILLEGAL_USE`, invalid type/memory raises
  `SystemException.ILLEGAL_VALUE`, negative length raises
  `NegativeArraySizeException`, and assertion/clear of non-sensitive arrays
  raises `ILLEGAL_VALUE` (null still raises `NullPointerException`).
- Fixed JC305 framework method-token remapping for `SensitiveArrays`: the
  imported static tokens 0–4 map to embedded method tokens 1–5 because the
  embedded class has a constructor at token 0. Before the fix, case 42 called
  the wrong native and raised `SystemException`.
- Added probe cases 42–47. PC, ARM M33, and RISC-V builds passed; focused cases
  passed; full VPCD suite passed
  `TEST:SUMMARY pass=50 fail=0 elapsed_s=138.900`. No hardware was accessed.
  Existing pinned Rust OS and packed CAP-member warnings remain.
- Next: audit `JCSystem.getAppletShareableInterfaceObject()` against the
  single-context runtime and determine whether a useful standards-compatible
  behavior can be implemented without inventing cross-applet state.

## Latest continuation — implement JCSystem.makeGlobalArray — 2026-10-18

- Implemented runtime-wide JCRE-owned array handles for boolean, byte, short,
  int (when enabled), and Object arrays. Global arrays are marked transient
  with `CLEAR_ON_RESET`, resolved through any active applet heap, and cleared
  on reset. Invalid type reports `SystemException.ILLEGAL_VALUE`; negative
  length reports `NegativeArraySizeException`.
- Global-array references are rejected from applet instance/static fields and
  array components. Global `Object[]` accepts null; writes of applet-owned
  objects are rejected because this VM does not yet implement shareable object
  ownership. Global Object[] casts recognize the wildcard Object component.
- Added JCSystem probe cases 36–41. PC, ARM M33, and RISC-V builds passed. The
  focused global-array cases passed; case 41 was rerun after adding null-entry
  coverage and passed. Full VPCD suite passed
  `TEST:SUMMARY pass=44 fail=0 elapsed_s=144.901`. No physical board was
  accessed; the current board image remains the ARM smoke firmware.
- Existing pinned Rust OS and packed CAP-member compiler warnings remain.
- Next: continue auditing remaining JCSystem/runtime API stubs, starting with
  `JCSystem.getPreviousContextAID()` and applet-context limitations. Do not
  treat unsupported algorithm results as VM defects.

## Latest continuation — add concise hardware JCAlgTest runbook — 2026-10-18

- Created root `readme.md` with copyable steps to extract the official
  JCAlgTest 1.8.2 client, build/flash CHOUPI, expose the Pico through PC/SC,
  install the supplied JC305 CAP when needed, and run support and static
  performance operations against the exact physical reader.
- Checked the documented flags against `tools/cycle`, `tools/run-jcalgtest`,
  the PC/SC helper, and GlobalPlatformPro help. No tests or hardware commands
  were run for this documentation-only change.
- The board remains on `pico2_jcvm_smoke.uf2` from the preceding ARM cycle;
  run the documented CHOUPI cycle before physical applet tests.
- Next: continue `JCSystem.makeGlobalArray()` design work, or follow the
  runbook when returning to physical JCAlgTest validation.

## Latest continuation — real-device ARM smoke cycle — 2026-10-18

- Ran `PICO2_HOST_USB_ACCESS=1 ./tools/cycle arm` against the attached Pico 2.
  Both required architectures built successfully; only the ARM smoke image was
  flashed. The normal reset interface was missing, and the wrapper recovered
  with CDC 1200-baud BOOTSEL before loading the image successfully.
- Bounded 8-second serial capture passed: repeated
  `TEST:PASS random_stream_varies`, `TEST:SUMMARY pass=1 fail=0`; no
  `TEST:FAIL` or `FAULT`. Full output is in
  `artifacts/logs/cycle-arm-20261018T074707Z.log`.
- The device is now running `pico2_jcvm_smoke.uf2`, not CHOUPI. This verifies
  the ARM smoke target on hardware; it does not validate the Java Card applet
  or JCSystem calls. Reflash CHOUPI before resuming physical applet tests.
- Next: continue the `JCSystem.makeGlobalArray()` design audit/implementation
  separately; preserve the confirmed flash recovery path and keep unsupported
  algorithms expected.

## Latest continuation — scope remaining JCSystem global-array gap — 2026-10-18

- Audited `JCSystem.makeGlobalArray()` against the bundled JC305 API and heap
  model. A simple transient-array allocation would not satisfy its cross-applet
  access or global-array reference restrictions.
- The current VM owns one `Heap` per `Context`; handles and firewall checks do
  not represent globally shared arrays or prohibit storing global references
  in applet fields/arrays. No code or tests changed in this inspection step.
- Next: design a JCRE-owned global heap/reference kind and enforce the API's
  non-storable global-array rule before implementing `makeGlobalArray()`.
  Keep unsupported algorithms expected and heap reclamation disabled.

## Latest continuation — implement single-context JCSystem.isAppletActive — 2026-10-18

- `isAppletActive(AID)` now checks the supplied AID object's byte-array field
  against the active context's registered/restored applet AID. It returns
  false for null, malformed, unregistered, or nonmatching AIDs. Added case 35
  for the current applet and an unknown AID.
- PC, ARM M33, and RISC-V builds and the probe CAP build passed. Focused VPCD
  case 35 passed. Full suite passed 38/38 in 221.009 s. No physical device was
  accessed.
- Scope: one current context/basic channel only; applets active on other
  logical channels and cross-applet context state remain unsupported.
- Next: audit `makeGlobalArray()` semantics and clear-on-reset lifetime before
  implementing it. Keep unsupported algorithms expected and heap reclamation
  disabled.

## Latest continuation — honor unsupported object-deletion requests — 2026-10-18

- `JCSystem.isObjectDeletionSupported()` returns false, but
  `requestObjectDeletion()` silently returned. It now raises a catchable
  `SystemException.ILLEGAL_USE` (reason 6), as specified by the supplied API.
  VM-raised `SystemException` reasons are carried through the active `Context`
  and reset after dispatch.
- Added case 34. PC, ARM M33, and RISC-V builds passed; the probe CAP rebuilt;
  focused case passed (`019000`). Full VPCD suite passed 37/37 in 173.290 s.
  No physical device was accessed.
- A focused run initially used a stale generated probe CAP. Rebuilding with
  `./tools/build-jcsystem-transaction-probe` incorporated the new case.
- Next: audit remaining JCSystem stubs such as `makeGlobalArray()` and
  `isAppletActive()`, with semantic and lifecycle coverage before changes.
  Preserve expected unsupported algorithms and keep heap reclamation disabled.

## Latest continuation — preserve JCSystem transient-array lifetimes — 2026-10-18

- Changed invalid transient clear-event handling to catchable
  `SystemException.ILLEGAL_VALUE`. The four transient array factories, their
  element access, event reporting, and invalid-event behavior are covered.
- Fixed VPCD applet lifecycle handling: the applet context is reused on
  same-AID reselection; `CLEAR_ON_DESELECT` arrays are cleared after
  `deselect()`, and `CLEAR_ON_RESET` arrays are cleared on card reset/power-off.
  Previously, context recreation after deselection also discarded reset-event
  arrays. Cases 30–33 check both event boundaries.
- PC, ARM M33, and RISC-V builds passed. Focused transient and lifecycle VPCD
  checks passed. Full suite passed 36/36 in 114.875 s. No physical device was
  accessed.
- Next: audit remaining JCSystem API stubs and applet lifecycle behavior.
  Preserve expected unsupported algorithms and keep heap reclamation disabled.

## Latest continuation — dispatch VM-raised APDUException — 2026-10-18

- Mapped internal `Exceptions::APDUException` to framework export token 12
  (JC305 import remap is applied by the shared resolver) and set the reusable
  instance reason to `APDUException.ILLEGAL_USE` for APDU state-machine
  violations.
- Added cases 26–27: call `setIncomingAndReceive()` twice to trigger the
  internal invalid-state path, return without using the invalid APDU state,
  then read the captured reason on the next APDU. Focused VPCD passed; full
  suite passed 30/30 in 159.731 s. PC, ARM M33, and RISC-V builds passed. No
  physical device was accessed.
- The first probe attempted to send an outgoing response after triggering an
  APDU state error and got `6F00` on that follow-up send. The final probe
  returns immediately and verifies the catch result on the next command.
- Next: audit the remaining `JCSystem` transient-array methods and their
  clear-event behavior. Keep heap reclamation disabled and preserve expected
  `NO_SUCH_ALGORITHM` results.

## Latest continuation — instantiate the root java.lang.Object class — 2026-10-18

- Fixed `Class_Handler::getInstanceFieldsSize()` to stop at the root
  `java.lang.Object` class before resolving a superclass. Its old `do/while`
  attempted to resolve Object's absent superclass. Also removed `noexcept`
  from `JC_Instance` constructors that perform CAP lookups and field sizing,
  so malformed metadata can reach the VM's normal exception boundary.
- Added case 25, which instantiates `new Object()` and checks the returned
  reference. PC, ARM M33, and RISC-V builds passed. Focused VPCD case passed
  (`019000`); full suite passed 28/28 in 102.270 s. All 52 heap trace walks
  were complete with zero invalid refs or edge-load failures. No board was
  accessed.
- This resolves the object-allocation failure that initially obscured the
  ArrayStoreException test. Next audit remaining Java Card runtime exception
  paths; keep heap reclamation disabled.

## Previous continuation — dispatch JCSystem SystemException — 2026-10-18

- Generalized VM exception class lookup from `java.lang` to a package AID and
  class token. `Exceptions::SystemException` now resolves framework export
  token 13 and reuses a rooted instance. For the current invalid
  `JCSystem.getAvailableMemory` type path, it sets reason
  `SystemException.ILLEGAL_VALUE` before dispatch.
- Added case 24, which passes memory type 3, catches `SystemException`, checks
  the reason, and returns `019000`. PC, ARM M33, and RISC-V builds passed.
  Full VPCD suite passed 27/27 in 98.571 s; all 50 heap trace walks completed
  with zero invalid refs or edge-load failures. No board was accessed.
- Next: continue auditing VM-raised Java Card framework exceptions and
  investigate the separate `new java.lang.Object()` constructor failure.
  Unsupported algorithm rows remain expected; heap reclamation stays off.

## Previous continuation — dispatch ArrayStoreException — 2026-10-18

- Added the `java.lang.ArrayStoreException` export-token mapping (token 11) to
  the common VM-to-Java exception dispatcher. The focused probe creates an
  `Exception[]`, stores the active applet instance through an `Object[]`
  reference, and verifies the mismatch is caught with response `019000`.
- PC, ARM M33, and RISC-V builds passed. Full VPCD JCSystem suite passed
  26/26 in 116.840 s; all 48 heap trace walks completed with zero invalid
  refs or edge-load failures. No physical board was accessed.
- A separate probe using `new Object()` terminated the PC emulator with an
  uncaught `jcvm::Exceptions` before the tested array store. This is tracked
  in QKB as an independent object-construction issue. Do not use that variant
  as the ArrayStoreException regression.
- Next: audit remaining exception types and the object construction failure.
  Heap reclamation remains disabled.

## Previous continuation — dispatch VM runtime exceptions through Java handlers — 2026-10-18

- The interpreter now translates common VM-raised runtime exceptions into
  reusable, per-heap Java exception objects and dispatches them through the
  current Java exception table. Mappings cover `IndexOutOfBoundsException`,
  `ArrayIndexOutOfBoundsException`, `NegativeArraySizeException`,
  `NullPointerException`, `ClassCastException`, `ArithmeticException`, and
  `RuntimeException`. Unmapped/security failures retain the existing VM-fatal
  behavior.
- The new upper-bound probe initially exposed that `JC_Array` backing access
  leaked generic `IndexOutOfBoundsException`. All eight typed array read/write
  entry points now check logical array length first and raise Java's expected
  `ArrayIndexOutOfBoundsException`.
- PC, ARM M33, and RISC-V builds passed. Focused VPCD cases for null access,
  array bounds, division by zero, and negative array size passed (`019000` for
  each); the 50-cast ClassCastException regression also passed. Full VPCD
  JCSystem suite passed 25/25 in 104.317 s. All 46 heap trace walks completed
  with zero invalid refs or edge-load failures. No physical board was
  accessed.
- Next: audit remaining VM exception sources, especially type/store and
  security violations. The mapping is not exhaustive; keep heap reclamation
  disabled and preserve the existing expected `NO_SUCH_ALGORITHM` behavior.

## Previous continuation — dispatch and reuse ClassCastException — 2026-10-18

## Latest continuation — dispatch and reuse ClassCastException — 2026-10-18

- Fixed array `checkcast` atype mapping and then found the negative-cast path
  threw a C++ exception directly, bypassing Java catch handlers. It now
  resolves `java.lang.ClassCastException` from the active applet imports,
  creates/reuses a JCRE-owned per-heap instance, and routes it through
  `doThrow()`.
- The ClassCastException handle is included in heap roots. Case 18 performs 50
  mismatched casts in one APDU and verifies all are caught with one stable
  exception instance. Focused case passed. PC, ARM, and RISC-V builds passed.
- Full VPCD suite passed 21/21 in 110.937 s; 38 post-APDU trace walks were
  complete with zero invalid references and zero edge failures. Final sample:
  arrays 6/16, instances 6/14. No board was accessed.
- Next: audit other VM-generated runtime exceptions for Java handler dispatch,
  and continue root/alias coverage before any heap tombstones or reuse.

## Latest continuation — map Java Card array `checkcast` types — 2026-10-18

- Fixed `docheck()` to map bytecode array codes 10–14 explicitly to internal
  `jc_array_type` values. The direct enum cast compared decimal bytecode codes
  with enum values in the `0x10` range, making valid array casts fail.
- Added case 17, which casts the persisted byte array from `Object[]` to
  `byte[]`. Focused test passed (`019000`); cases 16 and 17 passed together
  after case 16 mutates their shared array. PC, ARM, and RISC-V builds passed.
- Final full VPCD suite passed 20/20 in 114.250 s with 36 complete trace
  walks, zero invalid refs, and zero edge failures. No hardware was accessed.
  A first full run after the code fix exposed a test-order expectation bug
  (case 17 expected the original byte after case 16 changed it); the assertion
  now checks array length instead.
- Next: add a negative mismatched-array cast case, then continue auditing root
  and persistent graph shapes. Heap reclamation remains disabled.

## Latest continuation — verify persistent instance array aliases — 2026-10-18

- Expanded the identity probe to include two fields on a persisted holder that
  reference one byte array. The test writes `0x2A` through one field and checks
  both reference identity and the value through the other field.
- Focused VPCD case 16 passed (`019000`). Full JCSystem/VPCD suite passed
  19/19 in 85.020 s with two post-APDU heap walks; all 34 walks completed with
  zero invalid refs and zero edge-load failures. Final counts: arrays 6/16,
  instances 5/13. No board was accessed.
- A first version used `Object` fields and explicit casts; it failed with a
  `ClassCastException` at checkcast opcode `0x94`. Typed `byte[]` fields pass,
  so general cast behavior is a separate untriaged issue (recorded in QKB).
- Next: isolate the cast failure and broaden heap graph/root tests. Keep
  reclamation disabled.

## Latest continuation — canonicalize persistent array children — 2026-10-18

- Added regression case 15: a persistent `Object[]` stores the same byte array
  twice and checks that both restored entries are reference-identical. The
  first focused run failed (`00`), exposing that array children lacked the
  persistent alias handling already used for objects.
- Persistent arrays now retain their canonical tag after a successful write;
  repeated references persist as tag aliases and resolve through the heap's
  canonical persistent-tag lookup. Temporary `PARRAYTRACE` output was removed.
- PC, ARM, and RISC-V builds passed. Focused VPCD case 15 passed (`019000`),
  and the full JCSystem suite with two heap walks per APDU passed 18/18 in
  81.386 s. All 36 walks completed with zero invalid refs or edge failures.
  No board was used.
- No reclamation or slot reuse was added. Cyclic arrays and other persistent
  graph shapes remain untested.
- Next: extend identity coverage to persistent instance fields and array
  mutation through an alias, then reassess what heap edges/roots are still
  missing before designing tombstones.

## Latest continuation — test repeated heap walks — 2026-10-18

- Bounded `PICO2_HEAP_TRACE` to 1–4 consecutive walks per APDU for host
  diagnostics. At repeat count 2, VPCD passed 17/17 in 96.618 s and emitted
  15 pass pairs. All 30 walks were complete, with zero invalid handles or
  edge-load failures; array and instance totals did not grow between paired
  walks.
- This confirms repeat traversal does not allocate duplicates for the graph
  shapes in the probe. It is not a general persistent-identity proof and does
  not establish complete roots for all applets. Keep repeated tracing opt-in.
- Next: expand root/identity tests to persistent reference arrays and aliases,
  then decide whether mark-only results can safely drive tombstoning.

## Latest continuation — add opt-in post-APDU heap diagnostics — 2026-10-18

- Added host-only `PICO2_HEAP_TRACE=1` diagnostics after the APDU `try` scope
  has destroyed the interpreter and temporary native references. The hook
  runs only when `Stack::empty()`; default and embedded builds do not trace.
- VPCD produced 15 trace samples, all complete with zero invalid references,
  zero edge-load failures, and no active-frame skips. The final sample showed
  arrays `6/16` reachable and instances `4/12` reachable. Lazy persistent
  reads grew heap totals during traversal. VPCD regression passed 17/17 in
  73.382 s. No board was used.
- These are candidates only; mark completeness across every runtime path is
  not yet proven, and no objects were cleared or reused.
- Next: audit persistent/runtime root paths and add focused root completeness
  validation. Keep the diagnostic opt-in and reclamation disabled.

## Latest continuation — preserve typed reference return roots — 2026-10-18

- `Stack` now tags the top-level `ARETURN` cache as a reference and exposes it
  through `Context::appendRootReferences()`. Primitive returns clear the tag;
  entering the next root frame clears stale return state.
- PC, ARM M33, and RISC-V builds passed. VPCD JCSystem regression passed
  17/17 in 70.323 s. No hardware was flashed or accessed.
- This does not scan active Java locals/operands or native C++ locals. A mark
  walk is only useful at a quiescent boundary after callers release references.
- Next: prove the APDU return boundary and root-return lifecycle, then run
  diagnostic marking there without enabling reclamation.

## Latest continuation — enumerate runtime exception roots — 2026-10-18

- Added per-heap reusable exception slots for the ROM and imported
  `CryptoException` class views, routed matching factory allocations to those
  slots, and included both handles in `Heap::appendRootReferences()`.
- PC, ARM M33, and RISC-V builds passed. VPCD JCSystem regression passed
  17/17 in 71.327 s, including 50 repeated throws with stable object identity
  and updated reason. No hardware was flashed or accessed.
- This completes only heap-owned runtime exception roots. The tracer is still
  not called by runtime code. The root-return cache loses its value type, and
  active Java words/native locals cannot be scanned safely.
- Next: inspect root-return lifecycle, then establish and validate a
  post-APDU safepoint for diagnostic mark accounting. Keep reclamation off.

## Latest continuation — add mark-only heap tracing — 2026-10-18

- Added `Heap::traceReachableFrom()` with separate array/instance marks, a
  deduplicating work queue, offset validation, and mark-vector growth while
  persistent edge reads append heap entries. It reports incomplete traversal
  for invalid references or VM edge-load exceptions. It never frees slots.
- PC, ARM M33, and RISC-V builds passed. The VPCD JCSystem/transaction suite
  passed 17/17 in 70.499 s. No hardware was flashed or accessed.
- The tracer is not invoked by runtime code and only covers references passed
  by its caller. The committed visitors still omit runtime CryptoException
  roots; Java stack words and native locals are not enumerated. This is not a
  collector and its counts must not authorize slot reuse.
- Next: expose all VM-owned roots, establish a quiescent post-APDU point with
  no outstanding native handles, then add diagnostic mark accounting across
  repeated APDUs. Preserve stable handles and keep reclamation disabled.

## Latest continuation — expose heap roots and typed edges — 2026-10-18

- Added `Heap::appendRootReferences()` for cached static references, plus
  `Context::appendRootReferences()` for context-held
  handles. The context visitor also maps retained APDU buffer `shared_ptr`s
  back to their stable array handles.
- Added `Heap::appendChildReferences()` to follow typed instance reference
  fields and Java reference-array elements. Persistent reads can deserialize
  new heap entries during this traversal.
- PC, ARM, and RISC-V builds passed. Full VPCD regression passed 17/17 in
  78.115 s. The initial ARM build caught a range-loop-copy warning; corrected
  before the passing build. No board was flashed.
- This is traversal support only: there is no mark-only traversal or slot
  reclamation. These methods do not inspect live Java stack words, C++ locals,
  or VM-owned roots stored outside the static-reference cache.
- Next: build a mark-only work-queue walk that tolerates deserialization growth,
  invoke it only after the APDU interpreter/native frames return, and validate
  its accounting across repeated APDUs. Keep reclamation and slot reuse out
  until root accounting is complete.

## Latest continuation — heap-root inventory — 2026-10-17

- Read-only source review after commit `425602b` mapped the remaining roots and
  edges. `Context` owns AID, registered/restored applet, and APDU references;
  `Heap` owns runtime CryptoException roots and the static-reference cache.
  The cache currently has no enumeration accessor.
- Instance fields carry `FieldType` metadata and compact references;
  reference arrays carry compact references too. Persistent instance fields
  and persistent array children deserialize lazily, so a marking traversal
  must tolerate heap growth and avoid invalidated iterators.
- Safepoint constraint: reference locals/operands in Java frames are untyped,
  and native C++ locals can hold handles. A first collector must run only after
  the interpreter and native APDU caller return, with no active frames or
  native handles. Context-held APDU refs and the stack's root return value
  still need explicit treatment.
- Heap offsets are 1-based list positions and `jref_t` has no generation
  counter. Erasure/compaction is unsafe; future tombstones must be validated by
  a mark-only visitor before slot reuse.
- No code, build, runtime test, or hardware operation was performed in this
  architecture step. Details and next step are in `.agent/QKB.md`.
- Next: add explicit `Context`/`Heap` root and typed-edge visitors; first run
  mark-only validation, then consider slot reclamation separately.

## Latest continuation — canonical static reference roots — 2026-10-17

- `GETSTATIC_A` previously deserialized object/array references from flash on
  every read, causing duplicate heap entries and broken Java reference
  identity. It now goes through `Static_Handler::getPersistentRef`, which
  caches references by package ID and static field in the current applet heap.
  Successful `PUTSTATIC_A` writes refresh the root.
- The per-heap roots are scoped to the selected applet context. The lifecycle
  trace confirmed context destruction on deselect and GP DELETE deselection
  before package removal.
- Added transaction-probe case 14. Focused VPCD case passed (`019000`); full
  VPCD regression passed 17/17 in 109.117 s. `./tools/build-pc`,
  `./tools/build-jcsystem-transaction-probe`, and `./tools/build arm` passed.
  No hardware or RISC-V build was used.
- This is root canonicalization only, not garbage collection or heap-slot
  reuse. If persistent transaction rollback is implemented for static fields,
  ensure abort also reconciles cached roots.
- Implementation commit: `425602b`.
- Next: design root enumeration and safe stable-slot reclamation for the static
  benchmark heap-retention problem; the canonical static-root cache is now in
  place, but it is not a collector. Keep unrelated worktree modifications
  intact.

## Latest continuation — prevent restricted-context USB probes — 2026-10-17

- Hardened shell and Python USB/PCSC guards to fail closed unless the caller
  explicitly sets `PICO2_HOST_USB_ACCESS=1` and `/proc/self/status` reports
  `NoNewPrivs: 0`. The marker is required because `NoNewPrivs` alone does not
  prove that execution used the host-access path.
- `AGENTS.md` and the physical-token reference now require each wrapper command
  to carry that marker and `sandbox_permissions: "require_escalated"` from its
  first attempt. Direct commands such as `lsusb` must use host access on the
  command itself. If host access is unavailable, stop hardware operations and
  continue offline.
- Validation in the current `NoNewPrivs: 1` context: shell syntax and Python
  compilation passed; all eight USB/PCSC wrapper entry points rejected in
  under one second; a simulated unreadable `/proc/self/status` rejected with
  the fail-closed diagnostic. No libusb/PCSC command was reached.
- Next: implement VM-managed exception reuse or correct object reclamation;
  the API-level static-field cache is not viable. ARM M33 only. Use host access
  from the outset for any physical validation.
- Implementation commit: `13808f4`.

## Heap reclamation architecture review — 2026-10-17

- Reviewed the tracked static SHA fix and the current `Heap`, `Stack`,
  `Context`, `JC_Instance`, `JC_Array`, and static-field handlers. The SHA
  repeated-array allocation was fixed by caching persistent instance fields;
  this does not add general object reclamation.
- A collector cannot erase or compact `Heap` lists because `jref_t` stores
  their 1-based offsets. The stack contains untyped words, so collection while
  bytecode/native calls run would miss roots. An empty-frame APDU boundary is a
  viable first safepoint, but not enough on its own: `GETSTATIC_A` reconstructs
  references from flash on demand, and persistent fields/reference arrays can
  keep transient objects reachable.
- Recorded stable-slot, safepoint, and root-enumeration requirements in
  `.agent/QKB.md`. No allocator code changed and no build or hardware check was
  run in this documentation/architecture step.
- The same review found a second concrete source path: `bc_getstatic_a()` calls
  `getPersistentField_Reference()` directly on every access, so static object
  and array references are reconstructed as duplicate heap entries rather
  than returned from a canonical root cache. QKB now records the exact finding
  and the need to handle cache invalidation on deletion/context teardown.
- Next: design a canonical VM root table for static references, then expose
  context/heap roots and typed graph traversal. Only after root enumeration is
  complete should stable-slot reuse be implemented. Preserve the independent
  PC/SC reader-loss investigation; do not repeat the long static suite yet.

## Supported-sweep allocation failure diagnosis — 2026-10-17

- The concurrent sweep monitor did record `malloc 12 failed to allocate
  memory` at line 62,594, correcting the earlier conclusion that there was no
  allocation failure. The official client failed at DSA/SHA-384 after 1,636
  rows, then lost PC/SC reader publication.
- Source inspection shows `CryptoException.throwIt()` allocates for every
  throw and `Heap::addInstance()` retains every object; no object collector or
  reclamation path exists. This explains cumulative heap exhaustion. Keep
  unsupported algorithms expected; do not implement them to avoid exceptions.
- A 50-throw VPCD experiment attempted to cache the exception in a Java static
  field. This is unsuitable because static references serialize through flash;
  the VM returned different object identity by throw 12. The experimental
  source and test were reverted. Do not repeat this approach.
- No source fix is currently retained. The next implementation should give
  Java Card runtime-owned exceptions a VM-managed per-context root or implement
  correct object reclamation, with a focused repeated-throw regression before
  retrying the physical supported sweep.
- Evidence: `artifacts/logs/monitor-arm-ccid-sweep-repro-20261017.log`,
  `artifacts/logs/jcsystem-exception-reuse-vpcd-v2-20261017.log`, and
  `artifacts/logs/build-pc-exception-reuse-v2-20261017.log`.

## Latest continuation — reproduce and recover supported-sweep reader loss — 2026-10-17

- Repeated the physical official `ALG_SUPPORT_EXTENDED` sweep with a bounded
  240-second serial capture running concurrently. It stopped again on
  `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384`, at 1,636 completed CSV rows (the CSV
  contains its header plus those rows). The serial trace recorded normal CCID
  traffic through RX event 3,556 / transfer 2,909 and repeated runtime PASS;
  it contained no firmware fault, allocation failure, `6F00`, or reset.
- After the sweep, the Pico remained USB-enumerated as `2e8a:0009`, but Java
  PC/SC listed only the two VPCD readers. The authorized ACL helper granted
  `/dev/bus/usb/001/010`, but the physical reader remained absent. Reflashing
  the same ARM image through BOOTSEL restored the physical PC/SC reader; the
  bounded physical probe then passed SELECT, GETVERSION, JCSystem info, static
  and repeated SHA-256 (including 50 calls), and expected unsupported
  RSA-SHA1.
- The failure is narrowed to the CCID/PCSC path after the long sweep; its cause
  is still unresolved. The older heap-retention OOM remains a distinct known
  risk and was not observed in this run. QKB and TODO include this evidence and
  recovery. Do not claim the full supported sweep passes.
- Logs: `artifacts/logs/jcalgtest-supported-arm-disconnect-repro-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-repro-20261017.log`,
  `artifacts/logs/flash-arm-supported-disconnect-recovery-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-recovery-20261017.log`,
  and `artifacts/logs/jcalgtest-physical-after-disconnect-reflash-20261017.log`.
- Next: investigate why long CCID/PCSC sessions lose reader publication and
  finish the supported CSV/reference comparison. Keep USB calls in host-access
  context. ARM M33 only; no RISC-V build or test.
- Investigation record commit: `437365d`.

## Latest continuation — report unsupported OneShot algorithms correctly — 2026-10-17

- Fixed `InitializedMessageDigest.OneShot.open(byte)` to throw its documented
  `CryptoException.NO_SUCH_ALGORITHM` instead of returning null. JCAlgTest
  called `close()` after `open()` and only caught `CryptoException`, so the old
  stub escaped as an NPE/`6F00` and poisoned later rows.
- Added a JCSystem transaction-probe APDU that verifies the returned reason.
  `./tools/build arm`, `./tools/build-pc`, and the host-access VPCD regression
  passed; final result was `TEST:SUMMARY pass=14 fail=0 elapsed_s=23.936`.
  The PC ROM mask must be rebuilt after API changes, or the emulator test runs
  stale bytecode.
- Flashed the rebuilt ARM image through `./tools/flash`; CDC reset fallback
  entered BOOTSEL and reported `FLASH:PASS`. The finite 8-second serial capture
  reported runtime PASS and no FAIL/FAULT. The physical JCAlgTest probe passed
  SELECT, version `1.8.2_jc305`, JCSystem info, repeated SHA-256 including a
  50-call workload, and expected unsupported RSA-SHA1 handling.
- The official physical `ALG_SUPPORT_EXTENDED` sweep passed the prior
  OneShot area and CSV records `InitializedMessageDigest.OneShot / ALG_SHA_256`
  as unsupported. It later stopped after 1,637 CSV rows with
  `SCARD_E_READER_UNAVAILABLE` at `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384`; it
  did not produce `6F00` before disconnect. The Pico re-enumerated and a
  bounded physical probe passed afterward. Full sweep remains incomplete; no
  unsupported algorithm was treated as a defect. QKB records this separately
  from the known heap-retention OOM.
- Physical USB actions used host-access execution from the outset. After board
  re-enumeration, the authorized helper granted node `/dev/bus/usb/001/009`.
  Do not use `pcsc_scan`.
- Logs: `artifacts/logs/build-arm-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/build-pc-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/jcsystem-oneshot-unsupported-vpcd-final-20261017.log`,
  `artifacts/logs/flash-arm-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/monitor-arm-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/jcalgtest-physical-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-oneshot-fix-20261017.log`, and
  `artifacts/logs/jcalgtest-physical-after-supported-20261017.log`.
- Nested API implementation commit: `f6776a8`; root regression/docs commit:
  `3b16eae`. Next diagnose the later
  supported-sweep disconnect and compare completed results with jCardSim and
  declared support. ARM M33 only; do not build or test RISC-V.

## Latest continuation — JCSystem memory overload golden coverage — 2026-10-17

- Extended the Java Card transaction probe with one APDU that calls scalar
  `JCSystem.getAvailableMemory(byte)` and the jc304
  `getAvailableMemory(short[], short, byte)` overload for persistent,
  clear-on-reset, and clear-on-deselect memory, then checks maximum and unused
  commit capacity. The golden desktop result is 22 zero bytes because the PC
  provider does not measure Pico heap/storage; physical nonzero values remain
  covered by the live Pico probe.
- `./tools/build-pc` passed and regenerated the ROM mask. The first restricted
  VPCD attempt failed to bind its local socket; rerun via host access passed
  `TEST:SUMMARY pass=13 fail=0 elapsed_s=25.154`. CAP conversion passed.
- Progress record: `jcsystem-memory-overloads-vpcd-golden`; commit `7734d9c`.
- Next: continue callback/GP lifecycle golden tests and transaction failure
  acceptance; do not treat the optional static suite as passing.

## Latest continuation — close remaining restricted USB paths — 2026-10-17

- Found two direct-libUSB tools that bypassed the existing restricted-context
  guard: `tools/bridge-pico-ccid-vpcd` and
  `tools/repair-jcalgtest-raw-ccid`. Also found `tools/cycle` started both
  architecture builds before the nested flash guard rejected hardware access.
- Added a shared Python `NoNewPrivs` check for the two direct USB tools and an
  early check in `tools/cycle`. In the restricted context, all eight USB/PCSC
  entry paths tested exited with the host-access diagnostic in 0.75–1.52 s;
  no libusb/PCSC access or builds started. Syntax checks passed.
- Future hardware operations: invoke the wrapper through host-access execution
  from the outset. The in-tool guard only prevents wasted attempts when a
  restricted execution is accidentally used.
- Commit: `68c3aa3` (`Guard all Pico USB tools in restricted context`).
- Next: continue golden JCSystem/lifecycle coverage from TODO after this
  fail-fast change is committed.

## Latest continuation — JCSystem memory accounting and hardware access guard — 2026-10-17

- Implemented Pico `JCSystem.getAvailableMemory` short and jc304 array forms.
  Transient capacity comes from linked Newlib free blocks plus unused `_sbrk`
  heap range; persistent capacity comes from remaining bytes in the writable
  64 KiB overlay. Short values saturate at `0x7fff`; array form returns the
  32-bit capacity as high and low words. `getMaxCommitCapacity` and
  `getUnusedCommitCapacity` remain zero while commit transactions are not
  implemented. The desktop target returns zero for allocator/overlay values.
- Both `./tools/build arm` and `./tools/build riscv` passed. The ARM firmware
  flashed after the standard path failed to initialize libusb; the host-access
  execution path succeeded, using the device's CDC reset fallback into BOOTSEL.
  An eight-second serial capture reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`.
- Physical JCAlgTest 1.8.2 probe passed on the exact reader. The live memory
  APDU returned persistent `47,777` bytes and transient reset/deselect
  `325,352` bytes in the 32-bit forms. The scalar persistent and deselect
  reports matched their saturated extended values. JCSystem install-time
  fields are stale across firmware updates; use the live P1=0 query. The
  applet's reset-RAM display adds its own benchmark arrays and is not directly
  comparable to JCSystem scalar memory.
- Systematic USB fix: `tools/common.sh` now detects `NoNewPrivs: 1`; flash,
  monitor, physical probe, and physical JCAlgTest wrappers fail immediately
  with the host-access instruction instead of starting libusb/PCSC attempts.
  The simulator mode remains available in the restricted context. Five
  fail-fast checks returned in under one second. From now on, invoke every
  flash, serial, USB, and physical PC/SC command through host access from the
  outset; do not probe the restricted context first. After USB re-enumeration,
  refresh ACL with the authorized `sudo -n /usr/local/sbin/pico2-pcsc-reader`
  helper in host access. Never use `pcsc_scan`.
- The session digests were reviewed as operational context only and not
  promoted into the topic wiki. The repository `.agent/QKB.md` remains the
  canonical project issue record.
- The physical static benchmark's heap-retention OOM remains unresolved and
  documented in `.agent/QKB.md`; do not clear offset-indexed heap entries.
- Evidence logs: `artifacts/logs/build-arm-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/build-riscv-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/flash-arm-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/monitor-arm-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/jcalgtest-physical-jcsystem-memory-accounting-continue-20261017.log`,
  and `artifacts/logs/jcalgtest-physical-jcsystem-live-memory-continue-20261017.log`.
- Commit: `a9a2091` (`Implement Pico JCSystem memory reporting and USB guard`).
- Next: continue golden JCSystem/applet lifecycle coverage and remaining
  acceptance criteria. Keep all physical commands on host access and preserve
  the unrelated pre-existing worktree edits.

## Knowledge base checkpoint — 2026-10-17

- Read the updated root `AGENTS.md` and created `.agent/QKB.md` as required.
  It records reproducible recovery for VM USB detach/reattach, applying the
  explicitly permitted PC/SC ACL helper after re-enumeration, and avoiding
  `pcsc_scan` because timed invocations previously left a 99.2%-CPU scanner
  behind. It also records the still-open physical static SHA repeat failure.
- Later checks through the permitted host-access path saw the running Pico as
  `2e8a:0009` at `/dev/bus/usb/001/057`. The exact helper granted node ACLs and
  host-context Java SmartcardIO enumerated the physical reader alongside both
  VPCD readers. Default sandbox commands cannot see the physical USB/PCSC
  context (`libusb -99`/empty reader list); run all hardware tools through the
  host-access execution path. Refresh ACL after every re-enumeration with the
  authorized helper.
- Read-only USB descriptor inspection confirms the CCID interface and
  endpoints `03`/`83`; the installed libccid table contains the exact
  `2e8a:0009` mapping. pcscd is socket activated and exits normally when idle,
  but Java queried an empty reader list while the client was active. The
  reader-publication cause remains unresolved.
- The wiki skill file path advertised by the tool catalog was not present in
  this VM, and no wiki MCP tools are exposed in this session. The repository
  QKB is therefore the maintained canonical issue record; session digests are
  not treated as topic knowledge.
- The official physical `ALG_PERFORMANCE_STATIC` run now reproduces the SHA
  failure in the proper host context: baseline average `529.0`; first measured
  256-byte/50-call APDU `AA9000` in 761 ms; next identical call ends in
  `SCARD_E_READER_UNAVAILABLE`. Following it, a fresh isolated SHA probe
  returned `F105`. A full CDC-to-BOOTSEL/application reboot restores the
  isolated probe without flashing.
- A focused APDU probe passed ten one-call SHA requests, the exact 50-call
  static request, and one further SHA request. That single-workload sequence
  does not reproduce the official client's failure. A later reboot left USB
  enumerated at node `001/062`, but the host-context reader list showed only
  VPCD. Direct CCID timed out before APDU; finite serial capture remained
  `TEST:PASS` with `GetSlotStatus` only. Latest details are in `.agent/QKB.md`.
- Next: restore physical PC/SC discovery (VM USB detach/reattach is the known
  recovery when guest reader ownership goes stale), then reproduce the exact
  sequence of baseline and repeated SHA measurements. Do not use
  `pcsc_scan`; it previously left an orphan at 99.2% CPU.

## Latest continuation — 2026-10-17

- The replugged Pico enumerated as `2e8a:0009` in the host-access context.
  `./tools/build arm` passed, then `./tools/flash build/arm/pico2_choupi.uf2`
  passed after its CDC fallback entered BOOTSEL. This flash was part of the
  explicitly requested ARM hardware investigation; no RISC-V build was run.
- Before flashing, raw CCID SELECT returned `9000`, but GETVERSION returned
  `F503`. The serial trace identified `VPCD:APPLET_PROCESS status=F503` at
  opcode `0x93`. After flashing the rebuilt ARM image, direct CCID probes
  passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, SHA-256 support,
  SHA-256 prepare/doFinal, and the JCAlgTest 256-byte / 50-call static SHA
  workload (`AA9000`, 0.718038 s). Transcript:
  `artifacts/logs/jcalgtest-physical-resume-20261016.log`.
- Bounded post-test serial output includes `TEST:PASS choupi_runtime` and no
  `FAULT`, but repeatedly prints `WATCHDOG_RECOVERY cause=other` with a retained
  marker. Determine whether this is stale diagnostic state; do not infer a
  runtime reset from the marker alone. Capture:
  `artifacts/logs/monitor-arm-physical-jcalgtest-20261017.log`.
- `pcscd.service` is inactive. Bounded `pcsc_scan -r` sees only Virtual PCD
  readers, so the official Java client could not target the physical reader.
  The static APDU test reproduces its workload over raw CCID but is not an
  official client run. Do not use sudo; continue using direct CCID until PC/SC
  is available.
- Next: investigate the repeated diagnostic marker, then continue JCSystem
  gaps. In particular, available-memory and commit-capacity natives still
  return zero because this port does not expose capacity accounting. Later,
  rerun official v1.8.2 `ALG_PERFORMANCE_STATIC` through the named physical
  PC/SC reader when the service is active.

## Watchdog marker follow-up — 2026-10-17

- The repeat was caused by diagnostic state handling, not an observed runtime
  fault: `main_arm()` retained the RP2350 scratch marker after copying it and
  printed `watchdog_recovery_pending` on every one-second status interval.
  `cause=other` reflected the ordinary reboot path, not a confirmed watchdog
  reset.
- Updated `main_arm()` to consume the scratch marker after copying it to RAM
  and clear the pending flag after its one-time report. `./tools/build arm`
  and `./tools/flash build/arm/pico2_choupi.uf2` passed.
- An eight-second serial capture printed exactly one
  `WATCHDOG_RECOVERY cause=other`, eight `TEST:PASS choupi_runtime` reports,
  and no `TEST:FAIL` or `FAULT`. A post-flash physical CCID run again passed
  SELECT, GETVERSION, JCSystem info, SHA-256 support, and static SHA
  (`AA9000`, 0.725005 s).
- Evidence: `artifacts/logs/build-arm-watchdog-marker-20261017.log`,
  `artifacts/logs/flash-arm-watchdog-marker-20261017.log`,
  `artifacts/logs/monitor-arm-watchdog-marker-20261017.log`, and
  `artifacts/logs/jcalgtest-physical-watchdog-fix-20261017.log`.
- Implementation/evidence commit: `0f5091a`.
- Next: run the official JCAlgTest client when PC/SC exposes the physical
  reader, then continue JCSystem capacity accounting. No RISC-V build/test.

## JCSystem memory accounting investigation — 2026-10-17

- Tried to expose `linked_list_allocator::Heap::free()` from
  `choupi/os/src/allocator/lib.rs` and use it in the JNI natives. The ARM link
  failed with `undefined reference to jcvm_current_heap_free_bytes` because
  the active RP2350 target compiles the platform-independent C++ VM but does
  not link `choupi-os` yet; root `CMakeLists.txt` explicitly documents Rust OS
  integration as later work.
- Reverted only this experiment's changes. The nested allocator file is back
  to its prior state, and `./tools/build arm` passes again. No flash occurred
  for this experiment. Keep the current zero returns until the active Pico
  allocator and filesystem expose measurable capacities; do not use the
  unrelated Rust heap or guess a number.
- Evidence logs:
  `artifacts/logs/build-arm-jcsystem-memory-20261017.log` (expected link
  failure) and `artifacts/logs/build-arm-jcsystem-memory-reverted-20261017.log`
  (pass).
- Investigation/evidence commit: `52d12fa`.
- Next: inspect the allocator actually linked by the Pico target and determine
  a correct bounded transient-memory query; persistent capacity and transaction
  commit capacity still need separate accounting. PC/SC remains inactive, so
  the official JCAlgTest client is still unavailable.

## Latest continuation — 2026-10-16

- User replugged the Pico, but this process still cannot access USB: both
  `lsusb -d 2e8a:0009` and `lsusb -d 2e8a:000f` report libusb `-99`; bounded
  `pcsc_scan -r` reports that the PC/SC service is unavailable. No flash or
  physical test was attempted.
- Corrected the earlier persistent-field status change. The OS FFI documents
  `fs_exists()` as returning 1 when a tag exists, so `JC_Instance::getFields()`
  must use `!= 0`. The prior `== 0` interpretation was wrong. With the fix,
  fresh JCAlgTest SELECT succeeds instead of returning `6F00` while restoring
  a missing field.
- `./tools/build arm` and `./tools/build-pc` passed. The focused JCSystem/SHA
  VPCD test passed `TEST:SUMMARY pass=12 fail=0 elapsed_s=44.119`.
- Two fresh VPCD emulator processes sharing one flash image completed
  `TEST:SUMMARY pass=22 fail=0`. Both passed SELECT, GETVERSION `1.8.2_jc305`,
  JCSystem, SHA-256 support and doFinal, and static SHA. The static APDU used
  JCAlgTest's 256-byte / 50-call settings and returned `AA9000` in `36.939 s`
  and `37.244 s`. These are host emulator measurements, not Pico results.
  RSA-SHA1 reported unsupported reason `03`, as expected.
- Full transcript: `artifacts/logs/vpcd-persistence-corrected-transcript-20261016.log`;
  emulator log: `artifacts/logs/vpcd-persistence-corrected-20261016.log`.
- Twelve additional repository regression wrappers passed (APDU utilities and
  parsing, VPCD protocol/session, GP DELETE/registry, install status,
  persistent transactions, RP2350 filesystem overlay, JCArray, JCVMArray, and
  ROM mask). `tools/test-jcarray` previously invoked `c++` directly and its
  handwritten stub missed the new `fs_read_1b_at` dependency; it now builds
  through `tools/env cmake`, with test-owned CMake/stub files. Short APDU tests
  now link the same correctly typed filesystem read ABI.
- `pcscd` and the vsmartcard VPCD driver packages are installed, but this shell
  cannot write the read-only `/run/pcscd` socket. A user/mount namespace was
  denied by the host. The official Java client still cannot run through PC/SC;
  keep using direct bounded VPCD APDU tests until the service is available.
- Next: restore guest USB/libusb and PC/SC visibility, then use the ARM build,
  flash through the wrapper, capture finite serial output, and run the named
  physical reader through JCAlgTest 1.8.2. Keep unsupported algorithms as
  expected; compare physical SHA static timing with the host result.

## Latest continuation — 2026-10-16

- User replugged the Pico, so the bounded physical-access checks were rerun.
  `./tools/doctor` completed successfully for host prerequisites, but both RP2350
  `lsusb` checks reported `unable to initialize libusb: -99`. PC/SC reported
  that its service was unavailable and emitted repeated invalid-context errors;
  the scan was interrupted immediately. No physical reader, BOOTSEL, or flash
  result was obtained. Do not repeat USB checks until guest USB passthrough and
  libusb work.
- `./tools/build arm` passed (`build/arm/pico2_choupi.uf2`), and
  `./tools/build-pc` passed. RISC-V remains out of scope.
- A fresh JCSystem VPCD attempt with a 60-second connection bound failed before
  the socket connected. The emulator log reaches the normal Java JCRE startup;
  its last recorded interpreter trace is a repeated bytecode loop around count
  25,961. Corrected the working-tree `fs_exists()` status check for persistent
  fields (`0` means present), but that does not resolve the JCRE startup delay.
- Tested skipping Java JCRE startup in VPCD mode, then reverted it: direct
  native storage initialization runs under the wrong context, and native CAP
  installation then throws. Preserve the normal JCRE startup path until a
  supported faster initialization is designed.
- Fresh evidence: `artifacts/logs/build-arm-fs-exists-status-fix-20261016.log`,
  `artifacts/logs/build-pc-fs-exists-status-fix-20261016.log`,
  `artifacts/logs/jcsystem-transaction-fs-exists-status-fix-20261016.log`,
  `artifacts/logs/jcsystem-transaction-restored-startup-20261016.log`.
- Evidence commit: `cb6f8aa`.
- Next: make the host VPCD JCRE startup reliably complete without a busy wait;
  keep time bounds to avoid VM saturation. Once guest USB enumerates the Pico,
  build ARM, flash through the wrapper, capture finite serial output, then run
  SELECT/GETVERSION/JCSystem before SHA-256 and named-reader
  `ALG_PERFORMANCE_STATIC`. Do not run RISC-V builds/tests.

## JC305 delayed-init security call trace — 2026-10-16

- The previous physical trace shows `AlgSupportTest` is stored after its three
  short transient arrays are allocated, then `CryptoException` reason 3
  (`NO_SUCH_ALGORITHM`) is thrown before delayed initialization completes.
  Source order identifies the next object as `AlgKeyHarvest`; its constructor
  calls `RandomData.getInstance(RandomData.ALG_SECURE_RANDOM)`, where the
  requested algorithm is 2. This is a hypothesis for the throw site until the
  firmware trace confirms it.
- Added bounded `CHOUPI:JC305_SECURITY_CALL` diagnostics in
  `choupi/src/jc_bytecodes/bc_invoke.cpp` for imported security static calls.
  Both `./tools/build arm` and `./tools/build riscv` pass. The ARM diagnostic
  image flashed successfully (`FLASH:PASS`). The physical device disappeared
  during re-enumeration immediately afterward; a single bounded visibility
  check found no device. Do not repeat USB probes until the Pico reappears.
- Compared all 34 JC305U3 security export classes by name against the generated
  CHOUPI security export. Restored the working-tree remap to the exact
  class-token correspondence: JC305 `RandomData` token `0x0E` maps to CHOUPI
  token `0x1C`; the prior worktree value `0x1E` pointed at
  `MessageDigest$OneShot`. Both architecture builds pass with the restored map.
  Evidence commit: `2ff11eb` (documentation and captures); source tracing is
  still mixed with earlier uncommitted changes in the worktree.
- Current user scope is ARM Cortex-M33 only. Do not build or test RISC-V even
  though repository AGENTS.md lists both architectures; build ARM before any
  new flash. `TODO.md` now reflects this scope and the latest physical APDU
  blocker.
- The pre-flash raw CCID probe still reproduced SELECT `9000`, with GETVERSION,
  JCSystem, and SHA support returning `F503`. The post-flash APDU probe did not
  run because the device was not visible. No digest or static performance
  measurement was made.
- Next: when USB is available, capture finite serial output, run bounded raw
  SELECT/GETVERSION/JCSystem probes, and inspect the resolved class/method
  tokens before modifying compatibility mappings. Keep unsupported algorithms
  as expected results.
- Evidence: `artifacts/logs/physical-jcalgtest-readonly-probe-20261016.log`,
  `artifacts/logs/physical-jcalgtest-security-call-trace-probe-20261016.log`,
  `artifacts/logs/monitor-arm-after-replug-apdu-20261016.log`.

## Physical applet delayed-initialization failure — 2026-10-16

- After replug, escalated host-access checks enumerate the Pico 2 CHOUPI reader
  at bus 001 device 029 (`2e8a:0009`) and `picotool info -a` identifies running
  mode. Direct raw CCID interface access works; bounded PC/SC still lists only
  Virtual PCD 00 00/01. No sudo, build, or flash was used.
- The current applet instance is selected successfully (`9000`), and serial
  confirms its persistent applet root restores (`RESTORE_READ_OK`,
  `SELECT restore=0`). Every first process command returns `F503`: GETVERSION
  (`B0E1`), JCSystem info (`B073`), and SHA-256 support (`B075`). The finite
  serial trace shows failure during `performDelayedAllocations()` after the
  applet fields for its large transient buffers are stored; it occurs before
  command dispatch. `TEST:PASS choupi_runtime` remains only the firmware smoke
  test.
- Exact evidence: `artifacts/logs/monitor-arm-current-apdu-f503-20261016.log`
  and raw APDU output in the active session. The corrected helper now includes
  the three-byte GP privilege field, matching historical INSTALL data, but
  reinstalling a fresh JCAlgTest instance still yielded `F503`.
- Next: identify the exception thrown in delayed allocations, focusing first
  on transient-array/helper-object construction and Java Card exception
  dispatch. Keep unsupported algorithms as expected results; do not add
  algorithms to suppress this initialization failure. Once GETVERSION and
  JCSystem pass on Pico, run SHA known-answer and named-reader static
  performance checks.

## Physical applet restore failure — 2026-10-16

- After the user replugged the Pico, the host-access context can see running
  CHOUPI (`2e8a:0009`, bus 001 device 013). The restricted shell still has no
  usbfs. `pcscd` is active but `pcsc_scan -r` lists only Virtual PCD 00 00/01;
  the device node is `root:plugdev` mode 0660 while pcscd runs as `pcscd`. No
  sudo command was run.
- Direct, sequence-checked libusb CCID access is available as user `claude`.
  Power on returned ATR `3B800181`. SELECT of JCAlgTest AID returned `6F00`;
  GETVERSION returned `6D00` because SELECT failed.
- Serial proves the AID/CAP metadata resolves (installed=1, package 63,
  class 0x0094), then instance restore fails: `RESTORE_STORAGE status=1
  length=0`, `RESTORE_READ_FAIL id=5 tag=03003F94FF`, and `SELECT restore=1`.
  This is a missing persistent applet root record. The periodic
  `TEST:PASS choupi_runtime` is only the firmware startup smoke test.
- No static benchmark, applet APDU test, source change, build, flash, or sudo
  operation was made. Full capture: `artifacts/logs/physical-ccid-restore-diagnostic-20261016.log`.
- Evidence commit: `74fcf20`.
- Next: because replugging changed the USB node, the user must rerun the already
  installed permission helper with `sudo -n
  /usr/local/sbin/pico2-pcsc-reader`; root `AGENTS.md` prohibits autonomous
  sudo. Then confirm `pcsc_scan -r` lists the Pico, repair its stale CAP/applet
  persistent state through the supported install/delete flow, and require
  SELECT plus GETVERSION to pass before rerunning JCSystem and SHA-256 probes.
  Afterward run the official v1.8.2 `ALG_PERFORMANCE_STATIC` client on the
  named Pico reader; report unsupported algorithms as expected.

## Host MPU ptrace denial no longer hangs — 2026-10-16

- The host VPCD regression's fresh-build timeout remains unresolved: the
  untraced `./tools/test-jcsystem-transaction-applet --timeout 120` did not
  reach VPCD. A separate diagnostic experiment showed that running the host
  emulator under `strace` makes its nested `PTRACE_ATTACH` fail with `EPERM`.
  The OS ignored that error and blocked in `waitpid`, which could hang the VM.
- Updated `choupi/os/src/arch/host/emulator.rs` to check `PTRACE_ATTACH`,
  `PTRACE_SETOPTIONS`, and `PTRACE_CONT`; on setup error it kills/reaps the
  child and exits with a clear message. Nested trace now exits promptly with
  `ERROR: host MPU emulator PTRACE_ATTACH failed: Operation not permitted`;
  `waitpid` confirms the SIGKILLed child was reaped. The PC build passes.
- Nested OS commit: `f419dde`; root evidence commit: `d4ab895`. Logs:
  `artifacts/logs/build-pc-ptrace-failfast-20261016.log` and
  `artifacts/logs/host-jcre-ptrace-after-fix-20261016.15050` through `.15053`.
- This fixes the ptrace-denial hang only. The direct host VPCD timeout did not
  show ptrace denial and remains an open issue; do not use the nested-strace
  error as an explanation for that separate timeout.
- Next: diagnose why an untraced host emulator stays in its initial JCRE
  runtime, then rerun JCSystem and broad applet tests. Physical PC/SC remains
  blocked by the missing `pcscd` ACL; no sudo was run.

## Fresh PC build VPCD startup timeout — 2026-10-16

- `./tools/build-pc` passed against the current source, linking the PC emulator
  and regenerating the ROM mask. A fresh bounded run of
  `./tools/test-jcsystem-transaction-applet --timeout 120` failed with
  `timed out waiting for emulator VPCD connect`; the client sent no APDUs.
- Stage markers locate the stall in `starting_jcre()` while running the initial
  `runtime(STARTING_JAVACARD_PACKAGE=0x12, class=0, method=1)`. The callback
  did not return to install the probe CAP or start VPCD transport within 120
  seconds. The log includes `CHOUPI:RESTORE_READ_FAIL id=5 tag=021302`. Further
  bounded diagnostics made no progress marker at 5,000 interpreted bytecodes;
  temporary diagnostic source edits were removed and the PC target rebuilt.
- The archived prior 11/11 host VPCD run used a binary built before the latest
  source edits and is therefore not current-source proof. Do not treat host
  applet execution as currently verified. The RP2350 startup pass is separate
  evidence and remains valid.
- Evidence commit: `e5e9efd`; logs:
  `artifacts/logs/build-pc-after-jcsystem-diagnostics-cleanup-20261016.log` and
  `artifacts/logs/jcsystem-continuation-regression-fresh-20261016.log`.
- Next: inspect and bound the initial host JCRE/GlobalPlatform singleton
  initialization, then rerun the JCSystem probe before the broad JCAlgTest
  regression. Avoid another long run until it reports stage/instruction
  progress.

## Array logical-size fix and latest RP2350 result — 2026-10-16

- Fixed `JC_Array::size()` so transient and ordinary Java Card arrays report
  logical element counts, dividing the backing byte length by the array entry
  width. Persistent arrays already take their logical count from the record
  header. The bug caused serialization of an `Object[]` with 72 elements to
  read element index 72 from a 72-element array and raise exception 25.
- Host regression: `artifacts/logs/host-vpcd-jc-array-logical-size-console-20261016.log`
  reports `TEST:SUMMARY pass=11 fail=0`; the 50-call static SHA-256 APDU took
  17.667 s on the PC interpreter (not Pico throughput). `./tools/build-pc` and
  `./tools/build arm` passed. The ARM image was flashed to the RP2350; bounded
  serial capture `artifacts/logs/monitor-arm-jc-array-runtime-followup-20261016.log`
  repeatedly reports `CHOUPI:STATUS sw=9000` and `TEST:PASS choupi_runtime`.
  This verifies startup only; no physical JCAlgTest APDU was run.
- Latest escalated check sees `Bus 001 Device 013: ID 2e8a:0009` and confirms
  `pcscd.service` is active. `pcsc_scan -r` lists only `Virtual PCD 00 00/01`.
  The USB node ACL grants `claude` read/write but not the `pcscd` service
  account; therefore Java/PCSC cannot select the Pico. This cycle did not run
  sudo, per repository instructions. The already installed access helper can
  restore the daemon ACL: `sudo -n /usr/local/sbin/pico2-pcsc-reader`.
- Implementation/evidence commit: `7817bd1`.
- Next: after the authorized PC/SC helper grants the daemon access, run a
  bounded `pcsc_scan -r`, select the exact Pico reader name with
  `./tools/run-jcalgtest --reader-name ... -op
  ALG_PERFORMANCE_STATIC`, and inspect both APDU status and SHA-256 results.
  If it is not available, continue host-side review; do not claim physical
  performance validation.

## RP2350 physical CHOUPI retry — 2026-10-16

- After replug, sysfs identified `/sys/bus/usb/devices/1-2` as RP2350
  BOOTSEL (`2e8a:000f`). Picotool required device access outside the restricted
  shell and confirmed RP2350, ARM, `boot type: bootsel`.
- `./tools/build arm` and `./tools/build riscv` both passed. `./tools/cycle
  choupi` rebuilt both, flashed `build/arm/pico2_choupi.uf2` successfully, and
  captured the board's serial output.
- Hardware runtime failed throughout the 18-second capture:
  `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`,
  `TEST:SUMMARY pass=0 fail=1`; CCID reported `stage=2`,
  `rx_events=0`, `tasks=0`. The USB device returned to running mode
  (`2e8a:0009`), but a bounded PC/SC scan exposed only `Virtual PCD 00 00/01`.
- The official `ALG_PERFORMANCE_STATIC` client could not be run on the physical
  card because PC/SC does not expose the Pico reader. No algorithm performance
  result is available. Investigate the startup `6F00` and PC/SC registration
  before claiming a hardware applet or JCAlgTest pass.
- Evidence: `artifacts/logs/physical-retry-build-arm-20261016.log`,
  `artifacts/logs/physical-retry-build-riscv-20261016.log`, and
  `artifacts/logs/cycle-choupi-20261016T154057Z.log`.

## Real-hardware availability recheck — 2026-10-16

- User asked for a real-hardware test. The guest could not access Pico USB: both
  `lsusb -d 2e8a:0009` and `lsusb -d 2e8a:000f` failed with
  `unable to initialize libusb: -99`; `timeout 5s ./tools/env picotool info -a`
  failed with `ERROR: Failed to initialise libUSB` (exit 250).
- `./tools/doctor` passed host prerequisites but explicitly reported that
  hardware status was not checked. A bounded `timeout 5s pcsc_scan -r` timed
  out while repeating `SCardEstablishContext: Service not available`; do not
  repeat until PC/SC is available.
- No build or flash was attempted: direct USB access is unavailable, and the
  build gate requires both architectures to pass before flashing. No physical
  validation is claimed. Resume with guest USB passthrough and verify a Pico
  USB ID plus `picotool info -a`, then build ARM and RISC-V before cycling.

## SHA-256 streaming probe and named-reader client check — 2026-10-16

- The focused VPCD applet probe now passes 12/12 assertions. It verifies
  SHA-256(`abc`) through `doFinal()` and through a method-local sequence of
  `update("abc")` followed by empty `doFinal()`; both return
  `BA7816BF01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD`.
- A field-retained probe returned the empty digest. Trace evidence showed the
  native bridge received a different state-array reference from the array
  field read by the Java implementation. That probe stored the returned
  `OneShot` implementation in an applet field, contrary to the class's
  documented temporary-object lifetime. It does not establish a general
  streaming failure. The current `getInstance()` factory returns `OneShot`,
  so a retainable digest implementation remains a design gap to resolve.
- `./tools/run-jcalgtest --reader-name 'Virtual PCD 00 00' -op
  ALG_PERFORMANCE_STATIC -fresh ...` exited 4: Java SmartcardIO saw no reader.
  `pcsc_scan -r` reported `SCardEstablishContext: Service not available` (and
  emitted a large repetitive error stream). The official client therefore
  could not run through PC/SC this session. Earlier official jCardSim timings
  remain below timer resolution; the host VPCD 50-call static SHA APDU returned
  `AA9000` in 16.853 s, which is emulator time, not Pico throughput.
- The PC build and local-lifetime VPCD probe pass. No firmware source change
  was retained, and no physical device was accessible; ARM/RISC-V builds and
  hardware flashing were not repeated in this diagnostic step.
- Evidence: `artifacts/logs/jcsystem-sha256-stream-method-local-vpcd-20261016.log`,
  `artifacts/logs/jcsystem-sha256-stream-method-local-console-20261016.log`,
  `artifacts/logs/jcalgtest-static-vpcd-readername-20261016.log`, and
  `artifacts/logs/jcsystem-sha256-native-stack-trace-vpcd-20261016.log`.

## SHA-256 doFinal known-answer and JCAlgTest static APDU — 2026-10-16

- The official simulator's SHA-256 timing remains unusable (0.05 ms/op average
  and -0.02 ms/op minimum). The official Java client on this VM cannot see the
  PC/SC readers, so no full-client static run or hardware result is available.
- Found and fixed the Java API `MessageDigest.OneShot.doFinal()` path: virtual
  self-dispatch to `update()` supplied a different transient state array than
  the subsequent native finalizer. `doFinal()` now retains one state reference
  and appends its input directly before finalizing. Java Card API nested commit:
  `be4fade`.
- A Java Card APDU probe checks SHA-256(`abc`) against
  `BA7816BF01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD`; the VPCD
  run passes 11/11 assertions. The broader JCAlgTest VPCD regression also
  passes 11/11. Its exact 256-byte/50-call static doFinal APDU returned
  `AA9000` in 16.853 s. This is PC interpreter/VPCD wall time, not Pico
  throughput; `AA` is the official applet's status marker, not digest data.
- ARM and RISC-V wrapper builds pass after the Java API change. No physical
  board flash or serial result was possible: neither Pico USB ID is visible,
  PC/SC is inactive, and libusb reports `-99` (see the physical-access record
  below).

## Java-level JCSystem transaction exception APDUs — 2026-10-16

- Added a small Java Card probe applet and repeatable host VPCD runner. It
  exercises begin/depth/commit, `TransactionException` reasons for nested
  begin and commit/abort outside a transaction, persistent value rollback on
  abort, and persistence after a later APDU. The current run passes 10/10
  assertions.
- Fixed the exception dispatcher’s handler range check for unwound caller
  frames: the saved caller PC points at the invoke opcode, and the CAP range
  applies from the following bytecode boundary. The focused Java APDU probe
  now catches the expected Java exception reasons rather than returning
  `6F00`.
- `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` pass. The
  broader JCAlgTest VPCD regression passes 11/11. No Pico was flashed and no
  physical result is claimed. The official `ALG_PERFORMANCE_STATIC` command
  with `--reader-name 'Virtual PCD 00 00'` cannot find a Java SmartcardIO
  reader in this execution context; retry when that reader is exposed.
- Current performance limits: the 50-call SHA-256 APDU takes 19.679 s on the
  PC interpreter/VPCD in the post-fix run. This is not Pico throughput. The
  static simulator's sub-millisecond SHA numbers are timer-resolution noise.
- Next: inspect and implement truthful `getMaxCommitCapacity()` and
  `getUnusedCommitCapacity()` behavior, keeping RAM/flash limits explicit;
  then rerun the official named-reader static operation once PC/SC is
  available. Physical test still requires both architectures to pass and a
  finite serial capture after flashing.
- Physical access was rechecked: `lsusb -d 2e8a:0009` and `lsusb -d
  2e8a:000f` return no device; bounded `pcsc_scan -r` reports
  `SCardEstablishContext: Service not available`, and libusb initialization
  reports `-99`. No flash attempt was made. Log:
  `artifacts/logs/pcsc-physical-access-check-20261016.log` (SHA-256
  `82111d1919e6e1d523fae3c5ffe5bd7709d3532dd9e9051381f623e1f69ba7af`). The
  raw bounded stderr flood was 260,631,857 bytes / 7,044,104 lines; the checked
  in artifact keeps the diagnostic summary and byte/line count.

## JCAlgTest static SHA-256 host timing — 2026-10-16

- Gated the noisy `PC_FS:WRITE` diagnostic behind `DEBUG` for PC builds. The
  previous release VPCD run emitted 665 per-write lines. `./tools/build-pc`,
  `./tools/build arm`, and `./tools/build riscv` pass.
- The fresh bounded VPCD applet regression passes 11/11. Its 50-call SHA-256
  static APDU returns `AA9000` in 15.578 s; this is host emulator time, not
  Pico throughput or a digest known-answer check. The release log has no
  `PC_FS:WRITE` entries.
- Official jCardSim `ALG_PERFORMANCE_STATIC` reports 0.05 ms/op for SHA-256
  `doFinal`, with a negative minimum, so its timer resolution cannot support
  that sub-millisecond claim. A prior official VPCD client run lost the card
  during SHA-256 measurement. The current PC/SC service is unavailable,
  SmartcardIO has no readers, and libusb access fails; no hardware was used.
- Next: once PC/SC is restored, rerun the official v1.8.2 static operation
  using the exact `--reader-name` for the intended CHOUPI reader. Analyze the
  SHA-256 row separately and check a known-answer digest; do not call the host
  VPCD result a physical performance measurement. Then resume the JCSystem
  transaction Java-exception APDU test and commit-capacity design.

## JCSystem transaction flush and RP2350 snapshot recovery — 2026-10-16

- The ARM filesystem overlay now defers `fs_sync()` while a Java Card
  persistent-write transaction is active. `commitTransaction()` disables the
  deferral and flushes the new overlay snapshot before clearing rollback data;
  `abortTransaction()` restores the saved values, disables deferral, and
  flushes the restored snapshot. A failed commit attempts the same rollback and
  reports failure.
- `./tools/test-rp2350-fs-overlay` passes, including 1,000 deterministic
  pseudo-random byte mutation cut-points while committing two records. Each
  simulated reboot restored the entire old pair, with no mixed pair. The
  focused transaction test passes 30 checks, including commit/abort flush and
  failed-flush rollback. `./tools/build arm` passes both images, and a fresh
  PC VPCD applet pass is `TEST:SUMMARY pass=11 fail=0`.
- This is host fault-injection evidence, not a physical power-cut test. The
  storage snapshot backend is now transaction-aware on RP2350, but the Rust OS
  filesystem path still lacks its designed multi-record meta-block. Commit
  capacity and Java-level `TransactionException` APDU coverage also remain.
- Next: test Java transaction reason codes through an applet APDU, then
  inspect transaction capacity and determine how the OS meta-block requirement
  relates to the RP2350 snapshot backend. Do not claim physical verification.

## Host filesystem interrupted-write recovery — 2026-10-16

- Added test-only host flash failure injection at each programmed 32-bit word
  and an FS regression that interrupts every word of a replacement record,
  reconstructs the filesystem, and checks that the complete old or new value
  survives. Added RAII guards so flash and sector locks are restored if an
  operation unwinds. OS subrepository commit: `9a2a25f`.
- Targeted host FS tests pass: 19 passed, 0 failed, 2 ignored. The test wrapper
  now compiles with the installed Rust 1.99 toolchain after removing obsolete
  plugin usage, replacing legacy inline asm fences, and allowing bindgen's
  null-based generated layout assertions. The full OS suite still stalls on
  MPU emulator tests that require ptrace in this sandbox; it was interrupted
  before completion. `./tools/build arm` passes both images.
- No flash, RISC-V build, PC/SC, or physical hardware test was run. This proves
  only single-record replacement recovery between host-mode word writes. It
  does not meet WP3's multi-record transaction/anti-tearing requirement or its
  1,000 randomized power-cut criterion.
- Next: design and implement a persistent filesystem transaction meta-block
  for atomic multi-record updates, then extend injected power-cut coverage to
  transaction activation and repair before assessing the hardware power-cut
  rig.

## JCSystem transaction implementation — 2026-10-16

- The native begin/commit/abort methods now use the persistent-write snapshot
  backend. `getTransactionDepth()` reflects the active state, and the Java API
  wrappers turn native status codes into `TransactionException` reasons.
  `Util.arrayCopy`, `arrayFill`, and `setShort` were corrected to begin/commit
  when they own the transaction and validate arguments before beginning.
- The Java Card API submodule change is committed as `b757a0a`.
- `./tools/test-persistent-transaction` passed 24 checks for initial/inactive
  state, nested begin, commit, abort, rollback, and filesystem failures.
  `./tools/build arm` passed both images and generated the API CAP/ROM mask;
  `./tools/test-pc-vpcd-applet --passes 2` passed 22/22 checks on fresh host
  emulator processes. Its static SHA APDUs took 14.566 s and 13.442 s on the
  PC emulator; these are not Pico throughput.
- Transaction `getMaxCommitCapacity()` and `getUnusedCommitCapacity()` still
  report zero. Snapshot rollback is in-memory and not power-loss safe; Java
  exception reason behavior has not been exercised by an applet APDU.
- The ARM build log includes 49 existing Rust OS warnings and two existing
  packed-member C++ warnings; no new build errors occurred.
- Logs: `artifacts/logs/build-arm-jcsystem-transaction-20261016.log`,
  `artifacts/logs/persistent-transaction-jcsystem-20261016.log`,
  `artifacts/logs/host-vpcd-jcsystem-transaction-final-console-20261016.log`,
  and `artifacts/logs/host-vpcd-jcsystem-transaction-final-20261016.log`.
- Next: add an applet-level transaction test that catches the Java reason
  codes, then implement truthful commit-capacity accounting and power-loss
  recovery. Physical hardware verification remains outstanding; PC/SC is
  unavailable to Java SmartcardIO in this execution context.

## GP DELETE dispatch and lifecycle state — 2026-10-16

- Added authenticated GlobalPlatform `INS=E4` dispatch using the strict
  single-AID parser. Malformed/unsupported encodings return `6A80`; unknown
  objects return `6A88`; deleting an installed package without P2's related
  objects bit returns `6985`; persistence failures return `6581`.
- Applet deletion erases its root and field records under a persistent write
  transaction and stores a restart-safe marker. Package deletion also clears
  package static roots, stores a package tombstone, commits the FS snapshot, and
  erases the reserved CAP slot. Marker reads use static tag type `02` and
  reserved package id `FF`, which the Java Card OS filesystem permits.
  Successful CAP LOAD/INSTALL clears stale deletion markers. GET STATUS consumes
  the restored loaded/installed state.
- `./tools/test-gp-delete` passed 11 parser checks; `./tools/build arm` passed
  both images; `./tools/build-pc` passed (the Rust OS emits 49 existing
  warnings). A clean rebuilt host VPCD pass completed 11/11 checks in 66.365 s,
  including SELECT, GETVERSION, JCSystem, support probes, SHA-256 doFinal, and
  the static SHA performance APDU. This verifies the normal host applet path,
  not the DELETE APDU.
- No SCP03-authenticated `E4` APDU, post-delete restart/GET STATUS, or physical
  hardware test has been run. Java SmartcardIO still exposes no PC/SC readers;
  no APDU reached the Pico and no flash was attempted.
- Next: create an authenticated card-manager regression for instance DELETE,
  package DELETE, wrong AID, and package dependency status; verify GET STATUS
  before and after a fresh restart. Then proceed to truthful GET DATA support.
- Implementation commit: `4f66267`.

## GP DELETE command parser — 2026-10-16

- Added `src/gp_delete.hpp` for the supported single AID-qualified DELETE
  command (`P1=00`, `P2=00` or `80`, exact `4F` AID TLV) and
  `tools/test-gp-delete` with 11 strict parser checks.
- `./tools/test-gp-delete` passed under `-Wall -Wextra -Werror`; `./tools/build
  arm` passed both ARM images. The helper is not yet called by the secure
  card-manager path. No APDU, deletion, restart, or hardware check occurred.
- Next: inspect flash metadata and object-store transaction semantics, integrate
  DELETE only when instance/package state remains accurate after restart and
  GET STATUS, then add an authenticated host APDU regression.

## Named JCAlgTest static performance selection — 2026-10-16

- The official launcher accepts either `--reader-name '<exact PC/SC name>'`
  or `--simulator`. A direct run against
  `Pico 2 CHOUPI CCID [JCAlgTest CCID]` exited 4 before launching JCAlgTest:
  Java SmartcardIO reported no visible readers, and no APDU was sent.
- The bundled jCardSim `ALG_PERFORMANCE_STATIC` run completed and produced a
  1,900-line CSV with 132 measured-operation sections. The tool session
  returned 130 after an interrupt, but the Java child kept running outside the
  session and wrote the complete client footer and CSV; do not repeat this
  workload just to recover its exit code. The report lists 290
  `NO_SUCH_ALGORITHM`, 204 `CANT_BE_MEASURED`, and 66 `ILLEGAL_VALUE` results.
  Treat these as jCardSim results, not CHOUPI support requirements.
- SHA APDUs returned `AA 9000`; jCardSim SHA-256 `doFinal` reported 0.02 ms/op
  average and -0.11 ms/op minimum, `update` 0.18 ms/op, and `reset` 0.03
  ms/op. The negative sample confirms timer/baseline noise. JCAlgTest exposes
  no digest bytes in this mode, so it does not verify SHA-256 correctness.
- No SHA firmware change is justified by simulator timings. Retry the named
  physical command only after Java lists a PC/SC reader; use a separate
  digest known-answer check to validate SHA-256 correctness.
- Evidence: `artifacts/logs/jcalgtest-static-pico2-reader-20261016.log`,
  `artifacts/logs/jcalgtest-static-simulator-20261016.log`, complete client log
  `artifacts/results/jcalgtest-static-simulator-20261016/ALGTEST_log_1792142853606.log`,
  and CSV in the same result directory.

## Diagnose RP2350 startup `6F00` — 2026-10-16

- The latest archived serial log (`artifacts/logs/arm-resume-boot-20261016.log`)
  shows repeated `CHOUPI:STATUS sw=6F00` and `TEST:FAIL choupi_runtime` after
  snapshot persistence was added. It contains no VM fault detail.
- Added `CHOUPI:VM_RUNTIME_FAILURE` output on ARM when the initial JCRE runtime
  returns a non-`9000` status. It reports status, exception ID, opcode, PC,
  stack depth, and opcode/depth history from the interpreter. `./tools/build arm`
  passed both images; no flash or physical test was run. RISC-V remains excluded
  by the user's target instruction, and the repository flash gate therefore
  remains unmet.
- Next: capture a finite serial log from an authorized current-image run and
  use the failure record to fix the boot-time exception. Do not treat a
  successful build as hardware validation.

## Current host JCAlgTest applet execution — 2026-10-16

- Rebuilt the current ARM target with `./tools/build arm`; both the smoke and
  CHOUPI UF2 images passed. The image build is not a flash or runtime check.
- Ran two fresh JCAlgTest 1.8.2 host VPCD emulator passes against the same
  persisted applet image. Each passed 11 checks:
  ATR, applet SELECT, GETVERSION (`1.8.2_jc305`), JCSystem info, supported
  algorithm probes, SHA-256 prepare/doFinal, the 50-call static SHA-256 APDU,
  and AES, RSA, random, and CRC16 support probes. Pass times were 66.980 s and
  68.283 s; the static SHA APDU took 15.510 s and 15.599 s. These are
  host-emulator timings, not Pico throughput. No digest known-answer check was
  performed.
- This closes the earlier host VPCD `process(APDU)` failure for these specific
  applet calls. It does not establish physical power-cycle restoration,
  complete lifecycle callback coverage, or the official full supported-
  algorithms CSV on CHOUPI.
- Evidence: `artifacts/logs/pc-vpcd-persistence-run-20261016.log` (SHA-256
  `9c1640df4653f9ebfbee683cd546a3b512d447d4c4b54ccfbf9cdaff44196188`),
  `artifacts/logs/pc-vpcd-persistence-pass-20261016.log` (SHA-256
  `18aff547718ebe03b4760a8bef346151ab4470eae1e73b671b2fda49284dac5c`), and
  `artifacts/logs/build-arm-continuation-audit-20261016.log` (SHA-256
  `56a55b6c1a15d4b1442d1ca4a734d69ebc4b66fdb3a3de3acd9b51305e825fe7`).
- Next: add a digest known-answer route and keep pursuing RP2350 lifecycle
  persistence. Do not flash while the RISC-V build gate is unsatisfied under
  repository instructions and the user’s ARM-only scope.

## JCAlgTest static SHA-256 target access — 2026-10-16

- `tools/run-jcalgtest` already supports direct target selection for JCAlgTest
  1.8.2: use `--reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID]'` for the
  named physical reader, or `--simulator` for bundled jCardSim. The reader
  helper matches the exact Java SmartcardIO name and passes its current
  one-based index to the official client.
- Retried the physical command with `-op ALG_PERFORMANCE_STATIC`; it exited 4
  because Java SmartcardIO listed no PC/SC readers. A bounded `pcsc_scan -r`
  showed `SCardEstablishContext: Service not available` and emitted an
  invalid-handle flood. Do not retry until PC/SC is restored; no APDU was sent.
- The archived jCardSim static run completed its SHA-256 section (250
  iterations of 256-byte input) but reported 0.05 ms/op average and -0.02
  ms/op minimum from raw samples of 14.20, 0.20, -0.80, 0.20, and -0.80 ms.
  This is timer-resolution noise. A separate CHOUPI PC-emulator test returned
  `AA 9000` for its 50-call SHA-256 APDU in 17.389 s, which is not Pico
  throughput. The official applet only reports operation status, not digest
  bytes; no independent digest known-answer result is available here.
- No firmware change is justified by these measurements. After Java sees the
  real reader, rerun static mode with the command above and verify digest bytes
  independently before evaluating SHA-256 correctness/performance.

## GET STATUS category and AID-search behavior — 2026-10-16

- Implemented the four supported GET STATUS categories: ISD (`P1=80`),
  applications (`40`), executable load files (`20`), and load files with
  executable modules (`10`). Responses use installed/loaded `CapLoadSession`
  metadata; application entries identify the load-file AID with `C4`.
- Search parsing requires a leading `4F` AID qualifier, supports wildcard
  `4F00`, exact AIDs, RID-prefix matching, and skips later well-formed
  qualifiers for P1 `40`, `20`, and `10`. Per GlobalPlatform 2.2.1, P1 `80`
  returns only the ISD and ignores the search qualifier; it no longer attempts
  to parse or filter that data. P2 first/next requests have per-session
  validation: GET NEXT requires a prior first/all request in the same session;
  P1 `80` cannot be continued, while other supported P1 values require their
  leading `4F` AID search on the next request. The current single-package
  registry returns no further record after its first response.
- The platform-independent first/all response builder now exercises all four
  P1 categories, exact response bytes, wildcard search, malformed search,
  empty results, and unsupported categories. Continuation validation is also
  covered for no prior query, ISD-only queries, valid AID search, missing AID,
  and unsupported P1. `./tools/test-gp-registry` passed 44 checks and
  `./tools/build arm` passed
  both ARM images. No hardware/APDU test was run; RISC-V remains intentionally
  out of scope, so the flash gate is not satisfied.
- Exact ISD E3 bytes and capacity errors are covered in the focused test. The
  P1 `80` rule follows GP Card Specification 2.2.1 section 11.4.2.1.
- Commits: `5efa359` (response behavior tests), `9c81875` (ISD correction),
  `7b55114` (implementation), `7970700` (remove stale hardcoded package AID),
  and `3e0c753` (TODO/progress/handoff). The package AID is read from the
  loaded CAP metadata.
- A bounded `timeout 6s pcsc_scan -r` attempt found
  `SCardEstablishContext: Service not available` and flooded stderr with
  `SCardReleaseContext: Invalid handle`; do not repeat until pcscd access is
  restored. No card command or flash was sent.
- Next: add host-level APDU coverage for GET STATUS query/error paths, then
  investigate truthful GET DATA responses and transactional GP DELETE.
- Continuation validation source commit: `b9810e7`.
- Validation logs: `artifacts/logs/gp-status-session-test-20261016.log` (SHA-256
  `32b6f462fe2bb2326002c121bee12e23861c3f7d504ab118c06efa91cd5e72ab`) and
  `artifacts/logs/build-arm-gp-status-session-20261016.log` (SHA-256
  `827b202dade3226dd02f80824a128a57638cdbc2f36fcce39a38f53d9f8bfc41`).

## Correct GET STATUS package AID tag — 2026-10-16

- Corrected the applet registry E3 encoder to return the Executable Load File
  (package) AID under `C4`. The previous `CC` tag denotes the associated
  Security Domain AID, so it misrepresented the JCAlgTest package as an SD.
- The GP Card Specification 2.2.1 registry data table identifies `C4` as the
  application's Executable Load File AID and `CC` as the associated Security
  Domain AID. The exact-byte regression now expects `C4`.
- `./tools/test-gp-registry` passed 7 checks; `./tools/build arm` passed both
  ARM images. No hardware test was run. DELETE/GET DATA and end-to-end
  card-manager validation remain outstanding.
- Next: add host-level APDU coverage, then assess truthful Card Data GET DATA
  support without inventing device or CPLC values.

## GET STATUS registry-entry encoding coverage — 2026-10-16

- Extracted the installed-applet E3 entry encoder used by the CCID GET STATUS
  response into `src/gp_registry.hpp`. The encoded entry is unchanged; for the
  supplied JCAlgTest AID it is 32 bytes with a 30-byte E3 value. The helper
  checks AID lengths and destination capacity before writing.
- `./tools/test-gp-registry` passed seven exact-byte, outer-length, capacity,
  and invalid-AID checks. `./tools/build arm` passed both the ARM smoke image
  and CHOUPI firmware image. No RISC-V build or flash was performed. No
  hardware GET STATUS APDU was sent because PC/SC is not available to this
  execution context; the other architecture is also intentionally out of
  scope, so the AGENTS.md flash gate was not met.
- Existing unrelated dirty hunks in `src/ccid_device.cpp` were preserved and
  excluded from the staged implementation.
- Next: continue GP DELETE / GET DATA and accurate package/applet lifecycle
  behavior, with physical verification only after host reader visibility and
  the required build gate are available.

## Official JCAlgTest reader selection launcher — 2026-10-16

- Added `tools/run-jcalgtest` for JCAlgTest 1.8.2. Use
  `tools/run-jcalgtest --reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID]'`
  (plus normal client options) to resolve an exact Java SmartcardIO PC/SC
  reader name and feed its current index to the official client. The selected
  name becomes report `-cardname` unless overridden. Use `--simulator` to
  select bundled jCardSim explicitly.
- The simulator route ran the official client with
  `-op ALG_SUPPORT_EXTENDED -fresh` and returned 0, producing a 4,595-line CSV.
  Its log contains jCardSim `NotImplementedException`/operation failures for
  algorithms that simulator does not implement; this validates launcher mode
  selection, not a clean CHOUPI algorithm comparison.
- The exact-name helper compiled and failed closed with exit 4 for an unknown
  name. Java SmartcardIO listed no readers in this execution context, so named
  PC/SC selection was not exercised against a real or virtual reader. No token
  access, APDU, or flash occurred.
- Re-ran the official `ALG_PERFORMANCE_STATIC` client via `--simulator`. It
  completed the SHA-256 section but the four-minute wrapper timeout interrupted
  the later RSA sweep (exit 124). Simulator SHA-256 reported 0.05 ms/op average
  over 250 × 256-byte operations, with raw samples yielding -0.02 ms/op minimum;
  this is timer-resolution noise, not useful throughput. Earlier direct
  CHOUPI PC VPCD coverage returned `AA 9000` for its 50-call SHA-256 static
  APDU in 15.676 s host-emulator time. Neither result measures Pico hardware.
- The prior official PC/SC static attempt on VPCD reached SHA-256 and received
  `AA 9000` for baseline samples, then the PC/SC card was removed before the
  measured samples completed. SHA digest bytes have not been known-answer
  checked. Do not treat expected unsupported algorithms as failures.
- Evidence: `artifacts/logs/jcalgtest-reader-selector-simulator-20261016.log`
  (SHA-256 `c68e2bc083e95af186c7032c958969ab6ba9e034af6d439fdbbb279b51aafd25`)
  and `artifacts/results/jcalgtest-reader-selector-simulator-20261016/`.
- Static run evidence: `artifacts/logs/jcalgtest-reader-selector-static-simulator-20261016.log`
  (SHA-256 `9e20a5e357b29b258d1d192ddb3f6cecb25bf9a60ca235f203ef09692eca31a0`)
  and `artifacts/results/jcalgtest-reader-selector-static-simulator-20261016/`.
- Next: once Java SmartcardIO can see `Virtual PCD 00 00` or the Pico CCID
  reader, run the launcher with that exact name and `-op ALG_PERFORMANCE_STATIC`.
  On the Pico, compare SHA-256 latency after the PC/SC run completes; known-answer
  digest validation remains separate.

## Parameterized JCAlgTest install regression — 2026-10-16

- Extended `tools/tests/pc_vpcd_applet_test.py` with `--install-params`. When
  set, it asks the PC emulator to assert the actual Java install-entry locals:
  local 0 references a two-byte array containing `C9 00`, local 1 is offset 0,
  and local 2 is signed `bLength` 2. Mismatch returns `6F00` and fails the test.
- The VM logged `VPCD:INSTALL_VM_ARGS status=PASS offset=0 length=2`, then
  installed the applet and completed a fresh VM install using `C9 00`.
- The complete one-pass regression passed 11 checks, including JCAlgTest
  `1.8.2_jc305` GETVERSION, JCSystem info, SHA-256 `doFinal`, and the static
  SHA-256 APDU with 256-byte input × 50 calls (`AA 9000`, 15.676 s). This is PC
  emulator time, not device throughput.
- This directly verifies the Java method-entry array contents and argument
  values before applet execution. JCAlgTest ignores the field at this length;
  the assertion therefore checks the VM entry frame rather than applet-side
  reads.
- `./tools/build-pc` and `./tools/build arm` passed. The host build emitted 49
  existing Rust warnings and two packed-member C++ warnings. No RISC-V build or
  hardware test was run.
- Evidence: `artifacts/logs/build-pc-install-args-assert-20261016.log`,
  `artifacts/logs/build-arm-install-args-assert-20261016.log`,
  `artifacts/logs/vpcd-install-params-vmcheck-test-20261016.log`, and
  `artifacts/logs/vpcd-install-params-vmcheck-20261016.log`.
- Next: continue the JCVM lifecycle golden tests and remaining card-manager
  acceptance criteria.

## JCAlgTest static SHA-256 follow-up — 2026-10-16

- The current SHA-256 implementation passes the focused PC VPCD regression,
  including the complete 50-call static performance APDU: `AA 9000` in
  17.389 s wall time. This is host-emulator timing, not Pico throughput. The
  official client’s earlier PC/SC run got through setup and five measurement
  samples, but the virtual card was then removed and the run did not complete.
- The JCAlgTest 1.8.2 CLI has no reader-name option. `-cardname` sets report
  metadata; `-simulator` chooses the bundled jCardSim simulator. For a PC/SC
  reader, the client lists readers and accepts a numeric index. Select by
  matching the requested reader name in that list, then feed its index.
- Current access is unavailable: `pcsc_scan -r` reports
  `SCardEstablishContext: Service not available`, while `lsusb` cannot
  initialize libusb (`-99`). No real board run or fresh official-client run
  was possible. Do not try to flash until USB/PCSC access returns.
- `./tools/build arm` passes on the current source. Digest bytes still need a
  known-answer check; JCAlgTest only reports the `AA` operation marker.
- Next: restore PC/SC, choose the physical reader by its listed name/index,
  rerun `ALG_PERFORMANCE_STATIC`, and add a digest known-answer probe.
- Evidence: `artifacts/logs/jcalgtest-sha-bridge-class-test-20261016.log`,
  `artifacts/results/jcalgtest-static-vpcd-20261016/ALGTEST_log_1792133041312.log`,
  and `artifacts/logs/build-arm-jcalgtest-static-reader-20261016.log`.

## Current JCAlgTest static SHA-256 checkpoint — 2026-10-16

- Implemented native transient-buffer update/final/reset operations for SHA-256. The buffered length occupies the last two bytes of a 2048-byte CLEAR_ON_RESET array; the maximum input per digest is 2046 bytes. This avoids persistent scalar counter writes and makes each `doFinal()` reset state for the next call.
- The focused one-pass VPCD applet regression passes 11 checks. Its JCAlgTest 1.8.2 static SHA-256 configuration uses 256-byte input and 50 `doFinal()` calls; the operation returned `AA 9000` in 17.389 s wall time on the PC emulator. Both `./tools/build arm` and `./tools/build riscv` pass. This host timing does not measure Pico hardware throughput.
- The official Java client selected `Virtual PCD 00 00`, but PC/SC marked it `NO CARD`; the emulator could not connect to `127.0.0.1:35963` (`Connection refused`). `vsmartcard-vpcd` is not installed, and the current PC/SC list has only two virtual readers, with no physical Pico reader. The official static client therefore did not produce a result CSV. No real board flash or static hardware run occurred.
- JCAlgTest checks only the `AA` success marker for this operation. Digest output has not been compared to a known answer.
- Next: enable the VPCD socket endpoint and rerun `AlgTestJClient.jar -op ALG_PERFORMANCE_STATIC` selecting `Virtual PCD 00 00`; add a digest known-answer APDU; then run on hardware only after the physical Pico reader is exposed through PC/SC.
- Source commits: API `c6c4da6`; root implementation/test `fa8c05b`.
- Evidence: `artifacts/logs/jcalgtest-sha-bridge-class-test-20261016.log`, `artifacts/logs/jcalgtest-sha-bridge-class-emulator-20261016.log`, `artifacts/logs/jcalgtest-official-vpcd-client-20261016.log`, `artifacts/logs/build-arm-sha-bridge-20261016.log`, and `artifacts/logs/build-riscv-sha-bridge-20261016.log`.

## Compact persistent filesystem snapshots and current physical state — 2026-10-15

- Updated snapshot serialization to version 2: only occupied overlay entries
  and used overlay bytes are stored, with a CRC and generation header committed
  last. `./tools/test-rp2350-fs-overlay` passes all 19 checks; both
  `./tools/build arm` and `./tools/build riscv` pass. ARM UF2 SHA-256 is
  `5955047a9d5016c93a8a6bc5d1054b822bdee90167e64ae40552e76c0c40188d`.
- `./tools/flash build/arm/pico2_choupi.uf2` successfully flashed BOOTSEL and
  rebooted the Pico. `./tools/monitor --seconds 8` then showed repeated
  `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`, and
  `TEST:SUMMARY pass=0 fail=1`. The image is running, but startup is unhealthy.
- The USB device re-enumerated as CHOUPI CCID. A bounded `pcsc_scan -r` lists
  only `Virtual PCD 00 00` and `Virtual PCD 00 01`; the physical reader is not
  available to Java Card tools. The daemon refused `pcscd -H` from this user
  (`Operation not permitted`). The repository helper
  `/usr/local/sbin/pico2-pcsc-reader` must be run by the user to refresh the
  ACL and restart pcscd; the current AGENTS.md prohibits autonomous `sudo`.
- Archived startup capture:
  `artifacts/logs/compact-fs-flash-boot-20261015.log` (SHA-256
  `6c1a434a856af3f142c22452f8df5791275f6b5bb4f2aeab52a71129796c6a59`). It
  repeats the `6F00` boot failure eight times during the bounded 8-second run.
- The requested JCAlgTest 1.8.2 `OP_ALG_PERFORMANCE_STATIC` run has not happened.
  After the Pico reader appears, reinstall `AlgTest_v1.8.2_jc305.cap` if the
  applet SELECT returns `6F00`, then run the static performance operation and
  inspect SHA-256 results. The test runner is
  `build/algtest-client-1.8.2/AlgTestJClient.jar`; its CLI flags are recorded in
  `artifacts/logs/jcalgtest-client-1.8.2-help-20261014.log`.
- Source inspection confirms its static `MessageDigest` performance loop calls
  `doFinal()` on 256-byte buffers but only checks the `0xAA` success marker;
  it does not validate the digest bytes. A separate SHA-256 known-answer APDU
  check is required to establish output correctness.
- The one-pass PC-emulator VPCD fallback also failed before sending APDUs:
  `artifacts/logs/jcalgtest-sha256-vpcd-pending-physical-retry-20261015.log`
  records a bounded connection timeout. The sandbox denied loopback bind in
  the first attempt; the elevated retry still could not connect.
- Source commit: `2ba182e`; progress record added for this step.

## Persistent RP2350 filesystem implementation — 2026-10-15

- Added two 256 KiB CRC-checked snapshot slots at flash offsets `0x100000`
  and `0x140000`. They end at `0x180000`, below the existing CAP slot at
  `0x1c0000`; the latest ARM image ends at `0x100b46fc`, before the first
  snapshot bank. `fs_init()` restores the newest valid generation, and `fs_sync()`
  writes a new slot without erasing the currently valid one. Applet APDU handling
  flushes dirty state; successful CAP installation flushes the registered root
  before marking CAP metadata installed.
- `./tools/test-rp2350-fs-overlay` passes all checks, including restore after
  restart, latest-generation selection, fallback after payload corruption, and
  tombstone persistence. `./tools/build arm` and `./tools/build riscv` pass.
- Physical validation is blocked by host access: the requested `pcsc_scan -r`
  returned `SCardEstablishContext: Service not available` and emitted repeated
  `SCardReleaseContext: Invalid handle` output until `pcsc_scan` was stopped.
  `lsusb -d 2e8a:0009` fails libusb initialization (`-99`), and `systemctl`
  cannot connect to the system bus. No flash or physical APDU was attempted.
- Limitation to measure on hardware: every dirty APDU currently writes the fixed
  128 KiB RAM snapshot and erases the alternate 256 KiB slot. Latency and flash
  wear are unmeasured; consider a compact/log-structured format after proving
  startup restore works.
- Next: restore VM USB passthrough and PC/SC service, then flash the built ARM
  UF2, power-cycle, check startup serial, SELECT the installed JCAlgTest applet,
  and rerun JCSystem plus SHA-256 `doFinal` probes.
- Source and test changes are committed as `56e625d`. Pre-existing CCID and
  diagnostic edits remain unstaged and untouched.

## Physical JCAlgTest SHA-256 doFinal pass — 2026-10-15

- The user-authorized `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper
  granted pcscd access to `/dev/bus/usb/001/033`. `pcsc_scan -r` listed the
  Pico alongside both virtual readers. GlobalPlatformPro 26.06.04 listed the
  existing JCAlgTest instance but its SELECT returned `6F00` after firmware
  reboot, so the supplied `AlgTest_v1.8.2_jc305.cap` was reinstalled; LOAD and
  INSTALL APDUs returned `9000`.
- Extended `tools/JCAlgTestApduProbe.java` to prepare and invoke SHA-256
  `doFinal`. Physical probe passed: SELECT `9000`, version `1.8.2_jc305`,
  JCSystem info `9000`, SHA prepare `AA 9000`, SHA `doFinal` `AA 9000`, and
  unsupported RSA-SHA1 `NO_SUCH_ALGORITHM` (`12 03`, status `9000`). Probe
  source commit: `33ad415`.
- The JCAlgTest marker establishes successful `doFinal` execution, not a
  digest known-answer comparison. Post-APDU serial still reports boot-time
  `CHOUPI:STATUS sw=6F00` / `TEST:FAIL choupi_runtime`; CCID had advanced to
  stage 7 and reported the latest host APDU. Keep the applet restore/boot smoke
  failure separate from the passing live-session SHA and JCSystem APDUs.
- Next: if digest correctness is required, add a physical known-answer test that
  can inspect output bytes. Separately fix startup persistent applet restore,
  then power-cycle and repeat the APDU probe.
- APDU log: `artifacts/logs/physical-sha256-dofinal-apdu-20261015.log`
  (SHA-256 `de5fbf9ff643efe139f0378c077fa28a69a1b3f1057868d82c9dd1844351f41e`).
  Post-APDU serial log: `artifacts/logs/physical-sha256-dofinal-post-apdu-serial-20261015.log`
  (SHA-256 `88ad5f1a1f077c5ae499682728dafabe45f36aabdd170cf3145aaa4137d024ee`).


## Physical SHA-256 flash and boot check — 2026-10-15

- After the user reattached BOOTSEL, ordinary sandboxed USB commands still
  failed with libusb `-99`, but a bounded elevated read-only check identified
  `2e8a:000f`; picotool confirmed RP2350 `boot type: bootsel` and the existing
  ARM CHOUPI image. Both required builds passed again.
- `./tools/flash build/arm/pico2_choupi.uf2` completed with
  `FLASH:PASS`; picotool reached 100% and rebooted the board. An 18-second
  finite serial capture then repeatedly reported `CHOUPI:STATUS sw=6F00`,
  `TEST:FAIL choupi_runtime`, and `TEST:SUMMARY pass=0 fail=1`; CCID remained
  at stage 2, endpoint 00.
- Post-flash `lsusb -d 2e8a:0009` sees `Pico 2 CHOUPI CCID`, but bounded
  `pcsc_scan -r` lists only the two Virtual PCD readers. The physical USB node
  re-enumerated as bus 001/device 033, so pcscd access needs its ACL refreshed.
  Do not use sudo in the autonomous cycle; ask the user to run
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`, then verify with
  `timeout 5s pcsc_scan -r`.
- Physical SHA-256 `doFinal` APDU remains untested. After PC/SC lists the Pico,
  reinstall the supplied JCAlgTest 1.8.2 CAP if needed, then run the bounded
  physical APDU probe and inspect its status plus serial output. The boot-time
  `6F00` smoke failure also remains unresolved.
- Serial artifact: `artifacts/logs/physical-sha256-dofinal-serial-20261015.log`
  (SHA-256 `069f486067d58198cd15a90ced6fc11eb4c6f5a11e62f75c488d5dfeee705255`).


## Physical SHA-256 retry — 2026-10-15

- Both target builds passed: `./tools/build arm` and `./tools/build riscv`.
  ARM UF2: `build/arm/pico2_choupi.uf2`, SHA-256
  `241eaa1592459b8fc1f7d38e0576437c9797202606c7a720ce3e43f964d8ad06`.
- The mounted `RP2350` volume's `INFO_UF2.TXT` identified UF2 Bootloader
  v1.1 / RP2350. However, `lsusb` could not initialize libusb (`-99`),
  picotool failed to initialize libusb (status 250), and the volume was mounted
  read-only. The copy failed with `Read-only file system`; firmware was not
  transferred. `lsblk` showed the volume while `/dev/sdb` and `/dev/sdb1` were
  absent from the guest.
- Bounded `timeout 5s pcsc_scan -r` ended with status 124 and
  `SCardEstablishContext: Service not available`; no CCID test can run in
  BOOTSEL. No physical runtime or serial result was observed.
- Stop here until the VM USB passthrough exposes a writable BOOTSEL volume with
  its device nodes. Recheck `lsusb -d 2e8a:000f`, `findmnt` (must be `rw`),
  and `/dev/sdb1`, then transfer the ARM UF2 and inspect the post-boot serial
  output and physical SHA-256 APDU.
- Detailed attempt record: `artifacts/logs/physical-sha256-dofinal-retry-20261015.log`
  (SHA-256 `e9a7531e1dea9de35eab44b6e34a0f160aa8eb371c413bb5ddb870ef2da40608`).


## SHA-256 doFinal repair — 2026-10-15

- Fixed the first `MessageDigest.OneShot.doFinal()` failure. CHOUPI's persistent
  object-field storage did not materialize Java's implicit default zero for
  `inputLength`; the first read raised `SecurityException`. The private
  `OneShot` constructor now explicitly stores `inputLength = 0`.
- Java Card API commit: `31d6ab4`. Root regression-test commit: `1bb209c`.
  The test prepares JCAlgTest's SHA-256 `doFinal` operation, sends one input
  byte, and requires its success marker (`AA`).
- Previously validated after the fix: `./tools/build arm`,
  `./tools/build riscv`, `./tools/build-pc`, and one VPCD applet pass with
  `TEST:SUMMARY pass=9 fail=0`; SHA-256 `doFinal` returned `AA 9000`.
- Physical verification remains incomplete. A BOOTSEL UF2 copy was attempted
  in the prior session, after which the mass-storage volume disappeared. In
  this session `./tools/monitor --seconds 8` waited its 30-second USB discovery
  window and failed with `ERROR: no USB serial device found`. No Pico USB
  device or serial port is currently passed through to the VM, so no physical
  SHA APDU or finite runtime log could be captured.
- Next: attach the Pico's post-boot CHOUPI USB interfaces to this VM, then run
  the bounded monitor and physical JCAlgTest SHA-256 `doFinal` probe. Do not
  claim the real-hardware check passed until its APDU and finite serial output
  are inspected.
- VPCD artifact: `artifacts/logs/jcalgtest-sha256-dofinal-final-vpcd-20261015.log`
  (SHA-256 `8a2670a7a510d5240afaaefb0317cc8b2e6ebdacc093568ca9ece205349076a0`).

## RP2350 SHA-256 implementation — 2026-10-15

- Implementation commit: `80855c3` (API subrepository commit: `79fd1f7`).

- Added `MessageDigest.ALG_SHA_256` via the existing exported
  `MessageDigest.OneShot` class to keep Java Card export class tokens stable.
  The implementation buffers up to 2048 input bytes in `CLEAR_ON_RESET`
  transient storage, then calls a native bridge. RP2350 ARM uses the Pico SDK
  SHA-256 hardware accelerator; the PC emulator uses Mbed TLS.
- `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc` passed.
  The VPCD JCAlgTest 1.8.2 run passed all 9 checks, including supported result
  `15 00` for SHA-256:
  `artifacts/logs/jcalgtest-sha256-support-vpcd-20261015.log`.
- Limitation: an exploratory JCAlgTest MessageDigest performance `doFinal`
  APDU returned `6F00` with a `SecurityException` at `GETFIELD_S_THIS` in the
  dynamic applet path. The native digest result was not independently checked.
  Do not claim digest known-answer or hardware validation yet.
- The physical `cycle choupi` rebuilt both architectures but did not flash:
  the USB wait timed out after 120 seconds because this VM could not see any
  RP-series device (`lsusb` reported libusb `-99`). Log:
  `artifacts/logs/cycle-choupi-20261015T132103Z.log`.
- Next: debug the JCAlgTest performance APDU state failure, add/run a bounded
  SHA-256 known-answer check, then flash and inspect serial output once USB
  passthrough is available to this VM.

## Physical JCAlgTest and JCSystem live-session pass — 2026-10-15

- Reinstalled `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` on the running Pico
  with GlobalPlatformPro; LOAD blocks and INSTALL returned `9000`.
- Extended `tools/JCAlgTestApduProbe.java` to assert the expected unsupported
  RSA-SHA1 result. `./tools/probe-jc-algtest` passed on the physical reader:
  SELECT `9000`, GETVERSION exactly `1.8.2_jc305`, JCSystem info `9000`, and
  RSA-SHA1 Signature result `12 03` (`NO_SUCH_ALGORITHM`) with `9000`.
  The JCSystem info handler exercises version, deletion-support, scalar and
  array memory queries, and maximum commit capacity.
- Both `./tools/build arm` and `./tools/build riscv` passed after the probe
  change. No firmware flash was performed; the change is host-side and the
  live applet instance is currently only in the RAM overlay.
- Limitations: the finite serial log still emits `CHOUPI:STATUS sw=6F00` and
  `TEST:FAIL choupi_runtime` from the boot-time runtime check. After reset,
  `fs_init()` clears the overlay, leaving registry metadata but no applet root;
  the physical SELECT had failed until the CAP was reinstalled in this live
  session. Do not claim power-cycle persistence or a clean boot health test.
- Next: implement and validate a durable RP2350 filesystem store that preserves
  the `0x1c0000` CAP slot, fix the boot smoke check, then power-cycle and repeat
  the same bounded APDUs.
- Logs: `artifacts/logs/gp-install-physical-live-recovery-20261015.log`,
  `artifacts/logs/jcalgtest-physical-jcsystem-unsupported-20261015-apdu.log`,
  and `artifacts/logs/jcalgtest-physical-jcsystem-unsupported-20261015-serial.log`.

## Physical JCAlgTest restore failure — 2026-10-15

- The user enabled PC/SC access; `pcsc_scan -r` shows `Pico 2 CHOUPI CCID`
  alongside two virtual readers. GlobalPlatformPro `--list` connects and lists
  ISD `A000000151000000` and applet instance `4A43416C675465737431` in lifecycle
  `INITIALIZED`.
- A bounded `./tools/probe-jc-algtest` connected with ATR `3B800181`, then
  SELECT returned `6F00`. Serial log
  `artifacts/logs/jcalgtest-physical-restore-recheck-20261015.log` shows the
  expected AID/package/class lookup, followed by
  `RESTORE_STORAGE status=1 length=0` and
  `RESTORE_READ_FAIL id=5 tag=03003F94FF`.
- Status 1 means missing record in the RP2350 C ABI shim. `src/choupi_pico2.cpp`
  clears the in-memory overlay in `fs_init()` on every boot. `cap_store.cpp`
  persists CAP bytes and registry metadata in the reserved `0x1c0000` slot,
  but does not persist Java Card applet instance fields. The applet is listed
  as installed after reset, while its root object is gone. Do not claim
  physical SELECT or JCSystem verification until this is addressed.
- Next: inspect the RP2350 flash image/layout and design a separate durable
  filesystem area that cannot overlap the firmware or CAP slot. Preserve the
  existing CAP slot and avoid flashing while investigating. Both architecture
  builds remain required before any later flash.

## Physical diagnostic firmware flash — 2026-10-15

- Rebuilt ARM and RISC-V successfully, then flashed the ARM CHOUPI image while
  the Pico 2 was in BOOTSEL. Picotool loaded the image to 100% and rebooted it.
  The 18-second health check still reported `CHOUPI:STATUS sw=6F00` and
  `TEST:FAIL choupi_runtime`; CCID state was stage 2, endpoint 0, with no APDU
  traffic. This is a boot smoke failure and does not yet say what physical
  applet SELECT does.
- USB enumerates `2e8a:0009`, but `pcsc_scan -r` lists only the two VPCD
  readers. `./tools/probe-jc-algtest` confirms the physical reader is absent
  from PC/SC and sent no APDU. `/dev/bus/usb/001/023` grants access to `claude`
  but not `pcscd`; pcscd is auto-exited.
- Next, the user must run the existing helper command
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` and confirm `pcsc_scan -r` shows
  `Pico 2 CHOUPI CCID`. The repository instructions prohibit this agent from
  using sudo in the autonomous cycle. Then run the bounded physical probe and
  inspect `RESTORE_STORAGE` / `RESTORE_READ_FAIL` / `RESTORE_READ_OK` serial
  output.
- Flash log:
  `artifacts/logs/cycle-choupi-20261015T112920Z.log` (SHA-256
  `369ae5865b5645067ab74e22811b18510c1cd290945ac1b510579bd8cbcb48de`).

## VPCD applet restore comparison — 2026-10-15

- The bounded `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 20`
  comparison passed `TEST:SUMMARY pass=9 fail=0` in 58.436 s after an approved
  localhost-socket retry (the default sandbox attempt failed `EPERM`). No source
  code changed for this validation.
- Host install wrote root record tag `03 00 3F 94 FF` with a four-byte object
  header. SELECT returned `9000`, GETVERSION returned `1.8.2_jc305`, and JCSystem
  info returned
  `0305000000000000000000002000FE01000000000000000000000000009000`. RSA-SHA1
  and CRC16 reported `NO_SUCH_ALGORITHM` with `9000`, which is expected.
- This confirms the emulator's install/persist/restore path but does not resolve
  the physical-card failure. USB remains inaccessible here: `lsusb` reports
  libusb `-99`, picotool cannot initialize libusb, and the PC/SC helper cannot
  run because `no_new_privileges` blocks sudo in this execution context.
- Next: when USB access is restored, flash the ARM CHOUPI image and issue the
  physical SELECT. The diagnostic now reports root `fs_read_inplace` status and
  tag; compare it with the successful host root tag before changing persistence
  or access-control behavior. Do not add algorithms for expected
  `NO_SUCH_ALGORITHM` rows.
- Full VPCD log:
  `artifacts/logs/jcalgtest-restore-after-safe-diagnostic-20261015.log`
  (SHA-256 `6f7c41916df69e2a4d443588f5ce914d18a3ac1acb01c5dd5a4e82a40506d629`).

## Physical applet restore read diagnostic — 2026-10-15

- Current focused issue: a physical JCAlgTest SELECT reaches the card and
  resolves package 63/class `0x0094`, but `Context::restoreAppletInstance`
  returns a null reference before applet `select()` runs. The registered
  instance root tag should be `03 00 3F 94 FF` (applet owner 0, package 63,
  class 0x0094, root field 0xFF).
- Added a diagnostic in `getPersistentField_Reference` to report read exception,
  tag bytes, and root record type. The lower read path now logs its original
  status; there is deliberately no second filesystem existence probe after a
  failure, since that could trigger the same context access assertion. Runtime
  restore behavior is unchanged. ARM and RISC-V builds pass. Commits:
  `6e1e2e6`, `c290cd1`.
- The latest `./tools/cycle choupi` rebuilt both targets but did not flash:
  picotool failed to initialize libusb (status 250), and `lsusb` also reports
  libusb `-99`. The previously authorized `sudo -n
  /usr/local/sbin/pico2-pcsc-reader` helper is blocked in this execution context
  by `no_new_privileges`. No hardware validation occurred in this session.
- Next: restore USB passthrough/access, flash `build/arm/pico2_choupi.uf2`, then
  capture a bounded SELECT trace. `RESTORE_READ_FAIL` means filesystem read
  failed or was denied; `RESTORE_READ_OK` includes record type and tag. Compare
  the exact root key against what install persisted before changing storage
  ownership or access rules. Continue focusing on applet execution and
  `JCSystem`; `NO_SUCH_ALGORITHM` is an expected unsupported-algorithm result.

## Official JCAlgTest PC/SC diagnosis — 2026-10-15

- The user ran JCAlgTest 1.8.2 through `Virtual PCD 00 00`; it collected about
  72 rows, then the 300-second timeout expired while sending signature
  algorithm 24 (HMAC-SHA1), APDU `B0 75 12 00 01 18 00`. The emulator was
  stopped cleanly after the timeout. Log and partial CSV:
  `artifacts/results/jcalgtest-vpcd/ALGTEST_log_1792050525367.log` and
  `artifacts/results/jcalgtest-vpcd/CHOUPI_VPCD_ALGSUPPORT__3b_80_01_81.csv`.
- Correction to the earlier namespace diagnosis: the agent is running on the
  same VM. The default command sandbox denies local socket operations
  (`EPERM`), but an approved host-socket command ran the emulator locally. A
  bounded replay of signature algorithms 1–24, with the same `B0 E2` object
  deletion request before each probe, passed all APDUs. Algorithm 24 returned
  `9000` in 3.026 s; total replay time was 97.284 s. This means the APDU and
  accumulated algorithm sequence do not reproduce the hang in the internal
  VPCD transport. Replay log SHA-256:
  `d3fecd2f656c27a2cd3d6b38b176dd526ef81ca47774e0cd94a0447df27f5f64`.
- The single HMAC APDU also passed after a reset in 3.323 s. No repository
  source was changed for these experiments. The last official-client attempt
  reached 300 seconds without a response; the current pcscd/VPCD listener is
  stopped, and no JCAlgTest or CHOUPI test process remains running. A non-sudo
  `systemctl start pcscd.service` attempt timed out, and a follow-up status
  query was denied access to the system D-Bus. No service state change was
  observed.
- Next: have the user restart the PC/SC service with the already established
  privileged command, confirm `pcsc_scan -r` sees the virtual readers, then
  replay the official client locally with a per-APDU watchdog and CHOUPI log
  capture. Compare its exact sequence and timing with the passing internal
  replay before changing runtime code. No physical token was used.

## VPCD ATR presence-probe implementation — 2026-10-15

- The VPCD reader probes presence by requesting the ATR before it powers the
  card. `Session` previously returned no response to an ATR probe while
  powered off. It now returns the ATR in either power state. Focused session
  test passes 14/14; `./tools/build-pc` and the PC `choupi` target build pass.
  Implementation commit: `ceb5101`.
- The rebuilt `build/choupi-pc/choupi` passed one bounded internal VPCD
  harness pass: connect 1.789 s, ATR 0.001 s, SELECT 0.161 s, JCSystem info
  13.934 s, supported-algorithm APDU 2.945 s; `TEST:SUMMARY pass=5 fail=0`.
  No physical token was used.

## Current JCAlgTest development checkpoint — 2026-10-15

- The bounded PC VPCD applet test now passes ATR, SELECT, JCSystem info, and
  supported-algorithm APDU in one emulator pass. Log:
  `artifacts/logs/jcalgtest-internal-virtual-table-token-fix-20261015.log`.
- Root cause: ROM-mask generation encoded internal virtual method references
  with a declaration index, not the public/package virtual-table token. It
  emitted token zero for APDU methods, causing `setOutgoing()` to resolve to
  `Object.equals()` and underflow. The generator now resolves each internal
  signature in the class method tables. Nested ROM-mask commit: `fa470e5`.
- PC ROM-mask generation, PC emulator build, and `./tools/build arm` pass. The
  ARM build needed `[[maybe_unused]]` on a diagnostic stage variable in
  `bc_object.cpp`; that broader file still contains pre-existing uncommitted
  CHECKCAST changes. No RISC-V build or flash was performed, matching the
  ARM-only scope. The current tree contains extensive pre-existing dirty files;
  stage only work belonging to each next step.
- The official JCAlgTest v1.8.2 host client has not yet been run against VPCD;
  CBC known-answer tests and physical-board process/power-cycle validation also
  remain outstanding. Do not mark AC1–AC5 complete.
- Release-only Rust host syscall prints are now gated by `debug_assertions`;
  nested OS source commit is recorded in the following progress reference.
  `./tools/build-pc` and the PC `choupi` target build pass. A fresh one-pass
  VPCD run passes in 23.525 s, and two fresh passes succeed (`TEST:SUMMARY
  pass=10 fail=0`) in 47.575 s. The release emulator log shrank to 47,015 bytes;
  its resource sample reported wall 23.48 s, user 0.48 s, system 6.61 s, 30%
  CPU, and 17,128 KiB peak RSS. No stable throughput conclusion follows from
  these single samples.
- `./tools/build arm` passes after the Rust logging change. No RISC-V build or
  physical flash was performed. The official client is currently blocked in
  this environment: the finite Java PC/SC probe sees no readers, `pcsc_scan`
  reports that the service is unavailable, and `vsmartcard-vpcd` is not
  installed. Do not use `pcsc_scan -r` as a wait loop; it floods errors when
  pcscd is down. Once the VPCD PC/SC driver and daemon are available, run the
  official supported-algorithms client against the host emulator and compare
  its CSV with the archived jCardSim reference; add CBC known-answer coverage
  before physical-card testing.

## Current performance measurement checkpoint — 2026-10-15

- `./tools/task-time report`: 45 successful intervals totaling 564.964 s,
  plus 41 non-OK intervals. Compile: 29 successful, 422.524 s total, 14.570 s
  mean, 58.213 s max. Test: 13 successful, 137.689 s total, 10.591 s mean,
  47.575 s max. Upload has no measurements.
- Two bounded release VPCD passes succeeded in 47.575 s. A later pass measured
  connect 2.554 s, ATR 0.001 s, SELECT 0.231 s, JCSystem info 16.437 s,
  supported-algorithm APDU 4.561 s, and shutdown 0.031 s. A separate
  `/usr/bin/time` sample reported 0.48 s user, 6.61 s system, 30% CPU, and
  17,128 KiB max RSS. This one sample suggests focusing phase/resource
  measurements on JCSystem info and the host OS bridge; it does not prove a
  spin loop. Release syscall diagnostics were gated, reducing the log from
  291,641 bytes in a timed-out run to 47,015 bytes in a passing run, without a
  demonstrated runtime speedup.
- Performance suggestions and metric interpretation are in `perf_tips.md`.

## Development performance suggestions — 2026-10-15

- Added a category summary to `perf_tips.md` from the existing task timer: 17
  successful intervals (5 compile, 9 test, 1 thinking, 2 tool-check) totaling
  52.821 seconds, plus 8 non-OK intervals. The note treats failures as iteration
  friction and does not infer performance from them.
- Recommendations: run focused host tests before firmware builds, check USB and
  PC/SC preconditions before long integration waits, collect comparable
  clean/incremental and CPU-time samples before tuning build parallelism, and
  separately time upload phases when hardware work resumes. Upload remains
  unmeasured.
- Validation: `git diff --check` passed; documentation-only, no build, test, or
  hardware operation. Next: continue the active JCAlgTest correctness work and
  add more timing samples only when they can be collected without interrupting
  that work.
- Commit: `be0a0e9`.

## Task-time summary and current performance suggestions — 2026-10-15

- Updated `perf_tips.md` from `./tools/task-time report`: 26 successful
  intervals, 183.503 s total; 18 non-OK intervals. Compile: 14 runs,
  156.315 s total, 11.165 s mean, 35.131 s max. Test: 9 runs, 22.437 s
  total, 2.493 s mean. Upload has no measurements.
- The latest VPCD attempt took 13.116 s, reached the JCAlgTest info APDU, and
  failed with `6F00`; the latest trace identifies `CHECKCAST` with
  `SecurityException` after the JC305 token-map fix. Treat it as failed
  diagnosis time, not applet throughput. This supersedes earlier
  `PUTFIELD_A`/`GETFIELD` diagnostics for current debugging.
- Suggestions now prioritize timing upload subphases, adding CPU/memory data to
  long commands, reducing failed iteration time, serializing heavy builds in
  the eight-core VM, and bounded USB/PCSC retries. No performance cause or
  speedup is claimed.
- Validation: `./tools/task-time report` and `git diff --check`.
- Next: continue JCAlgTest correctness work; after the APDU succeeds, record
  phase timings and gather comparable build and upload samples.

## VPCD performance guidance refresh — 2026-10-15

- Updated `perf_tips.md` to reflect the newest diagnostic: inherited
  `getReason()` resolves to package 0/offset 370, then an uncaught
  `SecurityException` occurs at `GETFIELD_S_THIS` (PC `0x3D98`) and the APDU
  returns `6F00`. The 215,263-byte trace contains repeated `FsRead1b` lines and
  scans 51 exception-handler entries, but no phase timing was collected, so
  these are profiling candidates rather than confirmed bottlenecks.
- Next: fix the field-access correctness issue, then capture a bounded one-APDU
  timing with aggregated filesystem-read and exception-dispatch counts. Keep
  the existing VPCD command deadline and compare wall/user/system time; no
  build, test, or hardware operation was run for this documentation update.

## Ranked performance experiments — 2026-10-14

- Added a priority table to `perf_tips.md` grounded in recorded measurements:
  Maven ROM-mask generation took 5:13 in one run; the latest failed VPCD test
  took 7.39 s wall with 0.27 s user and 1.98 s system CPU; successful ARM
  builds span 3.120–9.152 s. The table separates observations from proposed
  experiments and defines how to compare results.
- Updated the VPCD performance note to reflect the current exception/method
  lookup failure instead of the stale CP field entry 174 diagnosis. No build,
  test, hardware operation, or new timing sample was run for this docs-only
  update.
- Next: add phase timings to the VPCD harness and collect comparable Maven
  stage/build samples before changing runtime or build behavior.

## Performance guidance update — 2026-10-14

- Added the latest VPCD handler-offset run to `perf_tips.md`: 7.39 s wall,
  0.27 s user, 1.98 s system, 30% CPU, 17,264 KiB peak RSS; it still failed
  with `6F00`. This is a failure diagnostic and is not comparable as a speedup
  with earlier runs.
- Next, time the exact VPCD wrapper and record phase timestamps before drawing
  conclusions about where the elapsed time goes.

## JCAlgTest applet method lookup — 2026-10-14

- Fixed two CAP package-method lookup defects: package method references now
  mask off the high-bit method-kind flag before indexing the table, and the
  package virtual-method table pointer advances by a `uint16_t` element count.
  The former bad pointer read produced offset `0xFF00`; with both fixes the
  JCAlgTest call resolves to offset `15960` and starts executing.
- `./tools/env cmake --build build/choupi-pc --target choupi -j2` passed and
  `./tools/build arm` passed in the implementation step. The bounded
  `./tools/test-pc-vpcd-applet` still fails: ATR and SELECT pass, while the
  JCSystem info APDU returns `6F00`. The log shows `process(APDU)` reaches
  `GETFIELD_B_THIS` for CP entry 174 (field token 11 on class 148), then throws
  `SecurityException`. Class metadata for package 63/class 148 reports 13
  declared instance words, first reference token 0, and 9 reference fields;
  persisted field slot 11 is currently stored as a short. Java Card 3.0.5
  permits field tokens to resolve to implementation-specific offsets, so
  inspect the v1.8.2 CAP field metadata and installation layout before choosing
  a mapping. The VPCD harness now reaps the emulator process group with bounded
  SIGINT/TERM/KILL escalation; the last run left no emulator process behind.
- No physical device or RISC-V build was used. Next derive and validate the
  field-token-to-slot mapping from the CAP metadata and persisted instance
  layout, then continue with JCSystem info and the supported-algorithm APDU.
  Latest bounded trace: `/tmp/pico2-jcalgtest-vpcd-cleanup.log`; previous full
  trace: `artifacts/logs/host-vpcd-jcalgtest-20261014.log`.

## Performance tips refresh — 2026-10-14

- Updated `perf_tips.md` with the latest bounded measurements: host applet
  installation took 0.45 s (0.40 s user CPU, 6,120 KiB max RSS) in one sample;
  a PC CHOUPI JCRE smoke run failed after 34.837 s. The install sample is too
  small to justify optimization, and the failed smoke timing has no phase trace.
- Recommendations now prioritize adding timestamps to emulator launch, VPCD
  connection, ATR, SELECT, and APDU phases before optimizing the failed smoke
  path. Upload remains unmeasured. No builds, tests, or hardware operations
  were run for this documentation update.
- Next: capture phase timings on the next passing/failing PC smoke run and
  compare wall and CPU time to identify active compute versus waiting.

## PC vpcd APDU session integration — 2026-10-14

- Added a PC-only session state machine and routed VPCD power, reset, ATR,
  applet SELECT/DESELECT, and applet APDUs through the existing VM lifecycle
  functions in `main_pc.cpp`. The PC CMake source glob now uses
  `CONFIGURE_DEPENDS`, so new `.cpp` files are picked up automatically.
- `./tools/test-vpcd-session` passes 14 fake-card lifecycle/routing checks;
  `./tools/test-vpcd-protocol` passes 14 frame codec checks. `./tools/build-pc`
  and the PC `choupi` target build pass, as does `./tools/build arm`. The ARM
  UF2 SHA-256 is `b98b999a8837858ef655453db0dab1ec9ffa9e30d8513901980a54295ae41625`.
- `./tools/probe-jc-algtest` reports that `Pico 2 CHOUPI CCID` is absent, so it
  sends no APDUs. No hardware flash was attempted; no RISC-V build/test was run.
  Live VPCD TCP transport remains unverified in the sandbox, where socket
  syscalls were previously denied.
- Remaining: add golden applet tests that execute Java code, enable a real
  PC/SC/VPCD endpoint, then run JCAlgTest v1.8.2 on CHOUPI and compare its CSV
  with the archived jCardSim output. Do not treat the fake callback tests as
  evidence that the installed JCAlgTest applet executed.

## vpcd frame codec — 2026-10-14

- Added the PC-only vpcd frame codec in `choupi/src/vpcd_protocol.*` and a
  focused test wrapper. It handles partial blocking socket reads/writes,
  16-bit network-order lengths, and reports clean close, malformed empty frame,
  truncation, and I/O errors distinctly.
- `./tools/test-vpcd-protocol` passes 14 checks; the full PC `choupi` target
  builds, and `./tools/build arm` passes. No RISC-V build or flash was run.
- The sandbox denies `send()` (`EPERM`), so socket syscalls were compile-checked
  but not exercised. The codec itself is tested. Next integrate VPCD control
  messages and route APDUs through `choupi_vm_select_applet()`/
  `choupi_vm_process_applet()`; host applet installation/restore setup remains
  an open prerequisite.

## APDU CLA helpers — 2026-10-14

- Implemented Java Card 3.0.5 `APDUUtil` CLA parsing and wired the matching
  current-command helpers in `APDU`. `./tools/test-apdu-util` passes 17 cases.
- The first `./tools/build arm` uncovered a pre-existing CMake argument quoting
  bug in Java API regeneration. Fixed the `-Dbuild.dir` argument; the complete
  wrapper then passed Java API compilation/CAP conversion, ROM-mask generation,
  and ARM CHOUPI firmware build. No RISC-V build or flash was run.
- Nested Java API commit and root commit references are in the latest
  `progress.md` entries. Actual applet invocation remains unverified. Continue
  toward the host vpcd APDU path and lifecycle tests.
- A bounded `pcsc_scan -r` attempt with pcscd unavailable produced more than
  237 MB of repeated service-unavailable output before terminating. Do not run
  `pcsc_scan` in this environment until pcscd readiness is confirmed; use the
  bounded Java probe instead.

## Additional performance suggestions — 2026-10-14

- Expanded `perf_tips.md` with recommendations to diagnose the previously
  reported eight-core VM saturation, avoid tight polling in USB/PCSC readiness
  paths, and capture CPU/resource metadata alongside comparable wall times.
- No performance cause was measured in this documentation step; suggestions
  distinguish observed symptoms from unverified causes. The existing
  incremental PC-generation optimization remains in place.
- Commit reference is recorded in the latest `progress.md` entry. Next,
  continue host applet lifecycle/APDU coverage; the physical reader is not
  currently available in this execution context.

## Updated development timing sample — 2026-10-14

- Documentation commits: `171d404`, reference `9cbeb4e`; expanded sample
  commit `d27321d`.
- Latest `tools/task-time report`: five successful ARM builds at 3.120–9.152 s
  (mean 5.127 s), nine focused host tests at 0.458–5.184 s (mean 2.493 s),
  and 52.821 s total successful recorded work.
- Failed and interrupted attempts remain in the CSV and are excluded from
  successful averages. Upload time is unmeasured.
- A bounded `./tools/probe-jc-algtest` run found no `Pico 2 CHOUPI CCID` reader
  and sent no APDUs. Continue with host-side work until reader visibility is
  restored; no firmware flash has been attempted for the latest changes.

## RP2350 filesystem erase regression — 2026-10-14

- Implementation commit: `c8fc232`.
- Added `tools/test-rp2350-fs-overlay`, which compiles the actual RP2350
  filesystem source against host hardware stubs. It covers erase visibility,
  same-tag reuse, collision-chain lookup, and compressed child-tag reuse.
- The collision test found stale offsets after erase; compaction now scans all
  overlay slots. Final host test passes 13 checks (`TEST:SUMMARY fail=0`,
  4.039 s; the earlier full compile/test pass took 5.184 s), and
  `./tools/build arm` passes (9.152 s).
- No RISC-V build or board flash was performed. `perf_tips.md` and the raw
  timer CSV now contain the expanded sample. Next continue host-side lifecycle
  and JCAlgTest work.

## Latest ARM implementation — 2026-10-14

- Added synchronous persistent-write transactions around GP INSTALL. Snapshots
  preserve previous filesystem values, erase records created during a failed
  install, and cover indexed array/package-list updates.
- Added `fs_erase` to the RP2350 overlay backend using a retained tombstone
  entry. Deleted values are hidden from reads and the index slot can be reused.
- `./tools/test-persistent-transaction` passes (`TEST:SUMMARY fail=0`, 2.789 s).
  `./tools/build arm` passes (3.639 s). The preceding link failure was due to
  missing RP2350 `fs_erase` and is resolved.
- Implementation commit: `4e2d1a9`.
- No RISC-V build or hardware flash was performed. Board failure-path behavior
  remains untested. Rollback is not durable across power loss because CAP
  metadata and filesystem entries do not share a journal.
- Next: keep install atomicity open; validate failed-install cleanup and status
  words on hardware only after the repository build gate allows flashing.

## Install status words — 2026-10-14

- Implementation commit: `7c71176`.
- GP INSTALL failure stages now append a status word that distinguishes bad
  install data (`6A80`), missing package (`6A82`), storage/rollback failure
  (`6581`), unavailable transaction (`6985`), and unexpected exception
  (`6F00`). The diagnostic payload remains before the status word.
- `./tools/test-install-status` passes all ten mappings (`TEST:SUMMARY fail=0`,
  1.551 s); `./tools/build arm` passes (3.120 s). No RISC-V build or
  board flash was performed, so card-manager behavior remains unverified.
- Next: retain the board validation gap, then continue with a host execution
  path for applet lifecycle/APDU tests while honoring the ARM-only scope.

## Development task timer — 2026-10-14

- Added executable `tools/task-time` with manual intervals, command timing, and a Markdown report. Its append-only data is in `artifacts/metrics/development-time.csv`.
- Added `perf_tips.md` with evidence-based suggestions: preserve incremental CAP/ROM-mask outputs, measure Maven conversion separately, keep parallel jobs bounded, and fail fast when USB/serial nodes are absent. The sample remains too small for stable averages.
- Performance guidance committed as `6154378`.
- Timer implementation and sample committed as `eb36cbd`.
- Successful sample: ARM incremental build 3.903 s; `test-short-apdu` 0.458 s; manual analysis 4.641 s; two timer syntax checks at 0.055 s each. Report successful total is 9.112 s. A failed CLI invocation (11.839 s) and interrupted board monitor (82.460 s) remain in the raw CSV and are excluded from successful timing averages.
- Host CAP converter build completed in 3:29 before timing started, so that duration was not recorded.
- Current hardware visibility differs from the user's BOOTSEL report: guest sysfs sees `2e8a:0009` (CHOUPI CCID), while there is no `/dev/ttyACM*` or `/dev/serial/by-id/*` node. `pcsc_scan -r` reports service unavailable. No flash attempted: current work is ARM-only and RISC-V UF2 is older than the latest shared-source edit; root instructions require both target builds before flashing.
- Next: gather multiple build/test samples with `tools/task-time`; once both-target build gate and BOOTSEL/device-node visibility are restored, time upload and bounded serial validation. Do not infer stable speedups from this small sample.

## Current session state — 2026-10-14

- Latest physical cycle: `./tools/cycle choupi` rebuilt ARM, RISC-V, and the
  CHOUPI target successfully, then flashed the ARM CHOUPI UF2. Initial picotool
  discovery saw the running CHOUPI CCID firmware rather than BOOTSEL; its
  reset interface was unavailable, but the bounded CDC recovery eventually
  entered BOOTSEL and the retry loaded the image at 100%. The 18-second serial
  capture passed 18 repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0` checks, with no `FAULT` or `TEST:FAIL`. UF2
  SHA-256 `509d663da55b8d1becf53771f5ec3293e713a7fb9f1ee1cec31b96cff048cfb3`;
  log `artifacts/logs/cycle-choupi-20261014T135618Z.log`, SHA-256
  `f7fe9aaa9f77203d9a62053d96cf750ed4cd50d333215374919ea4336950939a`.
  After flashing, bounded `pcsc_scan -r` still reports no reader, so no applet
  APDU has been sent. Next restore PC/SC reader visibility and run the finite
  v1.8.2 APDU probe before attempting the full client.

- Latest ARM-only API step: `JCSystem.getAvailableMemory(short[], short, byte)`
  now checks arguments in Java before invoking the native method. Null buffers
  throw `NullPointerException`, invalid offsets throw
  `ArrayIndexOutOfBoundsException`, and invalid memory types use
  `SystemException.throwIt(ILLEGAL_VALUE)`. The native implementation retains
  equivalent validation as a backstop. Java API CAP conversion, ROM-mask
  generation, and `./tools/build arm` all passed. Existing converter warnings
  are from other API packages (`int` support / older export files). No RISC-V
  build/test or flash was performed. The nested API source commit is `7caa650`;
  ARM UF2 SHA-256 `509d663da55b8d1becf53771f5ec3293e713a7fb9f1ee1cec31b96cff048cfb3`.
  Build log `artifacts/logs/arm-jcsystem-java-validation-20261014.log`, SHA-256
  `969b7e02fb89bf4c4a9d09355cce52b78e16d146b68ae038e15aa2ef9f904e32`.
  Applet-level catches are still untested; continue Java runtime/API work and
  preserve the unrelated edits in the nested API repository.

- Latest ARM-only code step: `JCSystem.getAvailableMemory(short[], short,
  byte)` now raises distinct native `NullPointerException`,
  `ArrayIndexOutOfBoundsException`, and `SystemException` values for null
  buffers, invalid ranges, and invalid memory types. `./tools/test-jcarray`
  passes the validation and write-bound cases; `./tools/build arm` passes.
  Existing host test compiler warnings remain in CAP packed-member and
  const-qualified return declarations. No RISC-V build/test was run and the
  new image was not flashed because root AGENTS requires both target builds
  before flash. ARM UF2 SHA-256
  `c561daabc403eaa757dced3b2ecf662dbb9477568458edc61b3985704d5468d9`.
  Logs: `artifacts/logs/host-jcarray-available-memory-20261014.log` and
  `artifacts/logs/arm-available-memory-validation-20261014.log`. Native C++
  exceptions still do not prove Java catch/throwable behavior; continue API
  exception mapping or another JCAlgTest-used native gap. Code commit:
  `fe2f25b`.

- Latest physical step: after both required wrapper build gates passed, flashed
  `build/arm/pico2_choupi.uf2` directly while the user had the Pico 2 in
  BOOTSEL. `tools/flash` reached 100% and confirmed application reboot. The
  finite 18-second capture passed 18 repeated `TEST:PASS choupi_runtime` /
  `TEST:SUMMARY pass=1 fail=0` checks with no `FAULT` or `TEST:FAIL`. UF2 SHA-256
  `81fd209c5245c0c4f7977f561f0f3926575beb9168dff54113b380aad5163f03`; log
  `artifacts/logs/bootsel-choupi-runtime-20261014.log`, SHA-256
  `724ba860bb056e2af06c72b77796d75bf88604b714398eaea9a046b345a02b30`.
  Only the ARM image was flashed; no RISC-V hardware test was run. PC/SC is
  still unavailable in this execution context: the finite Java probe cannot
  find `Pico 2 CHOUPI CCID`; `pcsc_scan -r` emitted a rapid stream of service
  unavailable/invalid-handle errors and was stopped. Do not rerun that scan
  until pcscd availability is confirmed. Next restore PC/SC visibility, then
  run the v1.8.2 SELECT/GETVERSION/JCSystem probe and official supported-
  algorithms mode. Applet lifecycle/APDU behavior is still not validated.
  Hardware record commit: `3dbab14`.

- Latest ARM-only step: `Context::setAPDUOutgoingLength()` now rejects a
  `setOutgoingNoChaining()` length above either the command Le or the T=1
  maximum payload of 252 bytes. `./tools/test-short-apdu` passes the Le and
  frame-bound cases, and `./tools/build arm` passes. The updated firmware was
  not flashed; no RISC-V compile or test was run. Commit: `7448bec`.
- Previous ARM-only API step: implemented `JCSystem.makeTransientObjectArray()` as a
  zero-initialized transient reference array. Native-created `Object[]` uses a
  wildcard component marker so firewall checks accept any reference. The
  focused `./tools/test-jcarray` suite and `./tools/build arm` pass. The test
  checks factory type, length, clear event, and zero initialization through
  the factory path; assignment under the live Java firewall and use by an
  installed applet still need board evidence. The ARM image was built but not
  flashed. No RISC-V compile or test was run. Implementation commit: `9942d57`.
- Next ARM-only API step: `JCSystem.getPreviousContextAID()` now returns Java
  null because this runtime does not support cross-applet/shareable-interface
  context switches. `./tools/test-jcarray` and `./tools/build arm` pass; the
  new UF2 was not flashed. No RISC-V build or test was run. Commit is recorded
  as `ca0520a`.
- The user reports the VM PC/SC helper ran successfully. The bounded probe
  still finds no readers in this execution environment: normal Java access
  reports the named reader missing, and elevated Java access returns
  `SCARD_E_NO_READERS_AVAILABLE`. `systemctl` cannot connect to the VM system
  bus and `lsusb` returns `libusb: -99`; no APDU was sent. The probe was not
  retried with unbounded `pcsc_scan`.
- The user has returned physical testing to scope and asked to continue the
  complete project autonomously. This session regained USB passthrough through
  an approved elevated host-device invocation. `tools/monitor` now waits up to
  30 seconds for USB CDC enumeration, and `tools/cycle` uses the repeated
  runtime PASS/SUMMARY markers instead of requiring the one-time BOOT line.
  An earlier `./tools/cycle choupi` passed on the real board, with repeated
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The latest cycle
  rebuilt both architectures but this VM could not access USB (`libusb: -99`),
  so it did not flash or capture serial output. Applet behavior has not been
  validated in this execution context.
- Root `TODO.md` is the current acceptance-criteria tracker. The installed CAP
  blob and synthetic registry/SELECT path are not evidence that the Java
  constructor, `Applet.register`, or applet `process()` executed.
- A new `tools/build-pc` wrapper configures the CHOUPI PC build. The unnecessary
  Boost command-line dependency is removed; current Cargo no longer accepts the
  legacy `--out-dir` argument; and the JavaCard API build can select a Java 8
  compiler while Maven runs on the system JDK. Java API CAP conversion and the
  ROM-mask generation pass. Cargo dependencies were fetched into the local tool
  cache, and the complete PC VM now builds. The host target supplies an explicit
  empty dynamic-CAP store because the RP2350 reserved-flash backend is not part
  of the PC executable.
- The PC `main_pc.cpp` translation unit compiles using the generated ROM-mask
  JNI header. Existing packed-member warnings remain in upstream CAP headers.
- `Applet.register` receiver capture now flows through `Context` and
  `Interpretor`; after a successful install method return, `runtime_install_method`
  serializes the Java instance through `FlashMemory_Handler` under reserved
  field tag `0xFF`. The CAP header version 2 now stores its class index and a
  separate CRC while preserving version-1 header readability. ARM/RISC-V builds
  and the CHOUPI runtime self-check pass on the physical board. Reboot
  reconstruction, class-index dispatch, applet lifecycle/APDU handling, and
  install rollback remain incomplete. This is still a single applet slot
  (applet owner 0), with no atomic cleanup if the card-manager AID write fails.
- `Context::restoreAppletInstance(package, class)` now resolves the serialized
  root by tag, verifies the stored package and class, and eagerly decodes its
  persisted fields. CCID AID SELECT calls this helper when v2 metadata supplies
  the class index, returning `6F00` if restoration fails. It retains the
  reconstructed `Context` for the selected card session and clears it when
  another AID is selected. Both architecture builds and the board runtime
  self-check pass, but the v2 SELECT branch has not been exercised on the board
  and no Java lifecycle call uses the retained reference yet.
- The worktree contains preserved, uncommitted JC305 token-remapping and install
  diagnostics in `choupi/src/jc_bytecodes/bc_invoke.cpp`,
  `choupi/src/jc_handlers/jc_class.cpp`, `jc_cp.cpp`, `jc_cp.hpp`,
  `choupi/src/main.cpp`, and `src/choupi_pico2.cpp`. Do not overwrite or discard
  them. `CMakeLists.txt`, `SKILL.md`, and `choupi/src/jni.cpp` have separate
  pre-existing modifications and must remain out of unrelated commits.
- Last known candidate runtime builds passed for ARM and RISC-V after the
  Applet virtual-token high-bit adjustment. No board validation has occurred
  after that change.
- GP INSTALL most recently returned `6A80`. A proposed CP-tag mismatch was not
  established: the CAP begins with CP entry 1 tagged as an instance-field
  reference, but persisted watchdog scratch values are stale and the captured
  PC has not been mapped reliably to this CAP method stream.
- The GP INSTALL parameter field is now retained and passed to the static
  applet-install entry as a Java byte array, offset 0, and byte length. The last
  APDU trace carries `C9 00`; ARM and RISC-V wrapper builds pass after the
  change, but runtime behavior still needs physical validation.
- The VM now records invocation of the imported JC305 `Applet.register()` or
  `register(byte[], short, byte)` virtual method during install. GP INSTALL will
  not persist the selectable AID if Java install returns without that call.
  This does not yet persist the Java applet object or prove the method mapping
  at runtime.
- Next: implement/reload applet lifecycle state and make AID/object persistence
  atomic, then wire SELECT/process dispatch. Retry hardware validation when the
  USB device is exposed to this container.

## Objective

Implement `plan.md` for CHOUPI on Pico 2, following root `AGENTS.md`. The user
has connected the Pico 2 and requested real-board bring-up. Use JCAlgTest
v1.8.2 and preserve the provided source trees.

## Latest continuation

- On 2026-10-12, CCID applet AID SELECT now resolves and runs `Applet.select()`
  before returning applet FCI; a false result maps to `6999`. New AID SELECT and
  CCID power-off run `Applet.deselect()` before clearing the selected context.
  The concrete class resolver uses internal method token 7 for `select` and
  token 5 for `deselect` (JC 3.0.5 imported tokens have their own remapping).
  ARM/RISC-V builds and the physical cycle passed. The applet path itself was
  not exercised because re-enumeration moved USB to `001:011` and `pcsc_scan
  -r` still finds no reader. `Applet.process()` and APDU buffer delivery remain
  the next implementation step. Commit: `256986e`; hashes are in `progress.md`.

- On 2026-10-12, added top-level interpreter return capture to `Stack` and the
  `areturn`, `sreturn`, and `ireturn` handlers. A root method now records its
  result instead of trying to push it onto a nonexistent caller frame; nested
  calls retain their prior return behavior. This is groundwork for observing
  `Applet.select()`'s boolean result. ARM and RISC-V wrapper builds pass, and
  the physical cycle passed its finite CHOUPI runtime check. That self-check
  does not execute a Java root return, so return capture still needs applet or
  dedicated runtime evidence. Commit: `0061646`; hashes are in `progress.md`.

- On 2026-10-12, corrected ordinary `invokevirtual` dispatch to resolve through
  the receiver instance's concrete package/class virtual-method table. The VM
  now reads the declared method header to locate the receiver on the operand
  stack, checks for underflow/null before dispatch, remaps imported method
  tokens, then looks up the override from the runtime class. ARM and RISC-V
  wrapper builds pass. `./tools/cycle choupi` passed both build gates, entered
  BOOTSEL through CDC reset, flashed ARM, and passed the 18-second serial check
  with repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`.
  This runtime check does not execute a loaded applet override; PC/SC still has
  no reader, so no GP install or applet dispatch test was possible. The public
  method resolver change is committed as `fa54b76`; progress entry records the
  source and UF2 hashes.

- Latest check on 2026-10-12: the Pico enumerates as `2e8a:0009` at USB
  `001:003`; `pcscd.service` is active, but `pcsc_scan -r` reports no reader.
  The node ACL currently grants `claude` read/write access but has no `pcscd`
  entry, so the earlier ACL grant expired when USB re-enumerated. The
  repository-installed ACL helper must be rerun for the current node before
  GPPro can resume. Do not run it through sudo autonomously; the repository
  AGENTS instruction forbids sudo in autonomous cycles.
- Retried `./tools/build-pc` through the repository wrapper. Java Card API
  compilation and CAP conversion completed successfully (converter reported
  zero errors). The host build then stopped before the CHOUPI C++ target because
  Cargo could not resolve `index.crates.io` to fetch uncached `ipc-channel`
  0.13.0. The “34 native methods” ROM-mask summary is informational, not this
  build failure. Full log: `/tmp/pico2-build-pc.log` (outside the repository;
  not an archived project artifact).
- Next: restore the `pcscd` ACL using the already-installed helper, then rerun
  the exact JCAlgTest v1.8.2 INSTALL against the current firmware. In parallel,
  continue implementation work that can be validated by ARM/RISC-V wrappers;
  the PC/vpcd golden-test track remains network-blocked until `ipc-channel` is
  available.

- On 2026-10-12, implemented the JC 3.0.5 `NativeImplementation.arrayFindGeneric`
  byte-array search path with null, bounds, and array-type checks. ARM and
  RISC-V wrapper builds passed. The physical `./tools/cycle choupi` passed its
  two build gates, CDC BOOTSEL recovery, ARM flash, and 18-second serial check
  with repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`.
  The current native implementation intentionally handles byte arrays only;
  wider primitive-array semantics have not been implemented or runtime-tested.
  Log `artifacts/logs/cycle-choupi-20261012T034427Z.log`, SHA-256
  `4a3ba30db076432c0a0da7cb19f23b6726b248f57682defbc4fd278bf720bfa9`; ARM UF2
  SHA-256 `8f3099f3779eddd46e8e84fa3ff20415ac2aad13b4709cc01c9d9c233155a1dd`.
- On 2026-10-12, added a borrowed-`Context` `Interpretor` entry for a virtual
  method offset and initial receiver/reference locals, plus a C bridge for the
  retained selected-app context. Both `./tools/build arm` and
  `./tools/build riscv` passed. `./tools/cycle choupi` passed after the
  repository flash wrapper recovered via CDC 1200-baud BOOTSEL; the 18-second
  board capture repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. This primitive has not been called by SELECT or
  `process()`; lifecycle method lookup, APDU object/buffer setup, and PC/SC
  testing remain next. Log `artifacts/logs/cycle-choupi-20261012T033945Z.log`,
  SHA-256 `c6a964c2ddeef80cf2e7d790633feff3aafd1cf623bdbcf68cf5f9509c090ec7`;
  ARM UF2 SHA-256
  `e588e7c926aaf022e632c052a1a18129f6e9d6b572c4a12c65483cbd036073e8`.

- The imported class offset byte-order fix is on the board and passed both target
  builds, flash, and finite runtime checks. A bounded v1.8.2 GP INSTALL still
  resets the card after the final `84E60C00`. New watchdog instrumentation
  reports stage 26 with packed context `00820301`: resolved package ID 0, raw
  package token `0x82` (external import index 2), class token 3, and interface
  flag 1. Thus the superclass walk is interpreting a framework interface as a
  class; the exact `new` operand/initial allocated class is still unknown.
  Logs: `artifacts/logs/gp-install-import-context-20261011.txt` and
  `artifacts/logs/cdc-import-context-20261011.txt`. Both builds pass and the
  normal serial runtime self-check passes. Trace commit: `1731f4f3da58751d7ffbb2fe8bfecdfcf260c6c1`. Next add a watchdog origin field for
  the allocated class package/index and inspect the `new` operand/field walk.

- Corrected imported exported-class offset byte order in
  `resolveClassref()`. Both ARM and RISC-V wrapper builds passed; the elevated
  flash wrapper reset through CDC 1200 baud, entered BOOTSEL after 2 seconds,
  flashed to 100%, and reported `FLASH:PASS`. The 8-second serial monitor
  passed repeated runtime checks. PC/SC and GlobalPlatformPro listing worked;
  the exact JCAlgTest v1.8.2 CAP LOAD completed (`9000`), but final INSTALL
  `84E60C00` again stalled and reset the device after 16.9 seconds. Captured
  watchdog marker `opcode=8F count=1 stage=17 context=00000003` shows imported
  class lookup now reaches Class component offset 3, beyond the previous bad
  `0x0300` index. No successful INSTALL status or applet SELECT was observed.
  Fix commit: `86b96a3c2aefb14dbf698257271fa0ae90c58f9b`.
  Logs: `artifacts/logs/gp-install-class-offset-endian-fix-20261011.txt`,
  `artifacts/logs/cdc-class-offset-install-capture-20261011.txt`, and
  `artifacts/logs/cdc-class-offset-endian-fix-20261011.txt`. Next add markers
  after `resolveClassref()` dynamic checks, through `classref2class()`, and
  around the superclass field-size return; rebuild both targets, flash, and
  retry once.

- Extended the `new` allocation breadcrumbs into `Heap::addInstance`, the
  `JC_Instance` field array setup, and imported superclass resolution. ARM and
  RISC-V builds passed; ARM UF2
  `353781d080f00aa25da77990dc4eb6868b69dfcb2e00ea8fbe83a82c44d07af9` flashed
  successfully and the 8-second serial monitor passed the normal runtime
  test. GPPro again accepted all CAP LOAD blocks and stalled on INSTALL; after
  recovery, the 5-second monitor had no watchdog marker from this attempt. The
  prior build's saved marker is still
  `pc=101C4EFA opcode=8F count=1 stage=6`, which places the stall inside
  `getInstanceFieldsSize` immediately before `classref2class` resolves the
  superclass. The new image reports resolver stages 11-17 and the imported
  class/token context if a marker survives the next reset. GPPro trace:
  `artifacts/logs/gp-install-superclass-resolution-20261011.txt`; CDC capture:
  `artifacts/logs/cdc-superclass-stage-recovery-20261011.txt`. The CAP remains
  uninstalled. Next resume with the board in application USB, run the bounded
  install once, and inspect the new stage/context; if the watchdog marker is
  again absent, capture serial immediately on its re-enumeration before
  changing/resetting the board.
- The latest diagnostic flash adds a persistent substage marker for `new`. Both
  ARM/RISC-V builds passed, the `f38acbf...` ARM UF2 flashed, and the 8-second
  CDC capture passed the normal runtime test. A bounded v1.8.2 GP INSTALL again
  loaded all CAP blocks (`9000`) then caused the 15-second hardware watchdog to
  reset the board. The reopened CDC capture reported
  `WATCHDOG_RECOVERY ... pc=101C4EFA opcode=8F count=1 stage=2`: the first
  `new` bytecode completed constant-pool class resolution and wedged inside
  `Heap::addInstance`. The board recovered in application mode, but INSTALL
  returned no status word and JCAlgTest remains uninstalled. Logs:
  `artifacts/logs/gp-install-substage-20261011.txt` and
  `artifacts/logs/cdc-install-stage2-recovery-20261011.txt`. Next instrument
  the `JC_Instance` constructor and superclass field-size walk inside
  `Heap::addInstance`; then rebuild both architectures, flash, and retry once
  with CDC capture active.
- Startup polling fix, commit `ca9bba37aeb458c848bcfd85b4e301a3351801e`, is
  now confirmed on hardware. The Pico appeared in RP2350 BOOTSEL (`2e8a:000f`);
  `./tools/env ./tools/flash build/arm/pico2_choupi.uf2` reached 100% and
  returned `FLASH:PASS`. After the application rebooted, USB enumerated as
  `2e8a:0009`, CDC became available, and an 18-second monitor captured repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` with no `FAULT` or
  `TEST:FAIL`. The authorized PC/SC helper restored USB access, `pcsc_scan -r`
  listed `Pico 2 CHOUPI CCID`, and GlobalPlatformPro `--list` connected over
  T=1 and reported the ISD `OP_READY` with no applet instances.
- Retried the exact supplied JCAlgTest v1.8.2 CAP. SCP03 authentication and all
  CAP LOAD blocks returned `9000`; the final authenticated `84E60C00` INSTALL
  then stalled. After about 17 seconds the host got
  `SCARD_E_READER_UNAVAILABLE`; `2e8a:0009`, `/dev/ttyACM0`, and the PC/SC
  reader all disappeared from the VM. Trace:
  `artifacts/logs/gp-install-retry-20261011.txt`, SHA-256
  `6287cb1c09e0855913307e521eddaa48a3cbe60baa2133c3033ef9ac28125f0b`.
  No `9000` response, GET STATUS entry, or SELECT confirmation was received;
  the applet remains uninstalled/unconfirmed. Next: restore BOOTSEL/application
  USB passthrough, then inspect the latest persistent watchdog breadcrumb and
  isolate the runtime operation reached by INSTALL before attempting another
  CAP install.
- Startup enumeration follow-up, commit `ca9bba37aeb458c848bcfd85b4e301a3351801e`:
  disabling the SDK IRQ worker also removed the only TinyUSB polling during
  the early `stdio_usb_connected()` wait. The CHOUPI startup now calls
  `tud_task()` during that wait before testing CDC. ARM and RISC-V wrapper
  builds pass. The candidate `build/arm/pico2_choupi.uf2` (SHA-256
  `e6b0802c22f6a07fe1a908782429c22027ed047666c67d7a331a00d5ffc78f8c`) is not
  yet flashed. The prior image is stuck in its pre-enumeration wait, so no USB
  node is visible; an escalated `tools/flash` call is waiting for BOOTSEL
  reattachment. Next flash this candidate, then capture CDC, confirm CCID stage
  7/PCSC reader discovery, and retry GP INSTALL. No applet install is confirmed.
- Newest fix, commit `a62514564543790ad4d0a5719c81b7d5638c1e1e`: CHOUPI now
  disables Pico SDK's TinyUSB IRQ worker and services `tud_task()` and deferred
  CCID commands serially in the main loop. ARM and RISC-V builds passed. The
  cycle flashed the image, but the monitor could not find a CDC node after USB
  re-enumeration. After the user reinserted the board in BOOTSEL, the repository
  flash wrapper reported 100% and `FLASH:PASS`; the board then disappeared from
  the VM USB bus again, so runtime and CCID behavior are not yet confirmed.
  Last `lsusb` showed only the VirtualBox tablet and root hubs. The preceding
  image showed CCID stage 6 after two requests, indicating responses were
  queued but not completing; the new single-context loop is intended to fix
  that endpoint race and the prior one-second servicing delay. Next: when Pico
  USB is visible in the VM, capture the finite serial log, confirm stage 7 and
  PC/SC reader discovery, then repeat the exact JCAlgTest v1.8.2 GP INSTALL.
  Do not report installation unless GPPro returns `9000` and GET STATUS/SELECT
  confirms the applet.
- Follow-up after the user reattached the Pico: it appeared in BOOTSEL as
  `2e8a:000f`; `picotool info -a` confirmed `boot type: bootsel`, and another
  repository-wrapper flash completed at 100%. Immediately after the requested
  application reboot, `./tools/monitor --seconds 18` found no CDC node and
  elevated `lsusb` showed only the VM's root hubs and tablet. `VBoxManage` is
  absent in this guest, so it cannot capture the application's new `2e8a:0009`
  identity itself. Next required host action is to attach `2e8a:0009` in the
  VirtualBox USB menu; leave the board running CHOUPI. Then continue monitor,
  PC/SC, and GP INSTALL checks.
- Latest install watchdog diagnostic build (`artifacts/logs/cycle-choupi-20261011T145623Z.log`) passed ARM/RISC-V builds, flashed to 100%, and passed the normal CHOUPI runtime test. It adds a five-second interpreter-loop deadline and watchdog scratch breadcrumbs for the last raw INSTALL opcode. GlobalPlatformPro v1.8.2 `--install` still fails after sending INSTALL: the host sees `SCARD_F_COMM_ERROR`, CDC output stops, and pcscd loses the reader although USB remains `2e8a:0009`. Scratch recovery did not print because the event may not have been a watchdog reboot or USB reattachment lost the logger. The cycle wrapper's three picotool attempts and CDC 1200-baud fallback also failed on the stalled image. Latest failed APDU trace is `artifacts/logs/gp-install-watchdog-scratch-20261011-apdu.txt`; latest captured CDC trace is `artifacts/logs/gp-install-watchdog-scratch-20261011.txt`.
- Immediate next action: recover/re-attach the Pico USB device, then query pcscd and capture a fresh finite CDC log. If the breadcrumb is present, map its XIP PC to the CAP/VM handler. Otherwise instrument the CCID callback stages in persistent watchdog scratch and determine why the APDU callback stops servicing USB. Do not report the applet installed; no INSTALL `9000` has been observed.
- The watchdog diagnostic implementation is recorded in `progress.md` under `wp7-install-watchdog-trace`; code commit is `d86489e` and the progress SHA follow-up is `6d708a8`.
- The `wp7-gp-install-method-execution` integration code is committed as `7315ae1` with its progress entry. It wires authenticated INSTALL into the CAP install-method interpreter, implements the zero-valued memory native required by JCAlgTest, and fixes CAP handler-table method-offset translation and method visibility checks. Both architecture builds and the latest CHOUPI cycle passed, but GPPro INSTALL still wedges the CCID path before replying. The unrelated `arrayFindGeneric` signature edit in `choupi/src/jni.cpp` and pre-existing root `CMakeLists.txt` / `SKILL.md` changes remain unstaged and preserved.
- The callback-safe image (`build/arm/pico2_choupi.uf2`, SHA-256 `418c69e4e4c11f82e0866cf98479e9c1e61244e298044a7c1110ee9950900504`) is now flashed. The cycle entered BOOTSEL with CDC 1200-baud reset after picotool reported no reset interface, flashed to 100%, and passed the finite CHOUPI runtime test. Log: `artifacts/logs/cycle-choupi-20261011T152212Z.log`, SHA-256 `4ab97132130d7c775e40dcd5c1a54a79b93542aa215d7d6a88caf7cd22ee4db5`.
- GlobalPlatformPro v1.8.2 authenticated with SCP03 and transferred every CAP LOAD block successfully (`9000`), then sent authenticated `84E60C00` INSTALL. The board did not answer within 60 seconds; GPPro timed out with status 124. With serial writes suppressed, the previous recursive CDC output hypothesis is not sufficient to explain the stall. The reader disappeared afterward although `2e8a:0009` remained enumerated; the authorized PC/SC helper restored ACCESS_GRANTED but `pcsc_scan` still found no reader. `picotool reboot -f -u`, CDC 1200-baud reset, and `usbreset 2e8a:0009` did not recover the firmware. GPPro trace: `artifacts/logs/gp-install-callback-safe-20261011.txt`, SHA-256 `c3c4dbf12611b67c7f8bcfa4a531dececac7d35babe810d01ca188ccf8e13746`.
- Follow-up inspection found that Pico SDK `stdio_usb.c` runs `tud_task()` from a low-priority IRQ, and the CCID `driver_xfer` called `process_message()` inline. Commit `21d7b94` now queues complete CCID messages from that callback and calls `process_message()` via `choupi_ccid_task()` in the main loop. ARM/RISC-V wrapper builds pass; candidate image SHA-256 is `f34f79fce442792fb099e00977521c73e3d03f7892d7f9483cb34e4038016fab`. The new image was not flashed: a full cycle attempt could not enter BOOTSEL after the 45-second wait, three picotool requests, and CDC 1200-baud fallback. Log: `artifacts/logs/cycle-choupi-20261011T153015Z.log`, SHA-256 `6c0412947bb6d39e26210f5ff214ca26bae5709bd3b9e7190978d244e56dde1f`.
- Immediate next action: detach/reattach the Pico USB device in VirtualBox or power-cycle it, then flash the queued-CCID build and repeat the bounded v1.8.2 INSTALL attempt. This architecture change remains unverified on hardware. Do not claim the applet installed; the last INSTALL did not return `9000` and GET STATUS did not confirm an instance. Latest hardware-validation record commit `53adb84`.

- Added composite USB CDC + CCID firmware to the ARM CHOUPI image. The board
  enumerates as `2e8a:0009` with CCID interface 2. After both architecture
  builds passed, `./tools/env ./tools/cycle choupi` flashed the image and the
  runtime emitted `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`.
  Direct libusb CCID IccPowerOn, XfrBlock, and IccPowerOff exchanges succeeded;
  details are in `artifacts/logs/ccid-probe-20261011.txt`. XfrBlock still
  returns a deliberate `6D00` placeholder. There is no GlobalPlatform or Java
  Card APDU dispatch and no runtime CAP install yet. The host has the
  GlobalPlatformPro JAR artifacts, but `pcscd`/`pcsc_scan` are not installed.
- Next: inspect the CHOUPI VM entry points for APDU dispatch and whether it can
  instantiate/select the ROM-masked AlgTest applet. Wire CCID XfrBlock through
  that path before attempting a real GPPro install; preserve the current static
  ROM-mask install as a working fallback.
- CCID transport step committed as `ffa3ddf`.
- APDU lifecycle audit is in `artifacts/logs/apdu-lifecycle-audit-20261011.txt`.
  The current boot path runs a single static starter, the interpreter's
  non-static entry is a TODO, `APDU.getBuffer()` returns null, and applet
  registration/selection is absent. GlobalPlatformPro `--list` exits with
  `SCARD_E_NO_SERVICE`; only libpcsclite is installed, not pcscd. Next work is
  WP3: implement APDU buffer + applet lifecycle dispatch and identify a
  non-root PC/SC daemon/driver route for host validation.
- The APDU lifecycle audit is committed as `fea69ed`.
- A user-local PC/SC daemon attempt is recorded in
  `artifacts/logs/pcsc-user-local-20261011.txt`. `pcscd`/`libccid` were
  downloaded and extracted under `/tmp`, but unprivileged pcscd cannot create
  `/run/pcscd` (`Permission denied`). No system packages were installed and no
  daemon is running. Continue firmware work; retry GPPro once an unprivileged
  or system PC/SC socket is available.
- The user-local PC/SC experiment is committed as `43059dc`.
- After the CCID VID/PID plist arrays were aligned, the next PC/SC scan still
  found no reader. Direct inspection showed the Pico absent from the VM USB bus
  entirely (only root hubs and VirtualBox tablet in `lsusb`; no 2e8a sysfs
  device; picotool saw no RP-series device). See
  `artifacts/logs/pcsc-usb-absent-20261011.txt`. Reattach the Pico in VirtualBox
  and verify `lsusb -d 2e8a:0009` before repeating `pcsc_scan -r`.
- This passthrough diagnosis is committed as `f9efb4a`.

- Improved `tools/flash` to wait up to 120 seconds for an RP-series USB device.
  VirtualBox had stale USB passthrough until the user toggled the device off and
  on; afterward sysfs showed `2e8a:000f` RP2350 BOOTSEL and picotool flashed the
  watchdog candidate to 100%. When the VM stalled, the watchdog reset into ROM
  BOOTSEL; `picotool info -a` confirmed boot type `bootsel`.
- Added a bounded trace of instructions after `putfield_a`, which showed opcode
  `0xAD` (`getfield_a_this`) requesting repeated 36,870-byte persistent-array
  buffers. Persistent non-transient arrays use flash for element I/O, so their
  backing storage now keeps only the tag metadata; transient arrays retain full
  buffers. The subsequent dual-build CHOUPI cycle passed and emitted
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. Latest log:
  `artifacts/logs/cycle-choupi-20261011T090019Z.log` (SHA-256
  `a093b8d548205e6a11dba34954509d6667fc9947cf14c18116f6f92a217f8cf7`); flashed
  UF2 SHA-256 `48f170eb971f3e9f33505eeb0d40934eced68ad8b46c6f80a31248ad8741e3cb`.
- The rebuilt JCAlgTest v1.8.2 CAP archive has a different outer ZIP hash, but
  all 10 Java Card `.cap` component files match the supplied archive byte for
  byte, including `Method.cap`. The current ARM UF2 embeds the ROM-mask image
  generated from those JCA components, and the Pico's CDC output repeatedly
  reports `TEST:PASS choupi_runtime`. This is a static ROM-mask installation,
  not a GlobalPlatform APDU install. The firmware still has no APDU/CCID
  transport or runtime CAP installer.

## Current state

- The root repository tracks only the plan and RP2350 references at the
  starting revision. `choupi/` and `AlgTest_JavaCard/` are user-provided,
  untracked source trees; preserve them and commit only files belonging to a
  completed step.
- The CHOUPI C++ VM source is present. The OS matches pinned revision
  `654f374067b2e7c1571fec1410adcd1e41a60c96`; the ROM mask matches
  `ac9f75253d476a8b2a838d459727df6e7a4a5107`. The Java Card API starts from
  pinned revision `d490faf358d4740def8eaf60017d5cc367f7383e` with local patch
  commit `d570ce3` aligning Framework, Security, and Crypto package export
  metadata with JCAlgTest v1.8.2 imports. They remain nested Git checkouts
  inside the user-provided untracked `choupi/` tree. The OS's
  STM32CubeF4 submodule is not initialized; it is specific to the old STM32
  target.
- The user selected JCAlgTest v1.8.2. `AlgTest_JavaCard/src305` identifies
  itself as v1.8.2 and the matching `AlgTest_v1.8.2_jc305.cap` is present.
  Read-only Java Card SDK CapDump inspection confirms CAP format 2.1, package
  AID `4A43416C6754657374`, applet AID `4A43416C675465737431`, four imports
  (`A0000000620001` v1.0; `A0000000620102`, `A0000000620101`, and
  `A0000000620201` v1.6), size 131,538 bytes, SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  Generated API JCA packages match these identifiers and versions. The v1.8.2
  `src305` Java sources also convert to JCA with those imports; the ROM-mask
  generator now succeeds with all 21 API/applet JCA packages after nested
  ROM-mask parser fix `af1091315fd7fd17baeae773ecf7553fd785ae37`. Its
  Makefile's removed Maven assembly goal was corrected in nested commit
  `f2a3f0e8eb945e3fa027f359be78844cf1306f42`, and
  `./tools/env make -C choupi/javacard-sdk/rommask` now passes. The resulting
  source-derived image is `build/choupi-rommask/flash-algtest.bin` (currently
  524,300 bytes, SHA-256
  `967a883ebf87955c74af84265218685414f82eaf584cab7e60651d719d11eda3`). The
  rebuilt CAP archive hash differs from the supplied archive hash, but a
  component-wise comparison confirms all 10 `.cap` entries match byte for
  byte. ROMMask adapts the package for CHOUPI storage; the current UF2 includes
  this ROM-mask image.
  Filtered CapDump output: `artifacts/logs/capdump-v182-20261010.txt`.
- The root Pico build now has a `choupi_vm_core` object target that cross
  compiles all 34 platform-independent CHOUPI C++ translation units for both
  RP2350 ISA builds. `tools/build` stages the generated source-derived JCA
  header and requires it before claiming a successful CHOUPI-core build.
  Successful wrapper builds still produce only the `pico2_trng` UF2; the VM
  core is not linked, no Rust OS implementation is part of the image, and no
  APDU/CAP installation route exists yet. The object build suppresses four
  legacy diagnostic classes from upstream sources (`ignored-qualifiers`,
  `address-of-packed-member`, `unused-parameter`, and `array-bounds`); this
  needs review before treating the build as warning-clean.
- A repository-local toolchain is installed under `.local-toolchain/`; use
  `./tools/env <command>` to select it. It includes CMake/Ninja, ARM and
  RISC-V compilers, stable Rust with the Cortex-M33 target, Pico SDK 2.3.1,
  pioasm, and picotool. See `.agent/TOOLCHAIN.md` for versions and hashes.
  Setup and the hardware report are committed as `db99639`.
- The board was initially identified in BOOTSEL mode. Read-only
  `picotool info -a` reported RP2350 A4, QFN60, 2048 KiB flash, and an
  existing RISC-V image named `pico2_btc_miner`. Before replacing it, saved a
  read-only 2 MiB backup at
  `artifacts/backups/flash-before-smoke-20261010.bin`, SHA-256
  `49a650c2093df2e2d528ec4ef9adce788db95133c7303b5f96d852d8b5ecc078`.
  Flash size remains a device report pending independent physical confirmation.
- Added the root Pico SDK smoke target and `tools/build`, `tools/cycle`, and
  `tools/monitor`. The final `./tools/env ./tools/cycle arm` passed: both ARM
  and RISC-V built with warnings as errors, picotool reported the expected UF2
  families, and the board emitted repeated `TEST:PASS random_stream_varies`
  and `TEST:SUMMARY pass=1 fail=0` records during the bounded 8-second serial
  capture. The board currently runs this ARM smoke firmware. This check only
  confirms changing output from `get_rand_32()`; it does not assess entropy or
  execute CHOUPI. Final log: `artifacts/logs/cycle-arm-20261010T053019Z.log`.
  Implementation commit: `9bd89df`.
- Maven 3.10.0 and Eclipse Temurin JDK 8u504 are installed under
  `.local-toolchain/`. The Java Card API Ant `build` passes with JDK 8. The
  Maven wrapper is visible through `./tools/env`. `tools/doctor` checks the
  pinned `capmap-free` submodule and JC 3.0.5u3 API kit explicitly. The CAP
  parser is restored from the user-provided GitHub mirror at its exact pinned
  commit; the nested `.gitmodules` URL now points to that mirror, and the
  submodule remote was synchronized. Maven's local repository is redirected
  inside `.local-toolchain/`; required artifacts have been resolved and the
  ROM-mask build passes.
- Diagnostics and continuity setup are committed as `1616b59`.
- Fixed `JCVMArray::fill()` to populate each element and removed the ignored
  return qualifier from `size()`. The focused strict host test passed with
  `TEST:PASS jcvmarray_fill`; committed as `0d293ae`.
- Implemented the `putstatic_a` path and nested persistent reference-array
  element tags in the VM persistence layer. Syntax checks passed using a
  temporary variadic `ffi.h` shim; this does not prove ABI compatibility or
  runtime persistence behavior. The step is committed as `ab1957b`; see the
  latest progress entry.
- Added guards for transient/non-reference persistent arrays and index bounds
  in the child-reference writer. Its syntax check and the JCVM array test both
  pass; runtime persistence has not been verified. Commit: `7e04f97`.
- Implemented Java-object `athrow` table dispatch across current and caller
  frames, saving each frame's opcode address and restoring package context as
  frames unwind. Syntax checks pass, but no runtime tests are possible yet;
  native `Exceptions` mapping and uncaught status-word handling are still open.
  Commit: `476a938`.
- Fixed the typed class/interface matcher to return `FALSE` on non-match; the
  expanded syntax check passes. CAP hierarchy runtime tests remain pending.
  Commit: `6f7f1bc`.
- Implemented the object-deletion capability fallback natives (`FALSE` and
  no-op). Syntax check passes; target builds and runtime native dispatch tests
  remain unavailable. Commit: `512337f`.
- Repaired persistent instance class/tag handling and reference-field get/set.
  Syntax checks pass, but `JC_Instance::getFields()` still cannot reconstruct
  a persistent object's typed field vector; this remains open for WP5. Commit:
  `a740f5e`.
- Fixed persistent instance class-index initialization and reference get/set
  behavior. Syntax passes; filesystem round-trip behavior is not validated.
  Commit: `a05147e`.
- Persistent instances now retain their original filesystem tag separately
  from their field storage, and `getFields()` lazily rebuilds typed field
  values, including split-word ints and reference/array values. Setters also
  keep cached field types and values aligned with serialized records. Syntax
  and the array regression test pass. No persistence round-trip test is
  possible without the filesystem implementation; missing child records still
  cannot be typed from the current class metadata API. Commit: `5263602`.
- The interpreter no longer spins forever after a VM-internal exception.
  `run()` returns `0x6F00` on an uncaught VM exception, including an `athrow`
  with no matching handler, and the C runtime entry returns that status. This
  still does not map VM exceptions to Java objects or send a status over APDU.
  Commit: `ff6a4b8`.
- The `JCSystem` transaction information natives report zero active depth and
  zero commit capacity, matching the runtime's lack of transaction support.
  AlgTest's system-information path queries maximum commit capacity. Mutating
  transaction calls remain unimplemented. Syntax and array regression checks
  pass; target builds remain unavailable. Commit: `53660ca`.
- Fixed `JCVMArray` ownership semantics for mutable copies and moves while
  preserving non-owning read-only CAP views. `JC_Array` now lets its backing
  buffer owner free storage exactly once, allocates the full tagged persistent
  array payload, reports transient element counts correctly, and recognizes
  `CLEAR_ON_RESET`. Focused copy/move and transient-size tests pass; the latter
  uses AddressSanitizer with leak detection disabled due an environment-level
  LeakSanitizer failure. Commit: `6fa7232`.
- Added an RAII active-context binding around interpreter execution, implemented
  `isTransient`, and implemented primitive transient boolean, byte, short, and
  int array factories. Invalid lengths and clear events map to the specified VM
  exceptions. Syntax and array tests pass; target builds remain unavailable.
  Object transient arrays still lack reference-type metadata in their native
  signature, and actual reset/deselect clearing awaits lifecycle hooks. Commit:
  `e56d14f`.
- Added `JC_Array::clearTransientData()` and `Heap::clearTransientArrays(event)`;
  the heap clears only transient arrays registered for the requested event. A
  focused ASan test verifies reset and deselect selection. The clear primitive
  is not yet called by lifecycle dispatch because that dispatch does not exist.
  Commit: `6d3630e`.
- Implemented native primitive array copy for `arrayCopyRepack` and its
  non-atomic counterpart. The path checks ranges and matching array types and
  snapshots input to preserve overlap semantics. Focused ASan tests cover an
  overlapping byte copy plus bounds/type errors. Reference copying remains
  unsupported; transaction atomicity is unavailable. Commit: `8e92ec1`.
- Implemented `arrayCompareGeneric` for byte arrays with range checks, signed
  Java byte comparison, and ternary results. The focused ASan test covers equality,
  signed-byte boundary ordering, and normal less-than ordering. SensitiveResult
  tracking is not implemented. Commit: `92ef957`. A follow-up checked the
  Java Card primitive ordering and corrected the comparison to signed bytes;
  that correction is committed as `e088119`.
- Implemented byte `arrayFillGeneric` and its non-atomic counterpart. The
  helper repeats bytes from the supplied pattern offset and returns the target
  end offset. A focused ASan test covers repetition; compatibility with the
  missing Java Card SDK binding has not been verified. Commit: `bb5d2b8`.
- Expanded the host native test to call the transient byte-array factory,
  `isTransient`, and invalid length/event paths using a live `Context`. The
  linked ASan test passes, closing the earlier native runtime-test gap for
  these paths. Java exception dispatch and actual card lifecycle events remain
  untested. Commit: `64fe919`.
- Expanded factory coverage to boolean, byte, short, and int arrays, including
  both valid clear events and `isTransient(null)`. All pass in the linked ASan
  host test. Object array creation remains unsupported. Commit: `bacd8fc`.
- Implemented `selectingApplet()` as `FALSE` in the current runtime because it
  has no applet selection or pending-select state. The linked ASan test passes;
  this must become stateful with lifecycle dispatch. Commit: `3446422`.
- Implemented `getAssignedChannel()` as channel 0 for the single-channel
  transport. The linked ASan test verifies it; logical channels are not
  supported. Commit: `657e589`.
- Implemented the SensitiveArrays integrity-array capability probe as
  `FALSE`, matching the absence of integrity storage and verification in this
  runtime. The linked ASan test now checks the capability result; operations
  that require integrity arrays remain unsupported. Commit: `32c88dd`.
- Implemented `isIntegritySensitive(Object)` as `FALSE` for non-null objects
  and `NullPointerException` for null, since this runtime never creates or
  tracks sensitive arrays. Focused host tests cover both cases; commit:
  `3ef6da7`.
- The documentation finalization commit `bff7c23` accidentally included 63
  previously untracked provided source files from stale index entries. Their
  worktree copies were hash-verified and retained; corrective commit `ae783a2`
  removes them from the index again. Keep the provided source trees untracked.
- The requested `.agent/SESSION_CONTINUITY.md` was absent and has now been
  created. Root `progress.md` will be append-only.

## Next action

Continue the RP2350 port and determine whether runtime GlobalPlatform
installation is required beyond the verified static ROM-mask deployment. The
current ARM UF2 has been flashed and CHOUPI is running on the Pico 2. All CAP
component payloads from the supplied v1.8.2 archive match the JCA-rebuilt
package byte for byte; ROMMask adapts storage by relocating static fields. CDC
is debug output only, so GPPro cannot install or select packages over this
firmware. Do not claim APDU/CCID functionality. Preserve `.local-toolchain/`,
`AlgTest_JavaCard/`, and the CHOUPI source tree.

## Latest validation

- Current board bring-up: `bash -n tools/common.sh tools/build tools/cycle
  tools/monitor`, ARM build, RISC-V build, and `./tools/env ./tools/cycle arm`
  all passed. The 8-second capture emitted repeated
  `TEST:PASS random_stream_varies` and `TEST:SUMMARY pass=1 fail=0`; log:
  `artifacts/logs/cycle-arm-20261010T053019Z.log`.
- Dependency restoration: OS and ROM-mask checkouts match pinned full commits;
  the API is at its pinned commit plus local patch `d570ce3`. The
  `capmap-free` submodule is at `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`,
  and its nested URL points to the user-provided GitHub mirror. The doctor and
  ROM-mask Maven build pass. The source-derived ROM mask was generated, but
  CHOUPI firmware compilation and CAP installation remain unavailable.
- `bash -n tools/doctor`: passed. `./tools/env ./tools/doctor` reports all
  host tools, Pico SDK, API kit, and top-level sources present; expected exit
  status is 1 for missing `rommask/capmap-free/pom.xml`.
- Fresh `./tools/env ./tools/monitor --seconds 8` on the connected board
  returned 0 with 8 repetitions of `TEST:PASS random_stream_varies` and
  `TEST:SUMMARY pass=1 fail=0`; no `FAULT` or failure records appeared.
- JCAlgTest v1.8.2 CAP inspection: JC 3.0.5u3 CapDump returned 0. CAP format
  2.1, package/applet AIDs, four imports, size, and SHA-256 are recorded above;
  this exposed three API export mismatches. After patching API build settings,
  successful JCA output has exact matching identifiers and versions. Full
  ROM-mask import resolution remains unverified.
- Java Card API: `JAVA_HOME=$PWD/.local-toolchain/java8` with that JDK's `bin`
  prepended to `PATH`, then `./tools/env ant -f
  choupi/javacard-sdk/javacard-api/build.xml clean build`: `BUILD SUCCESSFUL`;
  Framework, Security, and Crypto generated JCA headers match the CAP imports.
- Latest VM syntax check: `TEST:PASS vm_core_syntax`; existing upstream
  warnings remain.
- Latest `./tools/test-jcvmarray`: `TEST:PASS jcvmarray_fill`.
- `./tools/build arm` and `./tools/build riscv`: wrapper absent; both calls
  returned status 127.
- Latest interpreter exception/status step: `./tools/check-vm-syntax` passed
  with `TEST:PASS vm_core_syntax`; `./tools/test-jcvmarray` passed.
- Latest array storage step: `./tools/test-jcvmarray` passed its fill and
  copy/move checks; `./tools/test-jcarray` passed `TEST:PASS
  jcarray_transient_size` under AddressSanitizer (LeakSanitizer disabled).
- Latest active-context/transient-native step: `./tools/check-vm-syntax`,
  `./tools/test-jcvmarray`, and `./tools/test-jcarray` passed; both target build
  wrapper calls returned 127 because `tools/build` is absent.
- Latest transient-clear primitive step: `./tools/test-jcarray` passed both
  `jcarray_transient_size` and `jcarray_clear_events`; syntax and JCVM array
  tests passed; both target build wrapper calls returned 127.
- Latest native array-copy step: focused ASan tests passed overlap and error
  cases; syntax and JCVM array tests passed; both target build calls returned
  127 because the wrapper is absent.
- Latest native array-compare step: focused ASan comparison checks, including
  signed byte ordering, passed; syntax checks passed; target build wrappers
  returned 127 because the wrapper is absent.
- Latest native array-fill step: focused ASan pattern repetition, syntax checks
  passed; ARM/RISC-V build wrappers returned 127 because the wrapper is absent.
- Latest transient-native runtime test: linked ASan test exercised factory,
  `isTransient`, and invalid argument paths; syntax checks passed. Target build
  wrappers were not rerun because only test files changed.
- Latest primitive-factory coverage: linked ASan test passed all four primitive
  array types and null handling; only test code changed.
- Latest selecting-applet fallback: linked ASan test and syntax check passed;
  ARM and RISC-V build wrappers remain absent.
- Latest basic-channel fallback: linked ASan test and syntax check passed;
  ARM and RISC-V build wrappers remain absent.
- Latest integrity-array capability fallback: linked ASan test passed and
  emitted `TEST:PASS jni_integrity_arrays_capability`; syntax check passed.
  ARM and RISC-V builds could not run because `tools/build` is absent.
- Latest integrity-sensitive query: linked ASan test passed and emitted
  `TEST:PASS jni_integrity_sensitive_query`; syntax check passed. ARM and
  RISC-V builds could not run because `tools/build` is absent.
- Firmware runtime validation is still pending for CHOUPI: the connected board
  runs the standalone smoke firmware and its bounded USB serial test passed.
- Latest physical cycle: `./tools/env ./tools/cycle arm` rebuilt ARM and RISC-V,
  flashed the ARM smoke image, and captured 8 seconds from `/dev/ttyACM0`.
  Output had eight passing `random_stream_varies` checks and eight passing
  summaries; no `TEST:FAIL` or `FAULT` appeared. Log:
  `artifacts/logs/cycle-arm-20261010T060906Z.log` (SHA-256
  `7bccbc03281e0f93c2e4068856903570b648bb44f51f452576162222072b7dd9`). The
  image is still only standalone smoke firmware; CHOUPI/JCAlgTest are not yet
  running on the board.
- Toolchain setup: `bash -n tools/env` passed. `./tools/env rustup target
  list --installed` includes `thumbv8m.main-none-eabihf`; CMake/Ninja, both
  compiler prefixes, and picotool version checks passed. `./tools/env
  ./tools/doctor` now passes after restoring `capmap-free`. Maven 3.10.0 is
  available through `./tools/env`; the API build passes with Temurin 8u504.
  CAP-parser Maven installation and ROM-mask packaging now pass. Root smoke
  wrappers pass for both ISAs. Board queries and
  cycles required reviewed escalation.
- Latest post-mirror board refresh: `./tools/env ./tools/build arm` and
  `./tools/env ./tools/build riscv` both returned 0 and validated their
  respective UF2 families. The 8-second `./tools/env ./tools/monitor
  --seconds 8` returned 0 from `/dev/serial/by-id/usb-Raspberry_Pi_Pico_...`,
  emitting eight `TEST:PASS random_stream_varies` and
  `TEST:SUMMARY pass=1 fail=0` pairs. Archived capture:
  `artifacts/logs/monitor-after-capmap-20261010.log` (SHA-256
  `9f4162fb209ebfb97a74d03517a55fe9d6e279b0354999ba7d7bbe7d6a76b5dd`). The
  board remains on the smoke image; this does not validate CHOUPI or CAP load.
- Previous deployment-readiness attempt found the parser missing and Bitbucket
  requested credentials; CHOUPI PC configuration also failed because Boost
  headers and `program_options` are absent. The user supplied a public GitHub
  mirror; `capmap-free` is now initialized at the exact pinned commit
  `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`, and `./tools/env
  ./tools/doctor` passes. With reviewed network access, the capmap Maven install
  and ROM-mask generator package both pass.
  Existing CHOUPI supports host/STM32 only; its OS lacks an RP2350
  architecture, and the root Pico SDK target remains the smoke app. USB
  CCID/APDU and on-card CAP installation are absent, so no CHOUPI/CAP flash was
  attempted. See latest `progress.md` record.

## Latest bring-up state (2026-10-10)

- The root now links `pico2_choupi.uf2` for `rp2350-arm-s`. `tools/build` also
  cross-compiles the VM core for `rp2350-riscv`; `tools/cycle choupi` enforces
  both builds before flashing. ARM and RISC-V build gates pass.
- The CHOUPI image contains `build/choupi-rommask/flash-algtest.bin`, generated
  from JCA rebuilt from the v1.8.2 `src305` sources. It is not the exact user
  CAP. The image starts on the Pico 2 and emits CDC logs, but startup fails:
  latest serial log `artifacts/logs/cycle-choupi-20261010T084216Z.log` reports
  `PUTFIELD_A` object `0x0000`, value `0x0007`, `NullPointerException`, and
  `6F00`. The wrapper's `TEST:FAIL choupi_runtime` is expected and must not be
  treated as a pass.
- Source debugging in the user-provided, untracked `choupi/` tree has locally
  fixed field constant-pool token resolution, primitive `NEWARRAY` bytecode to
  internal array-type mapping, and new arrays remaining heap-backed until
  persisted. It also has temporary exception and field-reference traces. These
  edits are not in a root commit; preserve them and never stage the full
  provided source tree. Current lead: inspect starter/app initialization and
  why it dereferences element 0 of a new reference array before assignment.
- `gp`, `pcsc_scan`, and `opensc-tool` are absent. The installed TinyUSB SDK
  provides CDC but no CCID class, and the current firmware has no APDU/card
  manager. The exact supplied CAP
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` remains uninstalled (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`). Do
  not report the source-derived prelinked package as installation of this CAP.
- New root changes for the CHOUPI target and cycle wrapper plus the failed
  board experiment are awaiting the incremental root commit. Recent successful
  board access required reviewed USB escalation; the cycle log is archived.

## Latest startup/storage diagnostics (2026-10-10)

- Added bounded VM exception output and a one-line heartbeat per million
  bytecodes in `choupi/src/interpretor.cpp`. Both ARM and RISC-V builds pass.
- Hardware diagnostic sequence found the earlier `6F00` was an `IOException`
  during `putstatic_a`: the Pico overlay exhausted its 24 file slots while
  persisting CHOUPI startup state. Fixed tables at 48, 128, and 512 slots also
  exhausted. A dynamic `realloc` prototype ran out of memory. Current
  `src/choupi_pico2.cpp` uses 2,048 compact metadata records plus a static
  128 KiB packed byte arena to avoid per-file heap fragmentation.
- The current packed-arena UF2 builds for both architectures. Its board cycle
  did not capture BOOT, heartbeat, or TEST output during 18 seconds; a separate
  60-second monitor was also silent. The last `picotool reboot -u -f` returned
  success, but the host then lost USB enumeration and `usbreset` could not find
  the device. The current image is not hardware-validated. Latest cycle log:
  `artifacts/logs/cycle-choupi-20261010T094700Z.log`; UF2 SHA-256
  `8990a465f60f89b9baa724d1c05856605e77d7224e090f6f16b4a711f172c8b8`.
- The exact CAP remains `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`, SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`, and is
  still not installed. Firmware currently has CDC only, with no APDU/CCID or
  CAP loader. Do not call the embedded source-derived ROM mask an installation.
- Next action: restore Pico USB/BOOTSEL access, capture the sparse interpreter
  heartbeat on the packed-arena image, and continue diagnosing startup before
  adding an exact-CAP installation route. Preserve all pending worktree edits.

## Latest continuation update (2026-10-10)

- User put the Pico 2 into BOOTSEL. `picotool info -a` confirmed RP2350 A4, chip ID `0xd92e743d5f640c0f`, and `boot type: bootsel`.
- The 2,048-record overlay image flashed successfully and confirmed startup still exhausted the overlay at 2,048 files / 10,394 bytes. A runtime null-reference optimization did not relieve the file-count growth.
- Increased overlay metadata capacity to 4,096. Both `./tools/build arm` and `./tools/build riscv` pass. The latest hardware cycle flashed the image and failed at 4,096 files / 20,754 bytes, while writing a six-byte record at tag length 27. The VM reports exception id 5 at opcode `0x7F`, then status `6F00`. Log: `artifacts/logs/cycle-choupi-20261010T095555Z.log`, SHA-256 `5f0fe823120a2db9cfc7fd09598c2a17106942f82df07883a8c6688615fbd9e4`.
- Next action: trace recursive persistence at deeper tags, identify cyclic/shared object expansion, and fix the serialization path rather than keep multiplying overlay slots. Continue requiring both builds before flash. The exact JCAlgTest v1.8.2 CAP is still not installed; firmware has CDC only and lacks an APDU/CCID or CAP loader.
- Preserve all unrelated staged or untracked work. Commit the current storage experiment with this progress record, then continue diagnosis.

## Latest parent-compressed storage update (2026-10-10)

- The Pico filesystem shim now uses a 16,384-entry hash index and 256 KiB
  packed arena. Tags compress repeated field-index byte pairs; most nested
  records refer to the parent tag by a stable index slot. Record updates use
  the stored key encoding when accounting for replaced bytes.
- The required nested OS prototype update is committed as
  `choupi/os` commit `f994d5e` (`Expand persistent filesystem tag buffers`).
- Both `./tools/build arm` and `./tools/build riscv` pass for the current
  source. First hardware capture after parent compression passed previous
  arena/file limits but failed on `SecurityException id=29` while descending
  a 61-byte persistent path (`artifacts/logs/cycle-choupi-20261010T101555Z.log`).
- Root implementation/progress commit: `f2da673` (`Compress nested CHOUPI
  persistence keys`).
- Increasing tag buffers to 128 bytes passed builds, but hardware then
  reported `*** PANIC *** Out of memory` shortly after BOOT
  (`artifacts/logs/cycle-choupi-20261010T101744Z.log`). Instance tags were
  changed to compact repeated-pair encoding in a 68-byte inline buffer with
  overflow storage. Two later cycles returned no CHOUPI serial lines after
  `MONITOR:START` (`101940Z`, `102236Z`). The 8-second independent monitor was
  also silent. Read-only `picotool info -a` sees a USB serial connection but
  reports no accessible BOOTSEL device.
- User has been asked to reconnect the Pico 2 while holding BOOTSEL. Continue
  independent source and continuity work while awaiting that response; do not
  flash until both architecture builds pass. The exact CAP remains
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`)
  and is not installed. Firmware exposes CDC only; no APDU/CAP loader exists.
- Next action: after BOOTSEL re-enumerates, reflash the current compact-tag
  build and capture serial output; shrink the static overlay arena if startup
  still exhausts the available RAM. Preserve unrelated changes and append a
  progress record plus a new commit for each validated step.
- The four latest cycle logs are being archived under `artifacts/logs/`; their
  outcomes are `SecurityException` at tag length 61, `Out of memory`, and two
  silent captures. No passing CHOUPI runtime evidence exists yet.
- The storage implementation is in root commit `f2da673`; the four capture
  files and this progress record are in root commit `87c1bbe`.
- A follow-up guard now rejects odd-length candidates before compressed-pair
  comparisons and prevents uncompressed 128-byte keys from overflowing the
  record header. ARM/RISC-V builds pass. Hardware validation is pending a
  physical BOOTSEL reconnect; current RP2350 is only visible as USB serial.
- The guard is in root commit `9b73e3a`.

## Latest recursive-persistence attempt (2026-10-10)

- User confirmed the Pico 2 was reinserted in BOOTSEL. `picotool info -a`
  identified the RP2350 A4 and confirmed `boot type: bootsel`; the CHOUPI
  image flashed successfully. Serial then showed 16,384 overlay records in use
  at 171,176/262,144 bytes, followed by Java Card exception id 5 and `6F00`.
- The repeated reference `0x0094` at increasing tag depths indicates cyclic
  object serialization. Added a persistent-object alias record to break a
  reference back to an active/canonical object, and mapped alias reads back to
  the target's persistent tag. ARM and RISC-V builds pass.
- A first cycle with a full-tag recursion stack panicked `Out of memory`
  (`artifacts/logs/cycle-choupi-20261010T110210Z.log`). Reduced the tracker to
  pointers and moved the canonical compact tag into each instance. The next
  cycle did not capture runtime output; afterward `picotool info -a` and
  `lsusb` both failed to initialize libusb. Its log
  `artifacts/logs/cycle-choupi-20261010T110427Z.log` is byte-identical to an
  older silent capture, so it does not prove which firmware ran.
- Current UF2 SHA-256 is
  `a0682558fd222e51b7e93b434a46dea99a0887853ef05c73ee6378793e80fef8`.
  Source changes and a failed experiment record are in root commit `654219f`. Preserve
  unrelated changes in the tree.
- Follow-up: `JC_Instance::setPersistentTag` now also marks a newly serialized
  instance persistent so later field updates write through its canonical tag.
  Both target builds pass again; current UF2 SHA-256 is
  `9cc85d9051fb9f55d52682d4f772ea16316d144fb123686381415203bad85fc6`.
  This refinement is in root commit `06fdf87` and has not been run on hardware.
- Next action: reconnect the Pico while holding BOOTSEL, verify it with
  `picotool info -a`, then flash/capture again. If runtime starts, verify that
  aliases reduce startup expansion and that no `TEST:FAIL`, `FAULT`, or timeout
  occurs. The exact CAP remains
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`),
  uninstalled; the current CDC firmware has no APDU/CCID or CAP loader.

## JCAlgTest package compatibility check (2026-10-10)

- Exact requested CAP remains `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`,
  SHA-256 `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
- Its import component requests `java/lang` AID `A0000000620001` v1.0,
  `javacard/framework` AID `A0000000620101` v1.6,
  `javacard/security` AID `A0000000620102` v1.6, and `javacardx/crypto`
  AID `A0000000620201` v1.6. Generated JCA metadata under
  `build/choupi-algtest/rommask-input/` reports matching package IDs and
  versions. This verifies package-description compatibility only; it does not
  prove CHOUPI runtime method resolution or CAP installation support.
- `./tools/env picotool info -a` still fails with `ERROR: Failed to initialise
  libUSB`; no flash was attempted. Host inspection found no GlobalPlatformPro,
  GPShell, PC/SC service, or smart-card reader tools. The current CHOUPI image
  exposes CDC only and has no APDU/CCID transport or CAP loader, so the CAP is
  not installed.
- Resume after the board re-enumerates in BOOTSEL: pass both architecture
  builds, flash/capture CHOUPI and resolve startup failures, then implement or
  establish an APDU and CAP installation path. Do not claim the CAP installed
  until successful on-device installation is observed.

## Latest Pico 2 flash attempt (2026-10-10)

- `./tools/build arm` and `./tools/build riscv` both pass for the current
  source. The normal sandbox cannot initialize libUSB, but elevated read-only
  `picotool info -a` detected the RP2350 as a CDC device running firmware.
- The authorized repository command `./tools/cycle choupi` was run with USB
  device access and rebuilt both targets. `picotool load -f -x` returned zero
  and the cycle proceeded to the serial monitor, but its log has no flash
  progress/completion message. The silent capture therefore does not establish
  whether the UF2 was written or which image is running. Candidate ARM UF2
  SHA-256 is `9cc85d9051fb9f55d52682d4f772ea16316d144fb123686381415203bad85fc6`.
- The 18-second monitor emitted no serial bytes and the cycle exited nonzero
  (`artifacts/logs/cycle-choupi-20261010T111732Z.log`, SHA-256
  `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`). This
  is byte-identical to earlier silent captures. The post-cycle device query
  sees an RP2350 CDC interface but does not confirm BOOTSEL or the running
  image. Do not treat this as a passing runtime test or as evidence of a usable
  JCVM.
- Next investigate early startup/CDC silence (including whether firmware has
  reached the USB log path) before continuing the persistent-alias experiment.
  Both architecture builds must still pass before any additional flash. Once
  CHOUPI runs, it still needs APDU/CCID or another host protocol and a CAP
  loader; the exact JCAlgTest v1.8.2 CAP has not been installed.

## Reduced SRAM arena experiment (2026-10-10)

- Changed the Pico overlay capacity in `src/choupi_pico2.cpp` to 8,192 file
  records and 128 KiB of record bytes. Both architecture builds passed; the
  CHOUPI load command returned zero and the cycle proceeded to capture, but
  the log did not show flash progress/completion and the running image was not
  independently verified.
- Capture `artifacts/logs/cycle-choupi-20261010T112137Z.log` had no serial
  bytes; its SHA-256 is `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`.
  A separate 8-second pyserial capture with DTR asserted was also silent.
- The ARM ELF `.bss` is now 232,860 bytes (was 462,236); `.data` is 9,920
  bytes. An RP2350 CDC device enumerated afterward, but it still emitted no
  BOOT/runtime message and the running image was not verified. More heap did
  not resolve the silence. Candidate UF2 SHA-256 is
  `0a265f608a5c5ea44f90bcb46ced51a308b8d909967ffd8071dd89ef6f6d675d`.
- Next flash the known-good TRNG firmware with the cycle wrapper to confirm
  serial capture, then trace CHOUPI startup before further overlay tuning. Keep
  both architecture build gates before each flash. The exact v1.8.2 CAP is
  still not installed; CHOUPI has no APDU/CAP loading path.

## USB capture/BOOTSEL diagnostic (2026-10-10)

- `./tools/cycle arm` passed both architecture builds and ran the ARM TRNG
  flash command, but its 8-second capture had no `BOOT`/`TEST` output
  (`artifacts/logs/cycle-arm-20261010T112446Z.log`, SHA-256
  `62457db617d9013e2c91a67bf6cfb2a74b8600d7c50b952b091d8dd9fc4e7e13`). This
  is a second independent image with the same silent serial outcome; the image
  currently running is therefore uncertain.
- `picotool info -a` after the cycle finds only the RP2350 CDC interface, not
  BOOTSEL. `picotool reboot -f -u` returned “The device was asked to reboot
  into BOOTSEL mode,” but a follow-up query still found CDC. USB descriptor
  reads also reported `Resource temporarily unavailable` for device status.
- Next required hardware action: physically unplug the Pico 2, hold its
  BOOTSEL button while reconnecting USB, then verify `boot type: bootsel` with
  `./tools/env picotool info -a`. Only then rerun the CHOUPI cycle. Both build
  gates must pass before flashing. The exact CAP remains uninstalled.

## GlobalPlatformPro client prepared (2026-10-10)

- Built the official GlobalPlatformPro `v26.06.04` source (commit
  `5f3f4a2c7f6ae147da1351955deaeec9872eff47`) in the ignored local toolchain.
  Self-contained CLI: `.local-toolchain/globalplatformpro/tool/target/gp.jar`,
  SHA-256 `38b464deabb20f48ba4fa1379354e41e913f635a537c9ea5cd2b1e919e0f908c`.
  `--help` and `--version` both work with Java 21; Maven build succeeded with
  tests skipped.
- No PC/SC socket/service or card reader is present. Current Pico CHOUPI
  firmware provides CDC and does not implement CCID/APDU, so GPPro is ready
  but cannot connect to it. Do not claim CAP installation until there is an
  APDU path and on-device install confirmation.

## BOOTSEL flash confirms CHOUPI heap panic (2026-10-10)

- Physical BOOTSEL was confirmed with `picotool info -a` (RP2350 A4, 2 MB
  flash). The next `./tools/cycle choupi` passed ARM and RISC-V builds, flashed
  the ARM UF2 with visible 100% progress, rebooted it, and captured
  `BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi` followed by Pico SDK
  `*** PANIC *** / Out of memory`. Log:
  `artifacts/logs/cycle-choupi-20261010T115001Z.log` (SHA-256
  `e01b623fc94299571e1f72fde115acf6b0a7e3bbbe853f6ae9bc9954775c2903`).
- Source inspection traced the panic text to `pico_malloc`'s allocation guard.
  Added temporary serial stage markers around `fs_init()` and `runtime()` in
  `src/choupi_pico2.cpp`; ARM and RISC-V builds passed. The following cycle
  could not flash because the board had left BOOTSEL; its monitor repeated the
  previous panic and its log is byte-identical to an earlier silent capture
  (`9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`).
- Current stage-marker UF2 hash: `b49438a577524d767b9c859fa2a0de49475c2bce461f29bc97b5c11b04322c65`.
  `picotool reboot -f -u` also failed to produce a BOOTSEL device. The user
  was asked to hold BOOTSEL while reconnecting USB; verify boot mode before
  the next cycle. The stage-marker image is not yet flashed. After identifying
  the allocation phase, address SRAM pressure and repeat both build gates and
  the real board cycle. CAP installation is still blocked by missing APDU/CCID
  transport and loader support.
- Added Pico SDK allocator diagnostics to the ARM CHOUPI target: it now emits
  failed allocation sizes and allocations crossing `0x20070000`, without
  logging ordinary low-address allocations. ARM and RISC-V wrapper builds
  passed. Diagnostic UF2 SHA-256:
  `19532c092fa19bcc7dd1e43e8d5aa84450755e112a02225ec12b697836f3cc9b`.
  This build has not yet been flashed; BOOTSEL reconnect is still required.

## Heap allocation localized on the Pico 2 (2026-10-10)

- After the user reconnected the board in BOOTSEL, `./tools/cycle choupi`
  passed both ARM/RISC-V build gates, visibly flashed 100%, and captured the
  diagnostic image. Log: `artifacts/logs/cycle-choupi-20261010T133620Z.log`
  (SHA-256
  `9846ef5043fbd4b74f828a4a4da3260b538933db5455d9bf8027c56c7e51a9fb`).
- `fs_init()` completed; the failure occurs after `CHOUPI:STAGE runtime:start`.
  The Pico SDK reports a 36,870-byte allocation succeeding at
  `0x20068FB0..0x20071FB6`, followed by another 36,870-byte allocation failure
  and `*** PANIC *** / Out of memory`. The request call site is not yet known.
- Reduced the overlay byte arena from 128 KiB to 64 KiB (saving exactly
  65,536 bytes of static SRAM) to increase room for the second runtime
  allocation. Both architecture builds passed. New candidate UF2 SHA-256:
  `a590f7b12e9a9ebc66ffa202921e9878cc12b8457d667ed65f29aba97046c9f5`;
  `.bss` ends at `0x2002B56C`, down 65,536 bytes from `0x2003B56C`.
- This reduced-arena image has not yet been flashed. The board is running the
  heap-diagnostic image; verify BOOTSEL after physical reconnection, then run
  `./tools/cycle choupi`. Do not call the runtime a pass unless the cycle
  captures the expected TEST summary. CAP installation remains unimplemented.

## 64 KiB overlay hardware attempt (2026-10-10)

- The user reconnected the Pico in BOOTSEL; `picotool info -a` confirmed RP2350
  A4 and BOOTSEL. The 64 KiB arena candidate passed both architecture build
  gates, flashed with visible 100% progress, and was captured by the monitor.
- It still fails after `runtime:start`: `malloc 16390 2006EFE0->20072FE6`,
  then `malloc 36870 failed to allocate memory`, followed by
  `*** PANIC *** / Out of memory`. Log
  `artifacts/logs/cycle-choupi-20261010T212007Z.log` SHA-256
  `4ea1a3f80e2bb24e5665d4e3e3e178e76fa6cc858a3e20fd5d3d31acee11761c`.
- UF2 hash `a590f7b12e9a9ebc66ffa202921e9878cc12b8457d667ed65f29aba97046c9f5`.
  This cycle was a runtime failure, not a pass. Board is now running CHOUPI;
  re-enter BOOTSEL physically before a subsequent flash. Exact CAP remains
  uninstalled.
- Candidate follow-up: the 32 KiB overlay plus 8-byte index build passes both
  ISA wrappers and produces `build/arm/pico2_choupi.uf2` with SHA-256
  `46f2b453d33ada46fa1cda9e681f18648f53e00ac966b76bc9aa1faf49bda137`.
  `picotool info -a` sees a USB serial app but no BOOTSEL device; a software
  `picotool reboot -f -u` request did not yield BOOTSEL enumeration. This
  candidate has not been flashed. Next action: hold BOOTSEL while reconnecting,
  confirm BOOTSEL, then run `./tools/cycle choupi`. The runtime and CAP install
  remain incomplete.
- A later `./tools/cycle choupi` passed both build gates but produced no serial
  bytes; its log is byte-identical to a prior silent capture. A separate
  20-second capture with the reader attached before `picotool reboot -f -a`
  was also silent. No successful flash or runtime is confirmed. The Pico
  enumerates as USB serial, not BOOTSEL. Next action remains a physical
  BOOTSEL reconnect, verify with `picotool info -a`, and repeat the cycle.
- A direct `picotool reboot -F -u` also reported a BOOTSEL request, but
  subsequent `picotool info -a` still found only the USB serial app (exit 249).
  This software recovery attempt did not flash anything; physical BOOTSEL is
  required for the next try.
- The board was later confirmed in BOOTSEL. The 32 KiB candidate flashed at
  100%, passed both architecture builds, completed `fs_init()`, and no longer
  runs out of memory. Its runtime fails with `VM_EXCEPTION id=5 opcode=0x87`
  (`IOException` in `putfield_a`) at `0x10043C01`; that address maps into
  ROM-mask tag `0114`, data offset `0x1B1`. Log SHA-256 is
  `d2b068c660382e1b828004aea92132bd3f8f0cdccac0086a17369fd3c63d5d2f`.
  Next: instrument `write_at()` rejection causes, rebuild both architectures,
  then rerun `./tools/cycle choupi`. The user-provided v1.8.2 CAP is not yet
  installed; CAP transport/loader work remains after runtime startup.
- Added `CHOUPI:FS_WRITE_REJECT` diagnostics in `write_at()` for missing records,
  out-of-range writes, index exhaustion, or overlay capacity; both architecture
  builds pass. UF2 SHA-256:
  `1de6bce7ed146f0a4a709f3180650305c6d62eb2a902d0eb95d774dd49b88e13`.
  The cycle could not enter BOOTSEL from the running CHOUPI app and captured no
  output, so this image is not yet flashed. Physically hold BOOTSEL while
  reconnecting and verify before the next cycle.
- Added load/store phase exception diagnostics around persistent
  `setField_Reference()`; ARM and RISC-V builds pass. The diagnostic candidate
  UF2 hash is `18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`.
  The board still reports USB serial, not BOOTSEL; flash the candidate after a
  physical BOOTSEL reconnect. CAP installation remains pending.
- User asked to enter BOOTSEL without asking them. Tried `picotool reboot -f`
  and `-F`, direct `picotool load -F`, CDC 1200-baud reset, and direct libusb
  class/interface request to reset interface 2. None changed the live device
  from USB serial (same bus/device 1:116); serial read remained empty. Do not
  claim the diagnostic image was flashed. The current VM appears unresponsive
  after its IOException (inference); the new diagnostics are compiled but await
  a responsive reset path. Continue software analysis without asking the user.

## USB recovery update (2026-10-11)

- Retried `picotool reboot -F -u`; it returned success but the device remained
  in USB serial mode. `usbreset 001/116` then reported `No such device`; the
  Pico disappeared from `lsusb`, and its xHCI root port now reports
  `not attached`. `/dev/ttyACM0` is absent. The per-port xHCI sysfs
  `disable` control is not writable to this account. No flash occurred.
- Keep the built diagnostic UF2 ready (`18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`).
  Continue source analysis; do not claim CHOUPI or JCAlgTest hardware success.

## Persistent field default handling (2026-10-11)

- `JC_Instance::getFields()` now checks `fs_exists()` before trying to decode a
  persistent field type. Missing per-field records are initialized as
  `FIELD_TYPE_UNINITIALIZED` with zero value, matching the existing reference
  getter's null behavior and avoiding an `IOException` for an absent record.
- ARM and RISC-V wrapper builds pass. New ARM UF2 SHA-256:
  `2860767cb505cddcc6c6ec372eeceb4b7d5bd5d0e77026c909ae816294e3de03`.
  The firmware has not been flashed: Pico USB port 3 remains `not attached`.
  Runtime behavior is unverified. On the next enumeration, enter BOOTSEL if
  available, flash through `./tools/cycle choupi`, and inspect the bounded log
  for the persistence phase markers and `TEST:SUMMARY`.

## Latest Pico field-load attempt (2026-10-11)

- The default-field candidate above was flashed from BOOTSEL at visible 100%.
  Both architecture builds passed. Runtime advanced through two reference field
  stores in package 20/class 57 and then stopped producing output after
  `CHOUPI:PUTFIELD_A phase=load index=0 package=20 class=121`. The 18-second
  cycle failed with no `TEST:SUMMARY`; a separate eight-second monitor was
  silent. Log: `artifacts/logs/cycle-choupi-20261011T053229Z.log`, SHA-256
  `909b217b6fa27f343c27dcf7bb3ec4f5c2f2909ca36bf25c2a335ddfeedd2134`.
- `picotool reboot -F -u` again left the app in CDC mode. A USB bus reset then
  removed it from the host bus. No BOOTSEL or serial device is currently
  enumerated.
- A new capped trace in `JC_Instance::getFields()` marks entry/exit of
  `fs_exists`, decoded type, and value completion. ARM and RISC-V wrapper builds
  pass. Candidate UF2 SHA-256:
  `a2deabc677b6672431386a1b0aef3eb05cc6debf906b1f0bc478d19f2b86f543`.
  It has not been flashed; use it after BOOTSEL enumeration returns.
- That trace reached persistent class 121 with a reported field count of 8,912.
  Each of fields 0–63 was processed far enough to complete its existence query;
  no full vector completion marker existed yet. A follow-up candidate adds
  bounded superclass-chain diagnostics to `getInstanceFieldsSize()` for
  package 20/class 121. Both builds pass; its UF2 SHA-256 is
  `5fa53345dbcabadbcaec146dc6aa07bc5e195f61fe320f49d6308e4fa8bedde4` and is
  unflashed. The board is again in CDC mode after the prior cycle.

## USB BOOTSEL retry and class-token investigation (2026-10-11)

- Retried `picotool reboot -F -u` with direct USB access. Picotool reported
  “The device was asked to reboot into BOOTSEL mode,” but the follow-up still
  found only the CDC app at bus 1/address 120 and no BOOTSEL device. No flash
  was attempted; the hierarchy-trace UF2 remains pending.
- Source inspection confirms `getClassInformation()` stores the CAP class
  component byte offset, and `getClassFromClassIndex()` uses that offset
  directly. Its companion `getClassRefFromClassIndex()` instead iterates
  class/interface records by ordinal. This inconsistency can affect persistent
  object class references. Fixed that helper to encode the stored byte offset
  in CAP byte order; both wrapper builds pass. Updated ARM CHOUPI UF2 SHA-256:
  `274b8128c4259b167c52bc4f363e96d88348dd1f9540f655932232d4f4ed568f`.
  The change does not explain the 8,912 field count by itself, since superclass
  traversal uses direct class refs.
- Next: after BOOTSEL is available, flash the corrected image, then collect the
  superclass trace. Do not use repeated USB bus resets; prior host reset
  attempts made the Pico disappear. The exact JCAlgTest CAP is still not
  installed and no APDU/CAP loader exists in the current firmware.

## Forced CHOUPI cycle after class-offset fix (2026-10-11)

- `./tools/env ./tools/cycle choupi` passed both wrapper build gates and invoked
  `picotool load -f -x` on the corrected ARM CHOUPI UF2. Picotool asked the
  device to reboot into BOOTSEL and waited, but the cycle log has no
  `Loading into Flash` progress and no “device was rebooted to start the
  application” confirmation. Treat the image as not flashed.
- The 18-second serial capture was empty, and its log SHA-256
  `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c` matches
  an earlier silent capture. Follow-up enumeration still sees CDC at bus
  1/address 120, not BOOTSEL. A force-mode `picotool info -a -f` also waited
  without yielding image metadata.
- Next: keep the corrected UF2 ready and continue offline class/persistent
  field analysis. No additional USB bus reset; the current app does not
  successfully service the BOOTSEL transition.
- A follow-up `picotool reboot -f -a` returned success, but an eight-second
  monitor capture after the application reboot was silent. Retrying
  `picotool reboot -f -u` then returned success without changing enumeration;
  `picotool info -a` still found CDC at bus 1/address 120 and no BOOTSEL.
  Therefore the application reboot did not recover a software BOOTSEL path.
- The user clarified they had not physically changed the board and asked for a
  reflash. Re-ran `./tools/env ./tools/cycle choupi`: both architecture build
  gates passed, but force-load again waited for BOOTSEL and printed no flash
  progress. The 18-second capture was silent; log SHA-256 again matched the
  prior empty log (`9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`).
  `picotool info -a` still sees only CDC at bus 1/address 120. The updated
  image is not confirmed flashed.

## BOOTSEL replug and persistent instance corruption trace (2026-10-11)

- The user replugged the Pico. `picotool info -a` then confirmed BOOTSEL and
  identified the previous `pico2_choupi` image. `./tools/env ./tools/cycle
  choupi` passed both builds and visibly flashed the current ARM candidate to
  100%, then rebooted into CHOUPI.
- The bounded runtime log is
  `artifacts/logs/cycle-choupi-20261011T061811Z.log`, SHA-256
  `09be7d9368d2d988d4d953f45daa37f5832b9991d152de1b1de424ceb7266d40`. It
  reaches the persistent load for package 20/class 121. Three hierarchy
  calculations each report one class with 15 declared fields, but the loaded
  `JCVMArray` reports 8,912 fields. It completes existence checks for fields
  0–63 before the 18-second capture ends without `TEST:SUMMARY`.
- Source audit found `Heap::addInstance(JC_Instance instance)` copied the
  object into `shared_ptr` using the implicit shallow copy. `JC_Instance` owns
  raw `fields` and overflow-tag pointers and its destructor frees them, so the
  by-value temporary can invalidate the heap copy. A move-only ownership fix
  has been applied and both wrapper builds pass; it remains unflashed because
  the board returned to CDC after the cycle. Updated ARM UF2 SHA-256:
  `92ddc5aa9497f3020a302a253003f6074ebfcdb656eff9b7196beb406e7cfb98`.
- Next: when BOOTSEL is available, run `./tools/cycle choupi` for the move
  ownership fix and confirm the field count is 15 and runtime reaches its test
  summary. JCAlgTest v1.8.2 still has no APDU/CAP installation path.

## Persistent field load fix and continuation trace (2026-10-11)

- User made the Pico available in BOOTSEL. `./tools/env ./tools/cycle choupi`
  rebuilt ARM and RISC-V successfully, flashed the ARM image to 100%, and
  rebooted the application. The ownership fix changed package 20/class 121's
  field vector from the corrupted length 8,912 to the expected 15; all field
  loads (indexes 0–14) completed. The 18-second cycle still failed without
  `TEST:SUMMARY`. Log:
  `artifacts/logs/cycle-choupi-20261011T070346Z.log`.
- Added a diagnostic candidate around `JC_Instance::setField_Reference()` for
  class 121. Both ARM and RISC-V builds pass. UF2 SHA-256:
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.
- Tried `picotool reboot -F -u`; it returned success, but `picotool info -a`
  still saw CDC, no BOOTSEL. `picotool load -f -x` then returned without any
  `Loading into Flash` progress or reboot confirmation. A finite serial monitor
  was silent. Treat this diagnostic candidate as unflashed.
- Next: use the candidate after BOOTSEL reappears, then inspect the class-121
  reference and whether execution reaches array lookup or persistent write.
  No JCAlgTest v1.8.2 CAP installation has occurred; firmware still lacks an
  APDU transport and CAP installer.

## Connected Pico CDC retry (2026-10-11)

- The user said the device was present. Host enumeration confirmed the RP2350
  at bus 1/address 124 as USB serial, while `picotool info -a` reported no
  BOOTSEL device. Both generic and explicitly addressed `picotool reboot` requests
  returned success text but the device stayed in CDC mode. Application reboot
  and finite monitoring also produced no serial runtime output.
- Re-ran `./tools/env ./tools/cycle choupi`. ARM and RISC-V build gates passed.
  Picotool requested a BOOTSEL reboot, waited, and showed no flash progress;
  serial capture was silent and the runtime gate failed. Log:
  `artifacts/logs/cycle-choupi-20261011T071557Z.log` (SHA-256
  `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`).
- `./tools/env ./tools/doctor` reports all host tools, Pico SDK, CHOUPI sources,
  and capmap-free present. The diagnostic UF2 remains ready at
  `build/arm/pico2_choupi.uf2`, SHA-256
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.
- Next: flash after the Pico appears in actual BOOTSEL mode, then inspect the
  new `PUTFIELD_A` continuation markers. The attached CDC interface alone does
  not expose a working software route into BOOTSEL in the current firmware.

## Flash and run reference-field continuation trace (2026-10-11)

- After the cycle and direct `picotool load` requests had waited without flash
  progress, a later `picotool info -a` found the device in BOOTSEL mode. The
  device had transitioned asynchronously; the by-id CDC node had disappeared.
- `./tools/env ./tools/cycle choupi` then passed both builds, visibly flashed
  `build/arm/pico2_choupi.uf2` to 100%, and rebooted the application.
- The trace log is `artifacts/logs/cycle-choupi-20261011T074012Z.log`, SHA-256
  `997029462d5e282bd5726b40c2985070f093fa3ca07dd2aad40fb19bd08937eb`. It
  confirms class 121 field vector length 15, all field values loaded, array
  lookups succeeding, and persistent store for field 0 returning. The run then
  stops emitting runtime output before `TEST:SUMMARY`.
- Current next step: add a capped interpreter instruction trace to identify
  where execution stalls after `PUTFIELD_A phase=stored index=0`. The flashed
  candidate hash is
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.

## Robust flashing wrapper (2026-10-11)

- Added executable `tools/flash`; `tools/cycle` now calls it after both builds.
  The wrapper runs force-load first, but requires picotool's explicit 100%
  flash and application-reboot messages. If force-load only requests BOOTSEL,
  it polls for BOOTSEL for up to 45 seconds, then retries reset requests with
  bounded 20-second waits and one CDC 1200-baud fallback.
- Script syntax validation passed. A full CHOUPI cycle passed ARM and RISC-V
  builds but the connected firmware remained CDC. The helper correctly failed
  instead of claiming that flash succeeded. Log:
  `artifacts/logs/cycle-choupi-20261011T074828Z.log`, SHA-256
  `bf11511b61eb8b41c6675c41ad9f8985945d20757fbab9bf2c7ea82f252bc05b`.
- The ARM link map includes both `tud_cdc_line_coding_cb` and
  `usb_reset_interface_control_xfer_cb`, but the current CHOUPI runtime is
  stalled and does not service either reset route. The candidate diagnostic
  UF2 is `build/arm/pico2_choupi.uf2`, SHA-256
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.
- Next: add an instruction-fed watchdog. On a watchdog timeout, the next startup
  should enter ROM BOOTSEL directly so future hung runs are recoverable by the
  host wrapper without a manual power cycle. The watchdog cannot rescue the
  currently installed image; it first needs one successful upload.

## Watchdog recovery candidate (2026-10-11)

- Added a 15-second RP2350 watchdog before VM startup. The interpreter refreshes
  it once per instruction via a Pico wrapper callback. If a handler blocks and
  the timeout resets the chip, startup checks `watchdog_enable_caused_reboot()`
  and calls `reset_usb_boot(0, 0)` before initializing USB CDC. This should make
  a future stalled VM enter BOOTSEL without depending on its USB task.
- ARM and RISC-V builds pass after routing the shared VM heartbeat through a
  board callback. The first direct hardware-header include failed because the
  shared object library lacks Pico SDK include paths; this was corrected.
- Candidate UF2 SHA-256:
  `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`.
  It remains unflashed: the current pre-watchdog firmware is present as CDC but
  does not service vendor reset or 1200-baud CDC reset requests. The robust
  `tools/flash` wrapper rejected the request-only force-load and exhausted its
  bounded recovery attempts. Latest log:
  `artifacts/logs/cycle-choupi-20261011T075539Z.log`.
- Next: flash this candidate once BOOTSEL is available. After a watchdog
  timeout, confirm the board reappears in BOOTSEL and that the host wrapper can
  flash it. Then resume the interpreter instruction trace.

## Final software recovery check (2026-10-11)

- Tried `picotool reboot -F -a`, waited 200 ms, then `picotool reboot -F -u`.
  Both commands printed their requested-mode success message, but an immediate
  `picotool info -a` still found the Pico in CDC mode at bus 1/address 126.
  The installed pre-watchdog runtime is not servicing its reset requests.
- The watchdog UF2 built for both architectures but remains unflashed. Its ARM
  image hash is
  `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`.
- Next: enter BOOTSEL once, run `./tools/env ./tools/cycle choupi`, and let the
  new watchdog provide autonomous BOOTSEL recovery if the VM stalls again.

## Latest continuation — CCID APDU exchange mode (2026-10-11)

- Changed the CCID class descriptor to `dwFeatures=0x00020042`: short APDU
  exchange plus automatic parameter configuration/negotiation. The old zero
  value declared character-level exchange while the firmware accepts complete
  APDUs through `XfrBlock`.
- Both architecture builds passed. The CHOUPI cycle flashed the updated ARM UF2
  and passed `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. The
  device's `lsusb -v` descriptor confirms short APDU mode and a 271-byte maximum
  CCID message. See `progress.md` and
  `artifacts/logs/cycle-choupi-20261011T101036Z.log`.
- Implementation and validation record committed as `4506ff7`; the final
  progress metadata follow-up records this SHA.
- PC/SC APDU transmission is still unverified. Flashing re-enumerated the Pico
  from USB node `001/016` to `001/018`; the ACL previously granted to `pcscd`
  did not persist, so pcsc_scan found no reader. The current libccid udev rule
  matches `ID_USB_INTERFACES=:020200:0a0000:0b0000:` and assigns group `pcscd`,
  but the live node still appears as `root:plugdev`; `udevadm test` confirms the
  expected group, so trigger the current device's rule and restart pcscd. Then
  test a raw APDU through GPPro. The current XfrBlock result remains placeholder `6D00`;
  applet dispatch, CAP install, and GlobalPlatform support are not implemented.

## PC/SC transport verified (2026-10-11)

- The user restored pcscd access after the Pico re-enumerated. `pcsc_scan -r`
  lists the Pico 2 CHOUPI reader. GlobalPlatformPro connected using T=1 with
  ATR `3B800181`, began a transaction, and sent `00A4040000`; the board returned
  `6D00` twice. The GPPro command exits 1 because this is the deliberately
  unimplemented XfrBlock placeholder response, not because PC/SC failed.
- This confirms USB CCID enumeration, activation, PC/SC transaction handling,
  and APDU-level XfrBlock request/response transport on the real board.
  Detailed output is in `artifacts/logs/pcsc-apdu-probe-20261011.txt` and the
  corresponding `progress.md` entry.
- Probe log and progress record committed as `eb6548f`; the metadata follow-up
  records this SHA.
- Next: find a minimal route from XfrBlock into the statically embedded
  JCAlgTest applet. Applet lifecycle and CAP loading remain unsupported; do not
  report the placeholder `6D00` as an applet response.

## GlobalPlatformPro list probe (2026-10-11)

- With PC/SC access restored, `gp --debug --list` connects to the Pico over
  T=1, receives ATR `3B800181`, and successfully transacts SELECT-by-empty-AID
  `00A4040000`. The board returns `6D00`, so GPPro exits 1 with
  `Could not SELECT default selected: 0x6D00 (Invalid INStruction)`.
- This verifies APDU transport but confirms AC1 is not yet met: no ISD select,
  GlobalPlatform card manager, applet registry, or SELECT dispatch exists.
  Trace: `artifacts/logs/gp-list-probe-20261011.txt`.
- Probe log and progress record committed as `e37ad51`; metadata follow-up
  records the SHA.
- Next: implement the minimal card manager/APDU lifecycle rather than treating
  the CCID layer as GlobalPlatform support.

## CAP applet AID lookup primitive (2026-10-11)

- Corrected `jc_cap_app::AID()` to read the CAP's one-byte AID length without
  applying a two-byte network-order conversion, which had included the first
  AID byte in the computed length.
- Added a bounded `jc_cap_applet_component::findByAID()` walker. It validates
  requested and stored AID lengths and ensures every variable-length applet
  record (AID plus install method offset) fits inside the component payload.
- `./tools/build arm` and `./tools/build riscv` pass. `git diff --check` passes.
  This is compile validation only: PC/SC service and libusb are unavailable in
  this VM session, and no hardware flash was attempted.
- No applet registry, SELECT, install execution, or APDU processing exists yet.
  Next integrate this lookup into the card manager and then build persistent
  interpreter contexts/lifecycle calls.

- The bounded CAP applet AID lookup implementation is committed as `52c7c7d`;
  a follow-up correction tracked its header as a regular file in `6f54e5e`.
  Both architecture builds passed for the implementation. The current session
  cannot access a PC/SC service or libusb device, so it has not flashed this
  source-only change.
- A direct read-only parse of the supplied v1.8.2 `Applet.cap` confirmed the
  component size is 14, with one applet AID `4A43416C675465737431` and install
  method offset `0x3DA3`; the variable-length traversal consumes the component
  exactly. Host hardware access in this session is unavailable (`lsusb` reports
  libusb initialization error `-99`, and the system bus/PCSC service is absent).

## Package-level AID resolution

- Added `FlashMemory_Handler::findAppletByAID()` to scan the persisted package
  bitmap and each installed CAP's Applet component, returning package ID and
  install-method offset. ARM and RISC-V builds both pass. The supplied
  JCAlgTest CAP's Applet component has been checked independently and matches
  the parser's size accounting.
- This lookup is not yet wired to CCID/SELECT, and it does not execute install
  or process methods. Implementing that requires persistent VM context/app
  instance state and a usable APDU buffer. `picotool info -a` currently fails
  at libusb initialization (`-99`), so no flash or hardware validation is
  possible in this session.
- The package-level registry lookup and its build record are committed as
  `fe7e12f`.

## Reflash after replug — 2026-10-11

- The updated ARM CHOUPI UF2 containing the AID parser and package-level
  registry lookup is now flashed. Both wrapper builds passed. Picotool force
  reset failed because it could not locate the reset interface; `tools/flash`
  then used CDC 1200 baud, entered BOOTSEL within 2 seconds, and loaded the UF2
  to 100%.
- The bounded hardware cycle passed with repeated `TEST:PASS choupi_runtime`
  and `TEST:SUMMARY pass=1 fail=0`, no `TEST:FAIL` or `FAULT`. Log:
  `artifacts/logs/cycle-choupi-20261011T111329Z.log`, SHA-256
  `354482ef6091f85a04b5192fea4a17574600f2523042b465a6410f72768b8009`.
  UF2 SHA-256: `ec437a81244d1729c4d6713825d50c79daec6252cb22041b009819cbca8b8a45`.
- Post-flash USB re-enumerated as RP2350 CDC bus 1/address 21. PC/SC has no
  active daemon/reader at present, so no post-flash APDU test was possible.
  Applet install/process lifecycle and APDU dispatch remain unimplemented;
  the registry lookup is not yet connected to SELECT.

## Faster flash recovery — 2026-10-11

- `tools/flash` now classifies `picotool load` failures. If picotool reports a
  missing reset interface, the wrapper goes directly to the known-working CDC
  1200-baud reset rather than waiting through 45+20 seconds of BOOTSEL polling.
  If libusb itself cannot initialize, it fails promptly instead of retrying
  inaccessible operations.
- Validated on the Pico 2: both builds passed, CDC fallback entered BOOTSEL in
  2 seconds, the UF2 flashed to 100%, and finite serial output passed with
  `TEST:SUMMARY pass=1 fail=0`. The failed reset-interface branch was exercised
  by the successful cycle. The libusb-init fast-fail branch was also verified
  without device access. Cycle log:
  `artifacts/logs/cycle-choupi-20261011T111923Z.log`, SHA-256
  `0eadba75a08908f07b30507e136d9546cdc5b1e5ad705de91898ed70857bc584`.
- The board still re-enumerates as CDC after flashing. PC/SC has no active
  daemon/reader, so APDU validation remains blocked; applet install/process and
  GlobalPlatform support are still not implemented.

## PC/SC after the latest flash

- Started `pcscd.service` without sudo; it is active. The Pico is present as
  `2e8a:0009` at bus 001/device 023, but `pcsc_scan -r` reports no reader.
  The previous per-node ACL did not follow re-enumeration. Current node:
  `/dev/bus/usb/001/023`. Once `pcscd` is granted read/write access to that
  node and restarted, rerun the PC/SC/GPPro probes. Do not use sudo from the
  autonomous cycle; the user previously supplied and ran the ACL command.

## Passwordless PC/SC recovery helper — 2026-10-11

- Added `tools/pico2-pcsc-reader-root`, which dynamically finds exactly one
  Pico 2 CHOUPI CCID (`2e8a:0009`), grants `pcscd` read/write access to its
  current USB node, and restarts `pcscd.service`. It rejects arguments and
  fails closed when the matching device is absent or ambiguous.
- Added `tools/install-pcsc-sudoers`. The user must run
  `sudo ./tools/install-pcsc-sudoers` once. It installs the helper as
  root-owned at `/usr/local/sbin/pico2-pcsc-reader` and creates a `visudo`
  checked NOPASSWD rule allowing the invoking user to run that helper with no
  arguments. No system file has been modified by this session.
- After setup, run `sudo -n /usr/local/sbin/pico2-pcsc-reader`, then
  `pcsc_scan -r`. Continue with GPPro/APDU probes after the reader appears.
- Shell syntax and standalone sudoers syntax validation passed. The setup is committed as `97c7eeb`.

## Installer visudo path correction — 2026-10-11

- The user's first run stopped before making system changes because the preflight
  looked for `/usr/bin/visudo`; this host provides `/usr/sbin/visudo`. Corrected
  the preflight path. `bash -n` and standalone sudoers parsing pass.
- Next: user reruns `sudo ./tools/install-pcsc-sudoers`; then invoke the helper
  and confirm `pcsc_scan -r` discovers the reader. Fix committed as `5421889`.

## GPPro after PC/SC recovery — 2026-10-11

- The user ran the installed recovery helper successfully; `pcsc_scan -r`
  finds `Pico 2 CHOUPI CCID`. An elevated GlobalPlatformPro 26.06.04 probe
  connected at T=1 (ATR `3B800181`) and sent default SELECT `00A4040000`;
  firmware replied `6D00`, and GPPro exited with invalid instruction. The
  trace is `artifacts/logs/gp-list-probe-20261011-reader-ready.txt`, SHA-256
  `14a11819fc90b44f3c4e3a9a9b3800e6904c9ca3472518718230a94dc2b54f21`.
- Transport and PC/SC are working; APDU dispatch remains a placeholder. Next
  implement default ISD SELECT (`9000`), build both targets, flash, and inspect
  GPPro's next command to guide incremental card-manager support. Probe record
  committed as `48a6656`.

## Default ISD selection and FCI — 2026-10-11

- The CCID driver now handles only `00 A4 04 00 00` as default ISD
  selection, returning minimal FCI for AID `A000000151000000` and `9000`.
  Other APDUs remain unsupported (`6D00`). Both architecture builds passed;
  the actual Pico 2 cycle flashed successfully and emitted
  `TEST:SUMMARY pass=1 fail=0` without `TEST:FAIL` or `FAULT`.
- GPPro 26.06.04 parsed the corrected FCI, detected the ISD AID, then sent
  SCP03 INITIALIZE UPDATE (`8050000008` + host challenge). That still returns
  `6D00`, so `gp --list` has advanced but does not pass. The first FCI attempt
  had an incorrect outer length (`6F0C`); corrected to `6F0A`.
- Latest cycle log: `artifacts/logs/cycle-choupi-20261011T114816Z.log`, SHA-256
  `b9afd73bf1c011b45c69944f3c0189d3f449cec2169823d1b8cd08555f9ad02d`.
  GPPro trace: `artifacts/logs/gp-list-probe-20261011-isd-fci-fixed.txt`,
  SHA-256 `da56ac7750d48a44cd75f5ce6949c7596bc26e98d4d38b6cccd9153fdc865cd7`.
- Next: implement SCP03 INITIALIZE UPDATE / EXTERNAL AUTHENTICATE using the
  default test keys, then implement GET STATUS and continue toward GP CAP load.
  Implementation and validation committed as `9e9e3ae`.

## Raw GP read-only probes — 2026-10-11

- GPPro raw APDU mode sent GET STATUS (`80F20000024F00`), GET DATA CPLC
  (`80CA9F7F00`), and GET DATA Card Data (`80CA006600`) without opening a
  secure channel. Each returned `6D00`; automatic discovery then SELECTed the
  default ISD and received its valid FCI and `9000`. No write command ran.
- Trace: `artifacts/logs/gp-raw-readonly-probes-20261011.txt`, SHA-256
  `8795b672b2dd211d2ccc18ac4f5e05579671b6f22f49adaa2d7752fbefb4df5b`. Next implement SCP03; then add tag-form GET STATUS
  and read-only GET DATA before any CAP loading. Probe record committed as `ab833a3`.

## SCP03 secure channel on hardware — 2026-10-11

- `src/scp03.cpp` derives SCP03 session keys and 64-bit cryptograms with
  AES-CMAC, validates EXTERNAL AUTHENTICATE, and authenticates subsequent
  commands. EXTERNAL AUTHENTICATE carries a trailing Le, so the firmware
  accepts both 21-byte case-3 and 22-byte case-4 forms.
- `gp --debug --list` exits 0 on the Pico: SCP03 EXTERNAL AUTHENTICATE returns
  `9000`; authenticated GET STATUS returns ISD `A000000151000000` in lifecycle
  `OP_READY`. GPPro's further registry probes return `6A88` for absent
  applets/domains.
- ARM and RISC-V wrapper builds passed. The real-board CHOUPI cycle passed
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. Logs:
  `artifacts/logs/cycle-choupi-20261011T120932Z.log` and
  `artifacts/logs/gp-list-probe-20261011-scp03-external-auth.txt`.
- Implementation and validation record committed as `ad08669`.
- CAP installation remains unsupported. Next run the authorized
  GlobalPlatformPro install attempt for the supplied JCAlgTest v1.8.2 CAP to
  identify the first GP LOAD/INSTALL APDU, then implement its sequence.

## JCAlgTest v1.8.2 install probe — 2026-10-11

- The exact supplied CAP hash is
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
- GlobalPlatformPro authenticated successfully, then sent secured
  `84 E6 02 00` (INSTALL [for load]) with the package AID and ISD AID. The
  board returned `6D00`; no LOAD block followed and the CAP was not installed.
  Probe log: `artifacts/logs/gp-install-probe-20261011.txt`.
- Probe and continuity record committed as `726d8e8`.
- Next: add strict parsing and state for INSTALL [for load], capture the LOAD
  command framing, and implement persistent CAP staging only after its size
  and flash space requirements are known. Existing embedded static ROM-mask
  package remains the boot-time runtime image.

## Runtime GP LOAD and applet SELECT — 2026-10-11

- The CCID path now accepts the exact JCAlgTest v1.8.2 GlobalPlatform LOAD
  sequence. It reassembles bounded C4 data blocks, checks component bounds and
  package/applet AIDs, stores package 20 in the 64 KiB runtime overlay, accepts
  INSTALL [for install and make selectable], and reports the instance in GET
  STATUS. Full short-APDU SCP03 MAC inputs are supported.
- ARM/RISC-V builds passed and the real board cycle emitted
  `TEST:SUMMARY pass=1 fail=0`. GPPro `--install` returned success; a later
  `--list` showed applet `4A43416C675465737431`, and SELECT-by-AID returned
  `6F0C840A4A43416C675465737431 9000`.
- Important limit: the Pico filesystem overlay is only RAM-backed. The CAP
  disappears after reboot/reflash. Java Card applet install-method execution
  and selected-applet APDU dispatch are also unfinished.
- The implementation and hardware validation are committed as `1107aaa`.
- Next: add a safe bounded nonvolatile CAP slot, ensure image/slot separation,
  and verify persistence across reboot. Then connect SELECT/process to the
  CHOUPI lifecycle. Hardware logs are recorded in `progress.md`.

## JCAlgTest CAP persistence across reboot — 2026-10-11

- Added a dedicated 256 KiB RP2350 flash slot at offset `0x1C0000`, with a
  CRC-protected header and payload. The firmware image ends at `0x1009FED4`,
  below this slot. The CAP is not exposed as the embedded CHOUPI filesystem's
  package record; that lookup collision caused a VM exception on the first
  persistence reboot attempt. After separating these paths, the CHOUPI runtime
  self-check passed again.
- CAP restoration now happens on CCID card power-on, after `fs_init()` has
  initialized the flash-backed CAP cache. GlobalPlatformPro confirmed the
  installed `4A43416C675465737431` instance survives a firmware reflash/reboot;
  registry listing reports it `INITIALIZED`, and SELECT returns its `6F/84`
  FCI with `9000`.
- Validation: both architecture builds passed; `./tools/cycle choupi` passed
  `TEST:SUMMARY pass=1 fail=0`; GPPro `--list` and SELECT-by-AID passed after
  reboot. Logs and hashes are in `progress.md` and
  `artifacts/logs/*persistent-cap-20261011*`.
- Commit: `a42a2a9a05ab65ad92edcb0729735d8cdac62bce`.
- Remaining: the applet's install method/lifecycle and selected applet APDU
  processing are not implemented. Next connect the persistent CAP to CHOUPI
  CAP loading, invoke installation/lifecycle methods, and route selected
  applet APDUs into the VM.

## Persisted CAP connected to CHOUPI package resolution — 2026-10-11

- `FlashMemory_Handler` now exposes the validated flash CAP as reserved package
  ID 63, returns it from `getCap(63)`, and checks its Applet component in
  `findAppletByAID`. This leaves static ROM package 20 unchanged. The CCID
  SELECT path requires that lookup to resolve the saved AID and install-method
  offset before returning FCI.
- First experiment scanned all enabled ROM packages before checking the dynamic
  CAP; it exceeded the USB servicing budget and the watchdog took the device
  to BOOTSEL. The resolver now checks the one dynamic CAP first and contains
  lookup exceptions at the C bridge. GPPro SELECT then returned the expected
  FCI / `9000` in 55 ms.
- ARM/RISC-V builds and the flashed CHOUPI runtime cycle passed. Artifacts and
  hashes are in the latest `wp7-vm-dynamic-cap-resolution` entry in
  `progress.md`. Implementation commit:
  `69616aea484ebf27307ec6fb758a71e99f1b682d`.
- Next: invoke the CAP's install method with the required applet constructor
  and install parameters; then preserve applet instance state and dispatch
  SELECT/process/deselect through the VM. No applet bytecode has run yet.


## Raw CAP method invocation entry — 2026-10-11

- Commit `3e49fc9c9cec1c38aeed431b2ba281ccb3556934` adds an interpreter
  constructor that starts a static method by its Method-component offset and
  seeds the `Applet.install(byte[], short, byte)` locals. Existing exported
  method startup remains unchanged. ARM/RISC-V builds passed and the real
  Pico cycle passed after USB access recovery (`TEST:SUMMARY pass=1 fail=0`).
- GPPro read-only SELECT-by-AID still returns `6F0C840A4A43416C675465737431 9000`
  after reflash (9 ms); see `artifacts/logs/gp-select-install-entry-20261011.txt`.
- This is only an invocation primitive. It has not been called yet. GP INSTALL
  discards its installation-parameter LV, JCAlgTest's constructor first calls
  unimplemented `JCSystem.getAvailableMemory` natives, and `Applet.register()`
  is a Java API no-op. Next parse/preserve install parameters and invoke the
  offset entry from INSTALL, then implement the first native call and truthful
  applet registration outcome.

## JC 3.0.5 imported class-token compatibility — 2026-10-11

- The JCAlgTest v1.8.2 CAP's imported API class tokens differ from CHOUPI's
  embedded export component. AID-based mappings were added for java/lang,
  javacard/framework, javacard/security, and javacardx/crypto and applied to
  class resolution plus external invokespecial class lookup.
- ARM/RISC-V builds passed. Real Pico 2 flash passed; the runtime self-test
  reported `TEST:SUMMARY pass=1 fail=0`. GPPro successfully LOADed the entire
  131,538-byte CAP. INSTALL now enters the Java install method, progressing
  beyond the prior superclass/interface wedge. The first attempt reached
  `IndexOutOfBoundsException` at `invokespecial` PC `0x101C4E1B`; after external
  constructor class-token translation it advanced to `NotYetImplemented` at
  `impdep1` PC `0x10047BF8`. The CAP is not installed.
- Logs: `artifacts/logs/gp-install-jc305-token-remap-20261011.txt` and
  `artifacts/logs/gp-install-jc305-method-token-remap-20261011.txt`.
- Implementation commit: `a71356d`.
- Next: capture the native index consumed by `impdep1`, map it to the generated
  native handler, implement the required constructor operation, then rebuild,
  flash, and repeat GPPro INSTALL. Keep USB recovery via the bounded
  `tools/flash` wrapper and user-authorized PC/SC helper.

## Latest continuation — 2026-10-12

- Corrected the JC305 applet registration observer to recognize imported
  `Applet.register()` virtual tokens 1 and 2. The exact v1.8.2 CAP CP entry at
  index `0x0179` is `03 82 03 01` (external `javacard.framework.Applet`,
  virtual token 1); the overload uses token 2. Change commit: `2c2728e`.
- Both wrapper builds passed. The physical ARM cycle entered BOOTSEL through
  CDC 1200-baud recovery after picotool could not find the reset interface,
  flashed successfully, and passed the finite runtime capture. Log:
  `artifacts/logs/cycle-choupi-20261012T040413Z.log`.
- A bounded elevated `pcsc_scan -r` timed out (124) without reader output. No
  GP INSTALL was sent after this fix. The previous INSTALL response remains a
  `SecurityException` at `invokevirtual` PC `0x101C4EF4`, operand `0x0179`, so
  the method dispatch itself is still unverified.
- Next: once the existing PC/SC service exposes the reader, retry the exact
  JCAlgTest v1.8.2 INSTALL. On repeat failure, add temporary dispatch-stage
  capture around the external Applet class lookup and public method-table slot
  resolution; do not infer successful applet registration from the observer
  change or runtime smoke test.

## INSTALL resolver diagnostics — 2026-10-12

- The CCID INSTALL failure payload now appends the last resolver stage and a
  32-bit context while retaining `6A80` as the final status word. Stages 27-29
  mark successful imported-class resolution and public method-table lookup;
  earlier classref stages remain 11-26. Code commit: `81ca2a5`.
- Both builds and a real Pico ARM flash/runtime cycle passed. Latest firmware
  UF2 SHA-256:
  `902c5c1c6453d4f3b9ceedaa00cb2c5cb1067adda60b73c81dac5aae38a3196a`.
  Log: `artifacts/logs/cycle-choupi-20261012T041446Z.log`.
- No PC/SC daemon or reader is currently available in this container; the
  diagnostic payload has not been exercised. The board is in CHOUPI application
  mode and CDC serial works. Do not claim the JCAlgTest applet is installed.
- Next: restore PC/SC reader visibility and retry JCAlgTest v1.8.2 INSTALL.
  Read the appended stage/context before changing method-token mappings.

## Virtual-method flag diagnostic — 2026-10-12

- Commit `1f41a70` adds stage 30 before `getPublicMethodOffset` checks the
  virtual-method public/package flag. Context packs the raw class reference
  into the high 16 bits and the method token into the low byte. Stages 27-29
  still identify successful class resolution and public method-table access.
- `./tools/build arm` and `./tools/build riscv` pass. Elevated
  `./tools/cycle choupi` flashed the ARM image to 100%; the 18-second CDC
  capture repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. Current UF2 SHA-256:
  `7e82e6de6ac4211cdc431cf4f301220042a033e8ba980554e02963a8121be352`.
  Cycle log:
  `artifacts/logs/cycle-choupi-20261012T055405Z.log`.
- Physical JCAlgTest v1.8.2 INSTALL remains pending. PC/SC is inactive; a
  temporary non-sudo `pcscd` attempt could not bind the existing
  `/run/pcscd/pcscd.comm` socket, and `pcsc_scan -r` found no reader. The normal
  sandbox also cannot initialize libUSB, while elevated USB access can flash
  and monitor the board.
- Next: recover/start the host PC/SC service using the already authorized
  host-side setup, verify `pcsc_scan -r` shows `Pico 2 CHOUPI CCID`, then rerun
  the exact JCAlgTest v1.8.2 GPPro INSTALL. Decode diagnostic bytes 24-28 and
  follow the captured stage/context. Do not infer install success from the
  runtime self-check.

## CAP virtual method-table byte order — 2026-10-12

- Commit `f8643e2` converts both public and package virtual-method table
  entries with `NTOHS` before treating them as Method-component offsets.
- Evidence: the GPPro INSTALL failure after stage-30 firmware returned
  diagnostic stage `0x1D` (29), context `0x00003302`. Stage 29 is written
  after reading the public table slot. On the RP2350 little-endian target,
  those bytes represent the CAP offset `0x0233`, which the prior code passed
  as `0x3302` to `Method_Handler`.
- Both wrapper builds and the real ARM Pico cycle passed; the updated image is
  `811504dc6897acca45cbc8b7ce4b6bcc3b847c1f55cc100e8304f978a7ce306e`.
  Cycle log: `artifacts/logs/cycle-choupi-20261012T061610Z.log`.
- The post-fix GPPro attempt did not reach the card: the reader list was empty
  after the cycle re-enumerated USB, so the previous ACL no longer covered the
  current `/dev/bus/usb` node. The CCID device is visible, but PC/SC currently
  has no reader.
- Next: run `sudo -n /usr/local/sbin/pico2-pcsc-reader` to grant access to the
  newly enumerated USB node and restart `pcscd.service`, confirm
  `pcsc_scan -r` lists `Pico 2 CHOUPI CCID`, then retry
  `./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar
  --debug --install AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`. Inspect the new
  failure stage/context or confirm successful registration and selection.

- Physical validation checkpoint — 2026-10-14. The user placed the Pico 2 in
  BOOTSEL; `./tools/build arm` and `./tools/build riscv` passed, then
  `./tools/cycle choupi` flashed `build/arm/pico2_choupi.uf2` successfully and
  captured an 18-second serial log. Repeated output was
  `CHOUPI:STATUS sw=9000`, `TEST:PASS choupi_runtime`, and
  `TEST:SUMMARY pass=1 fail=0`, with no `FAULT` or failure lines. Log:
  `artifacts/logs/cycle-choupi-20261014T070921Z.log`; UF2 SHA-256 is
  `6bf2fbf36f860dd6952c02a1ea91c90a54c661ece40e5796a4310f00ef797844`.
  `pcsc_scan -r` still reports `SCardEstablishContext: Service not available`,
  so the next action is restore PC/SC access for the newly enumerated CCID
  device, then retry the authorized GlobalPlatformPro install of
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` and verify SELECT/APDU behavior.
  A bounded GPPro retry confirmed the transport failure precisely:
  `SCARD_E_NO_SERVICE` (`SCardEstablishContext`, `0x8010001d`), so no INSTALL
  APDU reached the card. The CAP hash reported by GPPro was
  `38b464deabb20f48ba4fa1379354e41e913f635a537c9ea5cd2b1e919e0f908c`.
  Start `pcscd.service`, rerun the installed
  `/usr/local/sbin/pico2-pcsc-reader` ACL helper for the new USB node, confirm
  `pcsc_scan -r`, then retry the same GPPro command.
- Host-build check — 2026-10-14. `./tools/env cmake --build
  build/choupi-pc --target choupi -j2` completed Java Card API compilation and
  CAP conversion and built the capmap-free Maven modules, but Cargo could not
  resolve crates.io. An explicit offline retry failed immediately because
  `ipc-channel` is absent from the local Cargo registry, before C++ linking.
  The API conversions had zero errors; no host VM binary was produced. One
  attempt also reported the pre-existing GNU make jobserver FIFO
  `/tmp/GMfifo5`; no files outside the repository were changed. Resume by
  restoring PC/SC for the exact CAP install, then retry the host build when the
  crate is available offline or crates.io DNS works.

- Paused on 2026-10-12 at the user's request before unplugging the Pico. An
  incomplete applet `process(APDU)` implementation is present in the worktree:
  `Context` now tracks APDU buffer/response state, Java `APDU` delegates buffer
  methods to added `NativeImplementation` natives, and CCID `XfrBlock` dispatches
  ordinary commands into the selected applet. While reviewing array ownership,
  the Java `APDU` was updated to cache the native-returned byte-array copy and
  pass that exact array to `setAPDUOutgoing`; the latest Java/API change has NOT
  yet been regenerated or built. Do not treat this implementation as validated.
  `./tools/build arm` and `./tools/build riscv` passed before that last ownership
  correction. A physical `./tools/cycle choupi` was attempted after both builds
  passed but flash was blocked by `picotool` reporting `Failed to initialise
  libUSB`; the board was not flashed by that attempt. The `./tools/build-pc`
  rerun was interrupted at user request during Java API compilation. On resume,
  regenerate the ROM-mask JNI header with `./tools/build-pc`, note the existing
  crates.io DNS failure may still make its overall command exit nonzero after
  ROM-mask generation, then run both architecture builds and inspect/fix all
  compiler errors before any flash attempt. The working `tools/build` now reads
  the fresh generated header from `build/choupi-pc/jni.hpp`. User is unplugging
  the board, so physical validation must wait until it is reconnected and visible
  to this VM. Preserve all unrelated dirty/untracked files and do not stage them.

- Second pause checkpoint — 2026-10-12. The user resumed briefly, then asked to
  stop and write a handoff. I rechecked the current worktree: the incomplete
  APDU/process changes remain uncommitted in `choupi/src/context.{hpp,cpp}`,
  `choupi/src/jni.cpp`, `choupi/src/main.cpp`, `src/ccid_device.cpp`, and
  `tools/build`; the Java API sources on disk contain the cache/response-buffer
  changes described above. No additional source edits or builds were made in
  this resumed turn. `lsusb` still reports `unable to initialize libusb: -99`,
  so the Pico is not available to this VM. No build/flash command is running.
  Resume by regenerating the JNI header with `./tools/build-pc` after confirming
  the Java source edits are in the intended project tree; then build ARM and
  RISC-V, inspect/fix errors, and only attempt a hardware cycle when USB access
  works and both build gates pass. Do not commit the incomplete step or stage
  unrelated preserved work.

- Review checkpoint — 2026-10-13. User requested a code review without using the
  physical token. Review report committed as `d8afcd4` in
  `artifacts/reviews/code-review-20261013.md`. Eight defects were identified:
  native ABI mismatch, mismatched JNI/ROM artifacts, wrong lifecycle tokens,
  APDU class mapping/endian errors, stale cached APDU input, missing error status
  bytes, stale inherited virtual tables, and retained context after failed SELECT.
  Host-only `./tools/build arm` reproduced the native ABI link failure; log
  `artifacts/logs/review-arm-build-20261013.log`. No implementation fixes or token
  operations were performed. Implementation goal remains paused; resolve these
  findings before treating the current applet integration as runnable.

- Documentation handoff — 2026-10-14. User requested a Markdown fix list for
  another agent and explicitly prohibited implementation. Root `FIXES.md`,
  committed as `df4edca`, lists all eight review defects, affected files,
  correction requirements, recommended order, and host validation cases.
  No implementation edits, builds, tests, or physical-token operations occurred.

- Implementation checkpoint — 2026-10-14. The user resumed implementation and
  authorized the eight fixes in `FIXES.md`. Updated the native APDU array ABI,
  retained the Java-visible buffer across commands, paired the ROM image and JNI
  header from one source-fingerprinted generation, corrected the JCAlgTest
  lifecycle tokens and inherited method-table traversal, corrected APDU class
  import remapping/endianness, added a distinct no-applet sentinel and status
  bytes on bridge errors, and clear applet state after rejected SELECT. ARM and
  RISC-V wrapper builds pass. `./tools/cycle choupi` rebuilt both targets, saw
  USB device `2e8a:0009` in sysfs, then failed before writing firmware because
  picotool could not initialize libUSB. No hardware runtime output was captured.
  The current execution context also reports PC/SC `Service not available`; do
  not claim applet behavior was verified. Next retry `./tools/cycle choupi` when
  USB access is available, then install/select JCAlgTest v1.8.2 and verify
  repeated APDUs. The generated ROM image/header hashes and bounded logs are in
  the latest `progress.md` entry. Java API buffer changes are committed in the
  nested checkout as `534c1b1`; root fixes are committed as `55c26ce`. The prior uncommitted source/API trees and
  unrelated worktree files remain preserved.

## Latest continuation — 2026-10-14

- Implemented short-APDU receive metadata in
  `choupi/javacard-sdk/javacard-api/src/javacard/framework/APDU.java` and
  committed only that file in the nested API repository as
  `5ef3f90356c437ab658fb79ced1abde59ba3df9a`. `getIncomingLength()` calls the
  current APDU native length accessor; `getOffsetCdata()` returns 5 for the
  supported short-APDU format; `receiveBytes()` returns 0 because CCID buffers
  the complete short command before dispatch. Applet-level behavior is not yet
  verified.
- `./tools/build arm` and `./tools/build riscv` both passed after the change.
  The subsequent `./tools/cycle choupi` flashed the ARM UF2 and passed the
  18-second finite serial check with repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. Log:
  `artifacts/logs/cycle-choupi-20261014T073306Z.log` (SHA-256
  `914f437056f50088dd27f2b066aa051d97728d8fc76b241d8e4e2c7f3bd746ff`); UF2
  SHA-256 `c3958f3c3e9e0a35dfd42be552d4fc1183958d763afb63266cbc6a3f691b5680`.
- The user reports the PC/SC reader is visible. Agent-side GlobalPlatformPro
  first returned `SCARD_E_NO_SERVICE`; a bounded elevated run instead reported
  no available readers and was terminated at 25 seconds. Neither attempt sent
  an INSTALL APDU. Do not misreport this as an applet status-word failure. Do
  not use sudo; next continue with source work that can be independently built,
  and retry GPPro once reader enumeration works in this execution context.
- Current API source tree also has pre-existing modifications in
  `src/org/globalplatform/PlateformeGlobale.java` and
  `src/org/globalplatform/entity_file.java`; these were not included in the API
  commit. Root `CMakeLists.txt`, `SKILL.md`, the extensive diagnostic artifacts,
  and other source trees contain unrelated pre-existing work and must remain
  unstaged unless a step requires specific files.

- Continued APDU work on 2026-10-14: the four transport-info methods now report
  the supported T=1 protocol, default incoming IFSC 32, CCID descriptor
  `dwMaxIFSD` 254, and NAD 0. This is committed only as nested API change
  `6713938e9d7941aff9edb33a2ceecad72d355835`. ARM and RISC-V builds pass and
  the real-board cycle completed with `CYCLE:PASS`; log
  `artifacts/logs/cycle-choupi-20261014T075042Z.log`, SHA-256
  `9ecb112077c3df2eab59df9b53471d93699880cb882fc996c4a5354eea05853e`.
  The 18-second capture includes repeated runtime PASS/SUMMARY markers.

- Hardened finite serial capture on 2026-10-14: `tools/monitor` now sends TERM
  at the requested deadline and escalates to KILL after two seconds. A 2-second
  capture exited 0 and saved repeated CHOUPI runtime PASS/SUMMARY records to
  `artifacts/logs/monitor-timeout-watchdog-20261014.txt` (SHA-256
  `f76159c0af3797b520f45557a25d929575b6b9de280486006f3d711a18932c07`). This
  was tooling-only; the preceding firmware source step passed both builds and
  a full cycle. The most recent GPPro install retry after flashing timed out
  after 15 seconds without reader enumeration or APDU output; no INSTALL APDU
  reached the card.

- Continued on 2026-10-14. Nested Java Card API commit
  `0278fc9730e8915469ff35933c12549c39807831` makes `APDU.getBuffer()` retain and
  register the Java-visible array returned by the native bridge, so response
  extraction observes the same array modified by the applet. `./tools/build arm`
  and `./tools/build riscv` both pass. A fresh `./tools/cycle choupi` passed both
  build gates but could not see an RP-series USB device (`lsusb` reports
  `libusb: -99`; the flash wrapper waited for re-enumeration). It was stopped
  after more than five minutes without USB visibility, before any firmware
  write; no hardware runtime was captured. Log:
  `artifacts/logs/cycle-choupi-20261014T081438Z.log` (SHA-256
  `94e0fb59a7ae1510e906299f616bac3ad500ccac68c42f405f477979f2d16caf`); ARM
  UF2 SHA-256 `f59f4eec665d35ab678e652062ce6b4d8a135ea94a7aa643c52bdc21b0490b9c`.
  The supplied AlgTest code predominantly calls `setIncomingAndReceive()` and
  `setOutgoingAndSend()`; the remaining APDU stateful output methods, Le and
  `61xx` handling remain incomplete. Next inspect and implement the methods
  needed by the client, then retry board access when USB is visible to this VM.

- Implemented generic `ArrayLogic` primitive operations on 2026-10-14. Native
  array fill now repeats one typed byte/short/int value, compare handles those
  types with signed Java ordering, and search decodes 1/2/4-byte big-endian
  values for byte/short/int arrays. `./tools/test-jcarray` passes under ASan,
  including the new type and ordering cases; ARM and RISC-V wrapper builds pass.
  The post-change `./tools/cycle choupi` rebuilt both architectures but again
  found no RP-series USB device and did not flash or capture serial output.
  Test log `artifacts/logs/jcarray-generic-20261014.log` SHA-256
  `499791e60622ccaeaa24d5423f426b3aee1f34779d48703ddabe40301316c6cf`; cycle
  log `artifacts/logs/cycle-choupi-20261014T082552Z.log` SHA-256
  `94e0fb59a7ae1510e906299f616bac3ad500ccac68c42f405f477979f2d16caf`.
  Next continue the remaining JCAlgTest v1.8.2 native/API gaps and use host
  fixtures; retry hardware integration after the USB device is passed through.

- Host VM build checkpoint — 2026-10-14. Modernized the host Rust crate's
  obsolete feature gates, naked-function syntax, and inline assembly for the
  current stable compiler; nested `choupi/os` commit is `dd074eb`. Added an
  explicit empty `choupi_cap_store_data` implementation for PC because the
  RP2350 reserved-flash CAP backend is hardware-specific. The full offline host
  build now links, `build/choupi-pc/choupi --help` exits 0, and both
  `./tools/build arm` and `./tools/build riscv` pass. Logs are archived in
  `artifacts/logs/host-build-after-capstore-bridge-20261014.log`,
  `artifacts/logs/arm-after-host-modernize-20261014.log`, and
  `artifacts/logs/riscv-after-host-modernize-20261014.log`. Host build warnings
  remain in legacy Rust/Maven code and packed CAP metadata. The subsequent
  `./tools/cycle choupi` passed both build gates but USB access failed with
  `libusb: -99`; the device was not flashed and no serial output was captured.
  Cycle log: `artifacts/logs/cycle-host-bridge-20261014.log`. Next continue the
  JCAlgTest API/APDU work and use the working PC build for host fixtures; retry
  the board cycle when USB passthrough is visible to this VM.

- Short APDU/Le checkpoint — 2026-10-14. Added `src/short_apdu.hpp` with
  coverage for short APDU cases 1/2S/3S/4S. The CCID applet path now separates
  case-2 Le from case-3/4 Lc, preserves P3, rejects malformed and extended APDUs
  with `6700`, and stores expected response length in `Context`. Added
  `NativeImplementation.getAPDUExpectedResponseLength()`; Java
  `APDU.setOutgoing()` and `setOutgoingNoChaining()` now return Le, with zero
  mapped to 256. Nested API commit: `86345d2`. `./tools/test-short-apdu`, the
  full PC VM build, and ARM/RISC-V wrapper builds pass. The cycle rebuilt both
  architectures but could not flash because USB was absent to this VM
  (`libusb: -99`); no board output was captured. Logs:
  `artifacts/logs/short-apdu-tests-20261014.log`,
  `artifacts/logs/host-apdu-le-build-20261014.log`,
  `artifacts/logs/arm-apdu-le-build-20261014.log`,
  `artifacts/logs/riscv-apdu-le-build-20261014.log`, and
  `artifacts/logs/cycle-apdu-le-20261014.log`. Remaining work: implement APDU
  outgoing state/length and response chaining, then verify through a host APDU
  fixture and on the Pico when USB/PCSC are visible.

- Response chaining checkpoint — 2026-10-14. Added a fixed 1022-byte response
  continuation buffer in `src/short_apdu.hpp` and wired it into
  `choupi_vm_process_applet`: when applet output exceeds short Le, the bridge
  emits the first Le bytes plus `61xx`, then serves case-2 `GET RESPONSE`
  chunks. A new command, SELECT, and deselect clear pending output; nonzero
  GET RESPONSE P1/P2 returns `6A86`. `./tools/test-short-apdu` passes 13 parser
  and chunk cases; full PC, ARM, and RISC-V builds pass. Physical cycle again
  stopped before flash because this VM reports `libusb: -99`. Logs:
  `artifacts/logs/short-apdu-chaining-20261014.log`,
  `artifacts/logs/host-apdu-chaining-20261014.log`,
  `artifacts/logs/arm-apdu-chaining-20261014.log`,
  `artifacts/logs/riscv-apdu-chaining-20261014.log`, and
  `artifacts/logs/cycle-apdu-chaining-20261014.log`. Still needed: APDU Java
  send-state methods and an applet APDU exchange test.

- APDU output-state checkpoint — 2026-10-14. `Context` now resets state for
  each command, tracks Le and declared response length, enforces the 252-byte
  no-chaining limit, accumulates `sendBytes`/`sendBytesLong` output, and exposes
  the Java state constants. The API implements outgoing methods and
  `setIncomingAndReceive()` through new native calls; nested API commit:
  `5aef71e`. `./tools/test-short-apdu` covers parser, chunking, state transitions,
  buffer accumulation, invalid transitions, missing Le, and no-chaining bounds.
  Host, ARM, and RISC-V builds pass. Elevated `./tools/cycle choupi` successfully
  recovered BOOTSEL through CDC, flashed ARM, and captured repeated runtime
  PASS/SUMMARY markers with no FAIL/FAULT. UF2 SHA-256:
  `033ed8c9e1868434a0c801c47a0b6d1e03f652b62656f243521d8626118d97e1`. PC/SC
  is unavailable (`SCardEstablishContext: Service not available`), so no applet
  APDU was sent. Logs are in `artifacts/logs/*-apdu-state-20261014.log`. Next
  exercise case-2/case-4 output and GET RESPONSE using a host applet fixture or
  PC/SC when available; then continue JCAlgTest API gaps.

- Diagnostic checkpoint — 2026-10-14. `tools/doctor` now resolves host tools
  and `PICO_SDK_PATH` through `tools/env`, with normal PATH fallbacks. The
  bounded diagnostic exits 0 and reports the local CMake, Cargo/Rust, Maven,
  picotool, SDK, and required CHOUPI source files present. Log:
  `artifacts/logs/doctor-wrapper-env-20261014.log` (SHA-256
  `daa8356548514a6469b72ab53611b48dac1ff510877b338ad534ce951d5ba91b`). The
  diagnostic does not establish USB or PC/SC availability.

- Continued on 2026-10-14. Corrected `Util.arrayCompare()` in the nested Java
  Card API source so its greater-than branch returns `1` (commit `6dceb7e`).
  ARM and RISC-V wrapper builds both pass after ROM-mask regeneration; the
  converter still prints existing Java Card source/export warnings. The user
  enabled PC/SC and the bounded reader scan now sees `Pico 2 CHOUPI CCID
  [JCAlgTest CCID]`. GlobalPlatformPro `--debug --list` succeeds over T=1 and
  lists ISD `A000000151000000` in `OP_READY`; no packages/applets are installed
  (`6A88` for package/applet queries). Log:
  `artifacts/logs/gp-list-current-20261014.log` (SHA-256
  `da115a4a917ad1a00b819f69bfc96b39cac2288e033528d9db9ea1defb10d0fb`). The
  current ARM UF2 built after the API fix has not yet been flashed. Next run
  `./tools/cycle arm` (both architectures have passed their current build gates),
  inspect the bounded serial output, then retry installing the supplied
  JCAlgTest v1.8.2 CAP and follow fresh APDU traces if install fails. Windows
  reader enumeration and applet lifecycle/persistence remain unverified.

- Continued on 2026-10-14. The current ARM-only scope is explicit; do not run
  RISC-V builds/tests. `tools/build` now defaults to at most two CMake/Ninja
  jobs (`PICO_BUILD_JOBS` can override), and `./tools/build arm` passed. This
  reduces the all-core spikes caused by Ninja's default parallelism. Host USB
  and BOOTSEL wait loops sleep one second between bounded checks; the firmware
  idle loop only consumes Pico CPU. Beware that `pcsc_scan -r` can flood errors
  when pcscd is unavailable; use a timeout. Java `javax.smartcardio` compiles
  without an extra module. A bounded Java probe found no reader in this agent
  container; the PC/SC service is unavailable and the authorized helper fails
  because the container sets `no new privileges`. The previous hardware run
  already installed JCAlgTest 1.8.2 and SELECT returned `9000`; next validate
  GETVERSION (`B0 E1`) and the JCSystem info/test commands when PC/SC access is
  restored, then continue AC3/AC4 and persistence. Logs:
  `artifacts/logs/arm-build-limited-jobs-20261014.log` and
  `artifacts/logs/pico-apdu-probe-smartcardio-20261014.log`.

- Continued on 2026-10-14. Retrieved the official JCAlgTest v1.8.2 release ZIP
  to `build/downloads/AlgTest_dist_1.8.2.zip` (SHA-256
  `d1fea7c06b1a9687d310f19dfbcb3341208c97edf60e3b4b9d0cea12ffd0ef45`) and
  extracted the fat client to `build/algtest-client-1.8.2/`. Its JC305 CAP
  hash exactly matches the supplied CAP. Client `--help` works. The focused
  `-simulator -op ALG_SUPPORT_EXTENDED` run completed with exit 0 and produced
  a 4,595-line reference CSV at
  `artifacts/results/jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv`;
  stdout log is `artifacts/logs/jcalgtest-jcardsim-extended-20261014.log`.
  jCardSim itself throws `NotImplementedException` for some algorithms, which
  the client records as errors; do not treat those rows as CHOUPI failures.
  The release's all-mode `-selftest` advanced into performance tests and was
  stopped by its 180-second timeout (partial outputs only), to limit CPU load.
  Next compare a CHOUPI result against this simulator reference once the host
  PC/SC service is accessible; continue ARM-only and don't run the RISC-V
  build/test.

- Added the bounded `tools/probe-jc-algtest` host utility and
  `tools/JCAlgTestApduProbe.java`. It uses the JDK's built-in
  `javax.smartcardio`, selects AID `4A43416C675465737431`, requires the response
  version `1.8.2_jc305`, and sends JCSystem info. Compilation passed, but its
  30-second-bounded runtime probe found no exposed reader and sent no APDUs.
  This container's `/run/pcscd/pcscd.comm` is a socket without a live pcscd
  process; `lsusb` reports libusb `-99`, and sudo is blocked by `no new
  privileges`. When those host access constraints are restored, run
  `./tools/probe-jc-algtest`, then run the official host client. The module
  does not need to be separately installed.

- Performance guidance update — 2026-10-14. `perf_tips.md` now records the
  two-job build default after VM CPU saturation reports and proposes controlled
  clean/incremental build comparisons at 1/2/4 jobs, with CPU usage and wall
  time measured separately. It also prioritizes bounded USB/PCSC preflight and
  measuring upload/reboot/serial phases independently. Current sample remains
  5 successful ARM builds (mean 5.127 s), 9 host tests (mean 2.493 s), and
  52.821 s total; no upload or CPU-load comparison is measured. Documentation
  progress is recorded in `progress.md`; documentation commit: `49f848d`.

- JCSystem AID checkpoint — 2026-10-14. VM contexts now retain the applet
  instance package and AID; INSTALL initializes it before Java execution and
  restored selection passes the registered instance AID. Native
  `JCSystem.getAID()` builds and caches a framework `AID` object with its
  `theAID` byte array, resolving class token 6 through the JC305 import-token
  remapper. `./tools/test-jcarray` and `./tools/build arm` pass. Java-level
  `getAID()` behavior is not directly exercised yet. No RISC-V build/test or
  flash was performed; the root flash gate requires both architectures, while
  the active project scope excludes RISC-V. Next continue the API gaps, then
  add a permitted host applet fixture for direct Java/native result checks.
  Implementation commit: `cd58def`.

- PC build timing/incremental generation checkpoint — 2026-10-14. A full PC
  build passed but its Java API conversion took 1:01 and the Maven capmap
  reactor took 5:13; DNS failures while reading optional repository metadata
  caused most of the long wait. CMake now uses source-dependent stamp outputs
  for API and ROM-mask generation. After verifying generated output hashes and
  the current API source digest, the stamps were initialized from the successful
  build. Follow-up `./tools/build-pc` returned `ROMMASK:PASS` without invoking
  Java/Maven, and `./tools/env cmake --build build/choupi-pc -j2` passed with
  `Built target choupi`. PC build remains a development validation path only;
  vpcd/APDU support is still missing. No RISC-V compile/test or hardware flash
  was performed. Implementation commit: `0da5b1d`.

- Continued on 2026-10-15. The user restarted PC/SC and `pcsc_scan -r`
  listed `Virtual PCD 00 00` and `Virtual PCD 00 01`. With host socket access,
  `pcsc_scan` confirmed reader 0 inserted with ATR `3B 80 01 81` while the PC
  emulator was connected to port 35963. The first two official-client attempts
  raced emulator startup and reported `NO CARD`; waiting until PC/SC reported
  the inserted ATR resolved that race. JCAlgTest v1.8.2 then selected the
  applet (`9000`) and started recording algorithm probes through SunPCSC.
  A 90-second, low-priority run was intentionally capped after 14 CSV data
  rows; observed cipher probe times were about 3.8–5.7 seconds each. The
  earlier GETVERSION (`B0 E1`) and JCSystem info (`B0 73`) APDUs still return
  `6F00`; the applet reports its version as an error. This is an emulator test,
  not a physical-token or Pico firmware test. Next investigate the JCAlgTest
  constructor/version-field `NullPointerException`, and bound future official
  client runs to prevent prolonged VM load. The emulator process was stopped.
  Logs and CSV are in
  `artifacts/results/jcalgtest-vpcd-pcsc-probe-20261015/`.
- 2026-10-15 JCAlgTest emulator checkpoint: CAP installation now seeds static
  arrays from the CAP StaticField component inside the install transaction.
  Persistent primitive-array storage uses the serialized element count and
  skips the 3-byte header for byte/short/int element access. The earlier
  JCSystem short-array bounds failure came from dividing the stored element
  count by the short width. `./tools/test-pc-vpcd-applet --passes 1` now passes
  ATR, install, SELECT, exact GETVERSION (`1.8.2_jc305`), JCSystem info, and
  supported-algorithm smoke APDUs (`TEST:SUMMARY pass=5 fail=0`, 27.851 s).
  `./tools/build arm` also passes. No RISC-V build, PC/SC service operation,
  physical token, or flash was used. The implementation and test changes are
  committed as `33af96f`; the current development target remains the VPCD
  emulator. Next expand the bounded algorithm support coverage. Latest log:
  `artifacts/logs/jcalgtest-jcsystem-size-fix-20261015.log`.
- Repeat check on 2026-10-15: `./tools/test-pc-vpcd-applet --passes 2` passed
  both clean emulator installs with `TEST:SUMMARY pass=10 fail=0`; total
  per-pass times were 27.789 s and 29.546 s. GETVERSION, JCSystem info, and
  the supported-algorithm smoke APDU all returned `9000` on each pass. This
  uses direct VPCD. `pcsc_scan -r` currently reports that the PC/SC service is
  unavailable, and no physical token was used. Repeat log:
  `artifacts/logs/jcalgtest-jcsystem-repeat-20261015.log` (SHA-256
  `def236f08168911e5873b54084c11a108307dd9e55e6252697156e2d4afb0866`).
- Extended `tools/tests/pc_vpcd_applet_test.py` with the AES-128-CBC-NOPAD
  support APDU (`B0 75 11 00 03 0D 00 00 00`) and an assertion that the
  response prefix is `11 00`. A fresh install passed six checks
  (`TEST:SUMMARY pass=6 fail=0`); the APDU took 3.165 s and the emulator pass
  took 30.396 s. This test code change is committed as `16fe4b7`; log:
  `artifacts/logs/jcalgtest-aes-cbc-support-verified-20261015.log` (SHA-256
  `4061b2ef91a2e35f19211a77d1c7d3c7f8641a35391926c54da06cc97d70d438`).
- Extended the smoke harness with bounded RSA-SHA1 Signature, pseudo-random
  RandomData, and CRC16 Checksum APDUs. A fresh install passed all nine checks
  in 42.627 s. AES-CBC-NOPAD reported supported (`1100`); the other three
  reported `03` (`NO_SUCH_ALGORITHM`) with response status `9000`. The harness
  change is committed as `3bf05dd`. Direct VPCD only; PC/SC is unavailable and no
  physical token was used. Log:
  `artifacts/logs/jcalgtest-family-probes-20261015.log` (SHA-256
  `245aa44a2cec1482a8ddd2b740a6f26c100fd76c55b5eb190ad9438bbb675c9b`).

- User scope correction: `NO_SUCH_ALGORITHM` from JCAlgTest is expected for
  unsupported algorithms; do not add algorithms just to clear those rows.
  Focus on complete applet command execution and correct `JCSystem` calls.
- Renamed the leftover root project and generic board smoke artifact from
  `pico2_trng` to `pico2_jcvm` / `pico2_jcvm_smoke` in commit `ccb7605`.
  Both `./tools/build arm` and `./tools/build riscv` pass with the new names.
- Physical check 2026-10-15: `./tools/cycle choupi` flashed
  `build/arm/pico2_choupi.uf2` successfully after the CDC 1200-baud BOOTSEL
  fallback. Serial showed `CHOUPI:STATUS sw=6F00`, repeated
  `TEST:FAIL choupi_runtime`, and CCID `rx_events=0 tasks=0`; no APDU reached
  the applet. `pcsc_scan -r` listed only `Virtual PCD 00 00/01`, while USB
  enumerated Pico 2 CHOUPI CCID at `2e8a:0009`. Inspect the generated startup
  entry and restore the physical PC/SC mapping, then install the 1.8.2 CAP and
  verify the client-visible JCSystem APDU. Do not interpret absent algorithms
  as defects. Cycle log:
  `artifacts/logs/cycle-choupi-20261015T094100Z.log` (SHA-256
  `cac6eedfa0a1147323ed0e224444d99cfc8234fcc6e1bf8b8be8207a526f1422`).

## Latest continuation — 2026-10-17

- Investigated transient-memory figures on the Pico. Newlib `mallinfo()` and
  `sbrk(0)` experiments produced implausible values around 35 MB; a temporary
  pointer probe also returned values outside RP2350 SRAM. Reverted the
  estimate and restored the existing conservative-zero `JCSystem` behavior.
- `./tools/build arm` and `./tools/build riscv` both pass. The restored ARM
  image flashed successfully through CDC BOOTSEL recovery. A final raw CCID
  and SHA-256 static run could not start: guest USB lists the running Pico,
  but its node is `root:plugdev` mode `0660` and the current process lacks the
  `plugdev` group; PC/SC lists only Virtual PCD readers. The five-second
  `picotool info -a` reports no BOOTSEL device, as expected for running mode.
- Next: after access to the current USB node is restored, capture finite serial
  output and rerun JCSystem info plus the JCAlgTest SHA-256 static workload.
  To refresh the configured PC/SC ACL after USB re-enumeration, the user can
  run `sudo -n /usr/local/sbin/pico2-pcsc-reader`, then verify with
  `timeout 5s pcsc_scan -r`. Do not run sudo autonomously.
- Details: `artifacts/logs/jcalgtest-physical-jcsystem-memory-zero-20261017.log`,
  `artifacts/logs/jcalgtest-physical-break-probe-20261017.log`,
  `artifacts/logs/physical-access-blocked-20261017.log`.
- Documentation commit: `27456b6`.

## Static SHA state reproduction — 2026-10-17

- Read the three supplied session digests as operational checkpoints only;
  they add no canonical technical finding. The llm-wiki skill path advertised
  by the catalog is absent in this VM. Keep `.agent/QKB.md` canonical for this
  repository, as its instructions require.
- Extended `tools/JCAlgTestApduProbe.java` to replay the official SHA static
  prelude exactly: prepare 256-byte/50-repeat settings, five baseline
  `B041` APDUs with repeat count zero, then two identical measured `B041`
  APDUs. Probe output is saved in
  `artifacts/logs/jcalgtest-static-state-repro-20261017.log`; simultaneous
  bounded UART capture is in
  `artifacts/logs/monitor-alg-static-state-repro-20261017.log`.
- Confirmed `F105` is `CryptoException.ILLEGAL_USE` raised on SHA input-state
  append when current buffered bytes plus the 256-byte input exceed the
  2,046-byte native buffer. The latest serial trace shows the APDU completed
  with `F105`, `xfr=20`, healthy runtime reports, and later CCID status
  requests. A prior exact replay passed measurement one and then got
  `SCARD_E_READER_UNAVAILABLE`. See the new QKB section for details/limits.
- Root cause below that immediate overflow is not yet known: determine why
  state cleanup/reset does not keep the buffer length in range, and whether
  the reader error is the PC/SC manifestation of the applet failure. Do not
  report the original static failure fixed.
- Next: inspect reset/finalization and native exception cleanup across APDUs;
  add bounded state-length diagnostics if needed, then repeat the exact
  physical probe. The current probe intentionally stops at non-`AA9000`.
- Follow-up diagnostic: after the replay returned `F105`, the probe sent
  `B034` again; serial confirmed a fresh 2,048-byte transient array allocation
  and `PUTFIELD_A` of the digest reference, yet the next static APDU again
  returned `F105`. Log files:
  `artifacts/logs/jcalgtest-static-reset-diagnostic-20261017.log` and
  `artifacts/logs/monitor-alg-static-reset-diagnostic-20261017.log`. This
  weakens the theory that only one old digest object's state is stale. Inspect
  VM object/array reference behavior and the actual native array length next.

## Static SHA heap-exhaustion fix — 2026-10-17

- Temporary native tracing established that SHA state was cleared correctly:
  all 76 observed 256-byte appends entered with `current=0`. The actual failure
  was repeated transient-array deserialization. `JC_Instance::getField_Reference()`
  reloaded persistent references from flash instead of using `getFields()`;
  `Sha256Digest.input` was reconstructed on every digest call and its 2,048-byte
  backing allocation stayed in the applet heap until `malloc` failed and the
  second static APDU panicked. The earlier stale-buffer inference is
  superseded; see `.agent/QKB.md`.
- Fixed persistent reference reads to use the one-time loaded field cache.
  Removed temporary `jni.cpp` state-length instrumentation before the final
  image was built.
- `./tools/build arm` and `./tools/build riscv` passed. The fixed, non-traced
  ARM firmware was flashed. The physical focused probe passed five baselines,
  two consecutive 50-call static SHA APDUs, ten single-call SHA APDUs, another
  static SHA APDU, and a post-static call (`AA9000` throughout). The official
  v1.8.2 `ALG_PERFORMANCE_STATIC` client exited 0; SHA-256 doFinal averaged
  `2.01 ms/op` over 250 calls. No SHA `F105` or reader-unavailable error.
- Separate official-client result: 34 `NO_SUCH_ALGORITHM` rows are expected;
  1,866 other operations were marked `CANT_BE_MEASURED`, mostly with `6F00`.
  They are outside this stateful-array heap fix and remain separate diagnostics.
- Next: retain multi-APDU object identity coverage in future algorithm work;
  if continuing this client-wide pass, investigate the first reproducible
  `6F00` only as an applet/dispatch issue, not as a request to add unsupported
  algorithms.
- Logs: `artifacts/logs/build-arm-persistent-reference-cache-20261017.log`,
  `artifacts/logs/build-riscv-persistent-reference-cache-20261017.log`,
  `artifacts/logs/flash-arm-persistent-reference-cache-20261017.log`,
  `artifacts/logs/jcalgtest-static-cache-fix-physical-20261017.log`,
  `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log`, and
  `artifacts/logs/monitor-arm-persistent-reference-cache-20261017.log`.

## Static client residual `6F00` triage — 2026-10-17

- **Correction to the initial triage:** The first `6F00` is not the TRNG row.
  It is `RandomData.ALG_SECURE_RANDOM.setSeed()` (specification 2), whose
  prepare returned `AA9000`, five zero-repeat baselines returned `AA9000`, and
  first 50-repeat measurement returned `6F00`. The subsequent `ALG_TRNG`
  prepare and measurement also return `6F00`, so they cannot be classified as
  independent `NO_SUCH_ALGORITHM` follow-ons based on this run.
- The captured log confirms this is a supported-path failure before the later
  cascading errors. `RandomData.setSeed()` in the SDK checks the buffer range
  and otherwise intentionally does nothing for `ALG_SECURE_RANDOM`. The reason
  it fails during repeated invocation remains unresolved; do not implement
  TRNG or attribute the error to unsupported setup.
- This execution context currently cannot reach the physical device: `lsusb`
  reports `unable to initialize libusb: -99`, the authorized sudo helper is
  blocked by the container's `no new privileges` flag, and picotool cannot
  initialize libusb. The previous physical client log remains the evidence;
  no new hardware test was attempted.
- Next: in the host-access context, replay only the successful secure-random
  prepare, zero-repeat baseline, and one 50-repeat `setSeed` measurement while
  capturing bounded serial output. Identify the VM exception/status before
  changing code. Then check whether the subsequent TRNG rows still fail after
  isolating that first error.

## Latest continuation — RandomData token-order fix — 2026-10-17

- Reproduced the supported `RandomData.ALG_SECURE_RANDOM.setSeed()` failure
  on the real Pico with repeat counts 2, 5, and 50. One repeat passed; two or
  more failed at `GETFIELD_A_THIS` with stack overflow.
- The VM traces showed JCAlgTest's external RandomData token 2 resolving to
  method offset 1168, whose short return left one stack word. Comparing JC305
  `javacard/security/security.jca` to the embedded generated API export showed
  JC305 order `setSeed=2, nextBytes=3`, while the embedded API order is
  `nextBytes=2, setSeed=3`. Added a security RandomData virtual-method map for
  class tokens 14 and 32. QKB records the cause and descriptor-order lesson.
- Both `./tools/build arm` and `./tools/build riscv` passed. The clean,
  non-instrumented ARM firmware flashed successfully. The physical focused
  50-repeat probe returned `AA9000`; official JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` completed with exit 0. Secure-random setSeed passed
  all 250 invocations and averaged `0.36 ms/op`.
- Official run totals: 40 expected `NO_SUCH_ALGORITHM`, 1,855
  `CANT_BE_MEASURED`; 95 operation APDUs returned `6F00`. The first remaining
  `6F00` is DES `Cipher_doFinal()` prepare (`B031`, `ALG_DES_CBC_NOPAD`, key
  length 64), after the RandomData tests completed. This is unclassified and
  is the next targeted investigation. Do not treat all missing algorithms or
  `CANT_BE_MEASURED` rows as VM bugs.
- Build, flash, serial, probe, official log, CSV, and raw result artifacts are
  under `artifacts/logs/` and
  `artifacts/results/jcalgtest-static-randomdata-token-map-20261017/`.
- Next: isolate the single DES Cipher prepare APDU and serial exception trace;
  verify whether the failure is a cross-version virtual/static method mapping
  issue before changing cipher behavior. Keep the Pico test bounded and never
  use `pcsc_scan`.

## DES prepare probe continuation — 2026-10-17

- Added `--des-cipher-prepare` to `tools/JCAlgTestApduProbe.java`; it sends the
  exact first failing official `B031` payload for DES CBC no padding with a
  64-bit key and `Cipher_doFinal`.
- The probe compiled with `./tools/probe-jc-algtest`, then stopped before APDU
  transmission because this session sees no Pico PC/SC reader. The permitted
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` command is blocked by the
  container's no-new-privileges flag. Do not use `pcsc_scan` or other sudo
  commands to work around it.
- Code review narrows `prepare_class_Cipher()` to key build/set, Cipher factory,
  Cipher init, and secure-random input fill. The previous serial capture ended
  before DES, so it cannot identify the failing stage. QKB records the exact
  APDU and distinction from the separate unsupported TRNG request.
- Next: in a host-access session with Pico PC/SC, run the focused probe with a
  bounded serial capture and classify the exception frame before changing
  crypto support. No new hardware validation has occurred in this session.

## 2026-10-17 — JC305 Cipher token-order failure fixed on Pico

- Reproduced the exact first DES `Cipher_doFinal` prepare (`B031`) on the
  physical Pico. Compared JCAlgTest's JC305 crypto export to the embedded
  CHOUPI crypto export: JC305 token 3 is `init(Key, byte)`, while embedded
  token 3 is `getAlgorithm()` and `init` is token 1. Dispatching the latter
  returned a value for a void method, corrupted the operand stack, and caused
  a later `GETFIELD_A_THIS`/`6F00`.
- Added the JC305 Cipher map `{0,6,3,1,2,7,4,5}`. Kept the supporting
  cross-package interface resolution and DESKey interface mapping needed for
  key setup. See `.agent/QKB.md` for the paired token tables and broader
  lesson.
- `./tools/build arm` and `./tools/build riscv` passed. Flash succeeded after
  the wrapper recovered via CDC-to-BOOTSEL; a bounded boot capture reported
  `TEST:PASS choupi_runtime`. Focused `--des-cipher-prepare` returned
  `AA9000`. The official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` completed
  with exit 0; the DES CBC no-padding `Cipher_doFinal` and `Cipher_init`
  measurements passed. Totals: 68 `NO_SUCH_ALGORITHM`, 1,824
  `CANT_BE_MEASURED`. The first later `6F00` is a 3-key DES CBC ISO9797 M1
  prepare and is a separate open triage item.
- Artifacts: builds `build-*-des-cipher-token-map-fix-20261017.log`; flash
  `flash-arm-des-cipher-token-map-fix-20261017.log`; focused probe and suite
  logs under `artifacts/logs/`; result log and CSV under
  `artifacts/results/jcalgtest-static-des-cipher-token-map-fix-20261017/`.
- Next: preserve the source changes from this shared worktree. If continuing
  static triage, isolate the first remaining 3-key DES prepare `B031` and
  inspect whether its prepare status is unsupported or a runtime error. Avoid
  expanding implementation to algorithms JCAlgTest explicitly reports as
  `NO_SUCH_ALGORITHM`.
- Final cleanup after the official run removed the temporary DES virtual and
  interface dispatch prints. Rebuilt ARM and RISC-V, reflashed the ARM image,
  captured bounded clean boot/runtime logs (`TEST:PASS choupi_runtime`), and
  reran `--des-cipher-prepare`; it still returned `AA9000`. The committed
  diagnostic-build suite result remains valid; the final clean image has the
  exact focused prepare and runtime self-test retested.

## Latest continuation — 2026-10-17

- Fixed a real JSR return-address bug: `old_pc_t` held a reference to the live
  PC, and each `jsr` appended a list node without recycling it after `ret`.
  Saved return PCs now have value semantics, capture `instruction_pc + 3`, and
  reuse returned slots. More than 256 simultaneously active slots raises
  `StackOverflowException`. Implementation commit: `f689286`.
- Both `./tools/build arm` and `./tools/build riscv` passed. The final source
  image flashed successfully; bounded serial boot showed
  `TEST:PASS choupi_runtime`. The named physical PC/SC probe passed SELECT,
  GETVERSION `1.8.2_jc305`, JCSystem info, and DES prepare (`AA9000`).
- The full physical official `ALG_PERFORMANCE_STATIC` still fails at the first
  50-call AES-128 CBC no-padding `Cipher_doFinal` measurement. Its five
  baselines returned `AA9000`, averaging `654.6 ms`. During the measurement,
  serial ended with `malloc 44 failed to allocate memory` and
  `*** PANIC *** Out of memory`; JCAlgTest then reported
  `SCARD_E_READER_UNAVAILABLE`. The JSR defect is fixed, but the benchmark OOM
  remains unresolved. The exact 44-byte allocation owner needs instrumentation
  of VM frame depth and heap object counts.
- A fixed-stack CBC experiment was reverted because it did not prevent OOM and
  was not sufficiently validated. Existing cipher implementation is retained.
  Unsupported algorithms reporting `NO_SUCH_ALGORITHM` remain expected.
- Current board was running the final JSR-corrected image and panicked during
  the last AES measurement. The physical reader had reappeared at USB node
  `001/119` with ACL refreshed. After any re-enumeration, run the permitted
  `/usr/local/sbin/pico2-pcsc-reader` helper. Do not run `pcsc_scan`.
- Next: instrument per-APDU `Stack::push_Frame/pop_Frame` high-water depth and
  `Heap::addArray/addInstance` counts, rebuild both architectures, then repeat
  the exact AES static APDU on a freshly reset physical image with bounded
  serial capture. Preserve unrelated worktree changes.

## 2026-10-18 — Heap tracer global-array classification

- The next STATUS priority remains heap retention. The current worktree already
  has an opt-in, mark-only post-APDU tracer; no reclamation is enabled.
- Fixed a tracer bug: global-array handles encode an offset in the runtime-wide
  heap and must not be interpreted as local applet array slots. The local mark
  walk now skips global roots and their children.
- PC, ARM, and RISC-V builds passed. Focused JCSystem VPCD cases 36–41 passed
  with `PICO2_HEAP_TRACE=2`: 12 traces complete, no invalid handles or edge
  failures. One local array remained unreachable in cases 40–41; slots were
  not cleared or reused. Log:
  `artifacts/logs/heap-global-tracer-focused-vpcd-20261018.log`.
- An official JCAlgTest VPCD attempt was stopped at GETVERSION after the 20 s
  per-APDU timeout; the previous SELECT succeeded. Do not treat that as a
  regression result for the heap change or as a JCAlgTest pass.
- Next: continue the root inventory, especially any persistent/native roots
  outside `Context` and `Heap`; add tests that distinguish local from global
  handle marks. Do not enable tombstoning/reuse until that audit is complete.
- Preserve all unrelated dirty worktree files. No physical board was flashed.

## 2026-10-18 — Conservatively root loaded persistent objects

- Root inventory confirmed that persistent static fields are deserialized and
  cached lazily. The heap now appends all loaded objects and arrays with a
  persistent flash tag to its root set. This prevents collection from freeing
  a persistent object merely because its static field has not been accessed
  in the current context. Persistent objects may remain pinned indefinitely;
  deletion is unsupported.
- The `jref_t`/`shared_ptr<JC_Array>` inventory found Java references in
  `Context`, `Heap`, stack frames, typed instance fields, and reference-array
  contents. Post-APDU collection must still require an empty Java stack; APDU
  shared-pointer mirrors are already enumerated by `Context`. JNI temporary
  shared pointers are call locals and should have left scope at that boundary.
- PC, ARM, and RISC-V builds passed. JCSystem cases 14–16 passed under VPCD;
  six heap traces were complete with zero invalid refs or edge failures. Log:
  `artifacts/logs/heap-persistent-roots-focused-vpcd-20261018.log`.
- Next: finish native-handle/root audit and implement stable-slot tombstones
  plus reuse only after a complete mark. Validate collection after Java and
  native caller frames have fully unwound. No physical board was accessed.
- Evidence logs: final builds, flash, boot, focused reader probe, full official
  run and serial panic are listed in `.agent/QKB.md`.

## Physical cipher OOM root cause — 2026-10-17

- Continued with per-APDU heap and stack counters on the physical Pico. The
  official JCAlgTest 1.8.2 static run again passed five AES-128 CBC no-padding
  `Cipher_doFinal` baselines (average `566.4 ms`) and failed on the first
  50-call measurement with `SCARD_E_READER_UNAVAILABLE`. The board then
  recovered through the watchdog path.
- **Root cause identified:** VM arrays and instances are append-only in
  `Heap`; no garbage collector or object removal exists, and
  `JCSystem.requestObjectDeletion()` is a no-op. Cipher repeats allocate about
  two tracked arrays per call; the static suite accumulated 594 arrays and 192
  instances before the failing APDU. The frame stack was empty after every
  completed APDU (high-water 11), ruling out the JSR/frame leak for this OOM.
- The temporary counter instrumentation has been removed. ARM and RISC-V
  builds passed; the clean ARM firmware was flashed and bounded boot output
  again showed `TEST:PASS choupi_runtime`. Board is back in normal running mode.
- Do not delete indexed heap entries, clear the heap per APDU, or pool by array
  size: `jref_t` encodes list offsets and live refs may reside in statics,
  object/persistent fields, or active frames. Next implementation step is a
  GC/root-lifetime design that preserves handles, then test allocation/reuse
  across the full static workload. Until then, the official static suite is
  unresolved; unsupported `NO_SUCH_ALGORITHM` rows remain expected.
- Evidence files and hashes are recorded in the latest `Physical
  ALG_PERFORMANCE_STATIC heap retention` section of `.agent/QKB.md`.

## Runtime CryptoException reuse — 2026-10-17

- Fixed retained exception allocations by recognizing the pinned ROM
  `CryptoException` view (AID `A0000000620101`, class offset 234) and imported
  `javacard.security` view (AID `A0000000620102`, class offset 233), with a
  separate VM-owned root for each view in the applet heap. An earlier fixed
  ROM-only matcher missed imported allocations; the dynamic export-token
  attempt was rejected because framework CAPs share an AID but expose different
  export tables.
- VPCD transaction/SHA/JCSystem regression passed, including 50 throws with
  stable identity per view and changing exception reasons. `./tools/build arm`
  and `./tools/build riscv` passed. Physical ARM CHOUPI was flashed with
  `./tools/cycle choupi`; bounded serial output reported
  `TEST:PASS choupi_runtime`.
- Physical official `ALG_SUPPORT_EXTENDED` completed 4,520 rows (15 yes,
  4,505 no) and exited successfully. Four-minute serial capture had no
  allocation failure, `PANIC`, `FAULT`, or `TEST:FAIL`. The earlier 1,637-row
  supported-sweep stop is resolved for this build.
- Static mode remains open and is separate. The latest physical run completed
  a 250-call DES CBC `Cipher_doFinal` measurement at 28.64 ms/op, then lost its
  PC/SC reader on the next DES `Cipher_init` request. The bounded serial capture
  did not contain an allocation failure; after ACL refresh, PC/SC listed the
  Pico again. Earlier static evidence still directly shows append-only array
  OOM (594 arrays, 192 instances). Do not report `ALG_PERFORMANCE_STATIC` as
  passed; pursue safe array lifetime and reader recovery separately.
- Current board is running CHOUPI firmware. The authorized PC/SC ACL helper
  restored reader publication after the static attempt. Do not run
  restricted-context USB probes; invoke physical commands with host access
  from the first call.
- Evidence and SHA-256 digests are in the latest `.agent/QKB.md` section.
- Next: isolate the static DES init reader loss without repeating the full
  benchmark yet; then develop a safe heap/array lifetime strategy before
  another static run. Preserve the passing supported-sweep artifacts.

## RandomData.OneShot result correction — 2026-10-17

- Changed the unsupported `RandomData.OneShot.open()` factory from
  `CryptoException.ILLEGAL_USE` to its documented
  `CryptoException.NO_SUCH_ALGORITHM`. Added an assertion to the official
  JCAlgTest VPCD integration for APDU `B0752B00010200`; its response is
  `2B03FF...9000`. PC build and VPCD integration passed (`TEST:SUMMARY
  pass=11 fail=0`). ARM M33 build passed; no RISC-V build or test was run.
- Flashed the ARM image through host access and captured runtime PASS. The
  exact named physical reader then completed official v1.8.2
  `ALG_SUPPORT_EXTENDED`: 4,525 rows, 15 `yes`, 4,510 `no`, zero other
  statuses. The six RandomData.OneShot results are all `no`. Two bounded
  serial captures had no failure, fault, panic, allocation-failure, or CCID
  transfer-error marker.
- Cross-check found two false-positive `yes` rows in the physical results:
  `OWNER_PIN_X` and `OWNER_PIN_X_WITH_PREDECREMENT` factories return null.
  AC3 now has a complete physical run, but AC4 remains open until these rows
  are made accurate. Details and hashes are in the new support-comparison
  report and QKB section.
- Next: implement the two documented OwnerPINX types (or otherwise make the
  support report accurate without breaking the applet sweep), then rerun the
  focused support checks. Keep static-suite heap retention as a separate open
  issue. Current physical ARM image is running CHOUPI.

## OwnerPINx support and dispatch regression — 2026-10-17

- Implemented both OwnerPINx builder variants, correct predecrement behavior,
  and try-limit state handling. Fixed internal `SuperMethodRef` token creation,
  superclass method resolution and null-receiver checking, and inherited
  instance-field offsets. Successful `OwnerPIN.check()` no longer dispatches
  through an override that clears its validated flag.
- Full VPCD regression passed 16/16 in 59.295 s. After removing diagnostic-only
  hooks from field bytecodes, focused OwnerPINx case 13 passed again with
  response `030302000100000202000201030103009000`. PC and ARM builds passed.
  The PC build still reports pre-existing Rust and packed-member warnings;
  ARM build reports none. No physical token was used.
- Generic lessons are recorded in `.agent/QKB.md`. These changes are separate
  from the unresolved static benchmark heap-retention issue.
- Evidence logs: `artifacts/logs/jcsystem-ownerpinx-full-vpcd-20261017.log`,
  `artifacts/logs/jcsystem-ownerpinx-cleanup-vpcd-20261017.log`,
  `artifacts/logs/build-pc-ownerpinx-cleanup-20261017.log`, and
  `artifacts/logs/build-arm-ownerpinx-cleanup-20261017.log`.
- The official JCAlgTest VPCD wrapper was also attempted to check the two
  support rows. It installed and SELECTed the applet but timed out in GETVERSION
  after 90 seconds; no support query was reached. Trace points into the
  delayed `AlgPerformanceTest`/`JavaCardAES` S-box setup. Keep the official
  physical support comparison open until a later hardware validation.
- Commits: root `63a13eb`; validation notes `1ed8d0f`; API `0199d51`; rommask
  `051b389` and `f3c309a`.
- Next: continue from the open static benchmark heap-lifetime and reader-path
  issues. Do not access USB or flash hardware without a specific request.

## Latest continuation — stable-slot heap reclamation — 2026-10-18

- Added guarded mark-and-sweep to local applet heaps. It preserves reference
  offsets by leaving tombstone slots and reuses them on allocation. Persistent
  objects are pinned, global arrays are excluded, and embedded sweep runs only
  after interpreter scopes return with an empty Java stack and a complete root
  walk.
- ARM, RISC-V, and PC build wrappers passed. Focused VPCD cases 14–16, 36–41,
  and 49–54 passed (17 total) in 96.701 s; no invalid references or trace edge
  failures, and allocation batches reused slots. Physical ARM cycle passed
  `TEST:SUMMARY pass=1 fail=0`.
- Physical JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` ran 900 seconds and was
  stopped by the wrapper (exit 124) at RSA CRT 768-bit SHA PKCS#1 signature
  setup. The client log contains 1,297 `6F00`/`CANT_BE_MEASURED` cipher rows,
  with no allocation failure, OOM, fault, or reader-unavailable error. A
  bounded 240-second serial capture showed runtime PASS and no GC-skip/panic
  marker. Full static results are not established.
- The installation record in `progress.md` confirms GPPro 26.06.04 sent all
  LOAD blocks and final INSTALL for the exact supplied CAP; each returned
  `9000`, and subsequent SELECT/readback confirmed the applet.
- Next: investigate the static cipher `6F00` independently from expected
  `NO_SUCH_ALGORITHM`; rerun static after that fix. Then audit native-held
  roots and persistent-object deletion. Preserve unrelated pre-existing
  worktree changes and stage only the current step's files and hunks.

## Latest continuation — fix JC305 DESKey interface index — 2026-10-18

- The first static cipher prepare returned `6F00` with `SecurityException` 29
  at an `invokeinterface` call in key setup. Reflashing with collection
  temporarily disabled reproduced the same exception, so this was independent
  of the new heap collector.
- Cause: JC305's `DESKey` interface ref uses token 10; the generated CHOUPI
  security export uses token 12. Added the security-package interface map
  `10 -> 12`, separate from method-token remapping. ARM, RISC-V, and PC builds
  passed. The normal collector-enabled image flashed and passed its runtime
  cycle; the exact physical probe now returns `AA9000` for DES prepare.
- The serial monitor had hung in `stty` while opening a stale ACM node during a
  prior cycle. `tools/monitor` now bounds each `stty` retry with `timeout`; the
  following cycle and finite monitor capture completed. Avoid unbounded
  `pcsc_scan -r` probes; if needed, wrap the reader check in a short timeout.
- Next: rerun physical `ALG_PERFORMANCE_STATIC` after this mapping fix. The
  previous 900-second run and its 1,297 `6F00` cipher rows predate the fix; it
  is not evidence for current cipher behavior. Continue distinguishing
  expected `NO_SUCH_ALGORITHM` from unhandled exceptions.

## Latest continuation — bound USB serial setup retries — 2026-10-18

- `tools/monitor` now runs each serial `stty` attempt under a one-second
  `timeout` with a one-second kill grace period. A stale `/dev/ttyACM0` open
  previously blocked the cycle after flash despite the retry count.
- `bash -n tools/monitor` passed. The finite capture logged runtime PASS, and
  the next full `./tools/cycle choupi` completed with `CYCLE:PASS` and
  `TEST:SUMMARY pass=1 fail=0`.
- The corrected physical static run and its current failure analysis are
  recorded below.

## Latest continuation — completed physical static matrix — 2026-10-18

- The official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` run completed on the
  physical Pico 2 after the DESKey mapping fix. It produced 7,785 CSV rows,
  including working SHA-256 and DES CBC NOPAD measurements, 40 expected
  `NO_SUCH_ALGORITHM` results, and 1,852 `CANT_BE_MEASURED` results with
  `6F00`. No reader disconnect occurred during the run.
- Isolated the first DES CBC ISO9797 M1 prepare APDU. The official applet source
  catches `CryptoException` and should return a successful APDU with an
  unsupported-algorithm code. Instead it returns `6F00`; serial reports
  exception 29 at `ATHROW` and frames in package 63 around offsets 15671 and
  15705. A PC/SC reset was required to restore SELECT after the full run, and
  a single APDU reproduced the failure. This is an exception-dispatch bug to
  investigate; do not implement the unsupported padding algorithm.
- Artifacts:
  `artifacts/results/physical-perf-static-des-interface-fix-20261018/`,
  `artifacts/logs/monitor-static-des-interface-fix-20261018.log`, and
  `artifacts/logs/monitor-des-m1-reset-isolated-20261018.log`. Exact hashes and
  detail are in `.agent/QKB.md` and `progress.md`.
- Next: inspect `Bytecodes::doThrow()` handler-range and catch-class matching
  for the applet's `CryptoException` catch. Add a focused exception regression,
  validate on PC/VPCD, build PC/ARM/RISC-V, then flash and rerun a bounded
  physical static subset. Preserve the expected unsupported-algorithm result.

## Latest continuation — exception class-view alias fix, hardware retry pending — 2026-10-18

- CAP exception-table inspection confirmed the failing `prepare_class_Cipher()`
  call at package 63 method offset 15671 is inside its `CryptoException` catch
  range. The physical trace then showed `CryptoException` package 2/class 233,
  reason 3, followed by the catch throwing `ISOException(9000)` at package
  0/class 350. JC305's exported ISOException resolves to offset 316, so the
  old class cast failed and returned `6F00`.
- `bc_exceptions.cpp` now treats the ROM/imported CryptoException records
  (offsets 234/233) as one Java type while matching catch clauses, and accepts
  ROM ISOException offset 350 in the terminal SW path alongside export offset
  316. This should allow the unsupported Cipher variant to return `03 9000`.
- PC, ARM, and RISC-V builds pass. A physical image was flashed successfully,
  but the board failed to re-enumerate; the authorized
  `/usr/local/sbin/pico2-pcsc-reader` helper found no Pico, and no serial
  device was present. The PC VPCD harness installed and selected the CAP but
  timed out at GETVERSION after 20 seconds, before reaching the new APDU. The
  current code fix
  is therefore not validated; do not claim completion.
- Evidence: `artifacts/logs/monitor-des-m1-alias-fix-20261018.log`,
  `artifacts/logs/monitor-des-m1-iso-class-check-20261018.log`,
  `artifacts/logs/cycle-choupi-20261018T131939Z.log`,
  `artifacts/logs/vpcd-des-m1-iso-alias-20261018.log`, and the
  `*-catch-alias-final-20261018.log` build records. The final source removes a
  temporary ISOException diagnostic print but retains the exception dispatch
  trace that exposed the class offsets.
- Next: after the Pico is visible again, rerun the ARM cycle, refresh PC/SC
  access, and submit only DES M1 prepare. Require response `03 9000` plus a
  normal follow-up SELECT; then run a bounded static subset before the full
  matrix.

## Latest continuation — physically validate exception aliases — 2026-10-18

- Rebuilt current sources for ARM and RISC-V; both passed. `./tools/cycle
  choupi` rebuilt both, flashed the physical RP2350, and completed with
  `CYCLE:PASS` and repeated `TEST:SUMMARY pass=1 fail=0`.
- The first existing probe was for DES CBC NOPAD (algorithm 1), so it did not
  exercise the reported M1 failure. A corrected temporary PC/SC probe sent
  DES CBC ISO9797 M1 (algorithm 2), got `03 9000`, then reselected the applet
  successfully in the same session. This confirms the expected unsupported
  algorithm response and the ROM/imported CryptoException plus ISOException
  class-view fix on hardware.
- The official JCAlgTest 1.8.2 static run recorded 382 CSV rows. SHA-256
  measurements passed and M1 prepare calls produced `03 9000` and
  `NO_SUCH_ALGORITHM`; the client continued beyond M1. It later exited with
  `SCARD_E_READER_UNAVAILABLE` at a 3-key DES CBC NOPAD measurement. USB
  `2e8a:0009` remained enumerated, the first PC/SC helper call raced USB
  re-enumeration, and the second helper call restored the reader. A finite
  serial monitor showed `TEST:PASS`.
- Reproduced the exact 3-key DES CBC NOPAD prepare and 50-call measurement in
  a focused physical probe: both returned `AA9000`, follow-up SELECT returned
  `9000`, and the probe passed. This isolates the long-run failure to a
  transient PC/SC/reader availability event; its cause is unknown. Do not
  count the partial static matrix as full-suite success.
- Artifacts: `artifacts/logs/cycle-choupi-20261018T171028Z.log`,
  `artifacts/logs/physical-m1-exception-alias-20261018.log`,
  `artifacts/logs/jcalgtest-static-exception-alias-20261018.log`,
  `artifacts/logs/physical-des3-static-probe-20261018.log`, and
  `artifacts/results/physical-perf-static-exception-alias-20261018/`.
- Next: diagnose the transient PC/SC loss, then rerun the full static matrix.
  After that, continue the remaining JCSystem/context audit; keep expected
  `NO_SUCH_ALGORITHM` results out of the bug count.

## Latest continuation — unwind interpreter frames after exception returns — 2026-10-18

- Serial capture found `CHOUPI:HEAP_GC_SKIP reason=active_java_frames` after
  applet exception returns. `Interpretor::run()` returned directly from its
  ISO status-word and fatal exception paths, leaving frames in the shared
  `Context::Stack`; the after-APDU collector correctly refused to sweep an
  incomplete root set. Repeated unsupported-algorithm prepares accumulated
  memory and caused an OOM panic.
- Added `Stack::unwind()` and call it on aborted interpreter exits, clearing
  stale root return state as well as frames. PC, ARM, and RISC-V builds passed;
  the physical cycle passed. A hardware stress probe ran 50 DES CBC ISO9797 M1
  prepares (`03 9000` each), then 3-key DES CBC NOPAD prepare and 50-call
  measurement (`AA9000`), then SELECT; all passed. Serial capture showed no
  `HEAP_GC_SKIP` or panic during that sequence.
- The official static run now progresses farther but remains incomplete. With
  a 300-second serial capture it reached 512 CSV rows, then the collector path
  failed `malloc 3244` and panicked during AES CBC PKCS5 prepare. The client
  reported `SCARD_E_READER_UNAVAILABLE` afterward. A 1,622-byte allocation
  preceded the failing request; likely worklist growth in `Heap::traceAndMark`
  needs bounded reservation to the stable handle count. Do not report the full
  matrix as passing.
- The PC/VPCD suite still times out at GETVERSION before exercising applet APDUs;
  hardware results are from the Pico 2 only.
- Artifacts: `artifacts/logs/cycle-choupi-20261018T173718Z.log`,
  `artifacts/logs/physical-heap-unwind-stress-20261018.log`,
  `artifacts/logs/monitor-heap-unwind-stress-20261018.log`,
  `artifacts/logs/jcalgtest-static-heap-unwind-final-20261018.log`,
  `artifacts/logs/monitor-static-heap-unwind-final-20261018.log`, and
  `artifacts/results/physical-perf-static-heap-unwind-final-20261018/`.
- Next: bound `traceAndMark()` worklist capacity to the total local handle
  count, verify all traversed object references fit that bound, then rerun the
  same hardware capture. Continue the JCSystem/context API audit after the
  collector survives the full static matrix.


## Latest continuation — implement applet reselection state — 2026-10-19

- `Applet.reSelectingApplet()` previously always returned false. Added a VM-owned flag set only when a successful `deselect()` is followed by a SELECT-by-AID for the same installed applet. The flag is visible during `Applet.select()` and is consumed/reset around the callback. Power-off, reset, cross-AID selection, rejected selection, and clear-selected paths reset it.
- The CCID handler and PC VPCD session now pass whether a same-AID reselect is underway. The JC305 API bridge calls the native query; the probe applet records the value.
- `./tools/build arm` passed. Focused VPCD lifecycle cases passed, and the full JCSystem/APDU VPCD suite passed 69/69 in 442.556 s. The log contains all APDU results; no hardware flash or RISC-V build was run.
- Commits: API `814d409`; root implementation/tests/logs `c098a86`.
- Physical power-up validation was retried after reinsertion, but GPPro and `pcsc_scan -r` still listed only Virtual PCD readers. See `artifacts/logs/pico-powerup-persistence-20261019T102428Z.log`; no physical APDU ran.
- Next: resume the remaining Java Card API/runtime audit in TODO.md, and retry power-cycle persistence when the Pico is visible through PC/SC.


## Latest continuation — stop pinning unreachable persistent heap objects — 2026-10-20

- The prior conservative policy that treated every loaded persistent-tagged
  instance/array as a GC root caused the physical JCAlgTest static suite to
  panic with out-of-memory around transfer 798. Debug summaries showed zero
  static roots and growth to 1,082 arrays / 314 instances, most tagged and
  pinned.
- Changed root enumeration to follow current context/runtime handles and
  reachable object fields while leaving flash records intact. The collector
  can now reclaim RAM objects whose persistent records remain stored for later
  lazy restoration. Mark bits live on each heap object, avoiding temporary
  mark bitmap allocations during collection. VPCD persistence test passed 3/3.
- Official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` completed on the physical
  Pico at 2,262 transfers. It reported only expected `NO_SUCH_ALGORITHM`
  (1,772) and `CANT_BE_MEASURED` (95), no panic or GC traversal errors. Heap
  counts stayed at 83 array slots / 28 instance slots; live counts were 33 / 25.
  SHA-256 256-byte results: `doFinal` 3.10 ms/op, `update` 3.34 ms/op,
  `reset` 6.11 ms/op.
- Both builds passed. The diagnostic build flashed and passed runtime; then
  diagnostics were disabled, both targets rebuilt, and normal firmware flashed
  and runtime-tested. Final named-reader probe passed SELECT, GETVERSION
  `1.8.2_jc305`, and JCSystem info. The BOOTSEL report did not match the
  observed ID (`2e8a:0009` CHOUPI firmware); the reader was visible after ACL
  refresh.
- Serial showed one retained `WATCHDOG_RECOVERY cause=other` marker after the
  final flash, with normal runtime tests passing. This is the known stale
  scratch diagnostic on an ordinary reboot, not evidence of a watchdog reset.
- Evidence: `artifacts/logs/jcalgtest-static-gc-root-fix-20261020.log`,
  `artifacts/logs/monitor-static-gc-root-fix-20261020.log`,
  `artifacts/logs/cycle-choupi-20261020T082304Z.log`,
  `artifacts/logs/cycle-choupi-20261020T085457Z.log`, and
  `artifacts/results/physical-perf-static-gc-root-fix-20261020/`.
- Next: continue the Java Card API/runtime audit. Revisit completeness of
  static-reference roots and persistence aliases before extending reclamation
  to untested object graph shapes.

## Physical deploy API testing and dynamic package continuation — 2026-10-21

- Updated `src/cap_load.cpp`: INSTALL resolves package AID from each INSTALL
  command (GPPro power-cycles between LOAD and CREATE), and dynamic SELECT
  resolves instance AID to package/app AID and the per-applet install offset.
  Updated `AlgTest_JavaCard/deploy/api_card_runner.py` so an explicit INSTALL
  failure takes precedence over the preceding `CAP loaded` message.
- Physical Pico validation: ARM build passed; RISC-V was built before the
  user’s ARM-only steering arrived. After that steering, only ARM was rebuilt
  and flashed. The bounded serial monitor passed `choupi_runtime` (summary
  pass=1 fail=0). Both Multi applet CREATE commands and independent SELECTs
  returned `9000`; GPPro listed both AIDs selectable.
- The deploy API client shows two empty VPCD readers before the Pico, so use
  `--reader-index 3` for this VM. Core API 3.0.5 completed 291 calls:
  236 OK, 15 expected `UNSUPPORTED_PER_SPEC`, 6 WRONG_RESULT, 18 EXC_RUNTIME,
  12 SW_ERROR, 4 Ext applet not installed. A second Core run with Ext CAP had
  235 OK, 15 expected unsupported, 5 wrong, 17 runtime exceptions, 15 SW
  errors, and 4 Ext SELECT failures (`6999`).
- Security completed 901 calls (40 OK, 822 unsupported, 16 runtime exceptions,
  15 SW errors, 8 wrong results). Crypto completed 466 direct calls after a
  manual GP install retry (13 OK, 444 unsupported, 6 CryptoRuntimeException,
  3 SW errors). The runner’s first Crypto CSV is a partial-install false
  success artifact; do not use its 466 APPLET_NOT_INSTALLED rows as the final
  Crypto result.
- JavacardxExt completed 13 calls, all SW_ERROR. JavacardxSec completed 84,
  all SW_ERROR, then package cleanup succeeded. Javacardx and JavacardxInt
  loaded but failed INSTALL with 6A80; their package-only cleanup succeeded.
  Service completed 124 calls (77 OK, 28 wrong, 18 runtime exceptions, 1 SW
  error), but cleanup returned 6581. Biometry LOAD returned 6D00.
- Core fixes identified next: overlapping `Util.arrayCopy` and
  `arrayCopyNonAtomic`, OwnerPIN runtime failures, and Shareable interface
  results. Investigate persistent applet deletion returning 6581 before rerun
  stages that require freed slots. See `.agent/QKB.md` and `TODO.md`.
- Artifacts: Core and Crypto client CSVs, deploy stage CSVs under
  `artifacts/results/deploy/*20261021.csv`; Core Ext serial/test evidence in
  `/tmp/pico2-deploy-core-monitor-selectfix.log` and the direct client logs in
  `/tmp/pico2-deploy-core-probe/` and `/tmp/pico2-core-ext-probe/`.
- Commit: `875bf99` (implementation, runner correction, deploy evidence)
- Next: implement and validate Core `Util.arrayCopy` overlap behavior on ARM,
  then fix OwnerPIN. Preserve the current card packages and result CSVs until
  deletion behavior is understood.

## Physical Core overlap fix — 2026-10-21

- Changed `choupi/javacard-sdk/javacard-api/src/javacard/framework/Util.java`
  to copy backwards for same-array overlapping ranges where the destination
  begins inside and after the source. This also covers `arrayCopyNonAtomic`.
- Validation: `./tools/build arm` passed; the physical Pico 2 flash passed
  after CDC BOOTSEL recovery. Bounded serial output repeatedly reported
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. Deploy Core API
  3.0.5 ran 291 calls on reader index 3. Both overlap rows now report OK.
- Results are archived at
  `artifacts/results/deploy/pico2-overlap-retest-20261021.csv` and
  `pico2-overlap-retest-client-20261021.log`.
- Remaining findings include OwnerPIN runtime/SW failures and a wrong
  Shareable interface result. Some Ext scenario rows say `APPLET_NOT_INSTALLED`
  (`6999`); the normal Core applet test completed.
- Next: isolate OwnerPIN's first constructor failure, fix it, and rerun the
  physical Core API client. Then continue deploy areas and investigate package
  deletion `6581` before relying on runner cleanup.

## Physical OwnerPIN dispatch fix — 2026-10-21

- `choupi/src/jc_handlers/jc_cp.cpp` now maps JC305 OwnerPIN virtual-method
  tokens and PIN interface-method tokens to the embedded API order. The
  mapping was derived by comparing the supplied JC305 `framework.jca` with the
  generated embedded `framework.jca`.
- Validation: `./tools/build arm` passed. Flashed the physical Pico 2 through
  the wrapper's CDC BOOTSEL recovery. The bounded monitor passed repeated
  runtime checks. The deploy Core API 3.0.5 client then completed 291 calls;
  every OwnerPIN constructor, method, and PIN-interface row returned OK.
- Result: the pre-existing OwnerPIN runtime failures and false
  `getValidatedFlag` result are fixed on hardware. Current remaining Core
  findings are one Shareable wrong result, three generic UserException runtime
  probes, and four Ext scenario calls requiring the separate Ext applet.
- Next: run the deploy runner's Core stage with its Multi and Ext CAPs so the
  multi-applet and extended-length scenarios are exercised; then continue
  cleanup `6581` and the remaining deploy area results.

## Current session handoff — deploy multi-package and Shareable follow-up — 2026-10-21

- Scope: continue the user's request to test/fix deploy API applets on a real Pico 2, including multiple package uploads. User constrained validation to ARM only. Do not compile/test RISC-V.
- Physical evidence: the Pico was visible through the authorized PC/SC path. The official deploy Core 3.0.5 client ran with Multi and Ext scenarios against the firmware already on the card: 291 calls, 267 OK, 19 `UNSUPPORTED_PER_SPEC`, three `SW_ERROR`, and two `EXC_RUNTIME`. Core/Multi/Ext CAP operations and cleanup completed. Two runtime exceptions are in Shareable behavior; see `artifacts/results/deploy/pico2-core-resume-20261021.csv`. This was not a run of the newly built source.
- Physical multiple applet/package upload was exercised by the official runner: Multi created two MultiSelectable instances, and Ext loaded. The independently loaded package cases and concurrent package retention/deletion are not yet fully covered. CAPs were retained by the runner (`--keep`).
- ARM build `./tools/build arm` passed before the latest PC-only diagnostic instrumentation. Host build `./tools/build-pc` passed after the harness and trace edits. No RISC-V build was run. A flash attempt for `build/arm/pico2_choupi.uf2` was rejected by automatic approval because the repository gate requires both architecture builds before flashing. Do not bypass this gate; continue offline until an allowed validation route exists.
- New reusable test support: `tools/tests/pc_vpcd_applet_test.py` accepts `--applet-aid`, `--package-id`, and repeatable `--apdu`. `python3 -m py_compile tools/tests/pc_vpcd_applet_test.py` and `bash -n tools/test-pc-vpcd-applet` passed. Focused VPCD reproduction of Core Shareable APDU `B0830116010000` returned `049000` (test summary 11/11); this host shim is single-package/single-instance and is not evidence of multi-package physical behavior.
- The working tree contains temporary `PC_VERSION` diagnostics gated by `CHOUPI_VM_TRACE_SHAREABLE=1` in `choupi/src/jc_bytecodes/bc_exceptions.cpp`, `choupi/src/jc_handlers/jc_class.cpp`, and `choupi/src/jc_bytecodes/bc_invoke.cpp`. Remove these temporary traces before committing product code. The exception frames for the failing own-AID probe were package/PC: `56/6781`, `17/367` (`invokevirtual`), `0/690` (`invokestatic`), `56/3537`, `56/123`, and `56/7066`; caught at `56/7347`. No `invokeinterface`/resolver traces appeared for that APDU, so the earlier resolver-only diagnosis does not explain this path. Resolve package 17 PC 367 against the generated deploy `NativeImplementation` JCA before changing dispatch.
- There are extensive unrelated pre-existing tracked and untracked changes in the worktree. Preserve them. The generic VPCD harness change may be kept if its cleanup/argument behavior is reviewed; do not stage broad directories.
- Evidence: `artifacts/logs/monitor-shareable-current-20261021.log`, `artifacts/logs/deploy-core-shareable-vpcd-20261021.log`, `artifacts/logs/deploy-core-shareable-vpcd-trace-20261021.log`, and `artifacts/logs/deploy-core-shareable-vpcd-trace2-20261021.log` (plus the physical CSV above).
- Next actions: (1) inspect package 17 method bytecode/JCA at PC 367 and identify the thrown class `pkg15/class72`; (2) remove PC-only trace code after diagnosis; (3) implement the minimal Shareable fix and rerun `./tools/build arm` plus `./tools/build-pc`; (4) do not flash unless the repository's architecture gate is satisfied—if still constrained to ARM only, report the gate rather than bypassing it; (5) after an allowed flash, repeat the physical Core/Multi/Ext run and test concurrent package load, selection, GET STATUS, deletion, and reinstall; then proceed through the remaining deploy README areas. Preserve installed card data and existing result files.
- The final current-state build for the exact working tree and a flash of that image were not verified. No worktree changes were committed during the investigation.

## Shareable dispatch fix handoff — 2026-10-21

- The Shareable investigation above is resolved in the current working tree. The stale trace-based next steps in that earlier entry are superseded by this section. No temporary Shareable trace code remains in the described implementation.
- Root cause: when embedded ROM code called a virtual method on a dynamic applet instance, the ROM's embedded API method token was used against the applet's imported JC305 method table. The selected method sometimes had a different argument count, leading to a null receiver/runtime exception.
- Implementation: `choupi/src/jc_bytecodes/bc_invoke.cpp` retries lookup with an inverse imported-to-embedded virtual-token mapping only for ROM caller → dynamic receiver dispatch when the initial target argument count is inconsistent. `Class_Handler` performs the reverse mapping while walking dynamic classes, then restores the embedded token when lookup reaches the ROM superclass. An unresolved argument-count mismatch raises `SecurityException` rather than invoking with a malformed stack. `tools/tests/pc_vpcd_applet_test.py` gained `--select-only` for non-JCAlgTest applet smoke checks.
- Validation evidence for this exact implementation: `./tools/build-pc` passed with `ROMMASK:PASS`; `./tools/build arm` passed and produced `build/arm/pico2_choupi.uf2` (image end `0x100c6428`). No RISC-V build or hardware flash/test was run in this validation round. Respect the active ARM-only constraint and the root `AGENTS.md` requirement that both architecture builds pass before flashing; do not bypass either condition.
- Focused Core Shareable VPCD regression passed 11/11: own-AID response `0000 9000`, interface response `000101 9000`. Log: `artifacts/logs/deploy-core-shareable-vpcd-final-regression-20261021.log`.
- Select-only VPCD smoke checks passed 11/11 for Multi AIDs A and B independently, Security, Crypto, JavacardxExt, JavacardxSec, Service, and Biometry. Logs are under `artifacts/logs/deploy-*-vpcd-select-20261021.log` and `artifacts/logs/deploy-multi-applet-*-vpcd-select-final-20261021.log`. These are isolated emulator checks; they do not establish multi-package coexistence or full API coverage.
- Physical Core result `artifacts/results/deploy/pico2-core-resume-20261021.csv` (291 calls: 267 OK, 19 expected unsupported, 3 status errors, 2 runtime exceptions) predates this fix. Historical physical loading of Core, Multi, and Ext is evidence for that prior firmware only. The updated source has not been validated on the Pico.
- The worktree contains extensive unrelated edits from prior sessions. Preserve them and stage only intentional files/hunks if implementation commits are later requested. `TODO.md` contains current updated coverage notes, but also has unrelated existing worktree changes.
- Next: when policy/build constraints permit flashing, flash the ARM image and rerun the physical Core Shareable probes plus the deploy Core/Multi/Ext cases. Continue investigating the remaining Core status errors, package deletion `6581`, and deploy API areas. Do not claim hardware validation of this fix until the new image is flashed and its bounded serial log inspected.

## Current session handoff — deploy API physical coverage — 2026-10-21

- User task: continue the package/AID work by testing multiple applets and the API CAPs under `AlgTest_JavaCard/deploy` on the real Pico 2, fixing problems where practical. ARM-only validation remains in force; repository `AGENTS.md` still requires both architectures to build before flashing. Do not bypass the flash gate or build RISC-V.
- Reviewed the three supplied LLM Wiki session digests. They are redacted operational checkpoints with no project implementation details; they are not canonical project knowledge and were not promoted.
- Physical Pico was visible through the authorized PC/SC path. `PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader` granted access to `/dev/bus/usb/001/028`; `pcsc_scan -r` listed Virtual PCD readers and `Pico 2 CHOUPI CCID [JCAlgTest CCID] (D92E743D5F640C0F) 00 00`. No firmware was flashed in this work segment, so physical results below describe the firmware already on the card, not the latest source build.
- `./tools/build-pc` and `./tools/build arm` passed. The ARM output was `build/arm/pico2_choupi.uf2` (image end `0x100c6428`). No RISC-V build was run; no current-source image was flashed.
- Official Core API 3.0.5 client ran against reader index 3: 291 calls, 267 OK, 19 `UNSUPPORTED_PER_SPEC`, 3 `EXC_RUNTIME`, 2 `SW_ERROR`, and 0 wrong results. The remaining Core failures concern `JCSystem.getAppletShareableInterfaceObject` and OwnerPIN validated-flag operations. This card firmware predates the current source's focused VPCD Shareable fix; physical confirmation is still open. CSV: `AlgTest_JavaCard/deploy/results/Pico2_CHOUPI_current_APISUPPORT_3b800181.csv`; client log: `AlgTest_JavaCard/deploy/results/current-core-manual/APITEST_log_1792590032218.log`.
- Physical API area runs completed and were cleaned up where indicated by the runner: Security 901 calls (43 OK, 822 expected unsupported, 15 SW errors, 13 runtime exceptions, 8 wrong results); Crypto 466 calls (14 OK, 444 expected unsupported, 6 CryptoRuntime exceptions, 2 SW errors); JavacardxExt 13 calls (all SW errors); Service 124 calls (94 OK, 28 wrong results, 1 SW error, 1 runtime exception); Biometry 260 calls (4 OK, 256 SW errors); JavacardxSec 84 calls (all SW errors). These outcomes need classification against each CAP's README/spec before treating every failure as a VM defect. JavacardxExt and JavacardxSec expose optional external-memory APIs and are likely lower priority than framework behavior.
- The Crypto LOAD and first JavacardxExt LOAD returned `6D00` while the Security package occupied the final dynamic package slot. After Security was uninstalled, Crypto and JavacardxExt loaded and ran; cleanup succeeded. This indicates capacity exhaustion rather than CAP incompatibility. The Pico's dynamic package registry has a finite slot budget; inspect package count and reclaim test packages before sequential area runs.
- Crypto failures worth isolating: top-level `Signature()` and `Cipher()` probes returned `6F00`; DES/AES CBC NOPAD operations returned `CryptoException.ILLEGAL_USE` (reason 2). Current-source VPCD probes reproduced AES Cipher construction and operation (`B0838301010D00` -> `000D019000`, operation `B0838305010100` -> `0300029000`) with about 26 seconds per APDU. VPCD is slow here; avoid broad repeated probes without a specific hypothesis.
- KeyBuilder inspection was in progress when handoff was requested. Source `KeyBuilder.java` routes AES and DES types to `AESKeyImpl`/`DESKeyImpl`; the physical Security CSV includes persistent AES/DES `KeyBuilder.buildKey` failures. Next isolate whether the cause is imported token mapping, construction/dispatch, or expected unsupported key type. Relevant physical raw log: `/tmp/apitest_card_viumebfh/APITEST_log_1792590238375.log`; Security CAP AID `4A43416C675465737441706953656331`. Do not implement broad crypto algorithm support; user explicitly said `NO_SUCH_ALGORITHM` for unimplemented algorithms is expected. Focus on correct applet execution and framework/JCSystem behavior.
- `picotool save -f` backup attempt failed with `Unable to locate reset interface`; assume no new flash backup was created. No further mode forcing was performed. Local VPCD crypto probe logs are `artifacts/logs/deploy-crypto-cipher-probe-vpcd-current.log` and `artifacts/logs/deploy-crypto-cipher-operation-vpcd-current.log`.
- Worktree has extensive unrelated modifications and untracked test artifacts. Preserve all. No source files were edited in this session. `.agent/HANDOFF.md` and `progress.md` are already modified by earlier handoff work; append only. Git metadata was reported read-only by a prior session, so no commit was attempted for this handoff. Recheck Git write access before assuming commits are possible; do not stage broad pre-existing diffs.
- Next actions: (1) inspect deploy README expected outcomes for Crypto/Security and distinguish supported behavior from expected unsupported; (2) investigate persistent AES/DES KeyBuilder failure and Core Shareable/validated-flag findings against the latest source through focused host tests; (3) build ARM and PC after any code fix; (4) resolve the ARM-only versus both-arch flash gate before physical flashing; (5) once allowed, flash and rerun Core plus multi-package retention/GET STATUS/DELETE/reinstall, preserving a registry snapshot and results; (6) continue API areas in priority order, recording new causes in `.agent/QKB.md`.

## Current session handoff — Security KeyBuilder host failure — 2026-10-21

- User asked to stop implementation and prepare a handoff. No hardware commands were run this session; no real token was used.
- Active scope remains the deploy API / multi-package request. User's ARM-only restriction is active. Do not build/test RISC-V. Root `AGENTS.md` still says not to flash unless both architectures build; do not bypass that gate. Continue offline unless the instructions are reconciled.
- Focused Security VPCD reproduction: DES `KeyBuilder.buildKey(TYPE_DES, LENGTH_DES, false)` returned `6F00` on current host source. Log: `artifacts/logs/deploy-security-keybuilder-des-probe-vpcd.log`. Debug APPLET_PROCESS reported `status=6F00 exception=1 opcode=8D stack=2 op_history=B67A168D depth_history=01000102`; exception value 1 is `StackUnderflowException`.
- CAP inspection: the Security applet's internal static CP ref at index `0x00A1` points to Method offset `0x12FD` (4861), whose method declaration expects `nargs=3`, while the traced caller has operand depth 2. The caller at method PC 102 invokes `INVOKESTATIC 00A1`; locals began `000A,0000,0009,0003,0040,000C,0000`. `javap` of `ApiTestSecurityKeyBuilder` shows its helper should pass three arguments to `ApiTestSecurityBase.result`, and does not contain the traced `KeyImpl.clearKey()` call. This mismatch suggests a method-code/offset or invocation-resolution issue, but root cause is not established.
- Current `choupi/src/jc_bytecodes/bc_invoke.cpp` contains temporary debug `fprintf` markers `TRACE_STATIC_CP_READ`, `TRACE_STATIC_CP_TAG_OK`, `TRACE_STATIC_CP_REF`, `TRACE_STATIC_CP_INTERNAL`, and a gated detailed trace for package 57 / CP index `0x00A1`. Do not ship these diagnostics: remove only this session's temporary instrumentation after capturing any remaining evidence, preserving unrelated pre-existing source changes.
- Builds: `CHOUPI_JCVM_DEBUG=ON ./tools/build-pc` passed after temporary diagnostics; earlier normal `./tools/build-pc` and `./tools/build arm` passed before this turn's instrumentation. The final current tree has not been rebuilt in normal mode. No RISC-V build, flash, or current-source physical validation occurred. Restore normal host configuration with `./tools/build-pc` when resuming and build ARM after any source change.
- Next concrete steps: (1) inspect the history/rationale for `Method_Handler::getMethodFromOffset()` in `choupi/src/jc_handlers/jc_method.cpp`, especially subtraction of exception-handler table bytes; (2) compare the Security CAP Method component offsets, handler count, internal CP offset `0x12FD`, and classfile/JCA method layout to establish whether the resolved method is wrong; (3) remove temporary traces and reproduce the minimal test; (4) implement only after the offset/dispatch cause is proven, then validate on host and ARM; (5) update `.agent/QKB.md` if a reusable VM dispatch/offset defect is confirmed. Do not interpret expected `NO_SUCH_ALGORITHM` rows as defects.
- Preserve the extensive dirty worktree. Do not reset, clean, or stage broad paths. Existing evidence includes extracted classfiles under `build/deploy-security-classes` and VPCD traces under `artifacts/logs/deploy-security-keybuilder-des-*-vpcd.log`.

## Security Key interface dispatch fix — 2026-10-21

- This entry supersedes the unresolved-offset hypothesis in the preceding
  Security KeyBuilder handoff. CAP Method offset translation is consistent with
  the capmap-free parser: offsets include the one-byte handler-count field,
  followed by the exception-handler table; the VM's `offset - handler_bytes -
  1` indexing is correct.
- Confirmed root cause: the JC305 export defines Security `Key` class token 0
  with interface methods `clearKey=0`, `getSize=1`, `getType=2`,
  `isInitialized=3`, while the embedded API defines them in order
  `isInitialized`, `clearKey`, `getType`, `getSize`. The untranslated
  `getSize` call dispatched to `KeyImpl.clearKey`, then the caller underflowed
  at a three-argument static result call.
- Fix: `choupi/src/jc_handlers/jc_cp.cpp` now translates this imported JC305
  Security `Key` interface with `{1, 3, 2, 0}`, scoped to the Security package
  AID and Key class token 0. This is distinct from the existing DESKey
  interface method mapping. My temporary `TRACE_STATIC_*` prints were removed
  from `bc_invoke.cpp`; existing virtual dispatch changes there were preserved.
- Validation: `./tools/build arm` passed (image end `0x100c6430`);
  `./tools/build-pc` passed and reported `ROMMASK:PASS`. The actual Security
  3.0.5 CAP installed in VPCD and DES KeyBuilder APDU `B0 83 60 01 01 00 00`
  returned `00 00 40 01 90 00`, where the row records 64-bit key size and
  false/uninitialized status. The VPCD script reported `TEST:SUMMARY pass=11
  fail=0`. A separate exploratory set of six row-index APDUs all returned
  `9000`, but payloads `FC` and others are not yet classified. Logs:
  `artifacts/logs/deploy-security-keybuilder-des-remap-vpcd-20261021.log` and
  `artifacts/logs/deploy-security-keybuilder-types-vpcd-20261021.log`.
- No RISC-V build or physical device operation was performed. Per root
  `AGENTS.md`, flashing still requires both architectures to build; the user's
  ARM-only constraint remains active, so continue offline. Existing unrelated
  worktree modifications remain and must be preserved.
- Next: classify the remaining Security/KeyBuilder rows (do not count
  `NO_SUCH_ALGORITHM` as a defect), then continue the deploy Core/Security
  priority tests through VPCD. Resolve the ARM-only versus flash gate before
  attempting physical validation. The current Security result fixes only this
  supported DES KeyBuilder row, not general crypto algorithm coverage.

## Current handoff — stop after Security KeyBuilder fix — 2026-10-21

- User asked to document current work and stop. No source or test work should
  continue until the next user request.
- Latest confirmed implementation is the scoped JC305 Security `Key`
  interface token translation `{1, 3, 2, 0}` in
  `choupi/src/jc_handlers/jc_cp.cpp`. The `Key.getSize()` ordering mismatch
  root cause, builds, and focused VPCD DES KeyBuilder result are recorded in
  the preceding section and `.agent/QKB.md`.
- I attempted the next Core VPCD probe using the official Core CAP and APDUs
  `B0830116010000` and `B08321000000`, writing to
  `artifacts/logs/deploy-core-shareable-current-source-vpcd-20261021.log`.
  The log reaches `VPCD:APPLET_INSTALLED class-index=265` and
  `VPCD:CONNECTED`, but has no recorded custom-APDU response or test summary.
  Treat this probe as inconclusive; do not infer either APDU passed. No test
  process remained running at handoff.
- Constraints remain: user specified ARM-only; do not build/test RISC-V.
  Root `AGENTS.md` still blocks flashing unless both architectures currently
  build, so do not flash or bypass the gate. No physical-token operation was
  performed in this session.
- The worktree is heavily modified by earlier sessions. Preserve all dirty
  tracked and untracked files; do not clean/reset or broadly stage. `.git`
  metadata is read-only in this environment, so no commit was attempted.
- Resume actions: inspect the incomplete Core log and rerun the focused VPCD
  APDUs only if needed; classify the six exploratory Security KeyBuilder rows
  (their `9000` status words do not validate payloads); then proceed through
  deploy Core/Security priorities. Keep `NO_SUCH_ALGORITHM` and other
  spec-allowed unsupported results out of the defect count. Reconcile the
  ARM-only versus both-architecture flash gate before physical validation.
- Session digests reviewed on resume were operational checkpoints only and
  were not treated as canonical topic knowledge.

## Current session update — deploy Core and Security VPCD probes — 2026-10-21

- The user resumed implementation and directed ARM-only work. The user also
  requires continued deploy API applet testing; do not treat this update as a
  stop instruction.
- Re-ran Core Shareable probes against `AlgTestApiCore_305.cap`. The own-AID
  `JCSystem.getAppletShareableInterfaceObject` call returned `00009000`.
  An initially malformed interface-call APDU omitted `Lc=01,data=00` and
  correctly returned `6700`; corrected APDU `B0832100010000` returned
  `0001019000`. Together with SELECT, the harness reported 11/11 checks.
  Logs: `artifacts/logs/deploy-core-shareable-resume-vpcd-20261021.log` and
  `artifacts/logs/deploy-core-shareable-interface-resume-vpcd-20261021.log`.
- Re-tested Security persistent KeyBuilder row indexes 0–2 (DES) and 9–11
  (AES) against `AlgTestApiSecurity_305.cap`. Responses, excluding SW, were
  `00004001`, `00008001`, `0000C001`, `00008001`, `0000C001`, `00010001`;
  each ended in `9000`. They report the expected 64/128/192-bit DES and
  128/192/256-bit AES sizes with success flag set. The test harness summary
  was `TEST:SUMMARY pass=11 fail=0`. Log:
  `artifacts/logs/deploy-security-keybuilder-persistent-rows-vpcd-20261021.log`.
  This confirms host VPCD behavior only, not physical firmware.
- No source code changed in this continuation; no ARM rebuild, USB/PCSC
  operation, flash, or physical APDU was run. User's no-RISC-V constraint
  remains active; the repository's both-architecture flash gate still applies.
- Worktree remains heavily modified. Preserve all existing changes. The new
  VPCD log files and the append-only handoff/progress edits are uncommitted.
- Next: continue deploy Core framework probes or run a bounded Security API
  client test over an available host VPCD/PCSC bridge. Keep the README's
  priority order and treat spec-allowed unsupported algorithms as expected.
  Physical verification remains gated; do not bypass the no-RISC-V/build rule.

## Handoff checkpoint — OwnerPIN dispatch and Core API bridge — 2026-10-21

- User requested: “write down the current work and prepare for handover, then
  stop.” No further implementation, build, test, USB, PC/SC, or hardware work
  was started for this checkpoint.
- Latest source change is in `choupi/src/jc_bytecodes/bc_invoke.cpp`: for a
  public virtual call from embedded ROM to a dynamic receiver, resolve the
  override with the inverse-mapped importing-CAP token. The old argument-count
  fallback was insufficient because methods with the same arity could be
  confused. Trace evidence for OwnerPIN `getValidatedFlag` showed declared
  offset 795 versus incorrect target offset 741; inverse mapping targets 795.
  Focused OwnerPIN APDUs then returned false before PIN check and true after a
  successful check, with VPCD harness summary 11/11.
- The current `bc_invoke.cpp` still contains temporary `INVOKEVIRTUAL_REF` and
  `INVOKEVIRTUAL_RESOLVE` diagnostics. The host executable is the debug build
  (`CHOUPI_JCVM_DEBUG=ON`). A persistent VPCD emulator session was stopped
  after the client disconnected. Remove only the temporary diagnostic prints
  when resuming, retain the inverse-token logic, then restore the normal host
  build.
- Latest official Core VPCD client artifact is under
  `artifacts/results/deploy/core-vpcd-official-fixed/`. The observed CSV had
  progressed through 185 API calls (184 OK, one unsupported) before the client
  stopped receiving a response at `APDUException.IO_ERROR`. Raw VPCD probes
  for APDUException reason values 0–4 all returned `9000` with expected
  payloads and harness summary 11/11, including IO_ERROR (`000004019000`).
  This isolates the unresolved behavior to the official PC/SC/VPCD client
  path; it does not yet prove the bridge is the cause. The CSV has an extra
  initial metadata record, so do not infer row count by treating its header
  as a normal data row.
- Core own-AID and interface Shareable probes pass on host VPCD (`0000 9000`
  and `000101 9000`); six persistent DES/AES KeyBuilder size probes pass with
  expected payloads. These were host VPCD checks, not current-source physical
  validation.
- Build status for this newest OwnerPIN source state is incomplete: debug
  `./tools/build-pc` passed, but normal host build and ARM build have not been
  run after the change. No RISC-V build or physical flash was performed.
  User's ARM-only limit remains active while root `AGENTS.md` requires both
  architectures before flashing, so do not flash or bypass that gate.
- Immediate resume sequence: inspect/remove only the temporary trace code;
  run normal `./tools/build-pc` and `./tools/build arm`; rerun focused OwnerPIN,
  Shareable, and Security KeyBuilder VPCD regressions; continue Core client
  coverage after isolating the IO_ERROR PC/SC cutoff. Treat `NO_SUCH_ALGORITHM`
  as expected. Keep current deploy test artifacts and all unrelated dirty
  worktree files.
- This handoff supersedes earlier entries that call the Core VPCD run
  inconclusive before the OwnerPIN change. Earlier physical results remain
  results of the then-flashed firmware only.

## Current continuation — deploy Core official VPCD coverage — 2026-10-21

- Temporary `INVOKEVIRTUAL_REF` and `INVOKEVIRTUAL_RESOLVE` trace calls have
  now been removed from `choupi/src/jc_bytecodes/bc_invoke.cpp`; retain the
  inverse token mapping and argument-count guard. Normal `./tools/build-pc`
  passed with `ROMMASK:PASS`; `./tools/build arm` passed and produced
  `build/arm/pico2_choupi.uf2` (image end `0x100c63e0`). Existing Rust and
  packed-member warnings remain. No RISC-V build or flash was run.
- Current-source focused VPCD regressions passed: Core OwnerPIN, own-AID
  Shareable, and Shareable-interface APDUs returned `0000019000`,
  `0001019000`, `00009000`, and `0001019000` respectively; harness summary
  `TEST:SUMMARY pass=11 fail=0`. Six Security persistent KeyBuilder rows
  returned `000040019000`, `000080019000`, `0000C0019000`, `000080019000`,
  `0000C0019000`, and `000100019000`; summary 11/11. Logs:
  `artifacts/logs/deploy-core-ownerpin-dispatch-regression-20261021.log` and
  `artifacts/logs/deploy-security-keybuilder-dispatch-regression-20261021.log`.
- The deploy Multi CAP's AIDs ending in `41` and `42` each selected
  independently in fresh VPCD sessions (`9000`, summary 11/11). Logs:
  `artifacts/logs/deploy-multi-a-vpcd-current-20261021.log` and
  `artifacts/logs/deploy-multi-b-vpcd-current-20261021.log`. Host VPCD does
  not retain multiple packages/applets concurrently; do not claim that this
  proves concurrent installation.
- Full official Core client on host VPCD completed. Its final summary reports
  291 calls, 19 not-OK including 11 `UNSUPPORTED_PER_SPEC`, and 0
  `WRONG_RESULT`. All 14 APDUException rows passed in a separate class-scoped
  official-client run, including `IO_ERROR` and `throwIt(IO_ERROR)` (reason
  `0004`). The earlier “IO_ERROR disconnect” was caused by the 900-second
  whole-suite timeout, not an observed bridge failure. Artifacts:
  `artifacts/results/deploy/core-vpcd-official-full-20261021/` and
  `artifacts/results/deploy/core-apduexception-official-20261021/`.
- **CSV count:** the full Core CSV has 291 detail rows: 272 `OK`, 11 allowed
  unsupported, and 8 `APPLET_NOT_INSTALLED`, matching the client summary.
  Count both `javacard.` and `javacardx.` prefixes; matching only the former
  omits the `ExtendedLength` row.
  The missing scenario coverage is due to the host VPCD shim installing only
  Core; Multi and Ext CAPs are not simultaneously available there. The
  physical Core run predates this source revision.
- Physical device enumeration had previously shown the Pico reader, but no
  current-source firmware was flashed or tested. The user's ARM-only
  constraint still conflicts with root `AGENTS.md`'s both-build flash gate;
  do not flash or build/test RISC-V to bypass it.
- Next: improve host scenario setup or find a safe current-source way to test
  Multi/Ext without claiming concurrent package retention. Continue
  Security/Crypto official API coverage and investigate remaining failures by
  priority. Keep
  all dirty worktree files; `.git` metadata remains read-only, so no commit was
  made for this work segment.

## Current continuation — three-reader deploy startup — 2026-10-21

- The user confirmed all three physical Pico PC/SC readers were visible after
  running the access helper. A full API 3.0.5 parallel sweep found them, but
  one worker's concurrent SmartcardIO terminal re-enumeration failed before
  its CSV was created. The same exact reader lookup succeeded alone at index
  5, making a transient concurrent enumeration issue likely.
- Updated `tools/run-deploy-parallel` to capture `(index, exact name)` once and
  pass the pinned Java index plus an explicit GPPro `-r` reader name. This
  avoids a second concurrent Java reader lookup. Python compilation and diff
  whitespace checks passed.
- Confirmed with physical Security/Crypto/Service API 3.0.5 tests using all
  three readers: worker exits were all 0; uploads and cleanup passed. Security
  had 901 calls (820 allowed unsupported, 2 wrong results, 1 SW_ERROR), Crypto
  466 (434 allowed unsupported, 2 SW_ERROR), and Service 124 (28 wrong
  results, 1 SW_ERROR). The final invocation is recorded under
  `artifacts/results/deploy/parallel-20261021-225932/`. No firmware was
  flashed; results apply to the firmware currently on the tokens.
- In the unconstrained full-area run, `JavacardxSec` INSTALL returned `6A88`.
  CAP Import inspection lists `A0000000620001`, `A0000000620101`,
  `A000000062020901`, and `A0000000620205`; this does not establish a
  dependency on the separate Security test CAP. Review these API package
  imports against the intended profile before classifying or implementing
  them.
- The pinned index fix is currently uncommitted. The shared worktree already
  contains extensive unrelated modifications; `.git` is read-only in this
  environment. Preserve them and stage only deliberate files if commit access
  becomes available.
- Next: inspect the `JavacardxSec` API imports against the intended profile.
  Continue missing API implementation and investigate Security, Crypto, and
  Service result rows, ignoring `UNSUPPORTED_PER_SPEC` as a defect.


## Three physical readers confirmed and parallel deploy rerun — 2026-10-21

- The user refreshed the PC/SC helper. Elevated `pcsc_scan -r` listed all three
  Pico readers: D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437.
- Ran `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas
  Core,Crypto,Service --min-devices 3 --max-devices 3` through the elevated
  host-access path. The coordinator pinned Java reader indices 3, 4, and 5 to
  the corresponding exact Pico names and all three worker tests completed in
  about 50 seconds. No firmware was flashed.
- Crypto on D92 completed 466 calls and cleaned up (434
  `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, 11 OK, 3 `EXC_CARDRUNTIME`, 2
  `SW_ERROR`). Service on 5CFC completed 124 calls and cleaned up (94 OK, 28
  `WRONG_RESULT`, 1 `SW_ERROR`, 1 `EXC_RUNTIME`). Core on E0E completed 291
  calls and cleaned up (272 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional Ext
  calls not installed). Optional Ext INSTALL was refused with `6A88`; partial
  LOAD rollback passed. The coordinator returned 1 for that expected optional
  package refusal, not for a failed Core test or cleanup.
- Artifacts: `artifacts/results/deploy/parallel-20261021-231607/`. CSV SHA-256:
  D92 Crypto `202ea769fb83a6fa0b01fb4f7454423119ab560ada5f20b6fde9c19345deb5a8`,
  E0E Core `807e69453bee86d9842195e300a273e55fb3f921f3b99e545f8dd6a550569ed5`,
  5CFC Service `817f061f77f2f0d17744c4a0156e98ce68f099902b986fbc395375b7a4482122`.
- Next: keep one independent API area per Pico worker for predictable parallel
  validation. The setup is ready; continue API and runtime correctness work
  documented in `TODO.md`.


## Partial API client output now fails the runner — 2026-10-21

- `AlgTest_JavaCard/deploy/api_card_runner.py` previously treated any CSV as
  a successful client run, even when the Java client exited nonzero after a
  partial CSV. It now records `TEST_CLIENT_ERROR` with the child exit status.
- A synthetic client-process check wrote one partial CSV row and returned 143;
  the runner preserved the row and recorded a failed TEST stage. Python
  compilation and `git diff --check` passed. No card or VPCD was accessed for
  this validation.
- This corrects the earlier incomplete host Security comparison that was
  mistakenly summarized as successful after interruption. Its 551-row partial
  CSV is diagnostic only. Resume the Security stall investigation with this
  corrected runner before trusting any completion status.
- No commit was created; `.git` is read-only in the current environment.
# Latest physical validation — all remaining API deploy areas — 2026-10-21

- After the PC/SC helper exposed all three devices, ran
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas
  Security,Javacardx,JavacardxExt,JavacardxSec,JavacardxInt,Biometry
  --min-devices 3 --max-devices 3` through the elevated host-access path.
- Java reader indices 3–5 mapped to D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437. Read-only GPPro `--list` calls pinned to each exact name
  confirmed the mapping; the Security test ran on the physical D92 reader.
- Security completed all 901 calls and cleanup: 820 `UNSUPPORTED_PER_SPEC`,
  63 OK, 13 EXC_RUNTIME, 2 SKIPPED_BY_USER, 2 WRONG_RESULT, 1 SW_ERROR.
  Five extension areas were refused at INSTALL with `6A88`, and the runner
  removed each partial CAP LOAD successfully. Thus no API test calls ran for
  those five areas.
- Results: `artifacts/results/deploy/parallel-20261021-232828/`. CSV hashes
  are recorded in `progress.md`.
- No firmware was flashed. Continue Priority 2 install-atomicity coverage;
  do not attribute the expected missing-package `6A88` to VM execution.

## Physical package coexistence and deploy runner routing — 2026-10-21

- Fixed `AlgTest_JavaCard/deploy/api_card_runner.py`: `--reader-name` now
  resolves the exact Java SmartcardIO reader index in addition to passing the
  exact name to GPPro. Before the fix, upload succeeded but the Java client
  stopped at “Select index” and returned 1. Python compilation and diff checks
  passed; a physical `--test-only` run proved the corrected route.
- On physical reader E0E14525E64AF6ED, installed Core with `--keep`, then
  installed Crypto while Core remained. GPPro `--list` showed both CAPs and
  applets present. The official client then completed both areas while both
  packages coexisted: Core 291 calls (272 OK, 11 expected unsupported, eight
  scenario calls not installed because Multi/Ext scenarios were disabled),
  Crypto 466 calls (11 OK, 434 expected unsupported, 16 EXC_RUNTIME, 3
  EXC_CARDRUNTIME, 2 SW_ERROR). Uninstalled Crypto then Core; final GPPro list
  showed only ISD.
- Artifacts: `AlgTest_JavaCard/deploy/results/multi-package-physical-20261021/`.
- Hardware test used the image already present on the Pico; no firmware was
  flashed and these results do not validate current-source changes. Continue
  Priority 2 install durability and GP lifecycle work; keep optional missing
  API packages separate from applet-execution defects.

## JC305 canonical package aliases — 2026-10-21

- `Import_Handler::getPackageIndex()` now matches the standard JC305 AIDs
  `A000000062020901` and `A0000000620205` to CHOUPI's resident
  `javacardx/apdu/util` and `javacardx/security` package AIDs. Static method
  tokens for the imported APDUUtil and SensitiveResult classes are adjusted for
  the local constructor-at-token-zero layout.
- Validation: host and ARM builds passed; install-status 11/11; current-source
  host VPCD install/SELECT of `AlgTestApiJavacardxSec_305` passed 2/2; JCAlgTest
  VPCD regression passed 15/15. No physical image was flashed.
- Important limit: `SensitiveResult.java` still contains TODO/no-op methods,
  and its API behavior was not invoked in the smoke test. Do not mark the
  84-call JavacardxSec area complete. The unresolved next action is to exercise
  those calls and implement correct result tracking/assertion semantics or
  define the package as unavailable.
- Artifacts: `artifacts/logs/deploy-javacardxsec-import-alias-vpcd-20261021.log`
  and `artifacts/logs/jcalgtest-post-import-alias-vpcd-20261021.log`.

## Three-reader deploy coverage refreshed — 2026-10-21

- After the user confirmed the reader helper had run, the parallel runner found
  all three physical readers and mapped Java indices 3–5 to D92E743D5F640C0F,
  E0E14525E64AF6ED, and 5CFC5328F5BDE437.
- The Core/Crypto/Service batch completed concurrent upload, test, and cleanup
  in about 45 seconds. Crypto completed 466 calls (11 OK, 434 expected
  unsupported, 16 EXC_RUNTIME, 3 EXC_CARDRUNTIME, 2 SW_ERROR). Core completed
  291 calls (271 OK, 15 expected unsupported, four optional Ext calls not
  installed, one `Applet.deselect()` WRONG_RESULT); Multi applet setup and
  cleanup passed. Optional Ext INSTALL returned 6A88 and rollback passed.
  Service completed 124 calls (94 OK, 28 WRONG_RESULT, one SW_ERROR, one
  EXC_RUNTIME) and cleanup passed.
- The remaining five API areas were attempted concurrently. Javacardx and
  JavacardxInt INSTALL returned 6F00; JavacardxExt, JavacardxSec, and Biometry
  returned 6A88. All partial CAP LOADs were rolled back; no test methods ran
  for these areas.
- Artifacts and hashes are in the matching 2026-10-21 entry in `progress.md`.
  No firmware was flashed. The tests use the images already on the cards.
- Next: investigate the Core deselect and Service result mismatches. Continue
  import/profile diagnosis for install refusals. Maintain one independent area
  per Pico worker. Do not treat `UNSUPPORTED_PER_SPEC` as a bug.

## Three-reader deployment and current-source Dispatcher validation — 2026-10-22

- After the user ran the reader installer helper, `pcsc_scan -r` listed all three Picos both before and after testing: D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. The runner's dry run paired them with Java reader indices 3–5 and assigned Crypto, Core, and Service.
- The live three-worker batch completed uploads, API runs, and cleanup on all three boards. Crypto: 466 calls (11 OK, 434 allowed unsupported, 16 runtime exceptions, three card-runtime exceptions, two status errors). Core: 291 calls (272 OK, 15 allowed unsupported, four optional Ext calls unavailable). Service: 124 calls (94 OK, 28 wrong results, one status error, one runtime exception). Core optional Ext INSTALL returned `6A88` and rollback passed; this optional stage made the coordinator return 1 even though all workers exited 0 and all cleanup stages passed. No firmware was flashed.
- Re-ran the current-source Service CAP through host VPCD and decoded all 33 Dispatcher cases with the official deploy client's `ApiTesting` decoder: `SERVICE_API:SUMMARY total=33 pass=33 fail=0`; harness `TEST:SUMMARY pass=35 fail=0`. Logs: `artifacts/logs/service-dispatcher-official-decode-vpcd.txt` and `artifacts/logs/service-dispatcher-official-decode-vpcd-emulator.log`.
- Initial restricted VPCD test startup failed opening its loopback listener (`Errno 1 Operation not permitted`) before sending any APDU. Re-running the host-only emulator harness with escalated local socket access passed. This did not use libusb or the physical readers.
- Next: use the same current-source VPCD decoder to cover BasicService methods; RMIService `setInvokeInstructionByte` and `processCommand` remain explicit stubs. Physical Service mismatches above are from pre-existing firmware.
- No RISC-V build or physical flash was run. ARM and host builds passed in the prior implementation step; logs are `artifacts/logs/build-arm-service-dispatcher.log` and `artifacts/logs/build-pc-service-dispatcher.log`.

## BasicService and RMIService API decoder pass — 2026-10-22

- Ran 39 BasicService API cases plus five RMIService catalog cases through the
  current-source Service CAP on VPCD. The official catalog decoder reports
  `SERVICE_API:SUMMARY total=44 pass=44 fail=0`; harness reports
  `TEST:SUMMARY pass=46 fail=0` (ATR, SELECT, and 44 APDUs).
- BasicService state guards and CSF helpers matched expected responses. The
  RMIService null constructor was handled correctly. The catalog cases do not
  exercise a real RMI SELECT/INVOKE exchange. `processCommand()` is still a
  stub; `setInvokeInstructionByte()` does not retain its value.
- Logs: `artifacts/logs/service-basic-official-vpcd.txt`,
  `service-basic-official-decoded.txt`, `service-rmi-official-vpcd.txt`, and
  `service-rmi-official-decoded.txt`.
- Next: inspect the JC305 RMI protocol and existing remote object/export
  primitives before implementing the wire path. Keep the current ARM-only,
  no-flash gate.

## Three-device setup checkpoint — 2026-10-22

- The user reports that the reader installer helper has run and all three
  physical Pico 2 readers are now visible. This report was not independently
  rechecked in this stop request.
- Work was stopped before starting another deployment or issuing any USB/PCSC
  commands. No source, firmware, or test artifacts were changed in response to
  this report.
- On resume, first enumerate readers through the authorized host-access path
  and map the three stable reader identities. Then continue one independent
  deploy area per device, preserving bounded logs and per-device CSV output.
  Do not flash; keep ARM-only and the existing no-flash gate.
- The latest implementation checkpoint remains the current-source VPCD
  Service API decoder pass above. Its next code task is to inspect the JC305
  RMI protocol and existing remote-object/export primitives before implementing
  the RMI wire path.
- No validation or commit was performed for this handoff-only checkpoint.


## Nine-area three-reader deploy retest — 2026-10-22

- The user-reported three-reader setup was independently confirmed by the
  elevated host-access dry run. Stable IDs were D92E743D5F640C0F,
  E0E14525E64AF6ED, and 5CFC5328F5BDE437 (Java reader indices 3–5).
- `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --areas
  Core,Security,Crypto,Javacardx,JavacardxExt,JavacardxSec,JavacardxInt,Service,Biometry
  --min-devices 3 --max-devices 3 --out-dir
  artifacts/results/deploy/parallel-20261022-053706` completed all workers.
  Security: 901 calls (63 OK, 820 expected unsupported, 13 runtime exceptions,
  two wrong results, one status error, two excluded). Crypto: 466 (11 OK, 434
  expected unsupported, 16 runtime exceptions, three card-runtime exceptions,
  two status errors). Service: 124 (94 OK, 28 wrong results, one status error,
  one runtime exception). Core: 291 (271 OK, 15 expected unsupported, four
  optional calls unavailable, one wrong result). Core/Crypto/Service/Security
  CAP cleanup passed.
- Optional Core Ext plus Javacardx, JavacardxExt, JavacardxSec, JavacardxInt,
  and Biometry INSTALL each returned `6A88`; all partial packages were rolled
  back. This matches the known absent-import profile. The runner exited 1 to
  report these refused INSTALL stages. No current-source firmware was flashed;
  these results describe the firmware already installed on the boards.
- CSV hashes: `pico1.csv`
  `07953c4ee70bc53ef617d0fe98b4720ca04adfa0d48fe4f404ecf0dfb1aae751`,
  `pico2.csv`
  `f70b8a325f08fbf6f7eef66d137c5530599f147417c1303cf6c9f5b77dfb3697`,
  `pico3.csv`
  `b6c58cad34b9e7c48cfbb64962f1bc39b40144f35a10a784ac361445d1098f44`.
- Resume with current-source offline work. TODO's next concrete lifecycle gap is
  exercising authenticated GP DELETE over APDU and verifying GET STATUS and
  reinstall behavior after restart. Keep ARM-only and do not flash under the
  existing architecture build gate.


## PC VPCD default ISD selection checkpoint — 2026-10-22

- The full nine-area physical JC305 run above is complete and its results are
  archived. No real board was modified or flashed during the new work.
- Ran `./tools/test-gp-delete` (26/26) and `./tools/test-gp-registry`
  (47/47). These validate parser/registry helpers, not the authenticated GP
  APDU exchange.
- Reproduced plan WP6's host VPCD check with current-source PC emulator and
  bundled GPPro 20.01.23. `gp --list` first sent case-1 SELECT `00A4040000`;
  the emulator replied `6999`. The ARM CCID path already answers this APDU as
  default ISD selection, while the host VPCD transport lacked that case and
  card-manager APDU routing.
- Added the empty-AID ISD case and card-manager callback seam in
  `choupi/src/vpcd_session.*`; `./tools/test-vpcd-session` passes 21/21.
  `./tools/build-pc` and `./tools/build arm` pass. PC build still emits
  dependency Rust warnings and existing packed-member warnings. No RISC-V build
  or flash was run.
- Next: implement the host VPCD card-manager callback with SCP03, GET STATUS,
  CAP LOAD/INSTALL, and DELETE, then rerun GPPro `--list`, `--install`,
  `--delete`, and restart/registry checks on the Virtual PCD reader.
- Commit: not created; `.git` metadata is read-only in this environment.

## Fresh GPPro card-manager lifecycle — 2026-10-22

- USB enumeration sees all three Pico 2 boards as `2e8a:000f` RP2350 BOOTSEL
  devices (bus 001, devices 061–063). They are not PC/SC readers in this mode.
  Do not flash under the current ARM-only/both-architecture flash gate.
- The PC VPCD callback in `choupi/src/main_pc.cpp` was exercised with bundled
  GPPro 20.01.23 on Virtual PCD reader 00. The clean manager began with ISD
  only, authenticated SCP03, loaded and installed the supplied
  `AlgTest_v1.8.2_jc305.cap`, listed its selectable applet and package, deleted
  the applet instance, confirmed the package remained, deleted the package,
  and returned to ISD only.
- `--class-index` is optional now. Supplying it reconstructs one installed
  instance for resume tests; omitting it starts with an empty registry.
  Per-package CAP lookup waits for committed package state. Run
  `pcsc_scan -r` before starting the CHOUPI VPCD client so the Virtual PCD
  endpoint is active.
- Validation: `./tools/build-pc` passed; `./tools/test-vpcd-session` passed
  21/21 before this PC-only argument/state change. Full transcript:
  `artifacts/logs/gp-vpcd-card-manager-gppro-20261022.log`; emulator log:
  `artifacts/logs/gp-vpcd-card-manager-20261022.log`. No physical reader, flash,
  architecture rebuild, or hardware result was produced in this step.
- `PICO2_HOST_USB_ACCESS=1 ./tools/probe-jc-algtest --reader 'Virtual PCD 00 00'
  --version-only` passed after GPPro installed the CAP: SELECT, GETVERSION
  (`1.8.2_jc305`), and JCSystem info all returned `9000`.
- Next: test the saved image after clean emulator shutdown/restart. Current PC
  VPCD registry still models one dynamic package and instance.
- Commit: not created; `.git` metadata is read-only.

## Earlier AES cipher investigation note — superseded 2026-10-22

The prior investigation incorrectly left this item paused with a diagnostic
image. The caller-scoped interface remap above is now built, flashed, and
confirmed on hardware. The original investigation evidence remains in earlier
`progress.md` records.

## JC305 Signature token map validated; AES Cipher rows remain — 2026-10-22

- Added a package/class scoped virtual-method token map for imported JC305
  `javacard.security.Signature` (class token 15). The Crypto probe's
  `ApiTestCryptoProbeSignature` subclass overrides `getAlgorithm()`; its marker
  method now dispatches correctly through the imported Signature reference.
- Both `./tools/build arm` and `./tools/build riscv` passed. The ARM image was
  flashed to D92 serial `D92E743D5F640C0F`; the 20-second finite monitor had 20
  runtime passes and no reported fault. The exact-reader physical Crypto run
  completed upload, all 466 calls, and cleanup.
- CSV `artifacts/results/deploy/crypto-signature-vmap-d92-20261022.csv`
  (SHA-256 `5f3eb8024111c4081f049c1b20fe4a3eaf005b07dbd40e79e5616be68839c719`)
  reports 20 OK, 444 unsupported, one card-runtime exception, and one wrong
  result. Signature's previous `6F00` row is gone.
- Next: investigate the remaining AES CBC-NOPAD IV overload result. CAP
  disassembly shows the AES test passes an in-bounds 16-byte IV at offset 32;
  SDK source also accepts those dimensions. Trace dispatch and actual runtime
  values before modifying validation. Separately, reconcile the zero-length
  `doFinal` observation with the CAP and JC305 expected result before changing
  semantics. Do not implement other algorithms to reduce unsupported counts.
- `.git` remains read-only; no commit was created. Existing worktree changes
  were preserved.

## Physical JC305 CBC streaming and empty-doFinal fixes — 2026-10-22

- Fixed `SoftwareCipher.update()` so partial blocks are retained until the next
  update or `doFinal()`. Complete CBC blocks are processed during update and
  the chaining IV is carried across calls. `doFinal()` rejects a no-padding
  operation with no input, joins pending bytes to its final input, and resets
  IV/pending state after success. Aligned calls use the original input buffer
  directly; the temporary-array-per-call version exhausted resources during a
  50-call AES static measurement and returned `6400`.
- Both ARM and RISC-V build wrappers passed. ARM images flashed successfully
  to D92 and 2BED. Their finite monitors passed. The final physical Crypto CAP
  run on D92 completed upload, 466 calls, and cleanup: 22 OK, 444 expected
  unsupported, and no other call status.
- The AES static APDU sequence passed on D92. Prepare and five baselines
  returned `AA9000`; the 50-call `doFinal()` measurement also returned
  `AA9000` and `TEST:PASS` in 4,114 ms (baselines: 1,229–1,250 ms each). Full
  `ALG_PERFORMANCE_STATIC` was not rerun. PC/SC visibility fluctuated; the
  authorized reader helper restored the D92 and 2BED reader entries.
- Crypto CSV:
  `artifacts/results/deploy/crypto-stream-final-d92-20261022.csv` (SHA-256
  `467e3c71f26c2a5a927c7e33e34766893899083e34668802d9df6198fd4f3683`). Static
  transcript:
  `artifacts/logs/aes-cipher-static-after-streaming-fix-retry-20261022.log`.
- Remaining scoped work: run `ALG_PERFORMANCE_STATIC` only if needed for the
  next acceptance decision; do not try to eliminate `NO_SUCH_ALGORITHM` rows
  by implementing unrelated algorithms. The monitor continues to print the
  known `WATCHDOG_RECOVERY cause=other` marker alongside passing runtime
  assertions; this run had no `FAULT` or `TEST:FAIL`.
- `.git` remains read-only; no commit was created. Preserve all unrelated
  dirty changes.

## Current-source Security/Core physical deploy follow-up — 2026-10-22

- Reflashed current ARM image on D92 and 2BED after both architecture wrappers
  passed. D92 and 2BED finite serial monitors showed only passing runtime
  summaries. A two-reader parallel JC305 deploy run completed Security on D92
  (901 calls) and Core on 2BED (291 calls).
- Security counts: 77 OK, 820 expected unsupported, two skipped RSA-2048
  keypair cases, and two `Key.isInitialized()` wrong-result follow-ups. Both
  transient AES key setup calls returned `NO_SUCH_ALGORITHM`, so the dependent
  checks do not demonstrate a transient-key bug. Preserve the existing QKB
  classification; do not implement transient crypto storage just for these
  counts.
- Core counts: 272 OK, 15 expected unsupported, four Ext applet methods
  unavailable because optional Ext INSTALL returned `6A88`. Its rollback
  succeeded. Core and Multi cleanup passed. Security cleanup and worker exit
  passed.
- Artifacts: `artifacts/results/deploy/current-source-sec-core-post-cipher-20261022/`.
  Core CSV SHA-256 `b9d90093a17c9ee686ca615d36d90f972201efb79b1a54b0e013317a6ae516a4`;
  Security CSV SHA-256 `738fd65657523eaa9aca77881f7c194c77bae8161e0ce6af2348ffe697b16686`.
- Current-source physical API validation now covers Crypto, Security, and Core
  on the latest ARM image. Next useful implementation work remains the open
  Priority 3 RMI gaps in `TODO.md` or framework/profile items; do not reopen
  unsupported algorithms. Repository metadata remains read-only, so no commit
  was created.

## Final in-place CBC decrypt safeguard verified — 2026-10-22

- Added preservation of the last ciphertext block for in-place CBC decryption
  before the native routine writes output; the saved block becomes the next IV.
- Both architecture build wrappers passed. The final ARM image was flashed and
  verified on D92 and 2BED; finite monitors passed on both. On final D92, the
  exact Crypto CAP run completed all 466 calls (22 OK, 444 expected
  unsupported, zero unexpected statuses). The AES static 50-call `doFinal()`
  probe passed; measurement APDU elapsed 4,115 ms.
- Final Crypto CSV:
  `artifacts/results/deploy/crypto-cipher-inplace-final-d92-20261022.csv`
  (SHA-256 `aeafc4fddf95e785c062db87528ba2b090ce1ef1a7605da64828aef81741d4df`).
  Static transcript:
  `artifacts/logs/aes-cipher-static-final-20261022.log` (SHA-256
  `fc3c3668e83dcdff0e17a0962077a01cb8ee7430ab1e5ec7ca7a52ba2aad907e`).
- Security and Core had passed on both devices immediately before this final
  in-place-only edit; they were not rerun afterward. The edit is confined to
  Cipher CBC IV preservation. No full `ALG_PERFORMANCE_STATIC` run was made.

## Transitive custom exception RMI coverage — 2026-10-23

- Added applet-defined intermediate superclasses for the fixture's checked and
  runtime exceptions. The RMI methods still throw their leaf exception classes,
  forcing the VM's superclass walk to reach the nearest Java Card API class.
- `./tools/build-rmi-array-fixture` passed. Elevated
  `./tools/test-rmi-arrays-vpcd` passed 28/28 class-form checks and 3/3
  interface-form checks. Runtime encoded as `830900009000`; checked encoded as
  `830500009000`.
- This validates one intermediate level for both exception branches; longer
  chains, malformed hierarchy metadata, and all API reason codes remain open.
  No physical hardware was used and no firmware was flashed.
- Commit was not created because Git could not create `.git/index.lock`
  (read-only repository metadata).
- Next: add focused API exception reason-code coverage for remaining RMI maps.

## Preserve Java Card SystemException reason over RMI — 2026-10-23

- Added a fixture method that calls `SystemException.throwIt(0x1234)` and an
  exact expected RMI response. The first run showed the mapper walking past
  `SystemException` to `java.lang.RuntimeException`. JC305 framework token 13
  maps to embedded export token 22; the reason field is slot 0.
- Updated RMI exception mapping to identify the embedded SystemException class
  and return its 16-bit reason. `./tools/test-rmi-arrays-vpcd` passed 29/29
  class-form APDUs and 3/3 interface-form APDUs. ARM and RISC-V builds passed.
- Full CHOUPI ARM firmware flashed to D92 with its exact serial. The 18-second
  monitor passed 18 runtime checks. The physical probe installed the fixture,
  passed SELECT/int[] calls and SystemException reason (`822512349000`),
  removed the fixture, and verified JCAlgTest remained installed.
- `tools/cycle arm` flashes the ARM smoke image; `tools/cycle choupi --serial`
  flashes the JCVM. The smoke image was briefly used during this check, then
  replaced by full CHOUPI. D92 is left running CHOUPI.
- Commit was not created because `.git/index.lock` is unavailable in this
  read-only repository metadata environment. Other pre-existing modified files
  remain untouched.
- Next: cover reason preservation for other Java Card exception classes, then
  continue broader Service/RMI profile work.

## Three-token API 3.0.5 deploy and Service device comparison — 2026-10-23

- Ran all 11 API 3.0.5 CAP scenarios across three physical Pico readers. The full split is recorded in `artifacts/results/deploy/parallel-20261023-three-token-clean-1234/SUMMARY.md` with the worker CSVs and logs.
- Security, Crypto, JavacardxSec, and Service executable areas passed their worker stages. Core ran 291 calls (272 OK, 15 expected unsupported, 4 Ext methods unavailable); Core and Multi cleanup passed. Core Ext and optional Javacardx/JavacardxExt/JavacardxInt/Biometry INSTALLs returned `6A88`; rollback passed for every partial LOAD.
- Matched Security results were identical on all three readers: 901 calls, 77 OK, 820 unsupported, 2 skipped RSA-2048, and the same 2 transient AES `Key.isInitialized()` wrong results.
- Matched Service results differ by board: serials `8E464F4E2BBC0A19` and `2BED26ED20556535` each passed 124/124; D92 serial `D92E743D5F640C0F` reproduced 12 `SW=6400` Dispatcher errors in concurrent and solo runs. Upload/cleanup passed and all three readers remained available. QKB has the issue details.
- The attempted subset-only Security run uncovered that empty parallel-worker area lists become “all areas” in `api_card_runner.py`; see QKB. Do not use a subset with fewer areas than devices until the wrapper skips empty buckets or supports explicit replication.
- No firmware was flashed; builds were not needed. Final elevated dry run found all three readers. No PC/SC connection timeout or reader loss occurred.
- Next: compare board firmware identities; investigate why D92's Service Dispatcher results differ, then fix and validate the parallel wrapper's empty-bucket behavior.

## Full JC305 CAP suite after reflashing all three tokens — 2026-10-23

- `./tools/build arm` and `./tools/build riscv` passed. Flashed the full `pico2_choupi.uf2` to exact serials `8E464F4E2BBC0A19`, `2BED26ED20556535`, and `D92E743D5F640C0F`. All three cycle logs report `FLASH:PASS`, repeated `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, and `CYCLE:PASS`. The same resulting UF2 SHA-256 was `0945dd18ce32477b82c8f224a5f1c51f45cfe3b62a6a413809de629d354b938a`.
- Ran every API 3.0.5 area and Core Multi/Ext scenarios on each exact reader, including RSA-2048 calls. Each token completed 1,866 calls. Tokens 1/2: 579 OK, 1,281 expected unsupported, 4 Ext-dependent unavailable, 2 wrong results. Token 3: same plus 11 Service `SW_ERROR` (`SW=6400`), so 568 OK.
- The two transient AES `Key.isInitialized()` wrong results match on every token. Crypto: 22 OK / 444 expected unsupported. JavacardxSec: 84/84 OK. Core: 272 OK / 15 unsupported / 4 Ext applet unavailable. Core Ext plus Javacardx, JavacardxExt, JavacardxInt, and Biometry INSTALL returned `6A88`; all partial LOADs rolled back. Successful package cleanup passed everywhere.
- Reflashing an identical image did not change D92's Service error behavior; firmware image mismatch is ruled out. Investigate persisted card/runtime state and Dispatcher paths before implementing a fix. QKB contains the expanded finding.
- One `WATCHDOG_RECOVERY cause=other` diagnostic appeared in each monitor alongside repeated passing summaries. Existing progress/TODO records classify the retained marker alone as non-failure.
- Final elevated dry run found all three readers. Full report/CSVs are in `artifacts/results/deploy/full-20261023-three-reflashed-all-areas-1315/`; flash logs are `artifacts/logs/cycle-choupi-20261023T110123Z.log`, `...T110257Z.log`, and `...T110419Z.log`.
- Next: inspect the token-specific Service Dispatcher `SW=6400` behavior after identical image reflash; compare persistent card state and a fresh state if safely available.

## Selectable Java Card firmware capabilities — 2026-10-23

- Added a new top-priority implementation checklist to root `TODO.md` for the 15 selectable JCAlgTest v1.8.2 capabilities and the three requested build profiles.
- Initial source inspection found existing algorithm bodies in public API classes (`OwnerPINBuilder`, `KeyBuilder`, `MessageDigest`, `RandomData`, `Cipher`) and AES/DES/SHA-256/random native code in `choupi/src/jni.cpp`. The API submodule already has unrelated dirty state (`src/javacardx/crypto/Cipher.java` and untracked `sdks`); preserve it while working there.
- `tools/compare-jcalgtest-support.py` currently rejects an empty support list, so minimal configuration needs a deliberate comparator change. Existing default support declaration is 15 rows in `tools/jcalgtest-supported-v1.8.2.csv`.
- Next: implement strict `config/javacard-support.json` parsing and generated capability source/header, with tests for defaults, each of 15 flags, dependency conflicts, and minimal selection. Then wire both PC/Pico API build and `build-pc` ROM-mask generation to the validated configuration before extracting providers/native modules.
- Preserve existing uncommitted changes and use root build wrappers. Run ARM and RISC-V builds before any physical flash; flash the full ARM CHOUPI image only after both pass.

## Capability configuration generator — 2026-10-23

- Added `config/javacard-support.json` with one boolean per each of the current
  15 JCAlgTest support rows and `tools/javacard-support.py` to validate it and
  emit CSV, CMake, C-header, and normalized JSON metadata. CBC selections
  require a compatible selected key capability.
- The comparator now accepts an empty expected set. Focused tests pass 4/4;
  generated current-support CSV matches the existing 15-row CSV byte for byte.
- No firmware build or hardware operation was run in this tooling-only step.
- Next: wire generated selections into both PC and Pico builds and make config
  changes invalidate ROM-mask/JNI generation; then extract Java providers and
  native modules. Preserve existing dirty API submodule and unrelated VM/tool
  changes.

## Support config build wiring — 2026-10-23

- `tools/build-pc` now emits generated support metadata before CMake; both the
  nested PC build and root Pico build include the same generated CMake values.
  `tools/build` compares the support JSON hash against the PC generation
  manifest, forcing API/ROM-mask/JNI regeneration after config edits. API and
  ROM-mask custom commands also depend on the JSON/config metadata. Comparator
  default expected CSV now points to generated output.
- ARM wrapper and RISC-V wrapper both passed. The full ARM CHOUPI image was
  built but not flashed. ROM-mask/JNI output and manifest checksum checks
  passed. The API still contains current providers regardless of selection.
- Next: inspect factory behavior and extract optional Java implementations
  behind generated dispatch. Then split native functions and conditionalize
  both PC/Pico module/library source lists.

## Secure random provider extraction — 2026-10-23

- `RandomData.getInstance` now routes through generated `RandomDataProvider`;
  the secure-random implementation is a separate source template emitted only
  when enabled. The generated API source root is compiled into the ROM mask.
- Focused generator tests pass 4/4. ARM and RISC-V wrappers pass with the
  current all-enabled selection; API compiles 139 sources and mask/JNI output
  regenerates. No physical flash was performed.
- API submodule commit `4541b95` contains only `RandomData.java`; prior dirty
  `Cipher.java` and untracked `sdks` remain untouched.
- Next: extract selected key providers and cipher providers while retaining
  API signatures. Then finish SHA-256/PIN providers and split native modules.

## AES/DES key provider extraction — 2026-10-23

- `KeyBuilder.buildKey` delegates to generated `KeyBuilderProvider`; generic
  key storage and AES/DES concrete classes live in separate provider sources.
  Generation omits AES or DES provider source when that key family is
  unselected, and dispatch maps the individual supported key lengths.
- Focused tests pass 5/5, covering default, no-AES, and minimal generated
  source sets. Current-support ARM and RISC-V builds pass; API compiles 143
  sources, ROM-mask/JNI generation succeeds, and no hardware was flashed.
- API submodule now has provider delegation commits `4541b95` (RandomData) and
  `420c82a` (KeyBuilder). Existing dirty `Cipher.java` and untracked `sdks`
  remain uncommitted and preserved.
- Next: extract Java Card cipher provider(s) and then digest/PIN variants;
  split native modules and verify smaller no-AES/minimal ROM masks/images.

## CBC cipher provider extraction — 2026-10-23

- Public `Cipher.getInstance` entry points now call generated `CipherProvider`.
  CBC processing is an algorithm-neutral shared base with separate AES and DES
  provider subclasses, emitted only when corresponding cipher capabilities
  are selected. Minimal output contains no CBC implementation sources.
- The 5 focused capability tests pass. API compiled 147 sources, ROMMASK
  emitted a 36,547-byte image and JNI header, and ARM/RISC-V build wrappers
  passed. Full ARM image end is `0x100c9900`. No board flash was performed.
- The first extraction attempt removed the Javadoc opener and was rejected by
  javac; restored it and reran successfully. API submodule commit is `f327c66`.
- Next: extract SHA-256 and OwnerPIN providers, then split native modules and
  perform profile-by-profile ROM-mask, image, GPPro, and JCAlgTest checks.

## SHA-256 provider extraction — 2026-10-23

- `MessageDigest.getInstance` dispatches to a generated SHA-256 provider; its
  source is omitted when disabled. The public API no longer embeds
  `Sha256NativeBridge` calls. `OneShot.open` remains unsupported, and its
  signatures remain present without retaining native SHA-256 logic.
- Focused support tests pass 5/5. ARM and RISC-V wrappers pass; API compiled
  149 sources, ROMMASK is 36,452 bytes, and the full ARM CHOUPI image linked.
  No board flash was performed. API submodule commit `f6dab0e`.
- Next: extract optional OwnerPIN variants, then move selectable SHA/random/
  AES/DES native implementations into separate modules and split build library
  dependencies. Keep the AES source needed by SCP03 card management.

## OwnerPIN provider extraction — 2026-10-23

- `OwnerPINBuilder` now calls a generated `OwnerPINBuilderProvider`. Factory
  dispatch enables/omits the standard, X, and X-with-predecrement variants
  according to `config/javacard-support.json`; unsupported types throw the
  documented Java Card exceptions. Implementation helper sources are held in
  `tools/java-support/` and only emitted when selected.
- The focused support generator suite passes 5/5, and both `./tools/build arm`
  and `./tools/build riscv` pass using all 15 selected flags. No physical flash
  has been performed during this capability-selection task.
- API submodule change was committed as `727767c`; root commit and root
  submodule pointer update are pending.
- Remaining: extract optional native random, CBC AES/DES and SHA-256 JNI
  functions; condition PC and Pico native source/library selection; run three
  profile builds and verify generated expected CSVs, ROM masks and image sizes;
  then run full ARM hardware / GPPro / JCAlgTest acceptance where hardware is
  available.

## Native modules and profile build validation — 2026-10-23

- Optional JNI methods are no longer in the always-present
  `NativeImplementation` class. Generated bridges for random data, CBC, and
  SHA-256 only enter the API ROM mask when selected. `native_random.cpp`,
  `native_aes_cbc.cpp`, `native_des_cbc.cpp`, and `native_sha256.cpp` are
  separately selected by both PC and Pico build descriptions. `native_cipher`
  contains only shared JNI validation/dispatch. Mbed TLS DES/SHA and Pico SHA
  are conditional; Mbed TLS AES and Pico random remain because SCP03 uses them.
- Current, no-Java-Card-AES, and minimal configs each passed ARM and RISC-V
  wrappers. The current expected CSV matched the checked-in 15-row baseline;
  focused tests passed 5/5. Sizes (active ROM mask / fixed flash container /
  ARM binary / ARM UF2) in bytes: current `36574 / 524300 / 825560 / 1651712`,
  no-AES `36169 / 524300 / 824576 / 1649664`, minimal
  `32491 / 524300 / 812792 / 1626112`. Root config is restored to all enabled.
- API native bridge removal commit: `9d9ab43`; root implementation/progress
  commit is pending. Builds are not hardware acceptance: no profile has been
  flashed as part of this task. Next use elevated host USB access from the
  first USB probe, identify the physical reader, then perform finite runtime,
  GPPro install/select, and complete JCAlgTest comparison for each ARM image.
- Build logs are currently in `/tmp/jc-cap-*-{arm,riscv}.log` (current ARM
  final log is `/tmp/jc-cap-current-final-arm.log`). Output files are in
  `build/choupi-pc/` and `build/arm/` for the restored current profile.

## 2026-10-23 — Physical full-suite reflash follow-up

- Built both ARM and RISC-V successfully with the all-enabled support profile.
- Reflashed exact readers `2BED26ED20556535` and `D92E743D5F640C0F` using
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial <serial>` under the
  elevated host-access path. Both passed flash verification and the finite
  serial monitor (`TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`).
- Full API 3.0.5 suite completed on both with all nine areas and
  `--exclude ''`. CSVs: `artifacts/results/deploy/full-305-2BED26ED20556535-20261023.csv`
  and `artifacts/results/deploy/full-305-D92E743D5F640C0F-20261023.csv`.
  2BED Service passed 124/124; D92 Service returned 11 `SW=6400` among 124.
  Security and Crypto totals matched between devices but had respectively 79
  and 192 `SW=6F00` calls. See the newest QKB entry before interpreting these;
  unlike previous baseline runs, RSA-2048 exclusions were disabled.
- Optional `Javacardx`, `JavacardxExt`, `JavacardxInt`, and `Biometry` CAPs,
  plus the Core Ext scenario, were refused with `6A88`; rollbacks succeeded.
  `JavacardxSec` passed 84/84. The third token `8E464F4E2BBC0A19` is not
  exposed by this VM: both `lsusb -d 2e8a:` and `pcsc_scan -r` show only
  2BED and D92, so it has not been reflashed or tested in this session.
- Current UF2 SHA-256 is
  `d4fd102051767fce3ed72abeac3eef581c579b0e63b20048f1040b80fc364b57`.
  Cycle logs are `artifacts/logs/cycle-choupi-20261023T141401Z.log` and
  `artifacts/logs/cycle-choupi-20261023T141535Z.log`.
- Next: inspect exact Security/Crypto `6F00` calls and determine whether their
  change from baseline is caused by enabling RSA-2048 tests; retry the third
  device once it is attached to the VM. The token-specific D92 Service failure
  remains reproducible. Validation records were committed as `ea095bc`; the
  progress record's commit SHA was recorded in follow-up `5cd8ca9`.

## Current vs minimal JCAlgTest hardware initialization blocker — 2026-10-23

- Built both ARM and RISC-V in the minimal (0/15) and current (15/15) profiles. Both flashed to exact Pico 2 `2BED26ED20556535` and passed finite runtime monitor. Restored all-enabled config and reflashed the same board at the end.
- Fresh `AlgTest_v1.8.2_jc305.cap` on the same board: SELECT `9000`; current-profile GETVERSION `F103`; minimal-profile GETVERSION `6F00`. Neither profile reaches valid `yes`/`no` support rows, so the requested AES/SHA report comparison is not yet complete. Do not use the partial minimal CSV in `artifacts/results/jcalgtest-capability-profile-20261023/minimal/`.
- Fixed class-token map entries by matching JC305 and embedded security export names. Extended imported static-field lookup across all static bytecodes. These changes pass both builds but did not clear the applet init failure. Current `F103` is CryptoException reason 3 / NO_SUCH_ALGORITHM; the exact stage remains unidentified. Static-field access remains a plausible area, not a confirmed cause.
- Config is all true; board is running the current image. Current exact-reader GETVERSION transcript: `artifacts/logs/jcalgtest-current-version-probe-20261023.log`.
- Full comparison record and cycle paths: `artifacts/results/jcalgtest-capability-profile-20261023/physical-comparison.md`.
- Next: isolate `JCAlgTestApplet.allocateResources()` constructor stages with a diagnostic CAP or targeted imported class/static method probes. Once GETVERSION succeeds, rerun `ALG_SUPPORT_EXTENDED` on current and minimal profiles, compare each CSV to its generated expected CSV, and only then report AES/SHA deltas.

## Firmware/JCVM architecture guide — 2026-10-23

- Added `FIRMWARE_JCVM_ARCHITECTURE.md` (1,653 words, four sections) from active Pico ARM boot, CCID, VM, crypto and flash source inspection. It explicitly separates inactive STM32/Rust OS and the RISC-V smoke build.
- Documentation-only validation: source paths/symbols, section count, word count and SHA-256. No firmware build, flash or USB operation occurred.
- Preserve existing unrelated worktree changes. The guide is the completed deliverable; any future implementation work should inspect generated JNI output and the selected capability configuration first.


## Minimal JCAlgTest uncaught exception mapping — 2026-10-23

- Corrected JC305 class-token translations against names in the JC305 3.0.5 and embedded API `.exp` exports. Framework `ISOException` is embedded class token 18; JC305 security `CryptoException` maps to embedded token 26. The uncaught ISOException translator and CryptoException catch logic now use those exports.
- On minimal 0/15 image flashed to Pico D92, package SELECT returned `9000`; GETVERSION changed from `6F00` to `F103`. `F103` is the expected Java Card translation of `CryptoException.NO_SUCH_ALGORITHM` reason 3, so the internal uncaught exception is fixed.
- Both ARM/RISC-V wrappers passed under minimal and restored current 15/15 configurations. Current 15/15 ARM was flashed to D92 and finite monitor passed. PC/SC intermittently stopped enumerating D92 after reflashing, so the final current-profile APDU probe was not completed.
- Residual blocker: JCAlgTest 1.8.2 `AlgStorageTest` eagerly creates AES/DES engines and keys in its constructor. Those intentionally absent in the minimal profile throw `NO_SUCH_ALGORITHM`, which the applet translates to `F103`; thus no complete minimal support report exists yet.
- Next: update the JCAlgTest fixture path to tolerate unavailable algorithms while reporting them unsupported, then rerun complete support reports. Do not claim AES/SHA result comparison complete. Config is restored to all 15 capabilities enabled.

## Malformed CAP review archive — 2026-10-23

- Saved `todo/review_20261023.md` with five prioritized source-review findings: absent Class component, Method length underflow, unbounded bytecode PC, Import record overrun, and superclass cycles.
- No malformed CAP was executed; findings describe reachable code paths and still need VPCD reproduction before claims about exact runtime outcomes.
- Next for this review: add LOAD-time structural validation, then malformed CAP fixtures and bounded VM execution checks. Preserve unrelated worktree changes.

## ROMmask native metadata and generator cache invalidation — 2026-10-24

- ROMmask now emits `ROM_PACKAGE_INDEX_*`, `ROM_CLASS_INDEX_*`, and
  `ROM_FIELD_INDEX_*` macros into generated `jni.hpp`. Package order follows
  `Main.parsedFiles`; class indexes follow concrete class order in each CAP
  Class component (interfaces excluded); instance fields use their JCA field
  tokens. Verified emitted security metadata includes package `0x02`,
  `RSAKeyProvider` class `0x0A`, and its modulus field token `0x00`.
- Fixed the PC ROM-mask build cache so edits to ROMmask/capmap inputs trigger
  regeneration. `tools/rommask-source-hash` excludes build outputs and VCS
  metadata; `tools/build-pc` writes the hash manifest and `tools/build` checks
  it before reusing generated files. See `.agent/QKB.md` for the recurrence
  guidance. ROMmask source commit: `be3a712`; root integration commit:
  `8d0802d`.
- Validation: ARM and RISC-V wrappers passed for `asymm_crypto`; a physical
  cycle reflashed Pico `2BED26ED20556535`, verified flash, and passed repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` checks. The first
  picotool reset attempt lacked a reset interface; the wrapper recovered via
  CDC/BOOTSEL and completed successfully. Log:
  `artifacts/logs/cycle-choupi-20261024T165018Z.log`. UF2 SHA-256:
  `15de132afc30fff5f5cf35c639b559ead1465ccebee3b88da8fdbf748aefe67d`.
- Two CHOUPI CCID USB devices were attached and the ACL helper granted access
  to both. The firmware monitor passed on the identified board; a physical
  SmartcardIO deploy run was not performed in this step.
- Next: implement native object allocation and field access around the new
  metadata, then migrate RSA key operations from interpreted Java providers
  into direct native C++ API methods. Keep upstream CAPs unchanged.

## Native CryptoException dispatch and RSA NoPad range validation — 2026-10-24

- Added VM handling for C++ `Exceptions::CryptoException`: native code can set
  a pending reason, the interpreter resolves the applet's imported Security
  package, stores the reason on the runtime-owned CryptoException, and performs
  normal Java handler dispatch.
- The official `/deploy` Crypto run exposed RSA NoPad accepting an input equal
  to the modulus. Rejected `x >= n` before modular exponentiation and raised
  `CryptoException.ILLEGAL_USE` (5).
- Applied the pending-reason helper to all current native SHA, random, digest,
  cipher, MAC, and RSA error paths. Three invalid one-shot signature cases now
  return the expected `ILLEGAL_USE` reason 5 rather than reason 1.
- Validation on Pico `2BED26ED20556535`: ARM and RISC-V wrappers passed; cycle
  flashed and verified the ARM image, then the 18-second monitor passed. The
  first post-flash reset-interface lookup failed, but CDC/BOOTSEL recovery
  succeeded. Physical Crypto before the fix: 466 calls, 170 OK, 286
  `UNSUPPORTED_PER_SPEC`, 9 `EXC_CARDRUNTIME`, one `WRONG_RESULT`. After the
  range fix: 466 calls, 171 OK, 286 `UNSUPPORTED_PER_SPEC`, 9
  `EXC_CARDRUNTIME`, no `WRONG_RESULT`. After correcting native reason codes:
  174 OK, 286 `UNSUPPORTED_PER_SPEC`, 6 `EXC_CARDRUNTIME`, and no
  `WRONG_RESULT`; cleanup passed.
- Logs: `artifacts/logs/cycle-choupi-20261024T170718Z.log`;
  `artifacts/results/deploy/native-crypto-exceptions-2BED-20261024/pico1.csv`;
  `artifacts/results/deploy/native-crypto-exceptions-fixed-2BED-20261024/pico1.csv`;
  final reason-code CSV: `artifacts/results/deploy/native-crypto-reason-codes-2BED-20261024/pico1.csv`.
- Commit: `90d765b`.
- Next: migrate RSA key getter/setter and cipher methods out of interpreted
  provider bodies into direct native C++ methods. Preserve public API signatures
  and use unchanged `/deploy` CAPs for physical tests.


## Multi-board failover rule — 2026-10-24

- Clarified `SKILL.md` and `.agent/SESSION_CONTINUITY.md` to treat available,
  identified Pico 2 boards as a failover pool. Record and bypass a board that
  stops responding while continuing on another usable board; keep remaining
  workers active in parallel runs. Reconfirm board identity before flashing
  and preserve per-board evidence. Return to an unavailable board only after
  access is restored.
- This is a documentation-only rule update. No device access or firmware
  validation was performed for this step.
## Native RSA key component operations — 2026-10-24

- Migrated RSA key component getters/setters, initialization state, clearing,
  and type/size reads to native C++ methods in
  `choupi/src/native_rsa_key.cpp`. `RSAKeyProvider` retains Java Card storage
  construction only; its cryptographic key operations are native. The
  generated `KeyBuilderProvider` still selects the provider, and RSA cipher
  operations remain in the interpreted provider pending their native migration.
- VPCD initially returned `6F00` because native code compared generated ROM
  class token `4` directly with runtime class index `42`. Resolve the class via
  `Export_Handler::getExportedClassOffset()` and apply `HTONS`, matching the
  normal imported-class path. Direct `Heap::addInstance()` with the local token
  also failed; provider objects now use the normal JCVM `new` allocation path.
- Validation: support generator 8/8; ARM and RISC-V `asymm_crypto` builds pass;
  PC ROM-mask build passes; VPCD reports 20/20 checks, including RSA key
  setter/getter cases and cipher round trips. Log:
  `artifacts/logs/asymm-crypto-digest-vpcd-20261024.log`. No physical test was
  run for this step.
- Source commit: `85930f4`.
- Build-wrapper details: `tools/build` and `tools/build-pc` now regenerate ROM
  metadata and invalidate the source cache when Java support sources or the
  support generator changes. `tools/extract-rom-metadata.py` derives the
  generated header from `jni.hpp`.
- Next: migrate RSA cipher key validation/buffering dispatch from interpreted
  Java provider methods to native C++, preserving API signatures and upstream
  CAPs. Then run unchanged `/deploy` CAP coverage on available physical boards;
  fail over to another identified board if one becomes unavailable. Continue
  with RSA signatures, EC, key agreement, and key generation from `TODO.md`.

## Dual-profile Windows test package — 2026-10-24

- Fixed CMake source selection: both the root firmware and PC CHOUPI builds
  exclude `native_rsa_key.cpp` from the source glob and add it only if an RSA
  key capability is selected. The previous `symm_crypto` build failed because
  that file referenced RSA metadata omitted from the symmetric ROM mask.
- RSA cipher state/validation/buffering/padding/exponentiation methods now use
  native C++ dispatch. `RSACipherProvider` has no Java cryptographic method
  bodies. Support-generator tests pass 8/8; asymmetric VPCD passes 20/20.
- Implementation commit: `01fb41ac526f8537558753a834ad9d17faea4684`.
- ARM builds pass for `symm_crypto` and `asymm_crypto`. RISC-V wrapper builds
  pass for both profiles and build the VM core only. No physical flash or
  Windows PC/SC run was performed for this package.
- External archive:
  `build/external-test/pico2-jcvm-windows-test.zip`, SHA-256
  `faac7e7a83389970d6cbe75942ee9e16c8f7574249c9692b474b44d4198bc5f3`.
  It contains both ARM UF2s, unchanged upstream JCAlgTest 1.8.2 CAP, GPPro,
  official client, expected profile CSVs, comparator, and instructions.
  Windows execution results remain pending; continue native RSA signatures,
  EC/ECDSA, key agreement and key generation per `TODO.md`.

## Symmetric-only Windows test bundle — 2026-10-24

- At user request, prepared a package containing only the `symm_crypto` ARM
  UF2 (no asymmetric firmware), unchanged upstream JCAlgTest 1.8.2 CAP, GPPro,
  official client, expected support CSV, comparator, and Windows setup guide.
- Archive: `build/external-test/symm-crypto-windows-test.zip`, SHA-256
  `3d25700c4cc56b5417af2e06b3aa84219a504eb31c5e2305eba1e0e24e097acc`.
  UF2 SHA-256 is `e302de5a9e3e107ae50790293972e64b03c718194e43b1a16850e6e9177e69e3`.
- Archive integrity and member checksums pass. It contains only the symmetric
  image. No Windows host or physical card test was run. Asymmetric work is
  paused; its uncommitted in-progress source changes are preserved in the
  worktree and must not be treated as validated or packaged.
- Documentation/package commit: `31220622add7c273ccd27beda7afbdb72d1dc577`.

## Current card state and empty-image install validation — 2026-10-24

- User requested a CHOUPI image with no preinstalled JCAlgTest, then an install
  using GPPro and the unchanged upstream `AlgTest_v1.8.2_jc305.cap`.
- Fixed overlay cleanup. Older code had deleted parent tags before nested
  children, leaving inaccessible child records that eventually filled the
  65,536-byte persistent overlay. `fs_erase_prefix` now reconstructs stored
  child paths and tombstones their original encoded slots; it deletes deepest
  tags first and reclaims larger payloads first. Regression tests cover stale
  child tags and a full overlay.
- Fixed BOOTSEL flashing selection. Runtime PCSC/CDC is serial-addressable, but
  RP2350 BOOTSEL `2e8a:000f` loses that serial. `tools/flash` pins the stable
  sysfs USB port and switches to bus/address picotool selection after ROM
  re-enumeration. A direct BOOTSEL target is accepted only if unique.
- Physical board: `D92E743D5F640C0F`. The final ARM current-profile firmware is
  `build/arm/pico2_choupi.uf2`, size 1,665,024 bytes, SHA-256
  `43af89578ec5612643b75a8460c4743ef520bf6938d4659dccb443ef5ed166a8`.
  This is a firmware image without a baked-in JCAlgTest CAP.
- Card state now has the upstream JCAlgTest package installed. GPPro first
  deleted the stale package; `--list` showed only ISD. GPPro
  `--install AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` then completed both
  LOAD and INSTALL with `9000`. The CAP SHA-256 remains
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  The final GPPro list reports applet `4A43416C675465737431` as `SELECTABLE`,
  including after the final firmware flash.
- Validation: filesystem overlay test passes (`TEST:SUMMARY fail=0`); PC,
  ARM, and RISC-V builds pass. Final physical monitor passes
  `TEST:SUMMARY pass=1 fail=0`. PC/SC sees the card after the approved helper.
  GPPro transcript: `artifacts/logs/gppro-empty-install-physical-20261024.log`;
  final monitor: `artifacts/logs/monitor-empty-firmware-physical-20261024.log`.
- Reader naming: USB strings now identify `Pico2 JCVM CCID` and interface
  `JCVM CCID`. This VM's PC/SC stack still prefixes the reader with its model
  alias: `Pico 2 CHOUPI CCID [JCVM CCID] (D92E743D5F640C0F) 00 00`. Use this
  exact name for Linux GPPro commands. Windows reader naming was not tested.
- Commits: storage fix `f771800`; reader/flash workflow `986b277`.
- Preserve other uncommitted changes, especially asymmetric RSA signature
  work, support configuration, docs, and captured ALGTEST output. Do not stage
  them while committing unrelated work.
- Next: run the requested standard JCAlgTest client if continuing physical
  functional testing. The applet has not been exercised with a full client run
  as part of this empty-image/install validation.

## Symmetric-profile physical run and empty-card handoff — 2026-10-24

- The ARM `symm_crypto` UF2 is built at
  `build/arm/pico2_choupi.uf2` (1,675,264 bytes,
  SHA-256 `ff043cac0a8a5fc4770aa8783232e7da61a37fd90d01573f990137bfbbbbdbf4`).
  ARM and RISC-V wrapper builds passed. The standard upstream JCAlgTest CAP
  remains unchanged.
- On physical D92 `D92E743D5F640C0F`, official JCAlgTest 1.8.2
  `ALG_SUPPORT_EXTENDED` completed 4,525 rows: 73 `yes`, 4,452 `no`, 0 other.
  Output is in `artifacts/results/jcalgtest-symm-crypto-D92-20261024/`.
  The comparator now canonicalizes the legacy `javacard.crypto.Signature`
  section and alternate factory-method rows to selected capability identities.
  The archived report passes with 46 expected/actual identities, zero
  unexpected and zero missing. Unsupported algorithms remain expected; no
  algorithm implementation was added here.
- After testing, GPPro deleted applet AID `4A43416C675465737431` and package
  AID `4A43416C6754657374`. Final `gp --list` showed only ISD
  `A000000151000000`; the physical test card is left empty. Firmware reflashing
  alone preserves the persistent CAP store.
- `tools/flash` and `tools/cycle` now accept `--chipid` for stable selection
  among multiple boards already in BOOTSEL. The successful D92 cycle log is
  `artifacts/logs/cycle-choupi-20261024T213745Z.log`. The 2BED board was not
  given a final pass attribution because it disappeared from VM USB after
  flashing; use another visible board and confirm identity rather than
  attributing D92 results to it.
- Next: continue selected-profile acceptance on an identified board. The
  comparator mapping is covered by the support-generator test suite.
  `asymm_crypto` work remains paused at user request.

## Multi-board `symm_crypto` cycle follow-up — 2026-10-25

- `tools/flash` now recognizes a selected chip ID while the board is in either
  BOOTSEL (ROM chip-ID query) or runtime mode (USB serial equal to that chip
  ID). It uses the discovered runtime serial for the targeted CDC 1200-baud
  reset. A cycle started from runtime D92 with `--chipid` now succeeds without
  the prior 120-second USB wait.
- Target `8E464F4E2BBC0A19`: ARM and RISC-V builds passed, picotool verified
  the full UF2 write, but the board's CDC serial did not enumerate after
  reboot. Do not claim runtime validation for 8E. Log:
  `artifacts/logs/cycle-choupi-20261024T220417Z.log`.
- Failover to D92 `D92E743D5F640C0F` succeeded. The `--serial` cycle log is
  `artifacts/logs/cycle-choupi-20261024T220800Z.log`; after the flasher update,
  the `--chipid` runtime-start cycle also passed at
  `artifacts/logs/cycle-choupi-20261024T221145Z.log`. Both builds passed and
  the monitor reported repeated `TEST:SUMMARY pass=1 fail=0`. One retained
  `WATCHDOG_RECOVERY cause=other` trace printed once; this is known diagnostic
  state and did not recur or fail runtime checks.
- GPPro `--list` after the last D92 flash still contains only ISD
  `A000000151000000`. Leave D92 empty. Next continue physical `/deploy`
  coverage on D92 or another board that reappears as a PC/SC reader; keep 8E
  marked unavailable until its runtime USB serial returns.

## `symm_crypto` physical `/deploy` coverage — 2026-10-25

- D92 `D92E743D5F640C0F` completed Core, Security, and Crypto API 3.0.5 areas
  with the unchanged CAPs from `/AlgTest_JavaCard/deploy`. Each area cleaned up
  its CAPs. Final GPPro list contained only the ISD, so the physical card is
  empty again.
- Security: 901 calls, 97 OK / 804 `UNSUPPORTED_PER_SPEC`. Crypto: 466 calls,
  161 OK / 305 `UNSUPPORTED_PER_SPEC`; excluded only the runner-documented
  reader-risky RSA-2048 key-generation row. No wrong-result or runtime-error
  rows in Security/Crypto.
- Core: 291 calls, 253 OK, 15 per-spec unsupported, 17 `EXC_RUNTIME`, two
  `WRONG_RESULT`, and four `APPLET_NOT_INSTALLED`. All non-OK Core rows are
  OwnerPINx variants except the four ExtendedLength scenario rows (the Ext
  scenario CAP INSTALL was refused with `6A88` and rolled back). JCSystem has
  37 OK and one spec-permitted `requestObjectDeletion()` `ILLEGAL_USE`, with
  no JCSystem runtime-error/wrong-result rows.
- CSVs: `artifacts/results/deploy/symm_crypto-D92-20261025/`. The runner
  changes cwd to `AlgTest_JavaCard/deploy`, so keep absolute paths for output
  artifacts.
- Next: prioritize the OwnerPINx Core rows or continue remaining `/deploy`
  API areas. Leave the upstream JCAlgTest CAP unchanged and keep asymmetric
  implementation paused as directed.

## Remaining `/deploy` 3.0.5 areas on `symm_crypto` — 2026-10-25

- `JavacardxSec` passed all 84 calls. `Service` uploaded and ran 124 calls:
  78 OK, 35 `EXC_RUNTIME`, 9 `SW_ERROR`, and 2 `WRONG_RESULT`; investigate only
  if optional service/RMI APIs are in the next scope.
- `JavacardxExt`, `JavacardxInt`, and `Biometry` uploaded but INSTALL returned
  `6A88`; each was rolled back. `Javacardx` was skipped per the deploy README
  for a 3.0.5 card without framework packages; `JavacardxSec` was run instead.
- Final GPPro list showed only ISD `A000000151000000`; all successful area CAPs
  were removed. Combined result CSVs remain under
  `artifacts/results/deploy/symm_crypto-D92-20261025/`.
- Current focus remains the JCVM core / JCSystem contract. In the Core 3.0.5
  report, JCSystem was 37 OK plus the spec-permitted `requestObjectDeletion()`
  `ILLEGAL_USE`; the broader Core failures are limited to OwnerPINx and the
  unavailable Ext scenario applet. Do not spend time implementing algorithms
  that the active profile does not claim to support.

## Empty-card reflash confirmation — 2026-10-25

- Rebuilt both wrappers with `symm_crypto`; ARM and RISC-V passed. Reflashed
  ARM to D92 `D92E743D5F640C0F` after discovering it was in runtime mode rather
  than BOOTSEL. The cycle passed repeated finite runtime checks and produced
  `CYCLE:PASS`; one watchdog recovery diagnostic occurred.
- Refreshed PC/SC access and ran GPPro `--list` after flash. Only ISD
  `A000000151000000` was present. The current card is empty of applet/package
  CAPs, and JCAlgTest is not preinstalled in the firmware image.
- The ARM UF2 remains 1,675,264 bytes with SHA-256
  `ff043cac0a8a5fc4770aa8783232e7da61a37fd90d01573f990137bfbbbbdbf4`.
  Cycle log: `artifacts/logs/cycle-choupi-20261024T224042Z.log`.
- Only one Pico CCID reader was visible to this VM despite the report of
  multiple BOOTSEL tokens. Continue on any additional board once it enumerates;
  do not attribute D92 results to another token.
# Latest physical task — `symm_crypto` flash, explicit install, static benchmark
# Latest continuation — verify native APDU outgoing mirror as GC root (2026-10-25)

- Extended `choupi/test/jcarray_transient_test.cpp` to preserve and check the
  native APDU outgoing buffer mirror separately from the Java APDU buffer.
  Both APDU-held arrays and a cached static reference remain accessible after
  `Heap::collectUnreachableFrom`; the unrelated array is reclaimed.
- Validation: `./tools/test-jcarray` passed with
  `TEST:PASS gc_retains_static_and_apdu_buffers` and
  `TEST:PASS gc_reclaims_unrooted_array reclaimed=6`. Existing compiler warnings
  in shared JCVM headers remain; no new test failure.
- Next: broaden coverage to transient lifecycle roots and instance child graphs;
  physical power-cycle validation remains open.

# Latest continuation — verify collector traces reference-array children (2026-10-25)

- Extended `choupi/test/jcarray_transient_test.cpp` with a static-rooted
  wildcard `Object[]` containing a byte-array reference. After collection, both
  parent and child remain accessible; the collector also reclaims six unrelated
  arrays. The test uses `JC_Array::setReferenceEntry` and therefore exercises
  normal reference storage rather than manually writing encoded handles.
- The small host test target now has scoped linker stubs for type-resolution and
  persistent-reference calls retained by that setter; the wildcard array path
  does not invoke them.
- Validation: `./tools/test-jcarray` passed. Existing JCVM shared-header
  warnings remain.
- Next: test object-instance field child traversal and transient-array roots;
  physical power-cycle validation remains open.

# Latest continuation — collect typed top-level results correctly (2026-10-25)

- Extended the jcarray collector regression to keep an array through the
  top-level reference return slot, then set a primitive return and verify a
  newly unreachable array is collected. The run also preserves the existing
  static/APDU roots and reference-array child graph.
- Validation: `./tools/test-jcarray` passed with
  `TEST:PASS gc_tracks_reference_result_type` and all earlier collector checks;
  six unrelated arrays were reclaimed in the primary collection.
- Next: continue object-instance field graph and transient-root collection
  coverage. No physical power-cycle validation was run.

# Latest continuation — traverse and reclaim instance field graphs (2026-10-25)

- Expanded the focused heap test with one-field `JC_Instance` fixtures. A
  static-rooted instance retains its array child; an unrooted instance and its
  child array are reclaimed. The test uses the real `Heap` collector and its
  `JC_Instance::getFields()` traversal, with test-only construction/accessor
  stubs for the narrow host target.
- Validation: `./tools/test-jcarray` passed with
  `TEST:PASS gc_traces_instance_field_child` and
  `TEST:PASS gc_reclaims_unreachable_instance_graph`; primary collection
  reclaimed seven arrays and at least one instance.
- Scope: this validates heap graph marking over the instance field encoding;
  it does not prove CAP-driven instance lifecycle or persistence recovery.
- Next: cover transient reference roots/lifecycle behavior, then expand the
  focused suite to the full broader test set. Physical power-cycle validation
  remains open.

# Latest continuation — validate transient reference graph lifecycle (2026-10-25)

- Changed the collector test's rooted wildcard `Object[]` to a transient array
  with `CLEAR_ON_DESELECT`. It retains its child through the first collection;
  `CLEAR_ON_RESET` leaves it intact; deselect clears the edge; the next
  collection reclaims that child.
- Validation: `./tools/test-jcarray` passed with
  `TEST:PASS gc_clears_transient_reference_graph_by_event`; all earlier root,
  object-graph, and reclamation checks passed as well.
- Next: expand the host regression set around these changes and continue the
  remaining project priorities. Hardware power-cycle validation remains open.

# Latest continuation — exercise RMI registry roots through the collector (2026-10-25)

- Added an exported one-field instance and array child to the jcarray collector
  regression. The real Context root enumerator and Heap collector preserve both
  while the remote object is exported; after `unexportRemoteObject`, the next
  collection reclaims the instance and child. This closes the prior test gap
  where registry root enumeration was checked without actual collection.
- Validation: `./tools/test-jcarray` passed with
  `TEST:PASS gc_retains_exported_remote_graph` and
  `TEST:PASS gc_reclaims_unexported_remote_graph`; other GC checks passed.
- Next: continue remaining project acceptance work. No physical device was
  used for this host collector regression.

# Latest physical diagnostic — GPPro module GET STATUS and reader loss (2026-10-25)

- A read-only, bounded GPPro `--list` on D92 again authenticated and completed
  ISD select plus GET STATUS P1=80/40/20, then timed out at `84F21002`.
  `./tools/monitor --serial D92E743D5F640C0F --seconds 8` showed the last
  XfrBlock header was the module query, but the old trace could not distinguish
  whether its response was submitted.
- Added serial diagnostics for the APDU header associated with the most recent
  CCID response, response payload length, and status word. ARM/RISC-V builds
  passed. The first `cycle arm` command targets smoke firmware; `cycle choupi`
  was then used to restore the full image to D92 and passed its finite runtime
  monitor (`TEST:SUMMARY pass=1 fail=0`).
- Post-flash PC/SC exposed only Virtual PCD readers, so GPPro did not transmit.
  The approved helper then reported two USB nodes; `lsusb` showed Pico devices
  080 and 090, but bounded `pcsc_scan -r` timed out without readers. The new
  response telemetry has not yet captured a fresh module query.
- Next: retry on an exact physical reader after PC/SC enumeration returns;
  use the response APDU snapshot to determine if the firmware submitted P1=10.
  The physical module status path remains unverified.

# Current continuation — partial native RSA Signature slice (2026-10-26)

- Added direct native `Signature` API dispatch for `ALG_RSA_SHA_PKCS1` (ID 10),
  covering factory selection, initialization, message/precomputed-hash sign and
  verify, and signature length. Removed generated RSA Signature provider output.
  The test applet invokes only Java Card APIs; host-generated RSA vectors check
  known signatures, altered-signature rejection, precomputed hashes, and the
  out-of-range signature equal to the modulus.
- Validation: `python3 tools/test-javacard-support.py` passed 9/9; PC, ARM, and
  RISC-V builds for `asymm_crypto` passed (the final PC build was necessary
  because architecture builds do not refresh the emulator binary); VPCD passed
  26/26 checks, including all 6 RSA Signature vectors. The equal-to-modulus
  vector initially failed against a stale PC executable, then passed after
  rebuilding PC. No physical device was used: the user intentionally unplugged
  the token.
- This is not a completed native-only asymmetric profile. RSA Signature still
  consumes `RSAKeyProvider` objects. RSA key construction and RSA Cipher retain
  generated Java provider paths, and the signature implementation has not been
  audited for private-operation blinding/side-channel resistance or tested on
  hardware. Do not package/claim `asymm_crypto` as compliant yet. Implementation
  commit: `c40a965`.
- Next: migrate RSA key construction and key state onto native Java Card API
  methods, then RSA Cipher factory/operations, preserving the VPCD vectors.
  Extend RSA Signature algorithms only after those foundations and hardening.
# Current continuation — native ECDSA SHA signatures (2026-10-26)

- Implemented native C++ ECDSA SHA-1/224/256/384/512 `Signature` dispatch and
  arithmetic using Mbed TLS. Added profile selection, EC FP public-key
  storage, ASN.1 DER signature encoding/decoding, curve/domain validation,
  and test-only sign/verify APDUs. Upstream CAP files remain unchanged.
- The first ECDSA VPCD run failed on APDU 19 with an uncaught
  `NullPointerException`. Root cause was a missing JC305 `ECPublicKey` method
  token translation (interface token 19); mapping it with the EC token sequence
  fixed the failure. Details are recorded in `.agent/QKB.md`.
- Validation: `python3 tools/test-javacard-support.py` passed 9/9;
  `./tools/build-pc --profile asymm_crypto` passed; elevated
  `./tools/env python3 tools/tests/test_asymm_crypto_vpcd.py` passed 78/78
  (76 custom APDUs plus ATR and SELECT); ARM full-firmware and RISC-V VM-core
  wrapper builds passed. The approved helper
  `PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader` found
  zero physical Pico readers, so no flash or hardware test was attempted.
- ARM image: `build/arm/pico2_choupi.uf2` (1,980,928 bytes), binary end
  address `0x100f1b08`; generated PC ROM mask `build/choupi-pc/flash`
  (524,300 bytes).
- Remaining asymmetric work: EC key agreement and key generation, exact full
  support-report comparison, physical `/deploy` acceptance, and private RSA
  hardening. Retest the five ECDSA algorithms on an identified Pico as soon as
  it enumerates through the approved host-access path.
