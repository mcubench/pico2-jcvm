# Handoff — next steps only

Updated: 2026-11-15.

## Current external test image
- ARM `max_crypto`, normal mode; object deletion ON; debug/trace OFF; serial wait 0 ms; empty card.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-25216292-dirty.uf2`
- UF2 SHA-256: `740801210d2601891a5610e4cae5e67da516b49a50d30723cb918e96a0b43b97`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-25216292-dirty.zip`
- ZIP SHA-256: `398b13dcf3248bcadd52006c29b197ae7fb154e6739ad1404760a53c72135e21`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-25216292` in `.agent/EXT_REQUESTS.md`.
- Built from HEAD `252162925b44b32675b2f57bd5ed5bb3b94c712d` plus dirty worktree. ARM build, fixture verification, empty-card packaging, package checksums and ZIP integrity pass; 35,004 bytes remain. No physical flash/test was performed.

## Latest external report triage
- Incoming Windows run used superseded 805420e6, not current 25216292. RSA-1984 NOPAD one-operation baseline returned `AA9000`; the 50-operation measurement ended in PC/SC `0x8010002f`, then SELECT/reconnect errors and `SCARD_E_SERVICE_STOPPED`.
- Incoming client JAR is not the pinned 1.8.5 JAR; 3/66 classes differ (`AlgTestJClient`, `CardMngr`, `PerformanceTesting`). No chip ID or serial log, so firmware fault is not established.
- QKB and HOST-01/TODO contain hashes/details. Current external request asks for pinned-client telemetry on this image; preserve any already-running full campaign.

## Next actions
1. Await external option-2 results matched to this UF2 hash, especially RSA-1984 NOPAD; obtain Windows version, reader/ATR, chip ID, full client outputs and serial telemetry. Option 1 can run on another identified token.
2. MEM-01 T7 live-client VPCD and T8 physical acceptance remain open. Captured APDU replay completed 1,972 commands without VM/storage failure; repeated overlay replays show stable post-compaction live set but do not exercise CCID/PCSC or satisfy fresh-client acceptance. See TODO and `artifacts/results/t7-option2-aes192-post5904-watermark-20261115/`.
3. MEM-01 reviewer (2026-11-15): the replay watermarks (post-cleanup 367 files / ~15.4 KB stable) show the T4/T5 leak is fixed at the VM/storage level. New risk M13: pre-cleanup peak of 81,775 / 98,304 bytes (≈83%). A safe-point guard now keeps object-deletion requests pending during persistent transactions. T10 still needs pre-cleanup watermark checks and an exhaustion-handling design; immediate retry inside `fs_write` is unsafe because the Java stack is live.
4. CAPIN-01 offline coverage was refreshed: malformed external-superclass LOAD returned `6A80`, forbidden native-bridge Import INSTALL returned `6982`, stored missing Import/Class CAPs fail applet SELECT with `6F00` while Card Manager remains selectable, and unchanged JCAlgTest Descriptor-free install/select returns `9000`. Physical validation and broader Export consistency remain open.

## Hardware access
- Latest reader check: helper granted three ACLs and USB lists three `2e8a:0009 Pico2 JCVM CCID` devices. SmartcardIO connects to pcscd but blocks awaiting the list-readers reply; no PC/SC inventory is available. See `artifacts/logs/smartcardio-list-readers-strace-20261115.log`. Do not attribute this to VM/APDU behavior.
- Refresh with the approved helper and a bounded reader listing before hardware work; keep testing offline while the PC/SC reply stalls.
- Do not claim physical acceptance without reader enumeration, identity, flash and finite serial logs.
