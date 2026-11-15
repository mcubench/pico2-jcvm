# External validation requests

## WIN-OBJECT-DELETION-MAXCRYPTO-54BBA2B3-DIRTY — Test current deletion-enabled image

Created 2026-11-15 14:10 CET. This supersedes D508CAE8 for new tests. Use the
fresh normal ARM `max_crypto` empty-card image below. Object deletion is ON;
all debug/trace switches and startup wait are OFF. The persistent-storage
region is erased and no applet is preinstalled. The source tree had unrelated
dirty worktree and submodule changes at build time; CPLC identifies base HEAD
`54bba2b3` and build provenance is in `build/arm/build-provenance.txt`.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-54bba2b3-dirty.uf2)
- UF2 SHA-256: `11da9cebad8c04884084e95bbd40f5b7282a65a579e18abfb72abbeb8d81d98f`; size 4,127,232 bytes.
- ARM build passed with `mode=normal`, object deletion ON, and every debug
  option OFF. The linked image ends at `0x100f7b08`, leaving 34,040 bytes in the
  1 MiB firmware window. No physical flash or Windows test was performed here.

Flash in BOOTSEL by copying this UF2 to the `RPI-RP2` drive. Verify the exact
hash before copying. After the reader enumerates, use GPPro to install the
unchanged upstream JCAlgTest 1.8.4/jc305 CAP explicitly, then run option 2 with
the pinned 1.8.5 client. Return the complete result folder and serial log,
Windows version, reader/ATR, chip ID, image and client hashes, and any
disconnects, resets, stalls or timeouts.

## Superseded: WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8

Use `WIN-OBJECT-DELETION-MAXCRYPTO-54BBA2B3-DIRTY` above for new tests. Retain
the old image and request only as evidence for results already collected.

## WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8 — Test current deletion-enabled image

Created 2026-11-15 13:40 CET. This supersedes the AFE8ED5D candidate for new
runs. Use the normal ARM `max_crypto` image from source commit `d508cae8` plus
the tracked dirty worktree recorded in the package provenance. Schema-v5
arbitrary-prefix persistent tag encoding is included. Object deletion is ON;
debug/trace switches are OFF; persistent storage is erased; no applet is
preinstalled. The overlay remains at the INV-28 limit of 96 KiB / 8,192 slots.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-d508cae8-current.uf2)
- UF2 SHA-256: `91288e4963b1baa3d6959dcf1da30178c4d5ffbac5c22d83e7a0c1a45c931e96`; size 4,127,232 bytes.
- [Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-d508cae8.zip)
- ZIP SHA-256: `63fbae1ce480594952452533f78b6b71029adf0bcd49a6100745f827c4260a02`.
- ARM build passed in normal mode; linked end `0x100f7b08`, leaving 34,040 bytes. Package checksums and ZIP integrity pass. No physical flash or Windows run was performed here.

Unzip and follow the included README: verify the UF2 hash, flash through
BOOTSEL, confirm the card is empty, explicitly install the unchanged supplied
JCAlgTest 1.8.4/jc305 CAP with included GPPro, then run option 2 with the
included 1.8.5 client. Return the complete result folder and serial log,
Windows version, reader/ATR, chip ID, image/JAR hashes, and any disconnects,
resets, stalls or timeouts.

## WIN-OBJECT-DELETION-LOW-WATERMARK-D1130732 — Test deletion-enabled image within declared storage limit

Superseded by `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8` for new runs.

Created 2026-11-15 12:30 CET. This supersedes the withdrawn D73E3411 image,
the 72CBEEDC image, and earlier E9C4DEB3 candidate. Use the normal ARM
`max_crypto` image built from commit `d1130732` plus pre-existing unrelated
tracked worktree changes. Object deletion and automatic low-watermark
reclaim/compaction are ON; all debug/trace flags and startup wait are OFF. The
overlay remains at the INV-28 limit of 96 KiB / 8,192 slots. Persistent storage
is erased, and no applet is preinstalled.

This request is now superseded by `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8`;
retain only for results already collected against its old hash.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-d1130732-current.uf2)
- UF2 SHA-256: `80840af050b8a6e7f32c540a56b18421415143d4093c491bf9efcd41859d326c`; size 4,126,208 bytes.
- [Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-d1130732.zip)
- ZIP SHA-256: `5ef963a3bd654d449813d499e687c5040487c15a9251c10d1ce99261a6ab3ebe`.
- Exact source/submodule/build identities and input hashes are in the package's `BUILD-PROVENANCE.txt`.
- ARM build passed; linked end `0x100f7920`, leaving 34,528 bytes. Overlay tests, empty-card packaging, fixture verification, package checksums and ZIP integrity passed. The archive opens directly to `README.md`, `firmware/` and `tools/`. No physical flash or Windows test was performed here.

Unzip the package and follow its `README.md`: verify the UF2 hash in PowerShell,
flash through BOOTSEL, confirm the empty card, and explicitly install the
unchanged upstream JCAlgTest 1.8.4/jc305 CAP with GPPro. Run option 2 with the
included supplied 1.8.5 client. Return the full result directory and serial
log, Windows version, reader/ATR, chip ID, exact firmware hash, and any
disconnects, resets, stalls, or timeouts.

