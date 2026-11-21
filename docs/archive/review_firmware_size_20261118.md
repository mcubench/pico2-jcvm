# Firmware size review — 2026-11-18

Scope: the full JCVM firmware `pico2_choupi` for the Pico 2 (RP2350, ARM
Cortex-M33), normal `max_crypto` profile (object deletion ON, debug OFF).
This review asked how much flash each part of the firmware uses. The goal was
to see whether more cryptographic implementations would fit.

Summary of the current state (updated 2026-11-18 18:10 CET):

- Two defects used more than half of the flash: the embedded 512 KiB ROM-mask
  dump and a 66 KB zero buffer in `.data`. Both are fixed (steps 1–2).
- Two follow-up reductions are also done: newlib `fprintf` (step 3) and the
  Mbed TLS configuration (step 4).
- The image shrank from 1,016,636 B to **420,396 B** (−58.6%). Free space in
  the 1 MiB firmware window grew from 31.9 KB to **628,180 B** (≈613 KiB).
- Flash no longer limits new cryptographic implementations; RAM is now the
  tighter resource (see [RAM, for context](#ram-for-context)).
- All four steps pass a 0-warning build and the boot self-test on hardware.
  The code paths they change in GP card management, ECC, HMAC and
  object-deletion telemetry have not yet been exercised with APDU traffic. An
  external Windows run (`WIN-SIZE-MAXCRYPTO-4BDED540`) has been requested; see
  [External test](#external-test).

| | Before | After steps 1–2 | Current (steps 1–4) |
|---|---|---|---|
| Source | `c0270a65` (docs tip; sources as of `a746c295`) | `7adbe5cc` | `e17d25dc` (packaged at `4bded540`) |
| Image (`pico2_choupi.bin`) | 1,016,636 B | 460,868 B | **420,396 B** |
| Firmware end | `0x100f8328` | `0x10070844` | `0x10066a2c` |
| Free space in the 1 MiB firmware window (INV-28) | 31.9 KB | 587,708 B | **628,180 B** |
| Static RAM (`.data` + `.bss` + other RAM sections) | 335,536 B | 335,532 B | 335,164 B |
| `.data` (initial image stored in flash) | 77,464 B | 10,800 B | 10,432 B |

| Step | Commit | Step-Id | Saved |
|---|---|---|---:|
| 1. Embed only the ROM filesystem | `e582a6e3` | `20261118-1500-romfs-trim` | 489,255 B |
| 2. `cap_load_session` in `.bss` | `7adbe5cc` | `20261118-1503-cap-load-session-bss` | 66,513 B |
| 3. VM `fprintf` sent to the SDK printf | `6e640e32` | `20261118-1719-fprintf-wrap` | 19,552 B |
| 4. Firmware Mbed TLS user configuration | `e17d25dc` | `20261118-1727-mbedtls-user-config` | 20,920 B |
| External test package | `9963772e` | `20261118-1747-ext-test-4bded540` | – |

The firmware window stays at 1 MiB. Using the freed space for other purposes,
such as more CAP slots, would change INV-28 and so the specification.

## Method

- Input: `build/arm/pico2_choupi.elf` and its linker map `pico2_choupi.elf.map`
  (GCC 14.2, `-Os` for all firmware sources, `-ffunction-sections`,
  `--gc-sections`, no LTO).
- Each input section in the map was attributed to its object file. Objects were
  grouped into components.
- Flash use = `.text` + `.rodata` + `.data` (its initial image is stored in
  flash) + `.ARM.extab`/`.ARM.exidx`.
- The map lists merged string sections (`.rodata.str1.*`) at their size before
  merging. These entries were scaled to the real merged size.
- Component sums match the real binary to within about 270 B (alignment).
- The analysis scripts were one-off session tools and are not in the
  repository. The measurement can be repeated with `arm-none-eabi-size -A` and
  `arm-none-eabi-nm -S --size-sort` on the ELF, plus a pass over the map file.

## State before the fix

### Flash use by component

| Component | Flash (B) | % | Main items |
|---|---:|---:|---|
| **ROM-mask blob** (Java Card API + GlobalPlatform packages as bytecode) | 524,300 | 51.6 | Only 35,045 B are real data; 489,255 B are zero padding |
| JCVM core | 156,392 | 15.4 | interpreter 43.1K, object model/heap 32.4K, persistent objects 21.8K, CAP/class linking 18.7K, `context.cpp` 15.5K, `jni.cpp` native dispatch 12.9K, `main.cpp` 11.9K |
| **GP / card management + CCID** | 91,995 | 9.0 | `cap_load_session` initial image 66.7K, `cap_load` 9.7K, CCID/APDU routing 10.0K, `cap_store` 3.9K, SCP03 1.4K |
| mbedTLS + Pico hardware SHA/RNG | 83,530 | 8.2 | ECC 42.3K (29.6K curve tables), hashes 21.7K, bignum 9.5K, AES/DES 8.6K |
| Java Card crypto glue (`native_*.cpp`) | 61,413 | 6.0 | RSA signature 10.7K, signature 9.4K, key 8.2K, cipher 7.7K, RSA cipher 6.9K, ECDSA 5.2K |
| C/C++ runtime | 49,145 | 4.8 | newlib stdio/printf 26.1K, libstdc++ exception support 9.7K, malloc/string 8.1K, libgcc 5.2K |
| Platform (flash filesystem, `choupi_pico2`, LED) | 32,208 | 3.2 | |
| USB stack (TinyUSB + stdio_usb) | 9,157 | 0.9 | |
| Pico SDK runtime | 8,173 | 0.8 | |

C++ exception-unwinding tables (26.2 KB) are counted in the objects they
belong to.

Packages in the ROM-mask payload (package index taken from the record tag):
`javacard.security` 7.5K, `org.globalplatform` 6.8K, `javacard.framework`
6.6K, `javacard.framework.service` 3.1K, `fr.gouv.ssi.nativeimpl` 1.9K,
`javacardx.framework.tlv` 1.5K, the rest below 1.2K each.

### Defect 1 — the whole ROM-mask dump was embedded

`src/choupi_rommask.S.in` embedded `build/choupi-pc/flash` with `.incbin`.
That file is the host build's fixed 512 KiB flash dump (524,300 B). The
firmware reads it only through `find_rom_file()` in
`src/pico_filesystem.cpp`, which:

- starts at the fixed offset `kRomFsOffset = 0x20000`;
- walks tag/length/value records (header, tag, data, CRC-8);
- stops at the first zero header byte.

The dump's layout:

| Offset range | Content |
|---|---|
| `0x00000`–`0x1FFFF` (128 KiB) | all zero, never read |
| `0x20000`–`0x288E4` (35,045 B) | 21 ROM filesystem records + zero terminator |
| `0x288E5`–end (358,183 B) | all zero, never read |

### Defect 2 — a 66 KB zero buffer was stored in flash

The global `CapLoadSession cap_load_session` in `src/ccid_device.cpp` is
66,660 B. Most of it is the CAP load buffer. Three of its bytes have non-zero
default values in `src/cap_load.hpp`: `applet_class_index_ = 0xffff` and
`package_id_ = 0xff`. All default values are constants, so the compiler
constant-initialized the whole object into `.data`. Its initial image is
66,657 zero bytes and 3 non-zero bytes. That image was stored in flash and
copied to RAM at boot. `.data` was 77,464 B, of which 66,660 B was this one
object.

### Headroom before

`tools/check-firmware-flash-boundary` allowed about 31.9 KB before the CAP
slots at flash offset 1 MiB. Typical additions are estimated below; these
figures are not measured here. A small primitive (a few KB) would fit.
Ed25519/X25519 (about 10–20 KB) would be tight. Larger sets such as ML-KEM
or ML-DSA (about 20–60 KB) would not fit.

## The fix

### Step 1 — embed only the ROM filesystem (`e582a6e3`, Step-Id `20261118-1500-romfs-trim`)

- New `tools/extract-rom-filesystem` (Python). At build time it walks the
  record chain exactly as `find_rom_file()` does. It writes only the records
  plus one zero terminator to `build/arm/choupi_romfs.bin`.
- It fails the build if any non-zero byte lies outside the chain (before
  `0x20000` or after the terminator), or if the chain is malformed. A future
  layout change in the ROM-mask generator therefore cannot silently drop data.
- `CMakeLists.txt`: a custom command and target `choupi_romfs` produce the
  file. `choupi_rommask.S` depends on it and embeds it.
- `src/pico_filesystem.cpp`: `kRomFsOffset` removed; the walk starts at
  offset 0.
- Unchanged: the host ROM-mask generation, its `rommask_sha256`
  (`ab55f462…`), the VPCD/PC path (which reads the full dump through
  `__real_fs_read_inplace`), and the record bytes themselves (byte-identical).

### Step 2 — `cap_load_session` in `.bss` (`7adbe5cc`, Step-Id `20261118-1503-cap-load-session-bss`)

- `src/cap_load.hpp` declares `CapLoadSession() noexcept;`.
  `src/cap_load.cpp` defines it out of line as `= default`.
- The constructor is not visible to `ccid_device.cpp`, so it is not
  constexpr. The global is therefore zero-filled in `.bss`, and a 12-byte
  startup initializer sets the same default values before `main`.
- Member values, behavior and RAM use are unchanged.
- Some code takes `package_id_` by non-const reference. That ruled out
  changing the sentinel encoding or wrapping the fields in a helper type.
- If link-time optimization is ever enabled, check that the object stays in
  `.bss`. The compiler could then see the constructor and constant-initialize
  the object again. That would only cost size, not correctness.

## State after the fix (steps 1–2)

The "Current" column is the state after steps 3 and 4; see
[Current state](#current-state-after-steps-14).

| Component | Before (B) | After steps 1–2 (B) | Current (B) | Change, total |
|---|---:|---:|---:|---:|
| ROM filesystem (Java Card API + GP packages) | 524,300 | 35,045 | 35,045 | −489,255 |
| GP / card management + CCID | 91,995 | 25,495 | 25,495 | −66,500 |
| JCVM core | 156,392 | 156,392 | 156,363 | −29 |
| mbedTLS + Pico hardware SHA/RNG | 83,530 | 83,530 | 62,612 | −20,918 |
| Java Card crypto glue | 61,413 | 61,413 | 61,413 | 0 |
| C/C++ runtime | 49,145 | 49,145 | 29,681 | −19,464 |
| Platform | 32,208 | 32,200 | 32,200 | −8 |
| USB stack | 9,157 | 9,157 | 9,157 | 0 |
| Pico SDK runtime (incl. fill, linker stubs, wrapper) | 8,173 | 8,173 | 8,701 | +528 |
| **Image** | **1,016,636** | **460,868** | **420,396** | **−596,240** |

Component sums are taken from the map (string sections scaled); they differ
from the binary size by up to about 270 B of alignment. The small JCVM-core and
SDK-runtime differences after step 2 are attribution noise from the scaling of
merged strings, not code changes.

Sections after: `.text` 336,496; `.rodata` 87,280; `.ARM.extab` 17,248;
`.ARM.exidx` 8,992; `.data` 10,800 (was 77,464); `.bss` 324,444 (was
257,784). Static RAM is unchanged: the 66,660 B moved from `.data` to
`.bss`.

The largest flash users are now the Java Card crypto glue (13.3%), the JCVM
interpreter (9.4%), mbedTLS ECC (9.2%) and the ROM filesystem (7.6%).

## Validation

| Check | Result |
|---|---|
| `./tools/build arm --profile max_crypto` (both steps) | exit 0, **0 warnings** in a rebuild with the ROM-mask generation current. The step-1 build first regenerated the host ROM mask: its 134 Java/Rust warnings all come from the host stage, whose output hash is unchanged. Rebuilt UF2 byte-identical. |
| `ROMFS:PASS` | `records=21 bytes=35045 dropped_padding=489255` |
| `FLASH_LAYOUT` | `PASS remaining=521196` (step 1) → `PASS remaining=587708` (step 2) |
| `./tools/test-rp2350-fs-overlay` | `TEST:SUMMARY fail=0` |
| `./tools/build-pc --profile max_crypto --rp2350-overlay` | exit 0; host build compiles the changed `cap_load.cpp` |
| `nm` | `cap_load_session` is `b` (`.bss`) |
| `FLASH_LAYOUT` steps 3–4 | `PASS remaining=607260` (step 3) → `PASS remaining=628180` (step 4) |
| `nm` steps 3–4 | no `fprintf`, `_vfprintf_r`, `_dtoa_r`, `__sfvwrite_r`; no Brainpool/k1/25519/448/SHA-3 symbols; `mbedtls_md5_finish`, `mbedtls_ripemd160_finish`, `mbedtls_md_hmac` present |
| Mbed TLS config consistency | all 176 TUs with `MBEDTLS_CONFIG_FILE` also carry `MBEDTLS_USER_CONFIG_FILE` |
| `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --serial F42DD9D07D6D2B07` (after each of steps 1–4) | `CYCLE:PASS`, 18× `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, `CHOUPI:STATUS sw=9000`, no `TEST:FAIL`/`FAULT`. The boot self-test runs the ROM starter package, so ROM filesystem lookup works on hardware. |

Token: F42D (`F42DD9D07D6D2B07`), development cycle, storage preserved.
Token 07E7 was not touched; it was running an option-1 campaign.

Logs: `artifacts/results/flash-size-opt-20261118/step1-romfs-trim/`,
`step2-cap-load-session-bss/`, `step3-fprintf-wrap/` and
`step4-mbedtls-config/` in the same directory.

### Open items

These apply to all four steps.

- **No APDU traffic on the reduced images yet.** Not exercised:
  - GP LOAD/INSTALL/DELETE (`cap_load_session`, ROM package lookup during linking);
  - ECDSA, ECDH and EC key generation (step 4);
  - HMAC, including MD5 and RIPEMD-160 (step 4);
  - the `OBJECT_DELETION:*` telemetry, which now goes through the wrapped `fprintf` (step 3).

  F42D is on USB but missing from PC/SC. pcscd was not refreshed because the
  07E7 option-1 campaign shared it. Once F42D is in PC/SC and no campaign is
  live:
  - run GPPro list/install of the unchanged JCAlgTest 1.8.4/jc305 CAP;
  - run a JCAlgTest option 2 run with the 1.8.5 client.

  The external test above covers the same paths.
- F42D currently holds the empty-card external-test UF2 `a0b46b33…`, so a
  local run can start from the same image.
- The development cycles are not acceptance evidence. INV-27 requires an
  empty-card normal image and unchanged external clients.

## Follow-up reductions

Two further items from the opportunity list were implemented on the same day.
Both were validated with a 0-warning build and `CYCLE:PASS` (18× `TEST:PASS`)
on F42D (`F42DD9D07D6D2B07`).

| | After the fix | After step 3 | After step 4 |
|---|---:|---:|---:|
| Commit | `7adbe5cc` | `6e640e32` | `e17d25dc` |
| Image (`pico2_choupi.bin`) | 460,868 B | 441,316 B | **420,396 B** |
| Free space in the firmware window | 587,708 B | 607,260 B | **628,180 B** |
| UF2 SHA-256 | `5c4d068d…` | `52c36035…6fb6fca` | `1f57b78b…fcc7b01b` |

### Step 3 — VM `fprintf` sent to the SDK printf (`6e640e32`, Step-Id `20261118-1719-fprintf-wrap`)

Before: the shared VM sources write diagnostics and object-deletion telemetry
with `std::fprintf(stderr, …)` (interpreter, JNI, `bc_invoke`, `bc_object`,
`jc_class`, `jc_method`, `main.cpp`). That linked newlib's float-capable
`_vfprintf_r` (8,876 B), `_dtoa_r` (4,092 B), the mprec helpers,
`__sfvwrite_r` and locale code, next to the Pico SDK printf already in the
image. The earlier estimate said "two calls in `interpretor.cpp`"; there are
more, so replacing call sites one by one was the wrong approach.

Fix: `src/pico_stdio_stream.c` defines `__wrap_fprintf` and `__wrap_vfprintf`.
They pass stdout/stderr output to the SDK `vprintf` and fail other streams with
`EBADF`; the device has no other `FILE` stream. `CMakeLists.txt` links
`pico2_choupi` with `--wrap=fprintf --wrap=vfprintf`. The VM sources and the
host build are unchanged, and both streams already ended in the same USB serial
sink.

After: −19,552 B. The only visible format change is `%p`: 8 uppercase hex
digits without `0x` instead of `0x…` in lowercase. It occurs only in pointer
fields of `CHOUPI:VM_PROGRESS`, `CLASS_CAST_TYPES`/`INTERFACE_CHECK` and
`OBJECT_DELETION:POST_APDU`, which no tool parses.

### Step 4 — firmware Mbed TLS user configuration (`e17d25dc`, Step-Id `20261118-1727-mbedtls-user-config`)

Before: the firmware used the SDK's default `mbedtls_config.h` with all 13
named curves and SHA-3, although `native_ec_group.hpp` loads only
secp192/224/256/384/521r1. Other EC sizes use explicit domain parameters.

Fix: `src/choupi_mbedtls_user_config.h` is set as `MBEDTLS_USER_CONFIG_FILE` on
the `pico_mbedtls_headers` interface target. All 176 firmware translation units
that use the Mbed TLS config get it. It removes secp192k1/224k1/256k1,
brainpoolP256r1/384r1/512r1, Curve25519/448 and SHA-3. MD5 and RIPEMD-160 are
removed only when the profile does not select HMAC_MD5/HMAC_RIPEMD160.
`MBEDTLS_ECP_MAX_BITS` stays 521, and no native code uses `MBEDTLS_MD_MAX_*`.
The host config `choupi/src/mbedtls_config_pc.h` is unchanged. Support
reporting comes from the profile (INV-08).

Correction to the first version of this review: `max_crypto` **does** enable
HMAC (SHA-1/256/384/512, MD5, RIPEMD-160). MD5 and RIPEMD-160 therefore stay in
`max_crypto`, and the saving is 20,920 B rather than about 25 KB.

### Not tested for steps 3 and 4

The boot self-test does not run ECDSA, ECDH, EC key generation or HMAC. The
object-deletion telemetry lines that now use the wrapped `fprintf` print only
during APDU traffic. See [Open items](#open-items) and
[External test](#external-test).

## Current state (after steps 1–4)

Measured on `build/arm/pico2_choupi.elf` from `4bded540` (firmware sources
`e17d25dc`), using the same method as above.

| Component | Flash (B) | % | Main items |
|---|---:|---:|---|
| JCVM core | 156,363 | 37.2 | interpreter 43.1K, object model/heap 32.4K, persistent objects 21.8K, CAP/class linking 18.7K, `context.cpp` 15.5K, `jni.cpp` 12.9K, `main.cpp` 11.9K |
| mbedTLS + Pico hardware SHA/RNG | 62,612 | 14.9 | ECC 23.9K (14.2K tables, was 29.6K), hashes/md 19.6K, bignum 9.1K, AES/DES 8.6K, HW SHA/RNG 1.5K |
| Java Card crypto glue (`native_*.cpp`) | 61,413 | 14.6 | unchanged; RSA signature, signature, key, cipher, RSA cipher, ECDSA |
| ROM filesystem | 35,045 | 8.3 | 21 records |
| Platform | 32,200 | 7.7 | `choupi_pico2` 16.1K, flash filesystem/overlay 15.4K |
| C/C++ runtime | 29,681 | 7.1 | libstdc++ EH/RTTI 9.6K, printf/stdio 8.5K (was 26.1K), malloc/string 6.4K, libgcc 5.2K |
| GP / card management + CCID | 25,495 | 6.1 | CCID/APDU routing 10.1K, `cap_load` 9.8K, `cap_store` 3.9K, SCP03 1.4K |
| USB stack | 9,157 | 2.2 | |
| Pico SDK runtime (incl. fill, stubs, wrapper) | 8,701 | 2.1 | |

Sections: `.text` 312,648; `.rodata` 71,024; `.ARM.extab` 17,248;
`.ARM.exidx` 8,992; `.data` 10,432; `.bss` 324,444. C++ exception tables
(26.2 KB) are still counted in the objects they belong to.

Largest single flash users now: the Java Card crypto glue (14.6%), the
bytecode interpreter (10.3%), the ROM filesystem (8.3%), the object model and
heap (7.7%) and Mbed TLS ECC (5.7%).

### What fits now

With 628,180 B free, flash is no longer a constraint for new algorithms.
These are rough estimates, not measured here: Ed25519/X25519 (about
10–20 KB), ML-KEM or ML-DSA (about 20–60 KB each), or several of them
together, would all fit. Note that step 4 removed the Mbed TLS Curve25519/448
tables. Adding X25519 or Ed25519 later means re-enabling
`MBEDTLS_ECP_DP_CURVE25519_ENABLED` in `src/choupi_mbedtls_user_config.h`
(a few KB) or adding a separate implementation. The binding limit for
large-state algorithms is RAM.

### Specification invariants checked

- INV-28: the firmware window is still 1 MiB; only the free space inside it grew.
- INV-08: support reporting comes from the profile (`config/javacard-support.json`),
  which is unchanged. Every algorithm the profile reports still has its
  Mbed TLS backend; only unreachable curves and SHA-3 were removed.
- INV-24: no debug strings were added. The existing telemetry strings are still
  open (see the opportunity table).
- INV-27: the development cycles are not acceptance evidence. The external test
  package uses an empty-card normal image and the unchanged clients.

## External test

Request `WIN-SIZE-MAXCRYPTO-4BDED540` in `.agent/EXT_REQUESTS.md` (created
2026-11-18 17:52 CET) asks for JCAlgTest option 2 on Windows. It replaces
`WIN-SIZE-MAXCRYPTO-58B01075`, which shipped the steps 1–2 image and was not
returned.

| Item | Value |
|---|---|
| Image | normal `max_crypto`, built from `4bded540` (firmware sources `e17d25dc`), CPLC `0x4BDED5401CDE0EC4` |
| Empty-card UF2 | `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-4bded540.uf2`, SHA-256 `a0b46b33b48f13ffe9cb48debeddce7bad46fb1ba8911e0fc0ff28618b3fe6e9`, 2,938,880 B |
| ZIP | `build/external-test/pico2-jcvm-maxcrypto-deletion-4bded540.zip`, SHA-256 `be4b32f66c30bf78d3a282c8ce48116e487e9bf7c35c54456a0615bcbc7afa73`, 18,218,734 B |
| Clients | unchanged JCAlgTest 1.8.4/jc305 CAP, 1.8.5 client (SHA-256 `b6a753be…eacec3`), GPPro |
| Local check | exact UF2 flashed to F42D (storage wiped): 18× `TEST:PASS`, `CHOUPI:STATUS sw=9000`, no FAULT |
| Questions | Does the empty-card CAP install work? Does option 2 finish without reader loss or PANIC? Are the EC and HMAC results the same as on earlier images? Do `OBJECT_DELETION:TIMING` lines appear, below 15 s? |
| Status | **pending**, no results yet |

Logs: `artifacts/results/windows-package-smoke-4bded540-20261118T1648Z/`.

## Remaining reduction opportunities (not implemented)

Implemented items (newlib `fprintf`, Mbed TLS curves and SHA-3) are removed
from this list; see [Follow-up reductions](#follow-up-reductions).

| Opportunity | Estimated saving | Notes |
|---|---:|---|
| mbedTLS precomputed point tables off for the curves in use | about 13 KB | Slower ECC. |
| Diagnostic strings and their formatting code | about 7–13 KB | **INV-24 check needed:** the normal image contains strings such as `CHOUPI:INTERFACE_CHECK …`, `CHOUPI:CHECKCAST …` and `VPCD:APPLET_PROCESS …`. INV-24 says non-diagnostic images contain no debug strings. Decide which strings are required telemetry (e.g. `PRE_CLEANUP` for the headroom checker). |
| Link-time optimization | unknown, roughly 5–10% of code | Re-check the `.bss` placement of `cap_load_session` afterwards. |
| Native-method signatures pass `std::shared_ptr` by value | moderate | Refactor. |
| C++ exceptions (tables plus runtime about 40 KB) | about 40 KB | The VM implements Java exceptions with them, so this is a major redesign. |

### RAM, for context

Static RAM use is 335.2 KB of 512 KB (335,164 B; step 3 freed 368 B of newlib `.data`). The remaining ~176 KB must hold the C
heap, the stack, mbedTLS big-number temporaries and the Java Card heap. This
is the tighter resource for new algorithms with large working state, such as
ML-DSA. Largest static users:

| Item | RAM |
|---|---:|
| persistent overlay data (`overlay_bytes`) | 96 KiB |
| overlay index (`overlay`) | 64 KiB |
| `cap_load_session` | 66.7 KB |
| erase buckets | 20 KB |
| static CAP-validation buffers | about 44 KB |

`cap_load_session` is the obvious candidate if RAM must be reclaimed, for
example by sharing it with other load-time scratch buffers.
