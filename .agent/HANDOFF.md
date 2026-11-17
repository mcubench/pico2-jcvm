# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- LONGOP-01 T3 D1 implementation and focused physical criterion passed on F42DD9D07D6D2B07. Exact RSA-1984 PKCS1 ×50 APDU returned `AA9000` in 48.521 s; serial captured 47/47 CCID progress extensions. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T162912Z/F42DD9D07D6D2B07/`.
- Commit `2f1aac2d` rebuilt and passed full JCVM cycle on F42D. Normal `max_crypto`, object deletion ON, debug OFF, 35,904 bytes flash headroom; CPLC source ID `0x2F1AAC2DFFD54762`; UF2 SHA-256 `a13a27df22b0bf6002eef3f55beb3e82abe8b7e4cbf0ede3ed47c27686aa1fb8`. Log: `artifacts/logs/cycle-choupi-20261117T163736Z.log`.
- The earlier “no boards” report meant no attached board was running JCVM firmware. QKB documents identification from BOOTSEL/USB layers and full `cycle choupi` recovery. F42D was subsequently flashed and enumerated.
- A pinned 1.8.5 client diagnostic session completed four RSA-1984 ×50 samples, then hit its 300 s bound during the next sample; it is not option-2 acceptance.
- Current working image sampled available transient memory during RSA-1984 ×50: 119,360 bytes at entry, 82,696 minimum over 191 samples, 118,336 at return. The B043 response was `AA9000` in 48.507 s with 47/47 extensions. Aggregate periodic sampling misses sub-250 ms peaks and largest-free-block data; target RSA-1536 control remains open. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T164222Z/F42DD9D07D6D2B07/`.
- RSA-1536 private ISO14888 ×50 physical control also returned `AA9000` (1.491 s); target free space 117,688→117,432→117,688 over 3 samples, 131 live arrays / 34 instances at surrounding GC boundaries. RSA-1984 GC reclaimed 51 result arrays and ended at the same 131/34. Full paired logs: `artifacts/results/longop01-rsa1984-physical-20261117T165449Z/F42DD9D07D6D2B07/`. T1 remains open because sampling is periodic/aggregate.
- LONGOP-01 T1/D2, full T2 reproduction, T5 audit, T6 acceptance, MEM-01 T7 live option-2, and T8 physical campaigns remain open.

## Next actions
1. Improve target peak/fragmentation evidence and associate JC heap counts with each RSA profile; keep T1/D2 open meanwhile.
2. Reproduce the full Linux resume-list RSA-1984 and RSA-2048 private ×50 Cipher/Signature paths with the pinned client, retaining separate logs and serial telemetry. The prior client attempt completed four RSA-1984 samples but timed out; it is partial.
3. Audit flash-heavy DELETE paths for watchdog/progress service. Then pursue unchanged-client option-2 and MEM-01 T8 campaigns when board capacity allows.