## WIN-RSA1984-X50-SERIAL-20261115 — Capture serial evidence of the RSA-1984 ×50 failure

Created 2026-11-15 12:35 CET. Diagnostic run (not acceptance) for TODO
LONGOP-01. It needs only about 5 minutes per attempt. If the full option-2
run under `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8` is in progress, let it
finish first. The current image does not contain a LONGOP-01 fix, so it may
still fail at this operation.

1. Flash the current empty-card UF2 from `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8`
   (SHA-256 `91288e49…c931e96`). Install the unchanged JCAlgTest CAP with GPPro.
   Use the pinned 1.8.5 client and report its SHA-256; it must be
   `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
2. Copy `artifacts/results/windows-maxcrypto-20261115-25216292-dirty/pico2_maxcrypto_20261115_25216292-dirty____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list`
   next to the client. Keep an untouched copy and restore it before each
   attempt. Start option 2, type the card name `pico2_maxcrypto_20261115_25216292-dirty`
   exactly, and answer `y`. The next measured operation is RSA-1984 PKCS1
   `Cipher_doFinal()`.
3. Before starting the client, open the board's COM port at **115200 baud
   (never 1200)** in a terminal that writes a file with a timestamp on every
   line (for example Tera Term Log with timestamps). Check that `rx_events`
   in the `CHOUPI:CCID` lines rises with each APDU. Confirm the COM port and
   the reader are the same board: their Device Manager instance paths end in
   the same serial (chip ID).
4. Keep logging until 2 minutes after the client reports the error. Do not
   press `r`/`s` until then. Then record `certutil -scinfo`, `sc query SCardSvr`,
   `Get-PnpDevice -Class SmartCardReader | fl FriendlyName,Status,Problem,InstanceId`,
   and System event log entries from the failure window.
5. Repeat once from a fresh replug.

Return the complete timestamped COM log (the whole file, not excerpts), the
client log and CSV, the PowerShell outputs, the UF2 and JAR hashes, and the
chip ID. The key question is what the serial log shows during the ≈45 s ×50
APDU and right after it:
- a `CCID_RESPONSE apdu=B043000016 len=3 sw=9000` record;
- `*** PANIC ***` and its message;
- a boot banner;
- nothing.

## Superseded: WIN-OBJECT-DELETION-LOW-WATERMARK-E9C4DEB3

Use `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8` above for new testing. The
intermediate D73E3411 candidate changed the declared overlay limit and is
withdrawn; do not flash it.

## Superseded: WIN-OBJECT-DELETION-LOW-WATERMARK-45EC828F

Use `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8` above for new testing. Its
image predates the current checkout and should be retained only as historical
evidence.

## HOST01-READER-COUNT-20261115 — Restore bounded Pico reader inventory

Created 2026-11-15 09:25 CET. The approved helper currently detects four matching `2e8a:0009 Pico2 JCVM CCID` devices, exceeds its supported 1–3 device range, and refuses ACL changes. The subsequent bounded reader listing times out, so no device can be safely identified for the fresh-client or physical test.

Please make no more than three intended Pico 2 JCVM readers visible to this VM (for example, detach an unintended extra VirtualBox USB attachment). If all four are intentional, report that; do not change firmware. Then run from the repo root and return both outputs:

```bash
sudo -n /usr/local/sbin/pico2-pcsc-reader
PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8
```

No firmware flashing is requested. This request only restores bounded reader enumeration so the agent can identify an exact reader before any hardware operation.

## Superseded: WIN-OBJECT-DELETION-LOW-WATERMARK-D3061DDD

Use the newer `WIN-OBJECT-DELETION-LOW-WATERMARK-45EC828F` package below.

## WIN-OBJECT-DELETION-LOW-WATERMARK-45EC828F — Test automatic low-watermark reclaim fix

Created 2026-11-15 08:38 CET. This package supersedes the D3061DDD low-watermark build. It is a fresh no-debug ARM `max_crypto` image with persistent object deletion ON, automatic low-watermark reclaim plus tombstone compaction, and erased persistent storage (empty card). Source HEAD is `45ec828ff2acd098689ab4f3d72566745678c4b7` plus dirty worktree; see package provenance for tracked-diff and submodule hashes.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-45ec828f-low-watermark.uf2)
- UF2 SHA-256: `47c0e5f194139e3720e43292f4a47468cbb6892990c9e1724813389b469117b9`; size 4,126,208 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-low-watermark-45ec828f.zip)
- ZIP SHA-256: `6f17e5b1006537178560e082f2a073eedc55171a475754f214095c02e05616bf`.
- ARM build passed (linked end `0x100f7920`; 34,528 bytes remain); fixture manifest verification and ZIP integrity passed. The RP2350 overlay VPCD test passed 515/515 APDUs over 512 persistent-array replacements, preserved the live root, and observed one automatic trigger plus compaction. No physical or Windows validation was performed.

Flash with BOOTSEL, verify the hash, confirm the card is empty, then install the unchanged upstream 1.8.4/jc305 CAP explicitly with included GPPro. Run option 2 using the included supplied 1.8.5 client and capture serial telemetry. Return per-device result files, serial log, Windows version, reader/ATR, chip ID, firmware hash, and all disconnects or stalls.

## WIN-OBJECT-DELETION-LOW-WATERMARK-D3061DDD — Test low-watermark deletion

Created 2026-11-15 08:11 CET. This package supersedes
`WIN-OBJECT-DELETION-NORMAL-DIRTY-25216292`. It contains a no-debug ARM
`max_crypto` image with object deletion ON and erased persistent storage (empty
card). Source is HEAD `d3061ddd8392e6d1e1712378991eb9806778d770` plus a dirty
worktree; full source and submodule hashes are in the package provenance.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-20261115.uf2)
- UF2 SHA-256: `83423ee4f68e7c16b88741b09a6d126cf33f2ba86d407f5b00675968a45c3f1d`
- Size: 4,126,208 bytes; linked end `0x100f7928`, 34,520 bytes remain.
- [Complete Windows test package](../build/external-test/pico2-jcvm-object-deletion-on-windows-d3061ddd-dirty.zip)
- ZIP SHA-256: `c1aed51f971aa17d22feafb69427e340e5014fca086d0e97b6f3b382ea94ecb3`
- ARM build, empty-card UF2, 13 policy checks, filesystem overlay suite,
  package checksums and ZIP integrity pass. Fake-flash low-watermark test
  reclaimed 8,240/8,240 garbage objects and peaked at 79.2% arena use. No
  hardware test was performed here.

Flash and run JCAlgTest option 2 with the unchanged supplied 1.8.5 client.
Install the unchanged upstream 1.8.4/jc305 CAP explicitly after confirming
the card is empty. Return per-device result folders and serial logs, plus
Windows version, reader/ATR, chip ID, firmware hash, USB/PCSC errors, timeouts,
and stalls. If available, run option 1 on another identified token.

## Superseded: WIN-OBJECT-DELETION-NORMAL-DIRTY-25216292

Use `WIN-OBJECT-DELETION-LOW-WATERMARK-D3061DDD` above for new external runs.

## WIN-OBJECT-DELETION-NORMAL-DIRTY-25216292 — Test current deletion and memory fixes

Created 2026-11-15 06:56 CET. This package supersedes the `6547EC4E`
package below. The fresh ARM RP2350 `max_crypto` image has object deletion ON,
debug/trace OFF, serial wait 0 ms, and erased persistent storage (empty card).
It was rebuilt from HEAD `252162925b44b32675b2f57bd5ed5bb3b94c712d` plus the
current dirty worktree; exact tracked-diff and submodule identities are in the
package provenance. The build includes the current persistent-memory and CCID
fix source state.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-25216292-dirty.uf2)
- UF2 SHA-256: `740801210d2601891a5610e4cae5e67da516b49a50d30723cb918e96a0b43b97`
- Size: 4,125,184 bytes
- [Complete Windows test package](../build/external-test/pico2-jcvm-object-deletion-normal-windows-25216292-dirty.zip)
- ZIP SHA-256: `398b13dcf3248bcadd52006c29b197ae7fb154e6739ad1404760a53c72135e21`
- [Package instructions](../build/external-test/pico2-jcvm-object-deletion-normal-windows-25216292-dirty/README.md)
- ARM build passed with linked image end `0x100f7744`, leaving 35,004 bytes.
  Empty-card packaging, fixture verification, package checksums and ZIP
  integrity passed. No physical flash/test was done here.
- Inputs are unchanged upstream JCAlgTest 1.8.4/jc305 CAP and supplied 1.8.5
  client.

Flash this UF2 and report its exact hash, Windows version, reader name/ATR,
chip ID, full option-2 performance result and serial log. If possible, run
option 1 on another identified token. Include PC/SC/USB errors, timeouts,
resets, stalls and complete per-device result directories. Install the
unchanged CAP explicitly with GPPro after confirming the card is empty.
Fulfilled 2026-11-15: this image reproduced the RSA-1984 ×50 failure; results
are archived in `artifacts/results/windows-maxcrypto-20261115-25216292-dirty/`.
The RSA-1984 follow-up continues as `WIN-RSA1984-X50-SERIAL-20261115`.

## Superseded: WIN-OBJECT-DELETION-NORMAL-DIRTY-6547EC4E

Use the newer `WIN-OBJECT-DELETION-NORMAL-DIRTY-25216292` package above for
external testing. Its CPLC source identifier and firmware hash differ.

## Superseded: WIN-OBJECT-DELETION-NORMAL-DIRTY-7A396462

Use the newer `WIN-OBJECT-DELETION-NORMAL-DIRTY-2BA049C8` package below; it
was rebuilt from the latest checkout after subsequent memory-management fixes.

## Superseded: WIN-OBJECT-DELETION-NORMAL-DIRTY-2BA049C8

Use the newer `WIN-OBJECT-DELETION-NORMAL-DIRTY-C3309A61` package below. This
replacement includes the latest dirty-source bugfix state.

## Superseded: WIN-OBJECT-DELETION-NORMAL-DIRTY-C3309A61

Use the newer `WIN-OBJECT-DELETION-NORMAL-DIRTY-6547EC4E` package above.

## WIN-OBJECT-DELETION-NORMAL-DIRTY-C3309A61 — External deletion-enabled test (superseded)

Created 2026-11-15 01:36 CET. ARM RP2350 `max_crypto`, normal mode, object
deletion ON, all debug/trace OFF, zero startup wait, empty persistent storage,
and no preinstalled applet. Built from HEAD `c3309a61987c674c` plus dirty
tracked changes; exact root and CHOUPI OS diff hashes are in package
provenance.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-c3309a61-dirty.uf2)
- UF2 SHA-256: `12fc079ec6927e13aad2cc59acf41adade7cfc955f4bdceb81cf9cfdb41f679d`
- Size: 4,125,184 bytes
- [Complete Windows package](../build/external-test/pico2-jcvm-object-deletion-normal-windows-c3309a61-dirty.zip)
- Package SHA-256: `c543d8e739b6e5d73e954e95e0d36dff431f27f00abdfdb934a786e14d79f9c5`
- [Instructions](../build/external-test/pico2-jcvm-object-deletion-normal-windows-c3309a61-dirty/README.md)
- Build provenance records tracked diff and submodule identities, build flags,
  ROM mask, input hashes and firmware size. Firmware ends at `0x100f7744`,
  leaving 35,004 bytes before the configured limit.
- ARM build, empty-card UF2 creation, fixture verification and ZIP integrity
  passed. No physical flash or local hardware test was performed.
- Test assets are unchanged upstream JCAlgTest 1.8.4/jc305 CAP and supplied
  1.8.5 client.

Flash the provided UF2, record its exact hash, Windows version, reader
name/ATR, chip ID, complete client results and serial log. Run option 2 on one
board and option 1 on another identified board when available. Confirm the
card is empty before explicitly installing the supplied CAP with GPPro. Return
all per-device results and telemetry.

## Superseded external test details: WIN-OBJECT-DELETION-NORMAL-DIRTY-2BA049C8

Created 2026-11-15 00:47 CET. ARM RP2350 `max_crypto`, normal mode, object
deletion ON, all debug/trace OFF, zero startup wait, empty persistent storage,
and no preinstalled applet. Built from source HEAD `2ba049c88e32c4eb` with the
tracked dirty source diff recorded in package provenance.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-2ba049c8-dirty.uf2)
- UF2 SHA-256: `049b4eadf11a40ccae797a6564f4fb4baaeeb5eb017ec89bc8758497a4d2c5c5`
- Size: 4,125,184 bytes
- [Complete Windows package](../build/external-test/pico2-jcvm-object-deletion-normal-windows-2ba049c8-dirty.zip)
- Package SHA-256: `a60883c36effa810c54a392df1c7241ac03c3ecc332abbf0a95f4e888f8e3556`
- [Instructions](../build/external-test/pico2-jcvm-object-deletion-normal-windows-2ba049c8-dirty/README.md)
- Build provenance records the root diff and CHOUPI OS submodule identity, build flags, ROM mask, and input hashes.
- ARM build, empty-card UF2 creation, fixture verification, package checksums and ZIP integrity passed. Physical flash/testing was not performed.
- Test assets: unchanged upstream JCAlgTest 1.8.4/jc305 CAP and supplied 1.8.5 host client.

Please flash the provided UF2, record its exact hash, Windows version, reader
name/ATR, chip ID, results and serial log. Run option 2 on one board and option
1 on another identified board when available. Confirm the card is empty before
explicitly installing the supplied CAP with GPPro. Return all per-device
results and telemetry.

## Superseded: WIN-OBJECT-DELETION-NORMAL-DIRTY-219C1D1F

Created 2026-11-14 22:34 UTC. This package was rebuilt from the current
worktree after the latest source updates. Use this image for the next
external Windows run: ARM `max_crypto`, normal mode, object deletion ON, all
debug/trace flags OFF, zero serial delay, empty persistent storage, no applet
preinstalled.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-219c1d1f-dirty.uf2)
- UF2 SHA-256: `74b27919ac2165b750734eb2fae7bd29c87bdf536ee225fea8f896bdad5f20da`
- Size: 4,125,184 bytes
- [Complete Windows package](../build/external-test/pico2-jcvm-object-deletion-normal-windows-219c1d1f-dirty.zip)
- Package SHA-256: `8ea646a8e2d637f7b69f50ed1fc6e4a7a9dc343a61487a26f9772a9ce4c9218d`
- Instructions: [package README](../build/external-test/pico2-jcvm-object-deletion-normal-windows-219c1d1f-dirty/README.md)
- Source HEAD: `219c1d1f62277bf3`; source worktree was dirty. Packaged provenance records root tracked-diff SHA-256 `99938a657a68be0618b38c3c6ebcdf97d3b46b4a3f8f53eac115b8c4e40c9792` and CHOUPI OS submodule commit/diff.
- Build passed with 35,004 bytes remaining before the firmware limit. Empty-card packaging, fixture manifest verification, package checksums and ZIP integrity passed. No physical flash or hardware validation was performed here.
- Exercise assets: unchanged upstream JCAlgTest 1.8.4/jc305 CAP (SHA-256 `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`) and supplied 1.8.5 host client (SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`).

