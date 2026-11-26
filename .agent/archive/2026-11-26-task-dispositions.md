# Task dispositions — 2026-11-26

Current work and outstanding acceptance criteria live in [TODO.md](../../TODO.md).

## Completed substeps

### CAPIN-04

Verify the 64-frame boundary and following APDU on VPCD. The RMI unit suite also injects frame allocation failure and checks that caller state survives. See `20261121-2201-frame-limit-recovery`.

### CAPIN-07

Move import-independent Class and Export reference checks to LOAD. Superclass, vtable, interface index, and Export rejection passed in VPCD. A valid Descriptor-free CAP and the SIO lifecycle passed. See `20261121-2157-capin07-load-class-checks`.

### PROFILE-01

Verify the 5 KiB and multipart digest cases and per-APDU flash traffic on a normal physical image. See step `20261121-2138-ee11-streaming-digest`.

### MEM-01 T8

Linux physical option 2 passed on F42D (`73e1cede`) and option 1 passed on
07E7 (`b30dbcb8`). This closes the recorded T8 gate. Windows and newer-image
acceptance remain scoped separately.

## Historical scope and criteria

The [original port plan](../../docs/archive/plan.md) is historical. Its clean
integration gate carries forward under AC-01 and AC-02. The October
[fix checklist](../../artifacts/reviews/fixes-20261014.md) was archived by
`1e38a43e`. Its follow-ups map to the current CAPIN, GP, API, and HOST tasks.
No additional closure is inferred from that move.
