# Firmware-first handoff

The user connected PicoMEM in BOOT mode before the staged kernel was installed. The installed kernel remains RC1; no assistant shutdown or kernel installation is claimed. Independent source review approved an interim internal-SD boot with USB storage disconnected: legacy geometry is rejected, USB remains unavailable, and shared command status is released for BIOS SD use.

The previous RC1 firmware passed board readback verification. A fresh full 16 MiB recovery backup passed readback verification; its SHA-256 is 32c0f95412306e75d403d41358f62c8346a95c7f8e25b4f2a8116623533c1892. The new hotplug firmware passed flash/readback verification, SHA-256 ab48031e32ecdc944f4a4f85876d62a8eb04eecd52da88d353e11e407000cf0d. Independent QA verified contiguous backup coverage, predecessor bytes, candidate BIN/UF2 agreement, board identity and logs.

Only an interim internal-SD boot WITHOUT USB storage is cleared. The matching kernel must still be installed and read back, followed by an explicitly authorized shutdown and full power cycle with USB-C disconnected. No USB test has run on this firmware. USB service may be faulted during the interim old-kernel boot; no warm reboot substitutes for the final cold start.