Flash only the provided UF2 and record its exact hash, Windows version, reader
name/ATR, chip ID, result directory and serial log. Run option 2 on this board;
use a second identified device for option 1 when available. Install the CAP
explicitly with the included GPPro JAR after confirming the card is empty.

## Superseded: WIN-OBJECT-DELETION-NORMAL-97D9027E

Use `WIN-OBJECT-DELETION-NORMAL-DIRTY-50114A30` above; the 97d9027e image
predates the current worktree bugfix.

## WIN-OBJECT-DELETION-NORMAL-97D9027E — Test max_crypto with deletion enabled

Created 2026-11-14 21:30 UTC. Use this fresh empty-card Windows package for
object-deletion testing. This is a normal-mode ARM `max_crypto` build: object
deletion ON and all debug/trace options OFF. No applet is preinstalled; install
the unchanged upstream JCAlgTest CAP through GPPro after confirming PC/SC.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-97d9027e.uf2)
- UF2 SHA-256: `9157bc1a41661f04ded8fff40136c27825c427a90f87bc03742d84711fd358ad`
- [Complete Windows test package](../build/external-test/pico2-jcvm-object-deletion-normal-windows-97d9027e.zip)
- Package SHA-256: `eb1b6e9bdad555ddac5f1241f999c60a37ebffebda191644a10c250e0063b7f3`
- Package instructions: [README](../build/external-test/object-deletion-normal-97d9027e/README.md)
- Source HEAD and CPLC source ID: `97d9027e690475438890c1d5eddefba32245f29d`, `0x97D9027E69047543`; worktree dirty, exact tracked diffs are in packaged provenance.
- ARM build passed for `max_crypto`, `mode=normal`, object deletion ON, all debug OFF. Firmware ends at `0x100f7744`, leaving 35,004 bytes before the configured limit. Empty-card packaging, file checksums and ZIP integrity passed.
- Exercise CAP: unchanged JCAlgTest 1.8.4/jc305, SHA-256 `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`. Host client: supplied 1.8.5 JAR, SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.

