# Long filenames: interface audit and proposed next step

I investigated long filenames for the mounted PicoMEM USB and SD volumes on
25 October 2026, with AI coding assistance. **This is an implementation proposal,
not working long-filename support.** No firmware update or long-name command
probe was performed. The later screenshot below records the existing short-name
behavior on the tested USB-write kernel.

## What prevents an ELKS-only fix

PicoMEM's FatFs configuration enables long filenames and uses UTF-8 at its API.
Its directory reader receives both `FILINFO.fname` and the short `altname`.
However, `fs_find()` selects the short alias and converts it to an 11-byte DOS
FCB name. The DFS FINDFIRST/FINDNEXT replies contain that name in a fixed
24-byte record; the original long name is absent. Increasing the ELKS name
buffer cannot reconstruct information the firmware does not send.

I checked both the locally audited PicoMEM revision
`be7b2e6031ae93bb75733c363489aa9171a7205c` and the corresponding current default-
branch source files through GitHub. Both show this behavior:

- [FatFs LFN configuration](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/lib/FatFs_SPI/include/ffconf.h#L116).
- [Directory reader chooses the short alias](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/lib/my_etherdfs/fs.c#L238).
- [DFS serializes the 24-byte reply](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/lib/my_etherdfs/ethersrv-pico.c#L343).

The ELKS baseline also limits a directory entry's name to **26 bytes**.
`struct dirent` embeds `d_name[MAXNAMLEN+1]`; the kernel's readdir callback
truncates at that limit. For ASCII filenames, 26 bytes means 26 characters.
This is distinct from Minix's on-disk name limit. Changing the constant alone
would break compatibility with existing programs' directory buffers: the
readdir syscall ignores its count argument. Full-length names require a
compatible directory API design and corresponding libc/application changes.

- [ELKS name limit](https://github.com/ghaerr/elks/blob/e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0/elks/include/linuxmt/limits.h#L49).
- [Directory structure](https://github.com/ghaerr/elks/blob/e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0/elks/include/linuxmt/dirent.h).
- [Directory syscall](https://github.com/ghaerr/elks/blob/e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0/elks/fs/readdir.c#L26).

Do not probe the existing OPEN operation with long basenames. The audited
server splits the basename into a 16-byte local buffer using a copy without a
destination-length parameter. A new ELKS client must keep sending bounded short
aliases through that existing path; a long-name display is not permission to
send an arbitrary long name to the old firmware.

## Proposed first implementation

1. Add a versioned, read-only PicoMEM extension that resolves an existing short
   path to its original long basename. Keep the existing DOS replies unchanged.
   Request parsing must validate all sizes before copying. Use a separate
   metadata lookup so it does not disturb the shared directory cursor.
2. For the first ELKS implementation, request names that fit the existing
   26-byte directory API. Return an explicit too-long/unsupported-encoding
   result for other names, with the existing alias retained. Do not truncate
   long names into ambiguous or unopenable paths. A bounded reply can fit the
   current 256-byte payload; no shared scratch enlargement is required.
3. Keep each node's short alias for firmware operations and its optional long
   display name for ELKS lookup/readdir. Build firmware paths exclusively from
   aliases, including parent directories. Support lookup through either name.
4. Initially accept safe ASCII names, including interior spaces and mixed case.
   Validate components and reject control characters, separators and dot entries.
   If a display name collides case-insensitively with another node's display name
   or alias, fall back to the short alias. Resolve this over the complete bounded
   directory snapshot before publishing entries. UTF-8 display/case handling is
   separate follow-up work.
5. Enable the extension only after its capability/version and hardware profile
   are verified. Unsupported firmware continues with existing short names.
   Command numbers and capability advertisement need agreement with the firmware
   implementation; this proposal assigns no opcode and probes no unknown command.

This would improve ordinary `ls`, `cat` and `cp` on both mounts for names that
fit, without replacing every ELKS application. Longer names would still use
aliases. Full names beyond 26 bytes need a second phase with ELKS maintainers;
a dedicated listing utility could display longer names through a future extended
interface, but that would not give ordinary mounted-path operations full support.

## Validation before hardware deployment

The firmware side needs parser tests for length bounds, absent/media-changed
files, UTF-8 handling, and unchanged legacy DFS replies. The ELKS side needs
fixtures for 12/13/26/27-byte boundaries, spaces, mixed case, duplicate prefixes,
name/alias collisions, nested directories and old-firmware fallback. Checks must
prove opens still send short aliases, failed lookups do not corrupt snapshots,
and mouse/DFS ownership remains enforced. Test names should be placed on a
separate USB fixture by a host, initially mounted read-only for long-name tests.
The separately tested USB-write support does not yet create long filenames.

The tested USB-write kernel has only 272 bytes of near-code headroom. New logic
must respect the near/far segment budget and bounded heap use, then pass native
build, host tests and emulator boot before a planned firmware/kernel hardware
test. The long-filename interface remains a proposal; the USB-write work does not
change its 8.3-name restriction.

## Current PPC512 screenshot

I captured the live `mount` and `ls -l /mnt/usb` output on the PPC512. USB is
mounted read/write and SD read-only, but the directory listing still exposes
short aliases such as `PATCHS~1.SH`, `SYSTEM~1` and `TRASHE~1`. The original long
names cannot be inferred from these aliases. Titles and command labels provide
context; the directory output comes from the actual installed ELKS `ls`.

![PPC512 mounts and short filename aliases](screenshots/20261025-usb-lfn.png)

[Raw video-memory capture and checksum manifest](screenshots/captures-usb-lfn-20261025/) ·
[decoded screen text](examples/20261025-usb-lfn-screen.txt).
