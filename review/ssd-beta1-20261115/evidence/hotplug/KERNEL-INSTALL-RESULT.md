# Matching kernel installed; final cold boot pending

The user confirmed the interim boot at 10.0.0.67 without USB storage and explicitly authorized kernel installation and shutdown. The running RC1 kernel, root-only mount and expected USB initialization error were captured first. No USB test or mount was attempted.

The reviewed installer verified current/staged/recovery bytes, copied the new kernel, synced, and fully read back /linux to SHA-256 ae68c867c964ba0a895a5be7bc2d12a43e698551f11a8a67dbf9ac95919a2b52. The RC1 recovery copy remained unchanged. Independent saved-evidence QA passed.

A separate root-only mount check and sync succeeded, then shutdown -s -f was sent. Telnet became unavailable. This does not establish a visible physical halt; the user must confirm the screen before power-off. The matching kernel is installed but has not yet booted. USB-C and all USB storage must remain disconnected for the final full power cycle and first empty-media test.
