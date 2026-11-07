# JCVM project status

Updated: 2026-11-07

## Goal

Complete the RP2350 Java Card VM project in `plan.md`: run unchanged Java Card
CAP files on the Pico 2, provide native Java Card API behavior, support
GlobalPlatform installation and PC/SC use, preserve card state across power
cycles, and verify the end-to-end workflow on real hardware and Windows.

## Current state

- ARM and RISC-V max_crypto wrappers pass. The latest ARM build reports
  159/162 capabilities and ends at `0x100F0D4C`, leaving 62,132 bytes in the
  firmware window. The refreshed empty-card external-test UF2 is
  `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`
  (4,070,912 bytes; SHA-256
  `51509530b150d5c5f04dcaa23fa82becf16a0e0f2f661c0eb2f9ca165789d564`). Its
  CPLC source ID is `FF4492A9`; the dirty marker means it is not a clean release
  candidate. The package contains erased storage and no preinstalled applet.
- Native symmetric and substantial asymmetric crypto support, Java Card
  lifecycle and persistence behavior, multiple packages/applets, SIO contexts,
  firewall checks, CCID, CPLC, and GPPro LOAD/INSTALL/SELECT are implemented
  to varying levels of coverage. See `TODO.md` for evidence and limits.
- A user reports the no-startup-wait image became visible in Windows certutil
  almost instantly after replug. This is not yet a measured three-run timing
  result. Windows GPPro separately connected, selected the ISD and read
  CPLC/Card Data; `certutil -scinfo` still lacks ATR provider metadata.
- Latest authorized host-access check grants all three Pico USB nodes and
  identifies boards `8E464F4E2BBC0A19`, `D92E743D5F640C0F`, and
  `2BED26ED20556535` as runtime `2e8a:0009 Pico2 JCVM CCID` devices. None is
  currently published by `pcsc_scan -r`; no smartcard APDUs were possible.
  `pcscd.socket` is active and the service is socket-activated with `--auto-exit`.
  Starting/scanning and refreshing ACLs did not restore reader visibility.
  System journal details are unavailable to this account, so the cause is
  unknown. Treat this as a host/reader-registration blocker, not firmware
  behavior.
- The native JCSystem `CLEAR_ON_DESELECT` eligibility guard is committed as
  `4492a973`. Focused cross-CAP VPCD tests pass 2/2, same-CAP sibling SIO
  passes 6/6, full cross-AID SIO passes 35/35, and transient allocation
  boundary tests pass in int-disabled and int-enabled builds. Persistent
  inventory (22 cases), transaction/reclaimer and RP2350 overlay suites pass;
  physical multi-owner/power-loss acceptance is still open. The pinned fixture
  manifest verifies 81 files, including upstream JCAlgTest 1.8.4 and 48 API
  CAPs.

## Highest-priority remaining work

1. **CTX-01/03:** finish SIO lifecycle and owner-firewall audit; ordinary
   foreign references in object/static fields still fail closed, and native
   reference consumers/GC roots need complete owner-aware coverage.
2. **STORE/PERSIST:** measure available versus live/reclaimable storage and
   complete physical multi-owner and interrupted-write/power-loss acceptance.
3. **HOST-01/02 / AC-01:** diagnose why three USB-visible CCID devices are not
   registered by PC/SC; verify diagnostic timeout without DTR and capture three
   Windows cold-reconnect timings with no serial client and exact image hash.
4. **API-03 / GP-01 / AC-02:** close current-image upstream CAP coverage,
   physical installation/selection, and cold-power persistence gates.
5. **HMAC-01:** complete unchanged JCAlgTest 1.8.4 support acceptance; the
   simulator run currently exhausts persistent storage during RSA CRT 3072-bit
   probing, so investigate storage accumulation before another long sweep.

## Acceptance

The original plan's end-to-end criteria are **not all verified**. Historical
Linux GPPro/PCSC and selected real-board tests pass; Windows basic GPPro/APDU
use is user-reported. Current guest PC/SC access is unavailable despite USB
enumeration. Complete upstream CAP/client coverage, both packaged-profile
Windows workflows, Windows cold-reconnect timings, and full cold-power
persistence evidence remain open. Do not treat the implementation list or a
passing heartbeat as project completion. `TODO.md` is the detailed checklist;
`.agent/HANDOFF.md` and `.agent/QKB.md` hold run-specific evidence and diagnoses.
