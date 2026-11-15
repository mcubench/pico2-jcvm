# Handoff — next steps only

Updated: 2026-11-15.

## Current external test image
- ARM `max_crypto`, normal mode: object deletion ON; debug/trace OFF; serial wait 0 ms; empty card; no applet preinstalled.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-c3309a61-dirty.uf2`
- UF2 SHA-256: `12fc079ec6927e13aad2cc59acf41adade7cfc955f4bdceb81cf9cfdb41f679d`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-c3309a61-dirty.zip`
- Package SHA-256: `c543d8e739b6e5d73e954e95e0d36dff431f27f00abdfdb934a786e14d79f9c5`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-C3309A61` in `.agent/EXT_REQUESTS.md`.
- Built from HEAD `c3309a61987c674c`, worktree dirty; package provenance records tracked diff and submodule identities. ARM build passes, firmware ends `0x100f7744` with 35,004 bytes remaining. Fixture verification and ZIP integrity pass. No physical flash/test was performed.

## Next actions
1. Await the external Windows run. Match every result to the UF2 hash; collect Windows version, reader/ATR, chip ID, complete client results and serial log.
2. MEM-01 T7 live-client VPCD and T8 physical acceptance remain open. The full 1,972-command physical transcript replay passed from an empty card; four unrecorded source responses succeeded and no VM/storage errors appeared. Additional captured cycles reached 81,775-byte pre-cleanup peaks and returned to a stable 367-file / ~14.7–15.5 KiB post-cleanup set. These replays do not exercise CCID/PCSC transport or satisfy a fresh-client campaign. See TODO and `artifacts/results/t7-option2-aes192-post5904-loop2-20261115/`.
3. CAPIN-01 APDU-level PC VPCD validation passed: malformed LOAD returned `6A80`, GET STATUS excluded it while retaining the control package, and control DELETE returned `9000`. Physical acceptance remains open.

## Hardware access
- At 2026-11-15 01:39 CET, the approved helper granted ACLs for three USB nodes, but elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8` again timed out. No usable reader inventory was obtained. Do not attribute USB nodes to JCVM boards without bounded enumeration.
- Do not claim physical acceptance without reader enumeration, identity, flash, and finite serial logs.
