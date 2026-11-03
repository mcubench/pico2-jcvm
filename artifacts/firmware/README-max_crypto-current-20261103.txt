Current max_crypto ARM firmware for RP2350 Pico 2

Image: pico2-jcvm-max_crypto-arm-empty-card-current.uf2
Source build output: build/arm/pico2_choupi.uf2
Profile: max_crypto (147 of 150 declared JCAlgTest capabilities enabled)
Image size: 1,905,664 bytes
SHA-256: 86081613e6ab0e2ee88be2c679d6f2005672d283b51dfd5d0f6747de7205b2b9

The image was rebuilt using `./tools/build arm --profile max_crypto`; the
matching RISC-V core wrapper build also passed. The build enables RSA and ECC
support. ARM flash-boundary validation passed with 96,072 bytes remaining in
the firmware window. The UF2 is a direct copy of the build output; the two
files have matching SHA-256 hashes.

This is a firmware-only image; no applet or upstream JCAlgTest CAP is bundled.
The image starts with no preinstalled applet. Firmware updates preserve CAPs
already stored on a token, so to make the physical token empty after flashing,
use GlobalPlatformPro on Windows before uploading the upstream CAP:

```powershell
$reader = 'Pico2 JCVM CCID 0' # replace with the exact name shown by Windows
$appletAid = '4A43416C675465737431'
$packageAid = '4A43416C6754657374'
gp --reader $reader --list
gp --reader $reader --delete $appletAid
gp --reader $reader --delete $packageAid
gp --reader $reader --list
```

If either delete reports that its AID is absent, continue. Confirm the final
list has no `APP:` or `PKG:` entries before installing any CAP. If other AIDs
are listed, remove them too if the intended test requires a fully empty card.
Install the unchanged upstream CAP only after this check.

The previously packaged image hash `756fc23c...` was flashed and runtime-tested
on 2BED (`2BED26ED20556535`) and 8E (`8E464F4E2BBC0A19`); full API 3.0.5
Security and Crypto results are recorded in `.agent/QKB.md`. The current image
hash above includes a CCID receive-framing fix and was rebuilt with both
repository wrappers on 2026-11-03. Both architecture builds passed. It was
flashed to 8E from BOOTSEL and picotool flash verification passed, but the
finite serial monitor could not find a USB serial device afterward and PC/SC
listed no physical reader. Runtime validation of this exact hash is therefore
incomplete. Cycle log: `artifacts/logs/cycle-choupi-20261103T232841Z.log`.

The build used the shared worktree, which contains unrelated uncommitted local
source changes; this image may not be reproducible from a clean checkout until
those changes are committed or otherwise preserved.