Run `2 -> PERFORMANCE TEST` and, on another token if available, `1 -> All algorithms`. Return complete per-token result folders and serial logs, Windows version, reader name/ATR, chip ID and UF2 hash. Do not report this as hardware-validated by the development environment; no local physical flash was performed for this package.

## Superseded: WIN-OBJECT-DELETION-NODEBUG-CCID-DEADLINE-805420E6

This prior package is superseded by `WIN-OBJECT-DELETION-NORMAL-97D9027E`;
use the current normal-mode artifact above for new testing.

## WIN-OBJECT-DELETION-NODEBUG-CCID-DEADLINE-805420E6 — Retest long-run CCID fix

Created 2026-11-14 22:14 CET. Please use this fresh Windows package for the
next max_crypto object-deletion campaign. It contains a no-debug ARM acceptance
image with object deletion ON, serial wait 0 ms, and empty persistent storage.
It includes the fix for the long-uptime CCID response-retry deadline defect
described below. The JCAlgTest applet is not preinstalled; install the unchanged
upstream CAP explicitly with GPPro.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-nodebug-empty-card-ccid-deadline-805420e6.uf2)
- UF2 SHA-256: `5fa91117aeb6b2ac1c6ebb537447090016b27f0717bd14e528cd5d9437e4a6de`
- [Complete Windows test package](../build/external-test/pico2-jcvm-object-deletion-nodebug-windows-ccid-deadline-805420e6.zip)
- Package SHA-256: `6a3e343eda7f88c6f845939cf94d71a5ca13d67dbc2b5d268bc1a64ccc0f8bf0`
- Package instructions: [README](../build/external-test/object-deletion-nodebug-ccid-deadline-805420e6/README.md)
- Source HEAD: `805420e631276d5079c103ff281cf6b2f91fa149`; worktree was dirty.
  Packaged provenance includes tracked-diff and submodule hashes.
