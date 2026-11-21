# Issue knowledge base

Reusable diagnoses and recovery procedures. See [HANDOFF](HANDOFF.md) for last
observations, [TODO](../TODO.md) for unfinished work and [progress](../progress.md)
for evidence. The [November incident archive](archive/2026-11-21-backlog-compaction/QKB.md)
preserves the detailed records. The [older index](QKB_INDEX.md) covers October.

## Physical token roster

Known chip IDs. Confirm current attachment and exact reader names before use.
The suffix after the chip ID can change after re-enumeration.

| Token | Chip ID |
|---|---|
| EE11 | `EE116F37C7E4A04E` |
| 07E7 | `07E74C5D7A038DEC` |
| F42D | `F42DD9D07D6D2B07` |
| 22A9 | `22A9DEAAA3D09795` |

Historical tokens: `8E464F4E2BBC0A19`, `D92E743D5F640C0F`,
`2BED26ED20556535`, `48E9A50BB405634E`.

## USB, PC/SC and BOOTSEL

- Follow [AGENTS.md](../AGENTS.md) and the [physical-token procedure](../reference/physical-token-access/SKILL.md). Every USB or PC/SC operation needs host access from the first attempt. Use finite captures and select by chip ID. Run the full `tools/cycle choupi` for JCVM validation.
- USB `2e8a:000f` is BOOTSEL. USB `2e8a:0009` is runtime and may be generic firmware. Check product/interface and USB serial before investigating PC/SC. On BOOTSEL, pin the target using the documented identity procedure because the runtime USB serial may disappear.
- Before PC/SC, check HANDOFF and live processes. The approved helper `sudo -n /usr/local/sbin/pico2-pcsc-reader` restarts shared pcscd. Use it only for a USB-visible token missing from PC/SC and only when all campaigns are idle. Driver installation can cause the same interruption. A storage-preserving cycle of the same image recovered a live-but-unregistered token after a restart.
- Wait for the exact nonempty reader name before invoking GPPro or SmartcardIO. GPPro with an empty reader redirected management APDUs to another token and disrupted its campaign. GPPro CAP inspection can initialize PC/SC too.
- After an apparently successful flash followed by picotool failure, inspect the selected token's re-enumeration and bounded runtime output before deciding whether to retry. See [verified-flash incident](archive/2026-11-21-backlog-compaction/QKB.md#picotool-aborts-after-verified-flash--identity-pinned-runtime-recovery--2026-11-14).
- For a response stall, correlate applet return, filesystem sync, pending final response, IN submit/completion and USB/PCSC state. Idle IN with a pending final response once came from a stale time-extension retry state. Reader disappearance alone cannot identify that failure.

## Campaigns and VPCD

- Verify fixtures before acceptance. Use the pinned CAP and explicit current client JAR from AGENTS.md. Trust the JAR SHA-256 over the CSV version. Record image hash, reader, chip ID and serial telemetry per token. Normal empty-card images are required for acceptance. A development cycle preserves storage.
- Give each campaign a dedicated working directory and card name. Stale resume lists can silently skip methods. For an intentional resume, reuse the same card name and archived resume list, omit `-fresh`, and answer the actual resume prompt. Route reader selection by prompt.
- Direct VPCD replay needs an allowed localhost socket. PC/SC-backed VPCD can show a connected emulator while GPPro exits before sending an APDU. Preserve GPPro stderr and emulator logs, use direct VPCD where it answers the question, and defer any shared service restart while physical campaigns run.
- Deletion runs require the RP2350-overlay emulator. The runner rebuilds it and captures identity. Other VPCD suites can overwrite the shared PC build with a different profile, so build the suite's required profile first. Use a fresh emulator image and registry together, or deliberately resume both.
- The old RSA-3072 option-1 panic combined cached-key handle pressure and deletion-pass allocation failure. Smaller handles and a Java allocation reserve reduced pressure (`a1639b68`), and safe reclaimer deferral fixed the panic (`9228c583`). MEM-01 T7/T8/T10 evidence is indexed in the [archive](archive/2026-11-21-backlog-compaction/README.md). Use the focused same-prefix replay before rerunning a full campaign.

## Persistence and reclamation

- Distinguish live records, tombstones and stale indexes. Measure retained RAM, live handles, overlay occupancy and flash traffic independently. Low-watermark compaction must consider the pre-sweep free space and applet-requested deletion too.
- Unreadable roots, ambiguous aliases and invalid index structure must fail closed. A valid-looking body can belong to the wrong parent. Verify canonical tag hashes and trace reachable aliases through their bodies, package statics and foreign edges. A conservative partial RAM sweep is distinct from persistent erasure.
- Defer persistent reclamation during active transactions. Recoverable post-APDU reclamation failure can defer cleanup. Persistent-write exhaustion has a no-retry policy (MEM-01 T10) to avoid replaying applet effects.
- Preserve durable identity and rollback journal state on failure. Use fake flash for repeatable cut points and physical power cuts for acceptance. A successful storage-preserving firmware cycle proves a narrower recovery case.

## Build and API troubleshooting

- Follow the warning-free ARM flash gate. The pinned Rust OS and jobserver warnings were observed in [the digest build log](../artifacts/logs/build-arm-digest-streaming.log). Check the current wrapper output before treating the recorded blocker as current.
- Append new `NativeImplementation` methods to preserve existing tokens. Re-run the transaction/API suite after ROM API changes. Preserve CRLF or mixed line endings in source edits and inspect diff size.
- When a Java Card API call reaches the wrong operation, check the uploaded JC305 token order against ROM exports, including inherited Key methods. Use the full native descriptor for overloads.
- On physical crypto preflight, include explicit Le where required by the probe. For JCAlgTest AES-512 ECB, `01 90 00` at prepare is correct. The benchmark supplies an XTS-sized key to ECB. The AES-256 control passes with the same CAP and input (`4e219e15`). Distinguish this from OAEP input length and raw RSA input range errors.
