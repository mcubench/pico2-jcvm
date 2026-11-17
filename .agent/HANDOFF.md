# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Latest step commit `686332ec` corrects the native `ArrayLogic.arrayCopyRepackNonAtomic` path to pass `atomic=false`. ARM `max_crypto` build passed normal mode, object deletion ON, debug OFF; 36,008 bytes remain. UF2 SHA-256 `535253e22dc3088af894a4348f58fca1e86c18b82d78d2c91e1dbfc49aba14ce`.
- The full JCVM hardware cycle was not run: approved helper found 0 Pico 2 devices and elevated `./tools/list-readers --pico-only` returned `list() failed`. Refresh before any hardware work.
- T7 VPCD replay on max_crypto fails APDUs 1584/1586/1588/1590 (expected `9000`, actual `6F00`). Failing setting is unsupported Cipher algorithm 18 (`ALG_AES_BLOCK_192_CBC_NOPAD`); preserve `NO_SUCH_ALGORITHM`. In a bounded replay with `CHOUPI_VM_TRACE_EXCEPTIONS=1`, the native exception is correctly class-matched and caught by the CAP handler, then the handler rethrows at bytecode offset 15705. The final uncaught Java object has package 2 / class 102 (`SecurityException`) and APDU returns `6F00`. Trace: `artifacts/results/resource01-apdu-replay-vpcd-20261117T145515Z/emulator.log`; replay summary at `.../replay.log`.
- The trace shows `doThrow()` falls back to `SecurityException` when no handler matches and `enforce_exception_boundary` is false. Determine why the CAP rethrow is uncaught in this applet entry path before changing boundary semantics; do not equate this yet with the expected result behavior.
- Historical Nov 15 replay used a different ROM-mask hash (`184d2794…`) than current `ab55f462…`; this is not a proven same-build regression.
- Separate transaction case 89 still fails: `Util.arrayCopyNonAtomic()` tries a 32,793-byte persistent write while a 65,536-byte sector already holds 32,793 bytes; defrag leaves no replacement space. It uses the bytecode-intercepted Util path, not the just-corrected native ArrayLogic path. Trace: `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261117T144708Z.log`.
- RP2350 overlay linker roots are committed in `3fa22bc8`; current PC overlay hash before latest unrelated JNI edit was `d45561837614829be4c9a8814b61b434b7dd6b9fd5dbe2a34d4c7a122101d3c3`.

## Next actions
1. Explain CAP method and entry/caller stack at the synthetic rethrow (method offset 15705) and why no outer handler catches it; replay the same prefix with narrow tracing after any runtime fix.
2. Analyze the independent persistent Util non-atomic write exhaustion against RV-20261106-50 without weakening semantics.
3. Refresh the approved helper and reader list, then run a full `./tools/cycle choupi --profile max_crypto --chipid ID` only when an identified board is available.
4. Restart unchanged-client option 2 from fresh state when PC/SC works; physical options 2 and 1 remain acceptance gates. Host/token troubleshooting and external Windows action are tracked in `.agent/EXT_REQUESTS.md` and `.agent/QKB.md`.
