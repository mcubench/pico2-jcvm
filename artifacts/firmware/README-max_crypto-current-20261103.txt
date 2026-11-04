Current max_crypto ARM firmware for RP2350 Pico 2

Image: pico2-jcvm-max_crypto-arm-empty-card-current.uf2
Source build output: build/arm/pico2_choupi.uf2
Profile: max_crypto (147 of 150 declared JCAlgTest capabilities enabled)
Image size: 1,912,320 bytes
SHA-256: b964b6cf96fe9989d0740ff3fe9783d2a9c54ab9f92241282ba71ed6e7b71d57

The image was rebuilt from the current worktree using `./tools/build arm
--profile max_crypto`; matching `./tools/build riscv --profile max_crypto` and
`./tools/build-pc --profile max_crypto` builds also passed. The profile enables
RSA and ECC support. ARM flash-boundary validation passed with 92,780 bytes
remaining in the firmware window. The UF2 is a direct copy of the build
output; the two files have matching SHA-256 hashes.

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

This image includes all current worktree source changes. It has not been
flashed or runtime-tested on hardware after this rebuild. The previous image
was tested on Pico 2 `2BED26ED20556535`; that does not validate this rebuilt
binary.

The build used the shared worktree, which contains unrelated uncommitted local
source changes; this image may not be reproducible from a clean checkout until
those changes are committed or otherwise preserved.
