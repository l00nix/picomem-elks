# Late insertion without reboot: read-only gate passed

After the cold boot without USB and two ENODATA mount failures, the user inserted REDX20 directly without rebooting. The matching running kernel and installed image were reverified. A normal read-only FAT16 mount succeeded at /mnt/fat.

All seven test/reference files match their saved SHA256 hashes and sizes. Six original files also matched preverified copies on the PPC's internal filesystem using cmp. No USB file writes were attempted. The FTP session was closed before unmount; umount completed, only root remains mounted, and captured diagnostics contain no SSD, FAT, release or cache-invalidation error.

This demonstrates successful late insertion after the absent-media attempts. Orderly removal/reinsertion, a different-device swap, and current-binary write qualification remain pending. A successful unmount alone is not a general persistence guarantee; this gate was read-only.

Evidence: late-insert-hardware/result.json, command logs and seven binary captures. Sealed review archive unchanged; no source or firmware changes were made for this gate.
