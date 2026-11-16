# Cold boot without USB: bounded checks passed

The user confirmed a full power cycle with no USB storage. Runtime uname matches the hotplug build (15 Nov 2026 14:38:21 +0000), and the entire installed kernel matches the candidate SHA256 ae68c867c964ba0a895a5be7bc2d12a43e698551f11a8a67dbf9ac95919a2b52.

Two read-only mount attempts returned exit 1 with `mount failed: No data available`. The mountpoint was verified as a directory. After each attempt only the internal root remained mounted. Before/after diagnostics contain no recorded SSD errors or release failure. No USB data was present or written.

This verifies normal user-visible absent-media failure, not successful rebind or the absence of an internal fault latch. Late insertion and subsequent read-only file checks remain required. REDX20 identity and unmounted state were rechecked on shop before requesting the move.

Evidence: empty-hardware/result.json and capture.log. The sealed source/review archive is unchanged.