- Build provenance: ARM `max_crypto`, object deletion ON, all debug flags OFF,
  serial wait 0 ms, `mode=acceptance`; CPLC source ID `0x805420E631276D50`.
- Exercise CAP: unchanged JCAlgTest 1.8.4/jc305, SHA-256
  `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`.
- Host client: supplied JCAlgTest 1.8.5, SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.

Use the README for exact hash verification, BOOTSEL copying, PC/SC checks,
GPPro installation, campaign choices and serial capture. Run option 2 on one
identified token and option 1 on another if available. Return complete results,
serial logs, Windows version, reader/ATR, chip ID and UF2 hash. The ARM build,
empty-card packaging, package checksums and ZIP integrity passed. No hardware
flash or physical test was performed here.

## Superseded: WIN-OBJECT-DELETION-NODEBUG-BA7E4AA1

The image and package below are superseded by
`WIN-OBJECT-DELETION-NODEBUG-CCID-DEADLINE-805420E6`; use the new acceptance
image for further campaigns.

## WIN-OBJECT-DELETION-NODEBUG-BA7E4AA1 — Test fresh max_crypto acceptance image

Created 2026-11-14 21:43 CET. Please use this fresh ARM `max_crypto` image for
Windows object-deletion testing. It has object deletion enabled, all debug
logging disabled, serial wait 0 ms, and an empty persistent-storage region.
The card has no preinstalled applet; install the unchanged upstream JCAlgTest
CAP explicitly after PC/SC enumeration.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-nodebug-empty-card-ba7e4aa1.uf2)
- UF2 SHA-256: `88e620964a0a52a6d061f2a3c14f538227f11ba0962d6f32972d6bc534545b6e`
- [Complete Windows test package](../build/external-test/pico2-jcvm-object-deletion-nodebug-windows-ba7e4aa1.zip)
- Package SHA-256: `ea11fb0fd6274af68c9f7840c4ba2dc13dcae5a8eb2b9ea79997028c193ddeec`
- Package instructions: [README](../build/external-test/object-deletion-nodebug-ba7e4aa1/README.md)
- Source HEAD: `ba7e4aa1ba660952f4d7db5fc0321df70b9f5b0c`, with dirty tracked
  diffs; see packaged `BUILD-PROVENANCE.txt`. CPLC source ID is
  `0xBA7E4AA1BA660952`.
