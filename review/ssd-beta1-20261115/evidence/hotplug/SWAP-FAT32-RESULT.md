# REDX20 to PMBLUE without reboot: read-only gate passed

The user removed the successfully unmounted REDX20 and inserted PMBLUE directly without rebooting. The matching runtime, installed image and internal reference copies were reverified. A normal read-only FAT32 mount succeeded.

The 512-byte boot-sector capture exactly matches the pinned PMBLUE image and differs from REDX20's FAT16 boot sector. All eight small test/reference files matched their saved sizes and SHA256 hashes, including PMBLUE-specific content. Seven PPC-local cmp checks also passed. A raw read handle opened and closed while the filesystem remained mounted, and filesystem reads continued successfully.

No USB writes ran. The FTP session closed before unmount; umount completed, only root remains mounted, and diagnostics contain no recorded storage, release or skipped-invalidation warnings. The 512-byte diagnostic capture was stored on internal SD. The complete 34 MiB filler was not reread in this gate. These results demonstrate media rebinding and cache isolation, not full-device addressing or write qualification.

Evidence: swap-fat32-hardware/result.json, command logs, boot-sector.bin and eight file captures. Next is one reverse swap to REDX20, followed by bounded writes if read-only checks pass, then independent media verification. Kernel and firmware remain unchanged.
