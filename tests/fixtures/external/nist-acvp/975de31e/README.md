# NIST ACVP ML-KEM / ML-DSA known-answer subset

`pqc-subset.json` holds a small subset of the NIST ACVP FIPS 203 (ML-KEM) and
FIPS 204 (ML-DSA) vectors from
<https://github.com/usnistgov/ACVP-Server/tree/975de31eb83d87039ec88934fdc47d8c312b892d/gen-val/json-files>,
prompt fields merged with expected results. The SHA-256 of every source
`prompt.json` / `expectedResults.json` is recorded inside the file.

Regenerate with `tools/pqc-acvp-subset.py <dir with the five ACVP folders>
--revision <commit> -o pqc-subset.json`. HashML-DSA and `Sign_internal` on a raw
message are omitted: the Java Card 3.2 preview API cannot reach them.

Consumers: `tools/test-pqc-backend` (host, ASan/UBSan) and the PQC test applet
vectors.
