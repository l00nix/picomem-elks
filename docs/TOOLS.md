# Support and query tools (0.5-dev)

`pmconfig`, `pmcfg`, `pmdump`, `pmstat` and `pmmem` use one executable.
Companion names select their respective mode from `argv[0]`; they cannot invoke
configuration editing. `pmconfig` retains the existing editor and file format.

## PMDUMP support format

All multi-byte integers are little-endian. The file is exactly 964 bytes.

| Offset | Bytes | Content |
| --- | --- | --- |
| 0 | 8 | ASCII `PMDMP01` followed by LF |
| 8 | 2 | PicoMEM ROM segment |
| 10 | 2 | Configuration length, 768 |
| 12 | 1 | Source: 0 hardware, 1 offline fixture |
| 13 | 19 | Reserved, zero |
| 32 | 128 | Option-ROM identity prefix |
| 160 | 32 | Sampled shared BIOS variables |
| 192 | 768 | Validated configuration |
| 960 | 4 | CRC32 of bytes 0–959 |

The writer requires the known board/BIOS profile with healthy SD/config flags,
repeated matching configuration reads, and a consistent ROM/shared identity.
BIOS counters are sampled; the entire record is not a single atomic capture.
The file is created with `O_EXCL` and mode 0600, synced, then read back and
compared including exact length. Existing files and symlinks are refused.
A failed/partial write is retained for diagnosis and reported as a failure.

The bundle excludes the register-save area, disk buffer, command parameters,
WiFi credential structure and unrelated physical memory. Image names and
paths remain present. CRC detects accidental corruption, not malicious changes.
`tools/read-dump.py` validates format/profile/CRC and emits a small JSON summary.
The support format is deliberately distinct from `PMCFG02` staging/rollback
records and cannot be restored as a configuration file.

## PMMEM query transaction

The native ELKS build uses `CMD_GetMEMType` (`05h`) at management port `2A0h`.
Before each query it saves flags, disables interrupts, checks the incrementing
port, requires ready status, rechecks shared identity/IRQ mask/idle command
fields and compares all 768 configuration bytes against the captured snapshot.
No libc or system calls occur with interrupts disabled; generated IA16 assembly
was inspected, including failure branches.

The low argument is the 16 KB block index (0–63). The high byte is set to `FFh`
as a sentinel before issuing `05h`. Completion requires ready status, a replaced
sentinel and the original block index. This accepts a fast command whose busy
state was not observed, while rejecting an ignored command. Polling is bounded
at 8,192 iterations; this is an iteration limit, not a calibrated duration.
Only an error returned after this command permits a bounded status-reset
command (`00h`). A busy timeout is left alone. Interrupt flags are restored on
all exits. There is no automatic retry.

The BIOS block is queried first and must report BIOS type 08h or 0Ch. `--probe`
stops there. The full mode then queries the remaining blocks, restoring flags
between transactions. It samples the map across 64 queries rather than locking
out interrupts for the full report. Configured PC-owned types (32 and above)
are compared with active type zero, because they are not card emulation.
Other differences are printed literally; a difference is not automatically a
fault. For example, the PPC512's D4000h shared buffer reports active type 02h
where its configured map describes the BIOS region as 08h.

This is a query of firmware mapping metadata. It neither writes test patterns
to RAM nor changes memory mappings, EMS registers or configuration. The supported
profile and uniprocessor ELKS critical section are essential; this is not a
general cross-OS locking protocol for PicoMEM commands.

## PMSTAT passive and explicit query modes

Plain `pmstat` remains passive. `pmstat --usb` and `pmstat --disks` explicitly
issue commands 61h and 62h respectively. `pmconfig` accepts the same flags.
Only one live status query is allowed per invocation; offline fixtures and
mixed dump/memory/status modes are rejected. The existing board/BIOS profile
and stable full-configuration capture are required, followed by native root
and command-ownership checks.

The source header defines disk status as **62h**. The **64h** comment beside
its handler is stale; C dispatch follows the header. WiFi information (60h)
can reconnect and populate credentials, so it is excluded by the transport's
command allowlist. Mouse enable (52h) is also rejected.

### Bounded response and command ownership

After saving flags and disabling interrupts, the query verifies the port,
ready state, shared identity, idle command fields and all 768 configuration
bytes. It then fills only the first **1,024 bytes of command scratch at offset
886** with FFh. This area lies within the profile's 2 KB parameter region;
configuration and IRQ fields are outside it. Clearing this range prevents
stale strings, including a previous WiFi reply, from being returned. These
modes therefore write command scratch and I/O registers, while leaving
configuration and device-enable settings unchanged.

The command is issued with zero argument registers. A valid ready response
must replace the count sentinel. Polling is bounded at **32,768 iterations**;
this is not a calibrated millisecond timeout. Only a completed error from this
command permits a bounded reset (8,192 polls). A timeout does not reset or
retry a possibly active command. Unknown status values also expire at the
bound.

The response is copied while interrupts remain disabled: at most **8 lines**
(**6 for disk status**), **127 non-NUL bytes per line**, and **1,024 bytes total**.
Missing terminators, excess counts/length, FFh sentinel bytes and a changed
count are rejected before any response text is printed. The firmware FEh
bullet becomes a space; other control/non-ASCII bytes become `?`. The unused
scratch tail is never printed or included in support files. The report uses
only the local copy after restoring interrupt flags.

The USB handler examines cached mounted-device state and descriptions. The
disk handler formats configured image/geometry metadata. Neither handler
requests a reconnect, mounts a disk, enables a mouse or changes configuration.
Firmware debug printing can affect latency, and hotplug can change the cached
inventory; a response is a snapshot rather than a proof of input delivery or
an ELKS filesystem mount.

### Validation and limits

The native compiler emitted no calls in the interrupt-disabled status
transaction, including parsing and all failure branches. Host fake-hardware
tests cover normal/fast completion, busy/absent/stale identity, ignored commands,
timeouts, firmware errors/reset timeout, forbidden commands, empty responses,
count/line/total bounds, missing strings and text sanitization. They assert
that writes remain inside the command scratch region.

On the PPC512's supported profile, repeated queries completed after 2–3 status
polls. USB returned **Logitech Mouse** and disk status returned the boot image
and geometry. Configuration/support snapshots stayed identical and an HTTP
transfer passed its checksum afterward. Poll counts exclude identity checks,
scratch initialization and copying; they are not elapsed-time measurements.
This establishes the tested profile, not universal firmware latency. There is
no automatic retry, firmware-discovery query or background polling.

[Hardware results](HARDWARE-USB-20261023.md) · [ELKS mouse integration audit](ELKS-MOUSE.md)

References: [definitions](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_defines.h),
[handlers](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_cmd.cpp),
[cached USB status](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/USB_Host_driver/usb.c),
[DOS PMMEM reference](https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmmem.pas).
