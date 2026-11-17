# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- H1/H2/H3 memory fixes are committed through `de729d28`; normal max_crypto image hash is `b4a41ad8b30c279ad67de9ced804ca974c98c321c95a8dca5580421094231134` (35,748 B flash remaining). 07E74C5D7A038DEC was confirmed empty, flashed with the matching empty-card UF2, passed `cycle choupi`, then received the unchanged 1.8.4/jc305 CAP. The current card has since had exploratory JCAlgTest traffic; reset it to an empty-card state before any acceptance rerun.
- A runner prompt-order bug was fixed: exact-name reader resolution now answers only the reader-selection prompt, and menu inputs stay in client prompt order. The `attempt3` client log confirms menu 2, reader 07E7, submenu 2. It ended early; 37 pre-cleanup samples peaked at 340 files / 9,739 B (9.91%). That does not establish a plateau or acceptance. Earlier attempts are kept separately: attempt1 stopped at the reader prompt; attempt2 answered menu 1 and briefly queried F42D by mistake; attempt3 is the correctly selected but interrupted 07E7 run.
- Latest result directory: `artifacts/results/option2-acceptance-20261117T2113Z/07E74C5D7A038DEC/`. Identity and installation evidence are at the parent; per-attempt logs are under `attempt1`/`attempt2`/`attempt3` (attempt1/2 logs are at parent and attempt2 respectively). F42D's earlier long run from `option2-acceptance-20261117T1830Z/` is separate and was interrupted; it is not acceptance.
- MEM-01 T7/T8/T10 remain open. Windows package request `WIN-MEM01-MAXCRYPTO-DE729D28` is pending. Boards are volatile; refresh the helper and reader list before hardware work.

## Next actions
1. Refresh readers and helper, reflash the matching normal empty-card UF2 to 07E7, run `cycle choupi`, reinstall the unchanged CAP, and verify client/CAP hashes before starting a fresh option-2 campaign. Use the fixed watchdog path, exact reader name, `--serial 07E74C5D7A038DEC`, and verify the selected chip ID in the client log.
2. Let the 1.8.5 `2 -> PERFORMANCE TEST -> 2` run complete or reach the 10,800 s bound; inspect result completeness, resource plateau, headroom gate and serial health. If stable and available, perform `1 -> All algorithms` on a separate identified token from its own empty-card state.
3. Reproduce the RSA-loss → GPPro `gp --list` → authenticated `gp --uninstall` sequence with timestamped serial and APDU logs; the archived 15 s Windows transmit failure has no synchronized firmware evidence.

Before hardware work, refresh the approved reader helper, list exact readers, and follow `AGENTS.md`; never infer current board state from this handoff.
