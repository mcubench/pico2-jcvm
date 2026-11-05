# JCVM project status

Updated: 2026-11-05

## Goal

Complete the RP2350 Java Card VM project in `plan.md`: run unchanged Java Card
CAP files on the Pico 2, provide native Java Card API behavior, support
GlobalPlatform installation and PC/SC use, preserve card state across power
cycles, and verify the end-to-end workflow on real hardware and Windows.

## Current state

- ARM and RISC-V wrappers build; the RP2350 ARM firmware has passed physical
  serial and PC/SC/APDU checks. The current max_crypto ARM image is an empty
  card image with no preinstalled CAP/applet.
- Native symmetric and substantial asymmetric crypto support, Java Card
  lifecycle and persistence behavior, multiple packages/applets, SIO contexts,
  firewall checks, CCID, CPLC, and GPPro LOAD/INSTALL/SELECT are implemented
  to varying levels of coverage. See `TODO.md` for evidence and limits.
- A user reports the no-startup-wait image became visible in Windows certutil
  almost instantly after replug. This is not yet a measured three-run timing
  result. Windows GPPro has separately connected, selected the ISD and read
  CPLC/Card Data; `certutil -scinfo` still lacks ATR provider metadata.
- Current guest USB refresh found two runtime devices. D92 is a PC/SC reader;
  2BED is USB-visible with a CCID interface but absent from PC/SC. The third
  expected device was not visible. This host attachment/registration issue is
  tracked separately from firmware behavior.

## Highest-priority remaining work

1. **CTX-01/03:** finish cross-applet SIO proxy lifecycle and firewall-owner
   audits, including foreign persistent references and all reference-consuming
   paths.
2. **STORE/PERSIST:** validate reported memory accounting, live/reclaimable
   capacity, and power-loss safety across complete lifecycle transitions.
3. **HOST-02 / AC-01:** test diagnostic timeout without DTR; capture three
   Windows cold-reconnect timings with no serial client and exact image/hash.
4. **API-03 / GP-01 / AC-02:** close current-image upstream CAP coverage,
   installation/selection, and cold-power persistence gates.
5. Diagnose why one USB-visible Pico is not registered by guest PC/SC and
   continue hardware validation on any registered identified reader.

## Acceptance

The original plan's end-to-end criteria are **not all verified**. Linux
GPPro/PCSC and selected real-board tests pass; Windows basic GPPro/APDU use is
reported working. Complete upstream CAP/client, both packaged profiles,
Windows cold-reconnect timing, and full cold-power persistence evidence remain
open. Do not treat the implementation list or a passing heartbeat as project
completion. The authoritative detailed checklist is `TODO.md`; run-specific
handoff and known issues are in `.agent/HANDOFF.md` and `.agent/QKB.md`.
