Pico 2 JCVM max_crypto ARM image
Build profile: max_crypto (147 of 150 declared capabilities enabled)
Target: RP2350, ARM Secure, 2 MiB Pico 2 flash
The image boots to an empty card; JCAlgTest is not preinstalled.

Flash from BOOTSEL by copying:
  pico2-jcvm-max_crypto-arm-empty-card.uf2

Then install the upstream CAP package with GlobalPlatformPro and run the
JCAlgTest 1.8.2 client. Expected supported rows are in:
  pico2-jcvm-max_crypto-arm-supported.csv

Disabled profile entries are key_ec_fp_private_112 and the RSA/RSA-CRT
KeyPair 4096 entries. The provided JCOP4 comparison matrix also excludes
AES-CTR, so this profile's AES-CTR entry is a JCVM API extension.

SHA-256:
  c739e78a21763724596ffb81878447b69038f67f1ff63d83a7d548d0b5c980e9  pico2-jcvm-max_crypto-arm-empty-card.uf2
  81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0  pico2-jcvm-max_crypto-arm-supported.csv

Refreshed from current source on 2026-10-31 with
`./tools/build arm --profile max_crypto`. The ARM image is 2,086,400 bytes and
includes CAP 2.2 remote metadata checks for signatures, public method tokens,
internal remote-interface references, external remote-interface Import
indexes, and install-time resolution of external references to exported
interfaces with Export-token bounds checks, as well as the RP2350 overlay
implementation and the corrected JC305 APDU virtual-method map. ARM and
RISC-V `max_crypto` wrapper builds passed. No physical board was flashed.
