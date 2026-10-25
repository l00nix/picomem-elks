# Experimental USB-write build, 25 October 2026

I built this optional ELKS 8086 kernel with AI coding assistance. Compilation,
host tests and emulator boot passed. **This build is installed and USB writes
are hardware-tested on the PPC512.** Native creation, append, overwrite,
truncate, deletion and directory tests passed. Independent byte comparisons
passed after ordinary `cp`, unmount/remount and a normal ELKS reboot.
[Hardware results and screenshot](../../docs/HARDWARE-USB-RW-20261025.md).

The new `CONFIG_FS_PICOMEM_WRITE` option permits explicit USB read/write mounts.
SD writes remain rejected. `pmmount` defaults to read-only; use
`pmmount --rw usb /mnt/usb` only after unmounting an existing USB mount.

Supported operations: exclusive file creation, overwrite, append, truncate to
empty with `O_TRUNC`, delete, mkdir and empty-directory rmdir. Each accepted
write is closed, reopened and compared. This is not a power-loss durability
guarantee: the firmware masks close errors. Long filenames, rename, sparse
writes and persistent permissions are not supported. Use backed-up test media.

- `linux`: 126768 bytes, SHA256 `28c6bc8d7970472aac51ca8089ce34870973642b0e72e01bdd0950a8a09e5dbf`.
- `pmmount`: 3488 bytes, SHA256 `b2cc502b6ad36ffbd4419162345e2af182c364cfb249a6aec5fb91a908872653`.
- `pmrwtest`: 6208 bytes, SHA256 `ade389adf5d2d694343ec36c415cf04ea6913300b350ed7c37c145f43997f299`.
- `mount`, `pmmouse`, `pmfstest`: unchanged from the preceding build.
- `manifest.json`, `kernel.config`, `system.map`: exact source/artifact hashes,
  configuration and link map.

Near code uses 65264 bytes, leaving only 272 bytes below 64 KiB. Far code uses
39968 bytes, data 10736 and BSS 10192. The build adds 4648 bytes to the kernel image.
Tests: 71 filesystem scenarios in each build mode, 16 write scenarios, 30 mouse
scenarios, 524288 movement/button combinations and nine mount-listing cases.
[QEMU transcript](../../docs/examples/kernel-usb-rw-qemu-20261025.txt).

The native tester requires a new directory, for example
`/root/pmrwtest /mnt/usb/PMRW25A`. It refuses an existing directory and leaves
`KEEP.BIN` (3073 bytes) for independent checksum and remount/reboot validation.

The verified PPC recovery copies are `/root/linprerw` (previous kernel) and
`/root/mntprerw` (previous mount helper). Kernel backup SHA256:
`1d044390cf48e182d03dc277c9c449dd2cafba509d353668f0b6c743ab265a3c`.
A backup file is not a boot recovery menu; SD-card recovery access is required
if the experimental kernel cannot boot. Boot configuration is unchanged.

[Source, remote build and limitations](../../kernel/README.md).
