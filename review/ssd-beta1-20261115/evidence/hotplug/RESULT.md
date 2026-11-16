# Hotplug evidence consolidated; connection-loss review remains open

The same matching hotplug kernel and firmware completed boot-empty, absent-media, late-insert, same-device reconnect and different-device swap checks without reboot. Native FAT16 writes and independent whole-device verification passed. FAT32 final data integrity passed after a separately verified continuation: all twelve files including the full filler, both FAT tables, allocation, free-space hints and guard matched expectations.

The initial FAT32 mkdir lost its network response. Its original result is preserved as failed/incomplete; the command exit and connection-loss cause remain unknown. Subsequent recovery and integrity checks do not make this an uninterrupted pass. See HARDWARE-STATUS.md and KNOWN-ISSUES.md for the review boundary.

Hotplug adds net48 ELKS lines and net17 firmware lines over frozen RC1, with no additional TinyUSB patch. Native FAT/MINIX implementations and utilities are unchanged from RC1. Kernel image117752 bytes (+72). Production code and binaries did not change during qualification. The Amstrad display shortcut remains separate and retained.

The original source-review archive remains sealed and unpublished. Its pre-install snapshot is historical; the separate hardware evidence addendum records current deployment and results. Maintainer approval and any public release decision remain pending. PMBLUE is directly attached to PicoMEM and safely unmounted, with ELKS still running.

The subsequent single read-only `df` diagnostic completed in 179 seconds locally, returned the exact expected FAT free-space count and preserved its telnet connection. Its packet trace is incomplete, so it does not establish continuous networking behavior or resolve the earlier mkdir timeout. See df-timing-hardware/RESULT.md. No production code changed.
