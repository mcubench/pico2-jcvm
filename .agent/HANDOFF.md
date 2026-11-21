# Handoff — next steps only

Updated: 2026-11-20 21:15 CET.

## Current state
- **Live:** MEM-01 T8 option 1 on 07E7 (`9228c583` empty card), RESUMED 2026-11-21 09:12 CET after the first 12 h segment (1,362 methods, clean). Directory `artifacts/results/t8-option1-9228c583-20261120T2006Z/07E74C5D7A038DEC/`: segment 1 `client.log`/`serial.log`, segment 2 `client-resume.log`/`serial-resume.log`. Reader is now `... (07E74C5D7A038DEC) 00 00` (pcscd renumbered; card did not reboot). Do not touch 07E7 or restart pcscd until it ends; F42D (`... 01 00` now) is free.
- After the campaign (nothing live): restart pcscd with the approved helper (VPCD reader wedged), then run the pcscd-dependent VPCD suites against HEAD: test-sio-cross-aid-vpcd, test-vpcd-multipackage, test-same-cap-siblings, test-gppro-empty-install-vpcd (they cover CAPIN-05 invokespecial, CAPIN-07 Export/vtable checks with exporting CAPs). Use `PICO2_HOST_USB_ACCESS=1` and escalation.
- Firmware at HEAD (CAPIN-05/07 changes after 9228c583) has not been flashed; the live campaign uses 9228c583.
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- Waiting on `WIN-MAXCRYPTO-FLASH-A266B836` (`.agent/EXT_REQUESTS.md`).
- OOM T9 fix committed (`20261120-0419-oom-small-handles-reserve`); the live run is its end-to-end check.
- MEM-01 T7 **passed** 2026-11-20 (VPCD, RP2350 overlay, `a1639b68`): `artifacts/results/jcalgtest-support-vpcd-20261120T062332Z/`. T8 acceptance (options 1 and 2 on wiped tokens 07E7 and F42D) is next after the live physical run; VPCD readers `Virtual PCD 00 00/01` exist since 2026-11-20 (user install). Physical option 1 on 07E7 was resumed after a pcscd restart; its reader name is now `... (07E74C5D7A038DEC) 01 00`, logs `client-resume.log`/`serial-resume.log`.
- F42D is visible in PC/SC (`... (F42DD9D07D6D2B07) 00 00`); usable as the second token for T8.
- Priorities and the state assessment are at the top of `TODO.md`.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