- Linked firmware ends at `0x100f770c`; 35,060 bytes remain before the
  firmware storage limit. The empty-card UF2 is 4,125,184 bytes because it
  includes erased 1 MiB persistent storage.
- Build provenance: profile `max_crypto`; object deletion ON; debug CCID,
  heap-GC and persistent-GC OFF; serial wait 0 ms; mode `acceptance`.
- Exercise CAP: unchanged upstream JCAlgTest 1.8.4 / jc305, SHA-256
  `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`.
- Host client: supplied JCAlgTest 1.8.5, SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.

Use the README for exact PowerShell hash verification, BOOTSEL copying,
GPPro installation and serial capture. Run option 2 on one identified token;
use a second token for option 1 if available. Return complete results, serial
logs, Windows version, reader/ATR, chip ID, and exact UF2 hash. This package
has passed the ARM build, empty-storage packaging checks, checksum verification
and ZIP integrity check. It has not been flashed or tested on hardware.

## Superseded: WIN-OBJECT-DELETION-NODEBUG-0E67DF89

Created 2026-11-13 15:27 UTC. This earlier image is superseded by
`WIN-OBJECT-DELETION-NODEBUG-BA7E4AA1` above. It is an empty-card ARM `max_crypto`
image with object deletion enabled and all CCID, heap-GC, and persistent-GC
debug logging disabled (serial wait 0 ms). The build provenance says
`mode=diagnostic` because object deletion is enabled; it does not indicate
debug logging.

- [Complete Windows test package](../build/external-test/pico2-jcvm-object-deletion-nodebug-windows-0e67df89.zip)
- Package SHA-256: `e51d161d6622e77595fd704988e45229060a27550f2f3be575ff22d7aeb74657`
- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-nodebug-empty-card-0e67df89.uf2)
- UF2 SHA-256: `51572f69103c02dfd056f1772366d1cafbd8ac92d0da7967e9f310a6099f94f5`
- Source HEAD: `0e67df893aea78efbf1a428608a74060af090b14` (worktree dirty; see package provenance).

The package README contains hash checks, exact BOOTSEL copying, reader/ATR
checks, GPPro installation of the unchanged JCAlgTest 1.8.4/jc305 CAP, and
client 1.8.5 campaign steps. Please return the Windows version, reader/ATR,
chip ID, complete campaign result directory, and serial log. The previous
d63877f6 package and all root-diagnostic images are superseded for this new
run.


## PCSC-BOOTSEL-RECOVERY-20261113 — Restore physical reader and BOOTSEL access

Created 2026-11-13 08:19 UTC. The local option-2 clients have exited. After
the response watchdog failure, the approved Pico helper granted two USB nodes,
but `pcsc_scan -r` listed only Virtual PCD 00/01. Both identified runtime
devices (`2BED26ED20556535`, `D92E743D5F640C0F`) failed the wrapper's bounded
force-reboot attempts and did not enter BOOTSEL; no firmware was flashed.

Latest update 2026-11-13 09:38 UTC: after the user reported PC/SC restored, I
refreshed the approved helper using the required elevated host-access path. It
reported `Expected 1 to 3 Pico2 JCVM CCID devices (2e8a:0009, matching USB
strings); found 0.` No device was reset or flashed. The current development VM
still does not see a Pico reader; previous device states above are historical.

Please attach at least one Pico 2 to the development VM in VirtualBox
**Devices → USB** while it is in BOOTSEL. I will refresh the helper and identify
the exact board before any physical operation. No PC/SC service restart is
requested while no client is active.

