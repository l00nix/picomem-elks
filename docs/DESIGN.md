# Interface and transaction notes

## Read path

The viewer opens `/dev/kmem` read-only, scans option ROM locations for PicoMEM,
and decodes the common 256-byte configuration prefix. The tested ROM resides at
`D000:0000`, with shared memory at `D000:4000` and the configuration beginning
82 bytes into shared memory. `-s D000` selects the viewer's ROM explicitly.

The 0.3 development build adds `--details`. Common-prefix fields need no layout
query. The extension uses the same board/date/healthy-initialization guard as
the editor, reads the full 768-byte configuration twice, compares it with the
captured prefix, and rechecks the prefix, marker, board, init status and ROM
header. A mismatch retries the snapshot, up to three attempts. A read failure
or persistent mismatch produces no report. BIOS counters are sampled values;
they need not remain constant between reads. This is a consistency check,
not a firmware lock or a guarantee of atomic live status.

Unknown profiles retain the common 256-byte view. `-v` alone also retains that
size; `--details -v` dumps 768 bytes only after the extension checks pass. The
reader stops before the command parameter area, never sends a layout-detection
command, and bounds/sanitizes every displayed name and path. Image names are
configuration records, not proof of a mounted filesystem. The unused USB bit
field is explicitly not presented as an inventory. The editor's staging format,
writable fields and transaction are unchanged.

Staging requires exactly one supported board and two identical reads of the
full 768-byte region. It checks board ID 11, BIOS date 2026-06-16 and healthy
SD/config initialization. The management port for this profile is `0x2A0`.

The BIOS primary IRQ and the configured NIC IRQ are separate fields. On the
tested PicoMEM 2, hardware mask `0xA8` reports IRQ3/5/7. The primary BIOS IRQ
remained 7 while networking worked with NIC IRQ5.

## Staging format

A stage/rollback file is exactly 1,700 bytes. Multi-byte numbers are little-endian.

| Offset | Length | Meaning |
| --- | --- | --- |
| 0 | 8 | Magic `PMCFG02\n` |
| 8 | 2 | ROM segment |
| 10 | 2 | Management port |
| 12 | 2 | Configuration length, 768 |
| 14 | 1 | Board ID |
| 15 | 1 | Detected hardware IRQ mask |
| 16 | 16 | Reserved, initially zero |
| 32 | 128 | ROM identity/header |
| 160 | 768 | Before configuration |
| 928 | 768 | Proposed configuration |
| 1696 | 4 | CRC32 over bytes 0–1695 |

Only configuration offsets 207 (fast boot) and 215 (NIC IRQ) may differ between
before and proposed. Other deltas, invalid IRQs, wrong lengths and CRC failures
are rejected. CRC detects accidental damage; it does not authenticate a file.

Files use `O_CREAT|O_EXCL`, mode `0600`, exact-length reads and post-write
verification. A rollback file stores the original configuration in both halves.
Restoring it stages only the two editable original fields against the current
full snapshot, preserving unrelated current settings.

## Save path

Saving requires the native IA16 build and root. It compares the staged identity
and complete before snapshot with the live board, then creates, verifies and
syncs the mandatory backup before entering the hardware transaction.

The transaction preserves CPU flags and disables interrupts to prevent ELKS BIOS
disk I/O from competing for the PicoMEM command port. It tests the incrementing
port, checks command idle state and board status, and compares all 768 bytes
again. It writes only the two editable fields, checks their readback, and sends
`CMD_SaveCfg` (`0x08`). It requires a busy-to-ready transition for acknowledgment.

The polling loop is bounded by CPU iterations, not wall-clock seconds. The
generated 8086 critical section was checked for calls: no allocation, filesystem
operations, console output or other calls occur between CLI and restored flags.
No BIOS interrupt calls or `CMD_DEV_Init` commands are sent.

