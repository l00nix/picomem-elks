# Optional runtime diagnostics candidate

I am preparing a paired ELKS kernel and PicoMEM 2 firmware build to measure
file-service latency and observed peak stack use. Hardware results are still
pending. This is a diagnostic candidate, not a release candidate.

The [earlier baseline](RUNTIME-VALIDATION.md) measured complete operations from
the host, including telnet/TTY overhead. This candidate adds microsecond
measurements taken on PicoMEM itself, plus stack watermarks in both systems.

## Build and use

The kernel option `CONFIG_FS_PICOMEM_DIAG` defaults off. The dedicated
`kernel/ppc512-diag.config` enables it with the existing USB long-name creation
and rename options. It requires `CONFIG_FARTEXT_KERNEL`; full `CONFIG_TRACE`
must remain disabled. Build in a dedicated pinned ELKS checkout, or use the
existing remote build helper:

```sh
python3 kernel/remote-build.py BUILD_HOST /absolute/build-dir /absolute/cross-dir ./output --diag
```

Pair it with the firmware's default-off `PICOMEM_EXPERIMENTAL_DIAG` option,
which also requires the rename extension and 4 KiB core-0 stack reservation.
See the [firmware protocol and build notes](https://github.com/l00nix/ISA-PicoMEM/blob/codex/runtime-diagnostics/RUNTIME-DIAGNOSTICS.md).

After both candidates have been installed and USB mounted, root can run:

```sh
pmperf --reset /mnt/usb
# Run a bounded workload in a fresh test directory.
pmperf /mnt/usb
```

`pmperf` queries the mounted USB root through ioctl `0x5044`. It cannot query
SD or an arbitrary file, and the kernel refuses the diagnostic command before
I/O unless the ROM has the exact independent PMDG descriptor. Old firmware
continues to support normal file operations; it cannot supply these metrics.
Query/reset changes only diagnostic RAM, never media contents.

## Interpreting the report

`FW_STACK` reports observed core-0 stack use out of 4096 bytes. `K_STACK`
reports the greatest observed ELKS syscall-stack use out of 700 bytes, with
its PID and syscall number, invalid-paint count, and completed sample count.
It covers all tasks after reset, including the measurement utility itself.

Each command row contains count, minimum, maximum and total microseconds.
Command `ff` is the aggregate. For a nonzero count, total/count gives the
mean service time. The firmware interval includes DFS dispatch and PSRAM
restoration; it excludes the PC-side wait/transport and diagnostics queries.
The host benchmark still provides the separate end-to-end measurement.

Flag bit 0 means a 32-bit count or total saturated; bit 1 means firmware stack
painting was invalid. `pmperf` fails on either flag, a kernel invalid-paint
count, or a reported stack use reaching its reservation. Reset before each
bounded run; the aggregate total saturates after about 71.6 minutes of summed
service time. Individual commands must complete within one timer wrap.

Stack painting uses the actual stack pointer, masks interrupts, and preserves
an inactive gap below the live frame (64 bytes on PicoMEM, 16 on ELKS).
Measurements include instrumentation overhead. The firmware watermark can
include core-0 interrupt use; the ELKS measure covers syscall paths, not its
separate interrupt stack. Neither reports PicoMEM core-1 stack use. Observed
peaks are workload evidence, not a proof of worst-case bounds or protection
against stack overflow.

The firmware reply reserves bytes 28–51 as zero. After validating that reply,
the kernel fills bytes 32–47 with its own stack metrics; format version 1 is
at bytes 42–43. Firmware and kernel statistics are separate snapshots, not an
atomic global observation.

## Validation and hardware plan

The host tests, real FAT32-image checks, native build and emulator boot passed
on October 29, 2026. Emulator checks include unsupported diagnostic requests
and ordinary Minix rename behavior. The candidate uses 65392 bytes of near
code, leaving 144 bytes in that segment. ARM and IA16 disassembly checks
confirmed that the stack painters use the live SP without calling helpers
inside the painting loop. [Preflight results and artifact hashes](validation/runtime-diag-preflight-20261029.json).

Host tests cover request/reply bounds, exact ROM gating, root and USB-root
restrictions, malformed replies, copy failures, counter saturation, timer
wrap, query exclusion, stack-paint boundaries and preservation of live frames.
Existing long-name tests remain part of the build. The ARM link map must
retain separate 4 KiB core-0 and 2 KiB core-1 reservations without scratch
code/data overlap. The 8086 kernel must fit its 16-bit segments.

Before installation I save and verify the running kernel and a full PicoMEM
flash recovery image. Hardware testing will repeat the 18-batch workload in
a new USB directory, collect both reports, then verify retained file bytes
after clean remount. SD stays read-only. No hardware measurements for this
candidate are claimed yet.
