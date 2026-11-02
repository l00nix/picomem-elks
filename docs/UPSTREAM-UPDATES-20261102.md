# Published upstream progress updates — 2 November 2026

These comments describe the development candidate, not the code currently in
PicoMEM PR #164 or the releases on `main`. Both posted bodies were read back
and verified. See [current status and limitations](STATUS-20261102.md).

## PicoMEM PR #164

[Published comment](https://github.com/FreddyVRetro/ISA-PicoMEM/pull/164#issuecomment-6583280766)

I have continued the experiment on my Amstrad PPC512 with PicoMEM 2, with coding
assistance. The newer development candidate supports mounted ELKS USB reads and
writes, creation, append, truncation, removal and same-directory file rename
using long names. This work goes beyond the read-only metadata prototype
currently in this draft PR; I am keeping that distinction explicit while I
prepare the changes for review.

Nine media have completed guarded ELKS–Mac–ELKS data checks, including a USB-to-SD
adapter. DOS 8.3 compatibility also passed on that adapter/card with the current
firmware. I preserved file hashes and read-only FAT checks, and recorded macOS
index/log changes separately. The final warm-reboot copy/rename/delete and
unmounted reconnect test also passed.

On the current firmware, DOS compatibility also passed on the USB-to-SD card:
reads through existing 8.3 aliases and create/append/truncate/rename/delete,
followed by ELKS and independent Mac verification. This is legacy DOS file
access, not native DOS long-name support. A final ELKS warm-reboot test passed
native long-name copy, full readback, rename, deletion, remount and a safely
unmounted unplug/reconnect check.

The candidate is still experimental. One generic-stick read previously stopped
partway through a file and was followed by ELKS I/O errors. Later bounded reads,
including repeated full reads of that file, passed, but the cause remains
unresolved. A separate FTP connection once timed out before its greeting or any
file read; the kernel remained healthy and a later connection worked without a
restart. I am tracking those separately. No power-loss durability claim is made.

The current scope is 26-byte ASCII long names, FAT16/FAT32, one active USB disk,
and same-directory regular-file rename without replacing a destination. Internal
PicoMEM SD mounts remain read-only. Safely unmounted swaps have been tested;
removing mounted media, simultaneous multi-disk mounts, full Unicode and exFAT
read/write are outside this acceptance scope. Some devices report no cache
settings, so the experimental policy explicitly records assumed write-through
rather than claiming that write caching was proven disabled.

I would appreciate guidance on provisional command allocation and capability
advertisement, and whether the bounded long-name service and USB transport/cache
handling should be reviewed as separate changes. I plan to separate reusable
TinyUSB changes from PicoMEM file-service policy, with ELKS kernel and FTP fixes
reviewed in their own projects. I am keeping this PR in draft while that work
and the unresolved read failure are addressed.

## ELKS discussion #2841

[Published comment](https://github.com/ghaerr/elks/discussions/2841#discussioncomment-19356382)

I have an update on [picomem-elks](https://github.com/l00nix/picomem-elks). Alongside
the `pm` configuration and diagnostic utilities, I have been testing an optional
PicoMEM file-service driver on my Amstrad PPC512. The development candidate now
supports mounted FAT16/FAT32 USB reads and writes with ASCII long filenames up
to 26 bytes, including creation, append, truncation, removal and guarded
same-directory file rename. It requires matching experimental PicoMEM firmware
and ELKS kernel support; the current public read-only firmware PR does not yet
contain the full candidate.

Nine media have completed guarded ELKS–Mac–ELKS data checks, including a USB-to-SD
adapter. DOS 8.3 compatibility also passed on that adapter/card with the current
firmware. I preserved file hashes and read-only FAT checks, and recorded macOS
index/log changes separately. The final warm-reboot copy/rename/delete and
unmounted reconnect test also passed.

This is progress toward a release candidate, not a release announcement. An
earlier intermittent USB read failure remains unexplained despite subsequent
successful checks, and a separate transient FTP greeting timeout is being
tracked. The current scope is a single directly connected USB disk; PicoMEM's
internal SD remains read-only. Full Unicode, exFAT write support, removal while
mounted and power-loss durability are not established.

Following the earlier advice here, I am keeping the tools external during active
development. I plan to prepare the PicoMEM firmware, ELKS driver and independent
FTP corrections as separate reviewable changes. Feedback on that split and the
ELKS integration approach would be welcome.
