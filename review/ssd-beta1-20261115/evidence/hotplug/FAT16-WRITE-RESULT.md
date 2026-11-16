# Hotplug candidate FAT16 writes: PPC gate passed

After the read-only reverse swap from PMBLUE to REDX20 passed independent QA, the unchanged matching kernel/firmware pair ran one bounded native FAT16 workload in the previously absent hp1115 directory. No reboot or further USB swap occurred.

Create/subdirectory, 40,997-byte copy, append, truncate, rename, file delete, directory delete and a new proof file passed, with diagnostics checked between mutations. Intermediate readbacks matched exactly. FTP sessions closed before unmount. Sync, release and subsequent read-only mount completed without recorded storage or cache-invalidation errors.

All seven original test/reference files matched after remount, including six PPC-local comparisons, and hp1115/proof.txt matched the expected 28 bytes. The temporary file, renamed file and subdirectory are absent. Final unmount completed; only the internal root remains mounted. Fourteen captures are preserved.

This is PPC-side evidence, not yet independent media qualification. The next gate is a read-only full-drive capture on shop, checking the eight expected test files plus the two existing Windows metadata files, both FAT tables, allocations, boot region, 16 KiB guard and remaining physical tail against the saved pre-test baseline. No repair or formatting is planned.

Evidence: fat16-write-hardware/result.json, expected-files-after.json, individual command logs and captures; reverse-fat16-hardware and QA audits. Production sources/binaries and sealed release-review archive remain unchanged.