Completed firmware errors restore the shared editable fields and request error
reset. A timeout leaves a possibly active writer undisturbed and reports
uncertainty. Neither path establishes the previous SD file's integrity. Firmware
owns the actual file format and write operation; the host cannot promise an
atomic SD write or independently verify persistence.

The utility does not reinitialize a live NIC. Changing the saved IRQ must be
paired with the ELKS driver setting and a separate reboot. The real-machine
network test confirms working traffic after that sequence on the supported board.

## Relationship to CH375USBTools

I used the DOS Pascal PMCFG and PicoMEM firmware sources as references when
writing this separate native C utility for ELKS. This is not a fork or a direct
translation of the Pascal implementation. The tools share a general purpose, not a codebase or command-line interface.
The primary ELKS executable is now named `pmconfig`; `pmcfg` remains an alias.

I checked the upstream PMCFG source on 22 October 2026; its latest file-changing
commit was [19f07350](https://github.com/jdredd87/CH375USBTools/blob/19f07350bcdba17322d6e40b2efe8c55b7e5ecf7/PicoMEM/src/pmcfg.pas).
It explicitly performs no configuration writes. Its normal layout detection
uses a disk-list query; `/Q` skips that query and assumes the smaller layout.
Its supporting `pmcard.pas` uses DOS BIOS detection, direct segmented memory
access and a restricted command interface. Those mechanisms need an ELKS-specific
design rather than simply recompiling the Pascal programs.

| Area | DOS PMCFG | This ELKS utility |
| --- | --- | --- |
| Implementation | Pascal using DOS support units | Native 8086 C with ELKS libc |
| Read path | BIOS/port detection and shared memory; layout query unless `/Q` | ROM scan and `/dev/kmem`, no firmware command for viewing |
| Decoded information | Includes option ROMs, audio, SD/RAM speeds, RTC and a conditional CD-ROM name | v0.3 `--details` adds these plus BIOS variables and profile-checked image directories |
| Raw configuration dump | 256 or 768 bytes depending on detected layout | 256 with `-v`; 768 with `--details -v` after profile/consistency checks |
| Configuration edits | None | NIC IRQ and fast boot; explicit stage, backup, save and rollback |
| Editable firmware profiles | Not applicable | Tested PicoMEM 2 / BIOS 2026-06-16 only |

I recommend building on this ELKS implementation. A fork could be useful for
contributing changes to the DOS collection, but it would not supply next-boot
editing: that capability already exists here and is absent from upstream PMCFG.

Implementation status and remaining work:

1. Expanded inspection, a bounded support dump, passive status and active
   memory-map queries were tested in 0.4-dev. Version 0.5 adds tested explicit
   USB/disk status queries and receiver detection. [New results](HARDWARE-USB-20261023.md).
   [Hardware evidence](HARDWARE-TEST-20261023.md) and [tool design](TOOLS.md).
2. The existing board 11 / BIOS 2026-06-16 profile guards extended reads as well
   as edits. Additional firmware profiles need identifying captures and layout
   verification before support is added; none are assumed compatible.
3. Independently verify persistence with an SD readback or power-cycle test.
   The concrete procedure is in [the porting and validation plan](PORTING.md#hardware-validation).
4. Consider additional next-boot settings individually, with documented effects,
   validation, rollback and hardware testing. Keep memory and boot-image changes
   outside the initial scope.

The [other-tools assessment](PORTING.md) identifies which DOS functionality can
be adapted to ELKS and which needs kernel support or firmware changes.

## References

- [PicoMEM definitions and configuration layout](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_defines.h)
- [PicoMEM command implementation](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_cmd.cpp)
- [PicoMEM disk/configuration handling](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_libs/pm_disk.cpp)
- [jdredd87's Pascal configuration utility](https://github.com/jdredd87/CH375USBTools/blob/main/PicoMEM/src/pmcfg.pas)
- [ELKS PicoMEM networking investigation](https://github.com/ghaerr/elks/issues/2441)

These source references explain the protocol; they are not proof that every
firmware build implements the same layout. Broader support needs explicit
profiles and hardware verification.