## Test JCSystem object deletion on Windows

The initial package below has been superseded by the bounded-reclaimer package
in the next section. Do not use the old firmware hash for another upload.

Please run the refreshed diagnostic package and follow its exact hash check and
BOOTSEL copy commands:

- [Windows package](../build/external-test/pico2-jcvm-object-deletion-windows.zip)
- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-cdd175bc.uf2)
- Package SHA-256: `318c9e69e55b7a44d0865bf66e18049288fcaf91ef60e240004e2ee45d9edf51`
- Firmware SHA-256: `a66e7d2b33c4695fa2e3415e4d198300ae92089f1ab93dbcd259968132d871e0`
- Source commit: `cdd175bc66af28528817c6998008269ed0955b6a`; the build used a dirty
  worktree, disclosed with its tracked-diff hash in the package's
  `BUILD-PROVENANCE.txt`.
- The package README includes exact Windows PowerShell hash verification and
  BOOTSEL flashing commands. It contains the unchanged JCAlgTest 1.8.4/jc305
  CAP and matching client, GPPro, and serial capture helper.

The diagnostic image uses `max_crypto` with object deletion enabled. It starts
with an empty card and exercises the periodic `INS_CARD_RESET` request path.
Run `2 -> PERFORMANCE TEST` (1–3 hours) on one identified Pico. When a second
token is available, flash/install the same package there and run
`1 -> All algorithms` (5–8 hours). Keep per-device result folders, reader names,
chip IDs, firmware hash, and complete serial logs separate.

### Windows USB enumeration failure reported 2026-11-11

After BOOTSEL mode and copying the UF2, Windows reported an “unrecognized USB
device.” Local validation of the freshly rebuilt current-source image
(`artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-20261111-current.uf2`,
SHA-256 `8af721cfd59d22651fad5d17e0ef4dd81d1262402f3ba40cced1c232be410f94`)
on Linux passed flash verification, re-enumerated as a PC/SC reader, and
passed `TEST:SUMMARY pass=1 fail=0`; that does not identify the Windows cause.
Please return the exact UF2 filename and SHA-256 used for the failing upload,
Windows version, and the Device Manager status/error code and hardware IDs
(or the exact “Device Descriptor Request Failed” text if IDs are unavailable).

Return full result folders and serial logs, including partial output if a run
stalls. Preserve `OBJECT_DELETION:` lines and any `6F00`, `F205`, reset,
timeout, or PC/SC service errors. This diagnostic image is not for production;
normal builds keep deletion disabled.

### Fresh shared-budget Windows package — 2026-11-11

Use this package for the next Windows test. The older `1651bbdd` package has a
known shared-budget defect and must not be used. The new firmware includes the
shared eight-record limit across applet and package-static records. It is an
empty-card image; install the unchanged CAP explicitly after the reader
enumerates.

- [Windows package](../build/external-test/pico2-jcvm-object-deletion-shared-budget-73beff17.zip)
- Package SHA-256: `39dd82ada1c10eac539c6e77b033bb08097ffd7cab993c3a40c6cdfe91934f65`
- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-73beff17.uf2)
- UF2 SHA-256: `4353d4f2f068aa988fce7d4d92c667634ddf4d3684e4580c8dedae08c2efb3af`
- Source commit: `73beff17`; ARM `max_crypto`; object deletion enabled;
  CPLC source ID `0xFF73BEFF`; 48,752 firmware bytes remain.
- The package README has exact hash verification, BOOTSEL copy, PC/SC check,
  GPPro CAP installation, and serial capture commands. `BUILD-PROVENANCE.txt`
  discloses the dirty root diff and submodule source hashes used by the build.

Please first confirm Windows enumerates this exact UF2 with the reported
SHA-256 and provide the reader name and ATR. If Windows again shows an
unrecognized device, send the Windows version, Device Manager error/status and
hardware IDs (or descriptor-request error), and whether BOOTSEL returns after
unplug/replug. If it enumerates, explicitly install `AlgTest_v1.8.4_jc305.cap`
and run the 1.8.4 client following the package README; return results and full
serial output.

## Resolved 2026-11-11: Reattach Pico USB devices to the development VM

The user confirmed that the earlier absence was a VM USB passthrough problem.
The refreshed host helper now grants access to three Pico USB nodes. `lsusb`
sees all three and `pcsc_scan -r` publishes readers for `8E464F4E2BBC0A19`
and `2BED26ED20556535`. The third node (`D92E743D5F640C0F`) is not currently
published by PC/SC, so physical testing continued on the two available readers
instead of waiting on that board. No further VM reattachment action is needed
for the active tests.

## Closed 2026-11-11: Recover the PC/SC service for the local JCAlgTest campaigns

The helper and PC/SC scan now work for two identified Pico readers, and the
physical campaign continues on those devices. D92 remains absent from PC/SC
despite USB enumeration, but is not blocking the active tests; no service
restart or other external action is currently requested. If D92 is needed
again, reconnect it to the VM and report the result of `timeout 10s pcsc_scan -r`.

