# JCAlgTest host client 1.8.5

User supplied `build/AlgTestJClient_v1.8.5.jar` on 2026-11-12; moved unchanged
into `client/AlgTestJClient_v1.8.5.jar` (164953 bytes).
SHA-256: `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.

Use this host client for new full JCAlgTest support and performance campaigns
against the unchanged 1.8.4/jc305 CAP. Record client 1.8.5 and CAP 1.8.4
separately in results; old results retain their original client identity.
`client/lib` links to the preserved 1.8.4 distribution dependencies referenced
by the new JAR manifest. Runtime compatibility has not yet been tested.

Scripts and the fixture manifest intentionally retain their previous defaults.
Workers must explicitly select this JAR with `tools/run-jcalgtest --client-jar`
or a supported equivalent; see `tests/README.md`.
This JAR contains `algtestjclient.AlgTestJClient`, but does not contain
`algtestjclient.ApiTestJClient`. Keep the independent Java Card API suite on
its own supplied client.
