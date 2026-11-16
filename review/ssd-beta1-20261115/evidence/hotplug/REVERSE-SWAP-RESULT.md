# PMBLUE to REDX20: read-only reverse swap passed

Without rebooting, the user replaced the cleanly unmounted PMBLUE with REDX20. The FAT16 boot sector matched the saved full-drive baseline and differed from PMBLUE's FAT32 boot sector. All seven test/reference files and six PPC-local comparisons matched. Unmount completed with root only remaining and no recorded storage, release or cache-invalidation warnings. Independent QA passed.

This closes the planned read-only orderly hotplug sequence: empty boot, absent-media retries, late insertion, same-device reconnect, and different-device swaps in both directions. It does not qualify surprise removal, full physical capacity addressing, or writes. The separately reviewed bounded FAT16 write gate is next on this same attached drive.

Evidence: reverse-fat16-hardware/result.json and captures; qa/reverse-fat16-audit.json. Production code and binaries remain unchanged.
