# RP2350 flash layout and migration

The 2 MiB flash reserves `[0x000000, 0x100000)` for firmware. CAP3 keeps eight
`0x11000`-byte slots at `[0x100000, 0x188000)`. JFS2 now uses two `0x3c000`-byte
snapshot slots at `[0x188000, 0x1c4000)` and `[0x1c4000, 0x200000)`.
`src/flash_layout.hpp` owns these offsets and checks the region boundaries.
The largest serialized filesystem payload is `0x24000` bytes, so it fits the
smaller snapshot slot with its metadata sector.

On boot, the firmware first validates snapshots in the new slots. If neither
can be restored, it checks the old JFS2 slots at `0x100000` and `0x140000`,
selects the latest valid generation, and commits that RAM image to the first
new slot. The old slots are not erased during migration. If power is cut
before the new header is committed, a later boot retries from the old slot.
Once a new snapshot validates, it takes precedence over the old snapshots.
CAP3 locations and package IDs do not change.

The old JFS2 slots overlapped CAP3, so some existing cards may have no valid
snapshot left. Firmware now reports `FAULT FS snapshot unavailable; preserve
flash for recovery` and stops initialization when it sees a stored CAP3 or a
JFS2 header but cannot restore a snapshot. It does not initialize a blank
filesystem over that state. A valid old snapshot may still be stale relative
to CAP3 contents if the earlier overlap had already damaged another record;
the CRC proves only the selected snapshot's integrity.

Earlier CAP2 firmware used a separate slot beginning at `0x1c0000`. CAP3 does
not understand its metadata, and the second new JFS2 slot would overwrite its
code. If the CAP2 header magic is present, boot reports `FAULT legacy CAP2
present; preserve flash for recovery` without writing. Back up and inspect
that flash image before intentionally removing or converting CAP2 data. The
firmware does not automatically convert CAP2 package IDs or persistent tags.

Before deploying this layout to a card with valuable state, retain a full
readable flash backup and verify its hash. Do not erase the old storage area
as part of the upgrade. After deployment, require a bounded serial log with
`CHOUPI:FS_MIGRATE` or a normal restored boot and no `FAULT` or `TEST:FAIL`;
then verify installed applets and persistent data through the card interface.
The fake-flash test exercises all eight CAP slots, both JFS2 slots, migration,
and interrupted migration, but it cannot recover data already erased by the
old overlapping layout.
