# Optional PicoMEM kernel extensions

I am developing this experimental ELKS integration as l00nix, with AI coding
assistance. It combines the optional mouse driver with mounted USB/SD file
access and an **experimental, opt-in USB write mode** through PicoMEM's embedded DFS service. It does not
implement a USB host controller or expose either card as a raw block device.

The source baseline is ELKS `e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0`.
The first hardware profile is the PPC512's PicoMEM 2, board 11, D000 ROM dated
2026-06-16, primary IRQ7, NE2000 at 300h/IRQ5, USB enabled, SB/joystick disabled.
Unknown profiles fail before sending a command. Optional settings default off:

- `CONFIG_MOUSE_PICOMEM`: interrupt-driven mouse capture; opening is passive.
- `CONFIG_FS_PICOMEM`: the `picofs` filesystem and explicit mount callout.
- `CONFIG_FS_PICOMEM_WRITE`: optional USB writes; SD writes are always rejected.

The saved PPC test configuration enables all three. Configuration-menu defaults
remain off.

The filesystem and mouse capture are mutually exclusive. Unmount **both**
volumes before opening the mouse device. This deliberately leaves concurrent
mouse/filesystem operation for later validation. Mouse motion/button delivery
on the earlier Logitech receiver remains unvalidated; including the driver
in a kernel does not resolve the receiver's connection problem.

## Build locally or on a remote Linux machine

Use an isolated ELKS checkout and an already prepared ELKS IA16 toolchain/libc.
The utility repository remains the source of the additions; an ELKS GitHub
fork is not required. The remote checkout is the compilation workspace.

```sh
git clone https://github.com/ghaerr/elks.git /path/to/elks-picomem
git -C /path/to/elks-picomem checkout --detach e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0
ELKS_TREE=/path/to/elks-picomem \
ELKS_TOOLCHAIN=/path/to/prepared/elks/cross \
  bash kernel/build.sh
```

Or, with SSH keys/agent/config already working:

```sh
python3 kernel/remote-build.py build-host /home/me/picomem-build \
  /home/me/prepared-elks/cross ./kernel/build
```

The remote directory must be new or carry the wrapper's `.picomem-build`
marker. Credentials are never stored in this repository. The wrapper uploads
only kernel/mouse sources, builds remotely, and retrieves artifacts and logs.
No script here installs to the PPC or reboots it.

`apply.py` checks the exact upstream revision and preflights integration edits.
It adds separate `picofs.c` and `mouse-picomem.c` sources plus small build/VFS
hooks. `build.sh` copies the saved PPC configuration, generates configuration
before compilation, and cleans the kernel after selecting options. Override
`KERNEL_CONFIG` to build another combination. Never run it on an unrelated
working tree: configuration and generated build files are deliberately replaced.

Outputs include `linux`, the updated `mount`, `pmmount`, the mouse capture utility, configuration,
link map, and a SHA256/source manifest. The manifest checks 64 KiB segment
limits. Build timestamps mean repeated kernels need not be byte-identical.
External `buildext.sh` application packaging remains separate from this
kernel integration.

## Mounting and accessing files

After booting the optional kernel and installing `pmmount`:

```sh
mkdir -p /mnt/usb /mnt/sd
pmmount usb /mnt/usb
ls -l /mnt/usb
pmmount sd /mnt/sd
ls -l /mnt/sd
# Read/copy an explicitly chosen file, using its displayed short name.
# Unmount from outside either mounted directory:
pmmount -u /mnt/usb
pmmount -u /mnt/sd
```

These are real VFS mounts usable by ordinary `ls`, `cat`, `cp` and `lseek`.
The helper selects the experimental filesystem number 4 and defaults to read-only
mounts. `pmmount --rw usb /mnt/usb` explicitly requests USB writes when compiled
in; unmount an existing USB mount first. `--rw sd` is rejected. Existing ELKS `mount` binaries do not recognize `-t picofs`. `umount`
by mountpoint also works. The updated `mount` lists the actual mountpoints as `picomem-usb (picofs, ro)`
and `picomem-sd (picofs, ro)`. Install the bundled `mount` before booting this
kernel: older versions index a fixed filesystem-name table without bounds checks.
Listing uses kernel metadata only and sends no PicoMEM commands, including after
a transport fault. Capacity/free-space and inode totals remain unavailable
(reported as -1, with block size 0); they are not zero-capacity media. The stock
`df` is not extended here and may print `(null)` / `[Not a MINIX filesystem]` for
these device-less filesystems. Use the updated `mount` to inspect mountpoints.
The unnamed device identifiers 0:1 and 0:2 are experimental local reservations,
not assigned upstream device numbers.

