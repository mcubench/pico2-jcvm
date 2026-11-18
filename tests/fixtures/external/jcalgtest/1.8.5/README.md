# JCAlgTest host client 1.8.5

User supplied `build/AlgTestJClient_v1.8.5.jar` on 2026-11-12; moved unchanged
into `client/AlgTestJClient_v1.8.5.jar` (164953 bytes).
SHA-256: `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.

Use this host client for new full JCAlgTest support and performance campaigns
against the 1.8.5/jc305 CAP below. Record client and CAP versions
separately in results; old results retain their original client identity.
`client/lib` links to the preserved 1.8.4 distribution dependencies referenced
by the new JAR manifest. Runtime compatibility has not yet been tested.

Scripts and the fixture manifest intentionally retain their previous defaults.
Workers must explicitly select this JAR with `tools/run-jcalgtest --client-jar`
or a supported equivalent; see `tests/README.md`.
This JAR contains `algtestjclient.AlgTestJClient`, but does not contain
`algtestjclient.ApiTestJClient`. Keep the independent Java Card API suite on
its own supplied client.

The client reports `AlgTestJClient version; 1.8.4` in generated support CSV
metadata even though this supplied JAR is the 1.8.5 client. Identify client
versions by the selected JAR and SHA-256, not by that CSV field. The pinned
1.8.5 jCardSim reference is
`tests/fixtures/reference/jcalgtest/1.8.5/jcardsim-alg-support-extended.csv`.

## CAP `AlgTest_v1.8.5_jc305.cap` (active baseline, user directive 2026-11-18)

User built and supplied `inbox/AlgTest_v1.8.5_jc305.cap` on 2026-11-18; moved
unchanged here (131,590 bytes). SHA-256:
`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`.
`./tools/fixture-path jcalgtest.cap` resolves it and GETVERSION returns
`1.8.5_jc305`. Package/applet AIDs, imports, Header, Class, ConstantPool and
Descriptor components are identical to the 1.8.4/jc305 CAP. Only
`StaticField.cap` (version strings 1.8.4 -> 1.8.5), `Method.cap` (42 bytes,
same size) and `RefLocation.cap` (10 bytes) differ. That matches the
`perftest_class_Cipher` change that clears the RSA NOPAD input's top byte
before the setup encryption, removing the random `F105` (x >= n) failures. Use
this CAP with the 1.8.5 client for all new runs; the 1.8.4 CAP is historical.
