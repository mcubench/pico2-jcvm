Current max_crypto ARM firmware for RP2350 Pico 2

Image: pico2-jcvm-max_crypto-arm-empty-card-current.uf2
Source build output: build/arm/pico2_choupi.uf2
Profile: max_crypto (147 of 150 declared JCAlgTest capabilities enabled)
Image size: 1,907,712 bytes
SHA-256: a3fa5e7ff07c166b8220b72caef3871215d57710695be7e1c3f47eae0a7eeb8e

The image was rebuilt using `./tools/build arm --profile max_crypto`; matching
RISC-V core and PC emulator builds also passed. The build enables RSA and ECC
support. ARM flash-boundary validation passed with 95,020 bytes remaining in
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

This image includes cross-store CAP lifecycle recovery. It was flashed and
runtime-tested on Pico 2 `2BED26ED20556535`; the 18-second monitor repeatedly
reported `TEST:SUMMARY pass=1 fail=0`. Cycle log:
`artifacts/logs/cycle-choupi-20261104T005831Z.log`.

The build used the shared worktree, which contains unrelated uncommitted local
source changes; this image may not be reproducible from a clean checkout until
those changes are committed or otherwise preserved.
