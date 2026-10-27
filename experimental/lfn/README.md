# Long-filename prototype — offline only

I started this prototype on 27 October 2026, with AI coding assistance. It
implements and tests a read-only metadata exchange and ELKS-side name helpers.
**It is not connected to the PicoMEM command dispatcher or the ELKS mount
driver. No firmware image or kernel with long-name support is supplied.**
The existing binaries and default mount behavior are unchanged.

The prototype demonstrates the key operation against the actual PicoMEM FatFs
source and a generated FAT12 image: recover `Long Filename.txt` from
`LONGFI~1.TXT`, preserve the original directory cursor, and open nested files
using short aliases after looking them up by display name. This is a host test,
not a PPC512 hardware result.

## What I learned

The legacy DFS directory reply contains only an 8.3 alias. There is also a
subtlety in the audited FatFs implementation: **`f_stat()` on an alias returns
that alias as `fname`, not the original long name.** A fake backend did not
expose this; the real FatFs integration test did.

`fatfs-adapter.c` therefore opens the parent directory with a separate `DIR`,
scans up to 256 entries for the supplied alias, and closes that cursor on every
exit after a successful open. It restores PSRAM access on success and failure.
It never uses legacy DFS's global `dir_find`, opens file contents, or writes
media. Reaching the scan bound returns an explicit unsupported result.

A separate lookup for every entry can be expensive. Before hardware use I need
to measure firmware service time and ELKS interrupt latency, and consider a
bounded batch reply or separate extended enumeration to avoid repeated scans.
The host tests establish cursor independence, not acceptable hardware latency.

## Components

- `lfn.c` / `lfn.h`: bounded request/reply handling; alias and ASCII component
  validation; reply checks against the alias, attributes and size in the
  directory snapshot; complete-directory collision resolution; lookup by
  either display name or alias; alias-only paths through nested directories.
- `fatfs-adapter.c`: metadata lookup using PicoMEM's existing FatFs API.
- `tests/test-lfn.c`: malformed input, length/encoding boundaries, errors,
  cleanup, bounded scans, collisions, nested paths and fallback.
- `test-fatfs.py`: generates a disposable FAT12 image with mtools, compiles the
  real FatFs from an audited checkout, and exercises USB/SD drive selection,
  metadata decoding and nested alias-only file reads. Disk writes are refused
  by the test block layer and checked to remain zero.

Display names are ASCII, at most **26 bytes**, matching the existing ELKS
`struct dirent`. Spaces and mixed case work. Longer names and Unicode names
retain their short alias. Names are never truncated. Name collisions are
resolved case-insensitively among siblings: conflicting display candidates
are discarded before publication, and aliases remain usable. This does not
add long-name creation, rename, or support for non-ASCII short aliases.

The candidate entry table requires about 2.7 KiB for 64 entries with 16-bit
`unsigned`, before integration or other driver storage. It is a prototype
layout, not an approved increase to the PPC's kernel heap. The installed
USB-write kernel has only 272 bytes of near-code headroom; native compilation,
far-code placement and memory measurement remain required.

## Provisional payload format

No command number, capability location or hardware probe is assigned. These
are payload functions for a future separately gated firmware extension. They
must not be sent to stock firmware, whose unknown-command behavior is not a
safe discovery mechanism. Legacy DFS request/reply formats remain untouched.

Request, at most 135 bytes:

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `PMLN` |
| 4 | 1 | Version 1 |
| 5 | 1 | Operation: 0 capability, 1 name lookup |
| 6 | 1 | Path byte length |
| 7 | 1 | Reserved, must be zero |
| 8 | 0–127 | Absolute backslash-separated short path, no terminator |

The capability request has no path. Lookup requires at least one component;
root, traversal, drive prefixes, wildcards, embedded NULs and long components
are rejected before calling FatFs. The drive is a separately validated argument:
18 for SD (`0:`) or 20 for USB (`1:`).

Reply, exactly 52 bytes, zero-filled where unused:

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `PMLN` |
| 4 | 1 | Version 1 |
| 5 | 1 | Echoed operation (255 if not parsed) |
| 6 | 1 | Status: OK, unsupported, invalid, missing, too long, encoding, I/O |
| 7 | 1 | Name byte length; capability success reports maximum 26 |
| 8 | 1 | FAT attributes |
| 9 | 4 | Little-endian size, at most 2 GiB minus one byte |
| 13 | 13 | NUL-terminated short basename |
| 26 | 26 | Long basename bytes, remaining bytes zero |

The client rejects mismatched versions, lengths, aliases, sizes, attributes,
invalid names and nonzero name padding. Missing, overlong or unsupported names
fall back to the existing alias. A transport timeout must retain the existing
kernel fault handling; a name-decoding fallback does not authorize ignoring a
failed transport or resetting the hardware.

## Reproduce the tests

```sh
sh experimental/lfn/test.sh
python3 experimental/lfn/test-fatfs.py /path/to/ISA-PicoMEM
sh kernel/test.sh
```

The second test needs mtools (`mformat`, `mcopy`, `mmd`) and a C compiler.
I used PicoMEM revision `be7b2e6031ae93bb75733c363489aa9171a7205c`, with its
`src/lib/FatFs_SPI/ff15/source/ffconf.h`, the configuration included by the
source-adjacent `ff.h`. That file sets the LFN buffers to 128; the separate
`include/ffconf.h` says 255. Both enable UTF-8 LFN support. The distinction
does not change the prototype's 26-byte display limit.

On the Mac, 498 checks plus 10,000 malformed requests passed with AddressSanitizer
and UndefinedBehaviorSanitizer. The actual FatFs test passed for both drive
numbers, including preservation of a live directory cursor and a nested read
through aliases. Existing kernel host suites also passed. The tests do not
establish ARM firmware compilation, 8086 kernel fit, real USB/FAT32 timing,
media-change behavior, or PPC hardware operation.

## Remaining integration work

1. Define an explicit, identifiable experimental firmware capability and wire
   a bounded dispatcher handler; retain all legacy DFS replies. Obtain an
   upstream command allocation before treating it as a standard interface.
2. Wire the helpers into `picofs` behind a default-off option, with capability
   gating, bounded allocation and coherent directory snapshots. Preserve
   aliases for reads, writes, deletes and every parent path. Names created by
   the existing writer remain 8.3. Do not activate display-name lookup in only
   some mutation paths.
3. Build the paired firmware and ELKS kernel, check near/far segment and heap
   budgets, run emulator tests, then measure scan latency on hardware. Consider
   a more efficient metadata exchange before enabling routine enumeration.
4. Test a disposable USB fixture read-only first, with a known-good firmware
   recovery image and local access. The unresolved busy-unmount report also
   needs diagnosis before expanding hardware write tests.

[Original audit and proposal](../../docs/LONG-FILENAMES.md) ·
[Current tested mounting implementation](../../kernel/README.md)
