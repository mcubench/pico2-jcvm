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

The exact image hash was flashed and runtime-tested on physical Pico 2 boards
2BED (`2BED26ED20556535`) and 8E (`8E464F4E2BBC0A19`). Both ARM flash
verification cycles and finite startup monitors passed (`TEST:SUMMARY pass=1
fail=0`). On 2BED, the unchanged API 3.0.5 Security CAP completed 901/901 calls
(397 OK, 504 expected unsupported, no unexpected results). Crypto completed
466/466 calls on both boards (304 OK, 161 expected unsupported, one expected
`ILLEGAL_VALUE`/`EXC_CARDRUNTIME` for the undersized upstream SHA-512 PSS
vector); results matched the VPCD run. These runtime tests installed test
CAPs on cards with existing persistent content, so they do not establish that
those physical cards are empty. A firmware flash does not clear that content.

The image was rebuilt again with both repository wrappers on 2026-11-03. The
fresh ARM and RISC-V builds passed and the output hash remains the one above.
The build used the shared worktree, which contains unrelated uncommitted local
source changes; this image may not be reproducible from a clean checkout until
those changes are committed or otherwise preserved.
