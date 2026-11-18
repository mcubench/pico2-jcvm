# Firmware size review — 2026-11-18

Scope: the full JCVM firmware `pico2_choupi` for the Pico 2 (RP2350, ARM
Cortex-M33), normal `max_crypto` profile (object deletion ON, debug OFF).
This review asked how much flash each part of the firmware uses. The goal was
to see whether more cryptographic implementations would fit. Two defects used
more than half of the flash; both were fixed. Two follow-up reductions (newlib
`fprintf`, Mbed TLS configuration) are described in
[Follow-up reductions](#follow-up-reductions); after them the image is
420,396 B with 628,180 B of headroom.

| | Before | After |
|---|---|---|
| Source | `c0270a65` (docs tip; sources as of `a746c295`) | `7adbe5cc` |
| Commits | – | `e582a6e3` (ROM filesystem trim), `7adbe5cc` (`cap_load_session` in `.bss`) |
| Image (`pico2_choupi.bin`) | 1,016,636 B | **460,868 B** |
| Firmware end | `0x100f8328` | `0x10070844` |
| Free space in the 1 MiB firmware window (INV-28) | 31.9 KB | **587,708 B** |
| UF2 SHA-256 | `ce131357…30fb85f` (never flashed) | `5c4d068d…1d713c2a38` |
| Static RAM (`.data` + `.bss`) | 335,536 B | 335,532 B (unchanged) |

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

## State after the fix

| Component | Before (B) | After (B) | Change |
|---|---:|---:|---:|
| ROM filesystem (Java Card API + GP packages) | 524,300 | 35,045 | −489,255 |
| GP / card management + CCID | 91,995 | 25,495 | −66,500 |
| JCVM core | 156,392 | 156,392 | 0 |
| mbedTLS + Pico hardware SHA/RNG | 83,530 | 83,530 | 0 |
| Java Card crypto glue | 61,413 | 61,413 | 0 |
| C/C++ runtime | 49,145 | 49,145 | 0 |
| Platform | 32,208 | 32,200 | −8 |
| USB stack | 9,157 | 9,157 | 0 |
| Pico SDK runtime | 8,173 | 8,173 | 0 |
| **Image** | **1,016,636** | **460,868** | **−555,768** |

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
| `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --serial F42DD9D07D6D2B07` (after each step) | `CYCLE:PASS`, 18× `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, `CHOUPI:STATUS sw=9000`, no `TEST:FAIL`/`FAULT`. The boot self-test runs the ROM starter package, so ROM filesystem lookup works on hardware. |

Token: F42D (`F42DD9D07D6D2B07`), development cycle, storage preserved.
Token 07E7 was not touched; it was running an option-1 campaign.

Logs: `artifacts/results/flash-size-opt-20261118/step1-romfs-trim/` and
`artifacts/results/flash-size-opt-20261118/step2-cap-load-session-bss/`.

### Open items

- **Physical GP LOAD/INSTALL/DELETE check not yet done.** These are the only
  code paths that use `cap_load_session`. F42D is visible on USB but missing
  from PC/SC. pcscd was not refreshed because a live campaign shared it. Run
  the check once a token with this image is in PC/SC and no campaign is live.
  One option is installing the unchanged JCAlgTest 1.8.4/jc305 CAP.
- These are development images, not acceptance evidence. INV-27 requires an
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
during APDU traffic. Neither could be exercised locally: F42D is not listed by
pcscd, and pcscd was not refreshed during the live 07E7 campaign. A JCAlgTest
option 2 run on an image with these commits covers both.

## Remaining reduction opportunities (not implemented)

| Opportunity | Estimated saving | Notes |
|---|---:|---|
| mbedTLS precomputed point tables off for the curves in use | about 13 KB | Slower ECC. |
| Diagnostic strings and their formatting code | about 7–13 KB | **INV-24 check needed:** the normal image contains strings such as `CHOUPI:INTERFACE_CHECK …`, `CHOUPI:CHECKCAST …` and `VPCD:APPLET_PROCESS …`. INV-24 says non-diagnostic images contain no debug strings. Decide which strings are required telemetry (e.g. `PRE_CLEANUP` for the headroom checker). |
| Link-time optimization | unknown, roughly 5–10% of code | Re-check the `.bss` placement of `cap_load_session` afterwards. |
| Native-method signatures pass `std::shared_ptr` by value | moderate | Refactor. |
| C++ exceptions (tables plus runtime about 40 KB) | about 40 KB | The VM implements Java exceptions with them, so this is a major redesign. |

### RAM, for context

Static RAM use is 335.5 KB of 512 KB. The remaining ~176 KB must hold the C
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
