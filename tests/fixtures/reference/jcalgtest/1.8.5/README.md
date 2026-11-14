# JCAlgTest 1.8.5 jCardSim support reference

Generated with the supplied 1.8.5 host client and the unchanged 1.8.4/jc305
CAP on 2026-11-15. This is a simulator comparison reference, not Pico hardware
acceptance.

- CSV: `jcardsim-alg-support-extended.csv`
- Client JAR: `tests/fixtures/external/jcalgtest/1.8.5/client/AlgTestJClient_v1.8.5.jar`
- Client SHA-256: `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`
- CAP: unchanged upstream 1.8.4/jc305; SHA-256
  `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`
- CSV SHA-256: `8a8b7d75f77dbb77707dafdb6d100ac27c920a986c8509b8e7a6df09b6552746`
- CSV `AlgTestJClient version` field: `1.8.4` (the field does not identify the
  actual host JAR version; use the JAR hash above).

The 1.8.4-generated reference remains preserved in the sibling `1.8.4/`
directory as historical evidence. Comparing yes/no algorithm statuses between
the two simulator CSVs yields 4,406 rows each and no differences; performance
timings are expected to vary between runs.
