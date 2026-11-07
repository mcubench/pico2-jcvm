# External validation suites and test layout migration

User authorization: 7 November 2026. The active acceptance baseline is the
unchanged supplied JCAlgTest 1.8.4 / jc305 CAP and matching versioned client.
The Java Card API suite is independent and retains its different API client.

## Input inventory

`tests/fixtures/manifest.json` records 77 supplied files, original locations,
byte counts and SHA-256 hashes. These include all 48 API CAPs across versions
221/222/303/304/305, both clients with their relative library classpaths,
provided API runner/GP tool, and explicitly legacy 1.8.2 CAP/reference data.
The ingestion snapshot `provided-20261107` is not an asserted upstream release.
No external CAP/JAR/runner bytes were changed or downloaded.

JCAlgTest 1.8.4 CAP SHA-256:
`cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`.
The versioned client and the API client have different hashes; they are not
interchangeable. Old source305 and historical 1.8.2 output are not asserted to
match the new release. Supplied SDK/Ant dependencies are provisioned under
`.local-toolchain/java-card/provided/` and remain outside versioned fixtures.

## Migration steps

1. Supplied inputs copied byte-for-byte into the versioned fixture catalog;
   resolver and integrity checks added. `tools/fixture-path --verify` passes
   for 77 files; the catalog tests exercise tampering/path-escape rejection.
2. Test implementations move from tools/tests to tests/unit, integration,
   hardware and fixtures/local. Stable tools commands remain entrypoints.
   Active scripts/instructions switch to the pinned 1.8.4 input. Validation
   and final inventory are recorded as this step completes.
3. Preserve the entire original supplied directory in an ignored backup,
   retire active old-path dependencies, and validate moved wrappers and the
   new applet/client baseline. Historical results keep their original version.

No firmware runtime or cryptographic implementation is changed by this migration.
