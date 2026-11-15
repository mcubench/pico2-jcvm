# Handoff — next steps only

Updated: 2026-11-15.

## Current external test image
- ARM `max_crypto`, normal mode; object deletion ON; debug/trace OFF; serial wait 0 ms; empty card.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-6547ec4e-dirty.uf2`
- UF2 SHA-256: `0011be75271a035ea945fa7ba68fbd3f1f98ab3a4bc4160d9eec540169e2c3f8`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-6547ec4e-dirty.zip`
- ZIP SHA-256: `668f703c3c7b23232a3ab698ddd791b5ddda83a4029592b5061c6e1eeaaeb3d4`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-6547EC4E` in `.agent/EXT_REQUESTS.md`.
- Built from HEAD `6547ec4e88e365120248df08f32fca9d96ae4bf8` plus dirty worktree. ARM build, fixture verification, package checksums and ZIP integrity pass; 35,004 bytes remain. No physical flash/test was performed.

## Next actions
1. Await external option-2 results matched to this UF2 hash; obtain Windows version, reader/ATR, chip ID, full client outputs and serial telemetry. Option 1 can run on another identified token.
2. MEM-01 T7 live-client VPCD and T8 physical acceptance remain open. Captured APDU replay completed 1,972 commands without VM/storage failure; repeated overlay replays show stable post-compaction live set but do not exercise CCID/PCSC or satisfy fresh-client acceptance. See TODO and `artifacts/results/t7-option2-aes192-post5904-watermark-20261115/`.
3. CAPIN-01 offline coverage was refreshed: malformed external-superclass LOAD returned `6A80`, forbidden native-bridge Import INSTALL returned `6982`, stored missing Import/Class CAPs fail applet SELECT with `6F00` while Card Manager remains selectable, and unchanged JCAlgTest Descriptor-free install/select returns `9000`. Physical validation and broader Export consistency remain open.

## Hardware access
- Latest reader check: helper granted three ACLs and USB lists three `2e8a:0009 Pico2 JCVM CCID` devices. SmartcardIO connects to pcscd but blocks awaiting the list-readers reply; no PC/SC inventory is available. See `artifacts/logs/smartcardio-list-readers-strace-20261115.log`. Do not attribute this to VM/APDU behavior.
- Refresh with the approved helper and a bounded reader listing before hardware work; keep testing offline while the PC/SC reply stalls.
- Do not claim physical acceptance without reader enumeration, identity, flash and finite serial logs.