## Resolved 2026-11-12: PC/SC restored for D92 and 8E; 2BED needs reattachment

The fixed physical collector ran on `8E464F4E2BBC0A19` during the upstream
JCAlgTest support suite. Its serial trace shows `INS_CARD_RESET` completing
with `9000`, but the synchronous sweep reclaimed 1,417 applet records before
the host received `SCARD_E_NOT_TRANSACTED`. PC/SC then hung before listing
readers, even though `2BED26ED20556535` still passes its finite CDC runtime
monitor. I have bounded the collector to eight persistent records per APDU;
both architecture builds and the persistent-transaction unit suite pass, but
the physical batch-size fix still needs validation after PC/SC recovers.

### Result 2026-11-12 08:27 UTC

After flashing the current empty-card image, the approved helper and
`pcsc_scan -r` listed D92 and 8E. GPPro `--list` confirmed no APP/PKG entries
on either board, so a PC/SC service restart is no longer needed. The third
board `2BED26ED20556535` remains at USB `2e8a:0009` without its serial
descriptor and did not enter BOOTSEL after a path-specific CDC reset. It is
not safe to flash it until the ROM chip ID is visible again.

## EXT-USB-VM-REATTACH-20261111-1824 — Current VM USB passthrough status

The user confirms that the physical boards are available and the earlier
problem was VM passthrough, not a board fault. At 2026-11-11 18:58 CET, after
that update, the authorized helper still found zero matching Pico devices
inside this VM. No device was flashed or reset during this check. This is the
current VM visibility result, not a report that the physical boards are
unavailable or malfunctioning. The required host-access diagnostic also
reported `No accessible RP-series devices in BOOTSEL mode were found`.

When changing passthrough, attach at least one Pico USB device to the
development VM through VirtualBox **Devices → USB**. The next local action is
to refresh the helper and identify the reader; the bounded deletion tests and
both architecture build gates already pass. Host-side physical availability
alone does not expose its USB node inside this guest.

### Earlier runtime-reset passthrough event — 2026-11-11

The VM initially saw all three boards (`D92E743D5F640C0F`,
`2BED26ED20556535`, and `8E464F4E2BBC0A19`). A firmware-only flash attempt on
2BED could not use the reset interface; the CDC 1200-baud fallback did not
enter BOOTSEL, and the VirtualBox guest then lost all three RP2350 USB nodes.
No firmware was written to 2BED. The host-side 8E46 test did reclaim 1,417
unreachable applet records before its CCID transaction failed.

Please reattach the Pico devices to the development VM through VirtualBox
**Devices → USB** (including a BOOTSEL entry if Windows captured it). Keep
2BED in BOOTSEL if convenient; that will let me flash the new bounded-cleanup
image directly without another software reset. Otherwise reattach the runtime
devices and report when they appear. I will then confirm IDs, flash only after
the ARM/RISC-V builds, and continue on whichever reader is available.

## Retry JCSystem object deletion with bounded cleanup (source commit 1651bbdd)

**Superseded; do not flash this image for current validation.** Code review
found the eight-record allowance was independently applied to applet and
package-static namespaces, so one APDU could erase up to 16 records. A shared
total budget is now implemented and validated by host tests, VPCD integration,
and ARM/RISC-V build wrappers. A fresh Windows package from the shared-budget
firmware has not yet been produced.

A fresh empty-card test package is ready. It includes the diagnostic ARM image,
unchanged JCAlgTest 1.8.4/jc305 CAP and client, GPPro, and serial capture tool.
The reclaimer now deletes at most eight persistent records per APDU and resumes
on later APDUs, so a large cleanup will not hold the CCID response while
erasing the full backlog.

- [Download the bounded Windows package](../build/external-test/pico2-jcvm-object-deletion-bounded-windows-1651bbdd.zip)
- [Download the standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-1651bbdd.uf2)
- Package SHA-256: `a48df3e9c1dd981efaa3beb93f856f52cc9b100dc7c0f151d643fdb9df0548a1`
- UF2 SHA-256: `7fb42f4b5f706a3df26bc36ed5f2809de4d322f6662771b2d8722a3bdd0d40e4`
- Source commit: `1651bbdd`; ARM profile: `max_crypto`; object deletion:
  enabled; serial wait: `0 ms`; CPLC source ID: `0xFF1651BB`.
- UF2 size: `4,097,536` bytes. This is an empty-card image and erases the
  complete 1 MiB persistent-storage region when copied in BOOTSEL.

The package README contains PowerShell hash verification and exact BOOTSEL,
installation, and test steps. First confirm the device is recognized after
flashing and report its reader name and ATR; install the upstream CAP explicitly
with the packaged GPPro. Then choose `2 -> PERFORMANCE TEST` on one identified
token and `1 -> All algorithms` on a second token if available. Keep the
result and full serial log separate per chip ID. If Windows still reports an
unrecognized USB device, include the exact UF2 hash, Windows version, and
Device Manager status/error and hardware IDs. The bounded build has passed
unit tests and ARM/RISC-V builds but has not yet been tested on physical
hardware. Use it only as historical evidence, not as the current test image.
