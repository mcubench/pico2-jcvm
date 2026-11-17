# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- LONGOP-01 T3 D1 implementation and focused physical criterion passed on F42DD9D07D6D2B07. Exact RSA-1984 PKCS1 ×50 APDU returned `AA9000` in 48.521 s; serial captured 47/47 CCID progress extensions. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T162912Z/F42DD9D07D6D2B07/`.
- The earlier “no boards” report meant no attached board was running JCVM firmware. QKB documents identification from BOOTSEL/USB layers and full `cycle choupi` recovery. F42D was subsequently flashed and enumerated.
- A pinned 1.8.5 client diagnostic session completed four RSA-1984 ×50 samples, then hit its 300 s bound during the next sample; it is not option-2 acceptance.
- LONGOP-01 T1/D2, full T2 reproduction, T5 audit, T6 acceptance, MEM-01 T7 live option-2, and T8 physical campaigns remain open.

## Next actions
1. Run focused unit/syntax/diff validations after documentation edits, inspect the complete staged diff and preserve unrelated work.
2. Commit this coherent T3 implementation/evidence step with its progress record and Step-Id trailer.
3. Rebuild normal ARM `max_crypto`, then refresh the approved hardware helper and PC/SC reader list before any post-commit full JCVM cycle. Capture and inspect finite serial output; record the resulting firmware identity.
4. Continue LONGOP-01 by instrumenting embedded allocator/arena high-water for RSA ×50, then audit flash-heavy DELETE paths and pursue full option-2/T8 acceptance when a board is ready.
