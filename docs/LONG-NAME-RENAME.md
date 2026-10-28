# Same-directory file rename candidate

I am developing this next milestone as l00nix, with AI coding assistance, on
`codex/long-name-rename`. It builds on the hardware-tested long-name creation
candidate. **Rename has passed host/FAT-image tests, an 8086 build and emulator boot; physical
PPC512 rename testing is still pending.** This is not a release candidate.

## Scope and commands

The candidate renames regular files within one USB directory, including changes
of case. It refuses replacement of an existing file, open files, directory
renames and moves between parents. SD remains read-only. Names are limited to
26 ASCII bytes; the existing path, namespace and mutation restrictions apply.

```sh
pmmount --rw usb /mnt/usb
pmrename '/mnt/usb/Test/Old Long Filename.txt' '/mnt/usb/Test/New Long Filename.txt'
```

`pmrename` makes one `rename()` call and reports failure. Use this command for
the candidate. The pinned ELKS `mv` can unlink an existing destination before
calling `rename()` and can fall back to copy/delete; its behavior would bypass
some of the intended test restrictions. The no-replacement guarantee described
here applies to the PicoMEM rename operation, not arbitrary shell commands.

For a fresh test directory, `pmlfnren /mnt/usb/NEWTEST` exercises rename,
case-only changes, unchanged-name handling, append/readback after rename and
refusals for collisions, open files, directories and cross-parent moves. It
retains `Renamed Long Filename.txt` (3,073 bytes) and `Keep This File.txt` (4 bytes)
for independent verification. It refuses an already existing test directory.

## Kernel and firmware

Use `kernel/ppc512-rename.config` with `kernel/build.sh`, or pass `--lfn-rename`
to `kernel/remote-build.py`. The new `CONFIG_FS_PICOMEM_LFN_RENAME` option requires
creation, LFN and USB-write support. The saved PPC build also preserves its
existing display extension. All rename options default off.

The paired firmware is
[`codex/dfs-long-name-rename`](https://github.com/l00nix/ISA-PicoMEM/tree/codex/dfs-long-name-rename);
[protocol details](https://github.com/l00nix/ISA-PicoMEM/blob/codex/dfs-long-name-rename/LFN-RENAME-EXPERIMENT.md).
A separate exact ROM descriptor and capability response gate command `0x72`.
Older firmware receives no rename command. Earlier metadata and creation
commands are unchanged. The original metadata-only upstream draft PR remains
separate; this branch is not a new upstream PR.

The pinned ELKS syscall normally implements rename through link/unlink. A small
conditional callout routes PicoMEM paths directly to the driver; other
filesystems retain the existing implementation. The driver holds the parent
lock, requires a complete namespace, checks the source and destination, and
keeps the same inode number. It sends the source alias, expected attributes and
size, and the new basename. Firmware independently verifies the parent, refuses
collisions, calls FatFs once, then verifies the new name, alias, attributes,
size and disappearance of the old alias when the alias changes.

ELKS validates that reply and independently queries the legacy alias before
updating its name cache. An ambiguous error latches a fault; it does not retry
or attempt a rollback that could destroy data. Unmount/recovery and independent
inspection may be needed after a failed mutation. This does not promise atomic
recovery from power loss or safe unplugging while mounted.

## Validation and remaining work

- Firmware protocol/adapter sanitizer tests cover framing, bounds, invalid names,
  SD/directory denial, collisions, metadata mismatch, bounded scans, filesystem
  errors and PSRAM cleanup.
- Real bundled FatFs tests on disposable FAT12 and FAT32 images pass rename,
  case-only rename, collision refusal, remount and payload checks. Independent
  mtools reads find the new long name and exact payload.
- ELKS sanitizer tests cover 24 rename scenarios and 12 syscall routing/reference
  scenarios, in addition to the existing creation, write, LFN and mouse suites.
- Fresh and repeated integration into the pinned ELKS checkout pass without
  duplicate configuration declarations.
- IA16 kernel build: near text 65,360 bytes, far text 46,576, data 10,912 and BSS
  10,208. The near segment has only 176 bytes free.
- QEMU boots the kernel, checks absent-hardware handling and utility usage,
  and successfully renames/reads a Minix file while the old name disappears.
- Firmware builds with rename disabled reproduce the previous stock,
  metadata-only and creation UF2 hashes exactly.

Before considering a release candidate I still need real PPC rename tests,
remount/reboot readbacks, direct Mac name/content/FAT checks, DOS compatibility
checks for this firmware, and runtime stack/service-latency measurements. QEMU
cannot validate the actual PicoMEM file service. Directory moves, overwrite
semantics, Unicode and SD writes are outside this milestone.
