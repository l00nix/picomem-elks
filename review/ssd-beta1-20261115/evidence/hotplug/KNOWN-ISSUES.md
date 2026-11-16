# Known issues and scope

## FAT32 first-allocation connection loss

The first FAT32 mkdir lost its telnet response with OS socket errno60 while waiting for its exit marker. Fresh telnet and FTP checks were temporarily unreachable. The user reported a responsive console without rebooting or unplugging the drive. Reconnection showed no mkdir process, no recorded storage errors and the new directory present and empty. Sync, all reference checks, a separately reviewed continuation, and independent full-filesystem verification subsequently passed.

The original result remains failed/incomplete and its mkdir exit status is unknown. The cause is unresolved. A slow first allocation is plausible: the previous RC1 FAT32 mkdir took about128 seconds, but timing alone does not prove this caused the lost connection. Do not treat a network timeout as permission to replay a mutation, unplug a mounted drive or reboot while write state is uncertain.

The test helper now preserves partial replies on exceptions. This changes evidence collection only; it is not a production-network or storage fix. No kernel or firmware change was made in response to the interruption.

## Read-only scan timing follow-up

A single plain `df` on read-only PMBLUE completed in 179 seconds by PPC-local timestamps and returned the expected free-space count. Its telnet session returned successfully after 183.681 seconds. This confirms a slow native FAT scan, but does not resolve the earlier allocation-time connection loss. The passive capture lacks completion traffic and is explicitly incomplete; it cannot establish network behavior throughout the scan. No USB writes or production changes were made. See df-timing-hardware/RESULT.md.

## Supported experimental scope

One direct USB storage device and the tested flat FAT16/FAT32 filesystem layouts, using the native ELKS SSD and filesystem paths. Orderly hotplug means successful unmount, closure of all raw handles, then removal and insertion. Automatic mounting, hubs, multiple active devices, partition mapping and new long-filename creation are outside this candidate's scope.

Surprise removal and uncertain transfers remain fault-latched until power cycle. Release/fsync do not return a storage status through umount, so successful unmount alone is not a persistence guarantee; captured diagnostics and independent media checks were used. Power-loss recovery and full physical-capacity addressing have not been qualified.

The hotplug increment is net48 ELKS lines in three files and net17 PicoMEM lines in one file; no additional TinyUSB patch. Existing RC1 dependencies and fixes are still required. The kernel adds72 image bytes. No further production changes were introduced during these hardware tests.
