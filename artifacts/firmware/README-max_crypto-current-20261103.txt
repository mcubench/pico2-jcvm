Current max_crypto ARM firmware for RP2350 Pico 2

Image: pico2-jcvm-max_crypto-arm-empty-card-current.uf2
Source build output: build/arm/pico2_choupi.uf2
Profile: max_crypto (147 of 150 declared JCAlgTest capabilities enabled)
Image size: 1,905,664 bytes
SHA-256: 756fc23ca143f93625752855f2389eca201c6da11cd630b779f93d898f3d4bc6

The image was rebuilt using `./tools/build arm --profile max_crypto`; the
matching RISC-V core wrapper build also passed. The build enables RSA and ECC
support. ARM flash-boundary validation passed with 96,224 bytes remaining in
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

The exact image hash has not been runtime-tested on a physical Pico 2. The
authorized reader helper found no attached devices during this session. The
build was produced from the current shared worktree, which contains local
uncommitted source changes; this image may not be reproducible from a clean
checkout until those changes are committed or otherwise preserved.
