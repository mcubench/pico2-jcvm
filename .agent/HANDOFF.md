# Handoff — next steps only

Updated: 2026-11-15.

## Current image for external Windows testing
- ARM `max_crypto`; object deletion ON; debug/trace OFF; serial wait 0 ms; empty persistent storage.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-20261115.uf2`
- UF2 SHA-256: `83423ee4f68e7c16b88741b09a6d126cf33f2ba86d407f5b00675968a45c3f1d`
- Package: `build/external-test/pico2-jcvm-object-deletion-on-windows-d3061ddd-dirty.zip`
- ZIP SHA-256: `c1aed51f971aa17d22feafb69427e340e5014fca086d0e97b6f3b382ea94ecb3`
- External request: `WIN-OBJECT-DELETION-LOW-WATERMARK-D3061DDD` in `.agent/EXT_REQUESTS.md`.
- Build HEAD `d3061ddd8392e6d1e1712378991eb9806778d770` plus dirty source; CPLC source ID `0xD3061DDD8392E6D1`; linked end `0x100f7928`, 34,520 bytes remain. ARM build, policy tests, fake-flash overlay suite and package checks passed. No physical test was performed.

## MEM-01 status
- Safe-point automatic deletion begins at <=20 KiB free; while its request is pending and free remains low, it compacts deleted overlay records. Fake-flash scenario created/reclaimed 8,240 garbage objects, drained in 695 APDU boundaries, preserved live root, peaked at 77,852/98,304 bytes (79.2%). Log: `artifacts/logs/rp2350-fs-overlay-low-watermark-20261115.log`.
- T10 direct write-failure reclaim/retry remains open; collection cannot safely run with live Java frames. Fresh unchanged-client VPCD T7 and physical T8 acceptance remain open.

## Hardware access
- Last reader check in the previous handoff: USB devices enumerated, but SmartcardIO reader-list request timed out waiting on the PC/SC server response. Refresh the authorized helper and enumerate readers before any hardware claim or flash. Continue on another identified token if one is available.
- External Windows testing is independent evidence. Run option 2 on this image, install the unchanged 1.8.4/jc305 CAP explicitly with GPPro, use the supplied 1.8.5 client, and return per-device outputs plus serial telemetry and chip ID.
