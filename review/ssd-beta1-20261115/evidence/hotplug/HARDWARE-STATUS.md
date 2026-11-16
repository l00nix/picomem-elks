# Beta 1 hotplug hardware evidence — 15 November 2026

The tested kernel and firmware are unchanged from the sealed hotplug source-review archive. USB is unmounted on the PPC; PMBLUE remains directly attached to PicoMEM and safely unmounted. No additional hardware action is currently required for evidence consolidation. Nothing has been published.

| Gate | Result | Evidence |
| --- | --- | --- |
| Cold boot without USB, matching kernel | Passed | empty-hardware/ |
| Two absent-media mounts | Expected ENODATA, root only | qa/empty-hardware-audit.json |
| REDX20 insertion without reboot | Seven files, six local comparisons, clean release | qa/late-insert-audit.json |
| REDX20 orderly removal/reinsertion | Passed read-only | qa/reconnect-audit.json |
| REDX20 to PMBLUE swap | FAT32 boot sector, eight small files, seven local comparisons | qa/swap-fat32-audit.json |
| PMBLUE to REDX20 swap | FAT16 boot sector, seven files, six local comparisons | qa/reverse-fat16-audit.json |
| FAT16 bounded writes/release/remount | Passed | qa/fat16-write-hardware-audit.json |
| FAT16 independent image | Ten files, both FATs, allocation, full 252,182,528-byte device comparison | qa/fat16-final-image-audit.json |
| Initial FAT32 write run | Interrupted at first mkdir; exit unknown | qa/fat32-write-failure-audit.json |
| FAT32 recovery checkpoint | Existing empty directory synced; eight references exact | qa/fat32-continuation-review.json |
| Separate FAT32 continuation | Remaining operations and nine small files after remount passed | qa/fat32-continuation-hardware-audit.json |
| FAT32 independent image | Twelve full files, including 34 MiB filler; FATs, allocation, FSInfo, guard passed | qa/fat32-final-image-audit.json |

The FAT16 return capture is b812813ccf0b949db611db2bcb072dee71ca36288ed560c9eb7c49588fe0db5e. The FAT32 filesystem-plus-guard capture is 4e2011977ba0e75574f9e32cabec55aeab290f7b9b60adea718d3293136a717b (50,348,032 bytes). The latter covers the 48 MiB fixture plus its 16 KiB guard, not the entire 16 GB medium.

Both filesystems retain only the intended new hp1115 directory and 28-byte proof, adding two allocated clusters. Existing data, directory entries and allocation chains are preserved. Freed temporary clusters retain expected stale data; reclamation is not secure erasure.

The read-only lifecycle and final media-integrity gates passed. The original FAT32 connection loss remains a known issue, not a corrected defect or uninterrupted pass. Independent integrity success does not establish its cause. Further review should separate native filesystem/network responsiveness from USB ownership and media integrity.

## Read-only df timing follow-up

PMBLUE was reconnected without reboot, mounted read-only and checked against three reference hashes. One plain df completed in 179 seconds locally; the remote session returned exit 0 after 183.681 seconds, with the expected 49,152/13,475/35,677 FAT row. Storage diagnostics remained clear and USB is safely unmounted. Packet capture lacks completion traffic; network timing coverage is incomplete and the original mkdir root cause remains unresolved. No USB writes, kernel/firmware changes or reboot occurred. See df-timing-hardware/RESULT.md.