Filenames use firmware-provided 8.3 aliases; lookup is case-insensitive. Each
mount holds at most 64 nodes, including its root. Directory contents are
snapshotted lazily and remain cached until unmount; exceeding the bound returns
an error and discards that directory's partial snapshot. Paths are limited to
127 characters. Files larger than 2 GiB minus one byte are rejected. Reads use
256-byte firmware transfers, at most 1024 bytes per ELKS read syscall. Short
reads are supported by returning fewer bytes per syscall, not by fabricating
EOF when firmware unexpectedly fails to return the requested data.

Permissions are root-only: read-only directories 0500, files 0400; writable USB
entries add owner-write permission unless FAT marks them read-only. File
timestamps are currently reported as zero. The optional write implementation
supports create, overwrite, append, truncate-to-empty via `O_TRUNC`, unlink,
mkdir and empty-directory rmdir. Rename, hard links, sparse writes, general
truncate, persistent permission changes, raw-sector access, disk-image mounts
and USB-reset commands are not implemented.
An open file is closed before its ELKS read syscall returns, except where an
ambiguous transport failure makes issuing another command unsafe.

The SD volume is the card's outer filesystem, distinct from ELKS's Minix root
inside its configured disk image. Reading an actively changing disk-image file
would not produce a consistent backup. Do not use this prototype for live image
backups or read/publish WiFi credentials while collecting support evidence.

## Ownership, failures and current limits

PicoMEM has one shared directory cursor; its FINDNEXT ignores the supplied
position. A complete bounded directory scan runs without sleeping or releasing
the filesystem's ownership flag, so USB/SD scans cannot mix cursors. File reads
likewise finish open/read/close before permitting another filesystem operation.
Each firmware transaction saves interrupt state, checks the profile and all
768 configuration bytes, preserves shared disk scratch, and restores scratch
only after a confirmed completion. ELKS BIOS disk I/O cannot interleave inside
that critical span. Interrupts are restored between firmware transactions.

Polling is bounded by 262144 status reads, not a calibrated time deadline.
IRQ-off latency, concurrent networking and boot-disk traffic require real
hardware validation. Do not run raw PicoMEM management tools, DOS redirectors,
or an external shared-memory client while mounted. Do not hot-remove either
card. The current profile is real-mode 8086, not protected-mode ELKS.

An ambiguous timeout or malformed response permanently disables this kernel's
file service and prevents mouse activation until reboot. No automatic reset or
retry is sent. A read may return bytes already delivered before a later error;
subsequent reads report the fault. The installed firmware must be examined
before expanding the supported profile.

Firmware uses the same result for end-of-directory and directory errors. A
zero-entry root is rejected because presence cannot be established. An empty
subdirectory or an end after some entries cannot prove error-free enumeration.
A card changed externally while mounted invalidates the snapshot assumptions;
remount to refresh. Equal-size file replacements cannot be detected reliably.

## Tests

`sh kernel/test.sh` runs the read-only, USB-write and mouse suites; the build script also runs them before
applying or compiling changes. Individual commands:

```sh
cc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror \
  -fsanitize=address,undefined kernel/tests/test-picofs.c -o /tmp/test-picofs
/tmp/test-picofs
cc -std=c99 -Wall -Wextra -Werror -Wno-sign-compare \
  -fsanitize=address,undefined mouse/tests/test-driver.c -o /tmp/test-pmmouse
/tmp/test-pmmouse
```

The filesystem harness covers 71 read-only scenarios in both build modes, plus
16 USB-write scenarios. The build also tests the patched
ELKS `mount` listing with nine synthetic mount-table cases, including unavailable
slots, unknown types, missing block-device names and non-default mountpoints.

The harness runs the actual filesystem/transport source against fake firmware,
including directory cursor behavior, mount isolation, seek/read/EOF, scratch
restoration, exact-length read replies, rejection paths, cache bounds and fault
latching. Mouse tests cover filesystem exclusion and all 524288 combinations
of X/Y movement and three buttons. Synthetic tests and an emulator without
PicoMEM cannot establish working USB/SD access or mouse reports on the PPC.

The [mounted-filesystem hardware results](../docs/HARDWARE-KERNEL-FS-20261023.md)
record successful USB/SD mounts, reads/copies and unmount/remount on the PPC.

The earlier [USB directory evidence](../docs/HARDWARE-USB-FLASH-20261023.md)
is for the user-space `pmusb` client, not this mounted filesystem.

