# Generated-artifact history cleanup — 2026-11-05

The user authorized removing generated artifacts from history in the existing
working repository and updating current document commit references. GitHub
rejected the original history because a diagnostic log contained a
260,631,857-byte blob. The remote had no main branch before this cleanup.

All 2,038 original commits were retained, including commits made empty by
artifact removal. The full old-to-new mapping is
[2026-11-05-commit-map.tsv](2026-11-05-commit-map.tsv). Current documents had
1,364 hash references translated across eight files, without ambiguous
prefixes. Archived documents retain their original references; consult the
mapping. External repository hashes and artifact checksums were preserved.
Historical versions of documents were not edited merely to translate hashes.

Removed from history and future tracking:

- artifacts/logs/, artifacts/results/, artifacts/metrics/, artifacts/backups/
- firmware UF2 images under artifacts/firmware/
- review logs, generated probe/sanitizer JSON, and the iteration2 output folder
- .bak backups and Python bytecode/cache paths

The rewrite removed 602 tracked paths at the tip. Every one of the 484 retained
entries (including submodule entries) had exactly its original mode and object
ID. The largest remaining historical blob was progress.md, 2,064,891 bytes.
All 1,083 existing working files were checked byte for byte before and after
the branch/index switch. Local generated artifacts remain on disk, ignored.
Unrelated worktree and submodule changes were not committed by this cleanup.

The packaging reference CSV and a human-written comparison report were
preserved under reference/jcalgtest/. Both Windows packaging scripts now use
the tracked reference CSV. Firmware READMEs and support CSVs remain tracked;
UF2 files can be regenerated with repository wrappers or distributed as release
assets. No release was published as part of this cleanup.

Recovery data is kept locally in the ignored directory
artifacts/backups/history-cleanup-20261105/. It contains a complete verified
before.bundle, working/index patches, file checksums, filter rules, validation
results, and the temporary rewrite repository. The original HEAD and hashes in
these recovery files deliberately use pre-cleanup IDs. The bundle SHA-256 is
7c723dc6d09d9271df0e6e837ac37b4134d112a9e73f0a5f70954d2b20e15a6d.
The pinned upstream git-filter-repo v2.47.0 script SHA-256 is
67447413e273fc76809289111748870b6f6072f08b17efe94863a92d810b7d94.

Validation: complete bundle verification; filtered-repository fsck (no errors;
dangling objects from the shared source are expected); commit counts; exact
retained-tree comparison; historical size scan; working-file checksums; ignore
checks; packaging script syntax and CSV integrity checks. Firmware builds,
physical tests, and full Windows package builds were intentionally skipped:
no firmware source changed, and the packaging change only relocates an input.

Continue normal development in the existing folder. Do not merge old local
history back into main or push recovery refs/bundles. Original objects can
remain locally in reflogs; they are not reachable from cleaned main and are
not sent by an ordinary main push. Existing submodule repositories were not
rewritten.
