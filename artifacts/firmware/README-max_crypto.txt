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
  8ef6492df92514d9a677f25d8d2bb1eff59c28160fe2609063b9b311d8ed2727  pico2-jcvm-max_crypto-arm-empty-card.uf2
  81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0  pico2-jcvm-max_crypto-arm-supported.csv

Refreshed from current source on 2026-10-30 with `./tools/build arm
--profile max_crypto`. The ARM image is 2,074,112 bytes and includes the
CAP 2.2 remote metadata validation, including public method-token checks, and
RP2350 overlay implementation. No physical board was flashed.
