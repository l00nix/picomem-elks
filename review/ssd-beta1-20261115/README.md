# Minimal PicoMEM SSD — Beta 1 review candidate

This snapshot supersedes the partition-aware 8 November experiment. It is an experimental review candidate, developed with AI assistance and reviewed/tested locally. Upstream approval is pending. The separate utility downloads and historical nine-media results do not qualify this implementation.

## Review order and patch boundaries

1. **Core ELKS backend:** `ssd-picomem.c` and configuration/build/initialization wiring. Uses the existing SSD framework and whole-device `/dev/ssd`; no new VFS filesystem or partition scanner. The core can be assessed using native MINIX independently of the FAT corrections.
2. **Matching PicoMEM transport:** opt-in sector service on existing command/data ports, transfer handshake/READY correction, exclusive USB ownership and documented cache policy. Stock firmware does not provide this interface. Shared I/O changes require review beyond the new USB source file.
3. **TinyUSB dependency:** pinned MSC completion/result/actual-length and failure handling patch. This is a downstream dependency proposal, not an accepted upstream API. The earlier broad controller/recovery stack is excluded.
4. **Native filesystem and raw-device corrections:** separately identified FAT mirrors, directory high-cluster clearing, FAT32 root-parent handling, FSInfo invalidation, geometry bounds, reserved-bit preservation, and raw-offset bound. These are independently reviewable changes, not inherent requirements of the PicoMEM backend. They are included in the hardware FAT test build.
5. **Optional orderly hotplug:** separate SSD first-open/last-close hooks and firmware claim/release changes. Increment over RC1 is ELKS +55/-7 across three files and PicoMEM +29/-12 in one file, with no extra TinyUSB patch. Those are incremental counts, not the total implementation size.
6. **Optional user-space fdisk probe:** one file, +11/-6, 80 bytes larger on this build. Default `fdisk -l` additionally probes `/dev/ssd`; stock rendering, explicit operands and editing behavior remain unchanged. The prior filesystem-classifying variant is excluded.
7. **Local display shortcut:** preserved separately for the Amstrad test rig; not required for USB support.

## Device and filesystem scope

One directly attached USB mass-storage device, LUN 0, 512-byte sectors. The filesystem starts at device LBA 0 and is mounted as `/dev/ssd`. For FAT this means a sector-zero volume (superfloppy layout): its boot sector starts at LBA 0, followed by the normal reserved/FAT/directory/data structures. We deliberately prepared our test fixtures this way; arbitrary FAT-formatted USB sticks must not be assumed to use this layout. There is no MBR-partition mapping or `/dev/ssd1` support. This is a limit of this deliberately small implementation, not of FAT or USB generally. Raw-file requests beyond 64 MiB are rejected to avoid the existing 16-bit block-number wrap; this is a raw-access limit, not a 64 MiB filesystem or physical-medium capacity limit. The native filesystem implementation remains responsible for filenames; no new long-name creation/rename feature is added.

For a qualified flat FAT fixture and an existing empty mount directory:

```sh
mount -t msdos /dev/ssd /mnt/usb
# Finish file operations, close files, and leave the mount directory before:
cd /
sync
umount /mnt/usb
```

Stock fdisk may print empty or meaningless partition entries on flat filesystems. Its printed `/dev/ssd1` through `/dev/ssd4` labels are not proof of real partitions or working device nodes. The probe-only patch intentionally preserves that stock behavior.

Internal SD image boot remains on the existing BIOS path. There is no new native driver for the SD card's outer filesystem. Hubs, multiple devices/LUNs, exFAT and automatic mounting are outside scope.

## Orderly removal and failure limits

Booting without a USB stick, inserting one later, and replacing it after successful unmount and closing raw-device handles are supported by the hotplug extension. A claimed-device removal or uncertain transfer latches a fault until a full power cycle. It is not recovery from surprise removal. Separately, a missing ISA data-ready event can fail-stop with READY held, also requiring a full physical power cycle; this is stronger than an ordinary USB fault latch.

ELKS's existing release/fsync interface does not propagate a storage status to umount. A successful umount alone is therefore not evidence of persistence; the test records also check diagnostics and independently inspect the medium. Known enabled device caching requires flushes. Header-only cache replies use an assumed-write-through compatibility policy, which does not prove that caching is disabled. Power-loss safety is not qualified.

## Evidence and unresolved findings

- Earlier revisions passed native MINIX writes, including indirect-block allocation/reclamation, and Windows–ELKS FAT16/FAT32 transfers. Identify those revisions separately from current-pair evidence.
- The current hotplug pair passed empty-port attempts, late insertion, same-device reinsertion and FAT16/FAT32 swaps without reboot.
- REDX20: an 8 MiB flat FAT16 fixture on a roughly 256 MB device; bounded writes and independent full-drive, file, FAT-table, allocation and guard comparisons passed.
- PMBLUE: a 48 MiB flat FAT32 fixture on a 16 GB device; the complete filesystem and 34 MiB filler, FAT tables, allocation, free-space hints and guard passed independent verification. This is not a full 16 GB media test.
- One initial FAT32 mkdir lost its network response and has no captured exit status. A separately recorded continuation and final integrity checks passed. Do not describe that run as uninterrupted success.
- A later read-only df completed in 179 seconds with the expected count. The packet trace did not capture completion, so it neither proves continuous network responsiveness nor resolves the earlier interruption. Separate telnet session failures remain unexplained.
- The probe-only fdisk's live default output matches the concatenated stock hard-disk and USB tables byte-for-byte. A logging assertion concerning root free-space changes failed; the original record is retained and the saved output was validated independently.

## Reproduction and source provenance

The [RC1 patch catalog](rc1/patch-catalog.json), [dependency lock](rc1/dependencies.lock.json) and [two hotplug deltas](hotplug/source-delta.json) define the production sources. The [optional fdisk patch](fdisk/fdisk-probe.patch) is independent. Follow [BUILD.md](BUILD.md) to reconstruct the current candidate. [Current hardware status](evidence/hotplug/HARDWARE-STATUS.md) and [known issues](evidence/hotplug/KNOWN-ISSUES.md) supersede the frozen RC1 offline status; the dated historical reports remain unchanged.

This is a source-review publication, not a new binary release. Device images, full-flash backups, credentials, personal file contents and hardware-control scripts are excluded. Historical evidence can reference retained local artifacts; those references are not build dependencies. Host runners use mocks and do not reproduce USB electrical timing. The source-argument adaptations in hotplug/qa change only test input paths; production patches are unchanged.

ELKS base: `a872443c7303d4572b7af27c55f8fc3f3dfe8175`.
PicoMEM base: `be7b2e6031ae93bb75733c363489aa9171a7205c`.
TinyUSB base: `331c263400dd5ccf77264cab1fa6bbd9f374b44e`.

Installed hotplug kernel SHA256: `ae68c867c964ba0a895a5be7bc2d12a43e698551f11a8a67dbf9ac95919a2b52`.
Installed enabled firmware UF2 SHA256: `ab48031e32ecdc944f4a4f85876d62a8eb04eecd52da88d353e11e407000cf0d`.

These identify the tested pair, not a claim that all future rebuilds are byte-identical. Do not combine it with earlier mailbox/file-service binaries. Retain recovery copies before any test deployment.
