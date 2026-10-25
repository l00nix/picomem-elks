# Mount-reporting build, 25 October 2026

I built this optional ELKS 8086 kernel and its matching `mount` utility with
AI coding assistance. Native compilation, host tests and QEMU boot passed.
**This build is now installed and hardware-tested on the PPC512.** Both volumes
appear in `mount`, disappear when unmounted and return when remounted. File
read/seek/EOF checks passed on USB and SD.
[Hardware results and screenshot](../../docs/HARDWARE-MOUNT-20261025.md).

The filesystem now provides metadata-only `ustatfs` reporting. The updated
`mount` recognizes PicoMEM USB/SD and prints their actual mountpoints and
read-only flag. It also handles unknown filesystem types and missing device
names. Reporting sends no firmware commands. Capacity and inode totals are
unknown (-1); block size is unspecified (0). File access remains read-only.

- `linux`: 122120 bytes, SHA256 `1d044390cf48e182d03dc277c9c449dd2cafba509d353668f0b6c743ab265a3c`.
- `mount`: 5008 bytes, SHA256 `3b766f73266ec999557b36c1d1458b7459161ffa3ff2ece15bf7e97af79c1dab`.
- `pmmount`, `pmmouse`, `pmfstest`: unchanged helper binaries from 23 October.
- `kernel.config`, `system.map`, `manifest.json`: exact configuration, link map
  and artifact/source checksums.

The kernel uses 64944 bytes of near code, leaving 592 bytes below 64 KiB;
far code, data and BSS are unchanged. Tests passed: 71 filesystem scenarios,
30 mouse-driver scenarios, 524288 movement/button combinations, and nine
mount-table cases using the patched ELKS listing code. The QEMU boot listed
its Minix root and correctly rejected PicoMEM access without the hardware.
[Emulator transcript](../../docs/examples/kernel-mount-qemu-20261025.txt).

Install the matching `mount` utility before booting this kernel. The old
utility has an unchecked filesystem-name table and cannot display type 4.
Keep verified backups and recovery access. These files are not a disk image;
no automatic hardware installation is performed by the build scripts.

[Source and build instructions](../../kernel/README.md).
