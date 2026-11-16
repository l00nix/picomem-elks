# Hardware qualification remaining for RC1

No shutdown, reboot, firmware flash or kernel installation was performed during offline consolidation. PMBLUE was last confirmed unmounted on shop. The PPC remains on the earlier FSInfo kernel and header-cache firmware.

1. Stage the frozen candidate and a full-readback current-kernel recovery copy on the PPC. Confirm availability for physical recovery **before** installing or sending shutdown.
2. With the PPC halted and physically off, connect PicoMEM in BOOT mode. Take a fresh full-flash backup, verify the backup and frozen candidate, flash ON, and verify readback. Keep the backup paired with its old kernel. The newly built firmware is behaviorally unchanged but is a distinct artifact and needs this gate.
3. Disconnect USB-C, restore BOOT normal, attach PMBLUE directly before power-on, cold-boot from internal SD. Verify exact kernel/firmware identity, root/network/display operation, and no USB mount already active.
4. Read baseline files first. Then perform one bounded native FAT32 create/copy/append/truncate/rename/delete workload, sync, unmount, RO remount and exact file checks. Inspect diagnostics after each stage. Stop on any mismatch or storage error.
5. After successful safe unmount, move PMBLUE to shop for read-only full filesystem, filler, both FAT tables, allocation, FSInfo and guard comparison. No repair. Repeat the compact FAT16 gate on REDX20 with the same binaries and a cold boot.
6. Confirm normal manual mount/use instructions on the frozen pair and preserve results. Then prepare the public Beta 1 tag/release and a concise maintainer review note; publication requires the user's explicit instruction.

Normal manual syntax for a prepared flat FAT fixture after those gates:

```sh
mkdir -p /mnt/usb
mount -t msdos -o ro /dev/ssd /mnt/usb
ls /mnt/usb
umount /mnt/usb
mount -t msdos /dev/ssd /mnt/usb
# Work only in an isolated short-name test directory.
sync
umount /mnt/usb
```

Unmount success is necessary but does not independently prove write durability. Do not hot-swap and remount another device in the same boot. If a fault is reported, stop and preserve evidence; do not assume `sync` repaired it. MBR-partitioned sticks and `/dev/ssd1` belong to the earlier, superseded implementation and are not supported here.

Avoid deliberate power-cut/unplug fault tests on valuable media. Host fault injection covers the specified failure branches; electrical timing, power loss and explicit enabled-cache hardware are not represented as proven.
