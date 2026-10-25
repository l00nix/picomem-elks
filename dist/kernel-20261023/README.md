# PPC512 experimental kernel build, 23 October 2026

I built and tested this with AI coding assistance. These are ELKS 8086 files,
not DOS programs or a complete SD-card image. `linux` is a kernel file for the
existing Minix root filesystem. Do not flash it as a whole disk image.

- `linux`: 122056 bytes, SHA256 `b807a1a567b647b7c4daad13e112c553bdff80b68ba2fdfd6779e3cfe2eb8d2e`.
- `pmmount`: read-only USB/SD mount/unmount helper, installed as `/bin/pmmount`.
- `pmmouse`: optional mouse capture tool, installed as `/bin/pmmouse`.
- `pmfstest`: bounded read/seek/EOF/write-open-rejection checks; sends no writes.
- `kernel.config`: exact combined PPC512 configuration.
- `system.map`: link map for segment sizing and diagnostics.
- `manifest.json`: source, configuration, executable and link-map hashes.

Both `CONFIG_MOUSE_PICOMEM` and `CONFIG_FS_PICOMEM` are enabled. The source is
ELKS `e7a56a43` plus this repository's [kernel integration](../../kernel/README.md).
No mouse enable command runs automatically. USB/SD mounts are manual and
read-only. Unmount both before opening the mouse device.

The candidate booted in QEMU and on the real PPC512. USB/SD listing, nested
lookup, reading, copying, seek/EOF and read-only checks passed. Mouse passive
validation passed; real movement/button delivery remains unvalidated.

On the test PPC, `/root/linprepf` retains the immediately preceding mouse-only
kernel; its SHA256 is `477f27738a0bc0e73ae9925b1dfea784ce7125e391621b58d03f58549fd788a8`.
The original `/bootopts` was restored byte for byte after the test reboot.
A backup kernel file alone is not a boot recovery menu.