The [25 October mount-reporting hardware results](../docs/HARDWARE-MOUNT-20261025.md)
verify both volumes appearing, disappearing on unmount, and returning on remount.

[Long-filename audit and proposed firmware/ELKS extension](../docs/LONG-FILENAMES.md)
explains why the current interface cannot expose the original long names.

## Experimental USB write limits

I keep SD writes disabled because that card contains the running ELKS disk image.
USB writes use validated short names, at most 250 bytes per firmware request
and 1024 bytes per ELKS syscall. Each accepted write is closed, reopened and
compared byte-for-byte before returning success. Existing data can still be
changed before an error is detected: a reported error is not a rollback.

The audited firmware masks `f_close` failures and has an unreliable `mkdir`
status. I verify observable file content/size and directory existence instead
of trusting those responses or repeating mutations. These checks do not prove
power-loss durability, and no firmware flush guarantee is claimed. Use a USB
drive with backed-up, expendable contents for this experiment. Do not remove
it while mounted. `sync` alone is not evidence of a successful firmware flush.

Open-file deletion is refused. Created entries consume the fixed 64-node mount
cache; deleted slots are not reused until unmount. No write is issued after an
ambiguous transport failure; reboot is required to clear the fault.

`pmrwtest /mnt/usb/NEWTEST` creates a **new** directory, tests the supported
operations and leaves a 3073-byte `KEEP.BIN` for independent readback and
unmount/remount/reboot checks. It refuses an existing test directory. ELKS has
no enabled `ftruncate` syscall on this baseline; the native test uses `O_TRUNC`.
Creating new long filenames remains a separate firmware-interface task.

The [USB-write hardware results](../docs/HARDWARE-USB-RW-20261025.md) record
passing native write tests and independent readback after copy, remount and
normal reboot on the PPC512. USB is left mounted read/write; SD stays read-only.

## Experimental paired-firmware long names

The `codex/long-filename-prototype` branch adds default-off
`CONFIG_FS_PICOMEM_LFN`. Use `KERNEL_CONFIG=/absolute/path/ppc512-lfn.config`
with `build.sh`, or append `--lfn` to `remote-build.py`. This requires the
[matching firmware fork](https://github.com/l00nix/ISA-PicoMEM/tree/codex/dfs-long-names)
and an exact ROM descriptor. Stock firmware stays alias-only without queries.
Read-only and USB writable mounts can display and open existing 26-byte ASCII
names. Existing-file writes, append, truncate-to-empty and removal resolve the
displayed name to the stored short alias, including every parent directory.
New files/directories still require 8.3 names; rename is unsupported. Stock
firmware remains alias-only. The option adds a bounded 1,728-byte name table
per enabled mount. See [limits, tests and hardware status](../docs/LONG-FILENAMES.md).

Unsupported non-ASCII short aliases are omitted with a kernel diagnostic.
Other supported entries in that directory remain usable. Such a directory is
marked incomplete: creating or removing entries in it is refused, and it
cannot be removed as if empty. This is not full Unicode support; malformed
entries still fail the scan. External changes still require unmount/remount.

`pmlfnrw /mnt/usb/LFN271A/existing-name /root/backup` is a fixture-only native
probe for existing long-name append/overwrite/readback. It requires the current
file to match an independent backup (1–1024 bytes), leaves changed contents for
independent verification, and does not restore them automatically. Preserve the
backup and restore it after checking persistence. The host suite includes 14
combined LFN/write scenarios, including collision, omission, deletion-cache,
26-byte remount, SD protection and failed-write handling.

## Next candidate: long-name creation

`CONFIG_FS_PICOMEM_LFN_CREATE` is separately default-off and requires LFN and
USB-write support. Use `ppc512-create.config` or `remote-build.py --lfn-create`.
The native `pmlfnnew` probe is included in builds. Creation has passed PPC512,
Mac FAT/readback and DOS compatibility checks; see the [creation design and test procedure](../docs/LONG-NAME-CREATION.md).

## Same-directory file rename candidate

`CONFIG_FS_PICOMEM_LFN_RENAME` adds an optional direct rename syscall path,
paired with the separate firmware rename descriptor/command. Build using
`ppc512-rename.config` or remote `--lfn-rename`. `pmrename OLD NEW` performs
one rename call; `pmlfnren NEWDIR` creates fresh fixtures and checks rename
and refusals. Use `pmrename` for this candidate: ELKS `mv` may pre-delete the
destination or fall back to copy/delete.

Host/FAT-image tests and the IA16 build pass. PPC512 rename tests are pending.
See [scope, protocol and validation](../docs/LONG-NAME-RENAME.md).
