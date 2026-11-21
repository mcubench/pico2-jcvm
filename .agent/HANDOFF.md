# Handoff

Updated 2026-11-21. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Last observed physical state

- No campaign was recorded live after the 2026-11-21 T8 completion. Check
  live processes and USB identities again before hardware or PC/SC work.
- 07E7 retained its campaign state and later exhausted storage during a
  broad API probe. Temporary probe removal failed. Use EE11 for development
  while it remains usable. Confirm identities and exact reader names at use.
- F42D was restored to a normal max_crypto image after reclaim fault injection,
  with about 83 KB of 98.3 KB used. Its last recorded reader suffix was
  `02 00`. Use the [QKB roster](QKB.md#physical-token-roster) for full chip IDs.
- EE11 runs normal max_crypto from `90156a6b`. The symmetric KAT rerun
  panicked during probe cleanup. Package deletion and current accessibility
  are unverified. Its reader at test time ended in `00 00`.

## Next work

- Await Windows option 1/2 results for `v0.2-rc1` under
  WIN-MAXCRYPTO-RC-V0.2-RC1. The tag is local. Publish only with user authorization.
- Verify CAPIN-07 LOAD rejection and recovery on hardware. Preserve the SIO
  SensitiveResult regression when revisiting the full-record bound in
  `resolveClassref`. Import-dependent virtual method linkage still runs at
  INSTALL so packages can load before their dependencies.
- Validate the RAM-pressure collector from `20261121-2154-ram-pressure-reclaim`
  on an empty normal max_crypto EE11 image when the required host access path
  is available. Run the full symmetric KAT through cleanup with serial and
  flash telemetry. If the cutoff recurs, use `PICO2_DEBUG_HEAP_GC=1` for the
  allocation and collection samples. RSA/RSA-CRT 4096 physical runs remain
  deferred at user request.
- XDH is waiting on the named-parameter key ABI inputs
  (CRYPTO-XDH-ABI-20261121).
