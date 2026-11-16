# Same-device removal and reinsertion: read-only gate passed

The user confirmed REDX20 was removed after successful unmount, then reconnected directly without rebooting or mounting it manually. The matching kernel/runtime and internal reference copies were reverified. The normal read-only FAT16 mount succeeded.

All seven complete test/reference file captures matched the saved hashes and sizes, and six PPC-local cmp checks passed. No USB writes were attempted. FTP was closed before unmount. Unmount completed, only root remains mounted, and diagnostics contain no recorded storage, release or skipped-cache-invalidation warnings.

Evidence: reconnect-hardware/result.json, individual command logs and seven captures. This qualifies one orderly same-device reconnection. Different-device swapping and hotplug-candidate write qualification remain pending. It does not qualify removal while mounted or recovery from an uncertain transport fault. No kernel, firmware or source changes were made.
