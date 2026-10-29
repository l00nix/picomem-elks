# Runtime validation preparation

I am preparing stack and latency checks for the rename candidate. This is
still experimental work, not a release candidate. The existing kernel can run
the new `pmbench` utility; no kernel change is required for host-timed batches.

## Stack preflight

The IA16 compiler reports 130 bytes for `rename_inner`, 60 for `scan`, 96 for
`lfn_scan`, 14 for `lfn_query` and 32 for `transaction`. Caller frames, helper
functions and interrupt entry also matter: these numbers are not a complete
stack bound or runtime high-water result. The PPC configuration reserves
700 bytes per kernel stack. Full kernel tracing is currently disabled, and
the kernel's near code segment has only 176 bytes free.

The firmware compiler audit found a legacy delete path with at least 2112
bytes of nested frames against a 2048-byte core-0 stack reservation, before
deeper FatFs/USB calls or interrupts. An opt-in firmware candidate now reserves
the otherwise empty 4 KiB SCRATCH_Y bank for core 0 while retaining core 1's
2 KiB allocation. Its build/map checks pass and flashing passed readback
verification after saving a full recovery image. The ELKS reboot, hardware
workload and actual stack high-water checks remain pending. See the companion
firmware branch's `RUNTIME-VALIDATION.md` for the frame breakdown.

## Host-timed workload

Build `kernel/pmbench.c` with the normal 8086 utility flags, or use
`kernel/build.sh`. The workload is controlled by a host handshake; it is not
intended to run unattended directly from the ELKS command line.

Once the candidate is ready and the USB is mounted read/write, use Python
3.9–3.12 on the host (the harness uses `telnetlib`):

```sh
python3 tools/measure-pmbench.py PPC_ADDRESS /mnt/usb/PMBEN29 results.json
```

The native binary must already be at `/root/pmbench` on the PPC, or supply
`--program /path/to/pmbench`. The harness logs in as root with the test
machine's existing passwordless telnet setup. It refuses to overwrite an
existing result file. Use a **new** test-directory name of 1–8 uppercase
letters/digits. The workload refuses any existing directory and writes only
inside the one it creates. SD is never accessed.

Each operation runs three batches of four iterations:

| Operation | One iteration |
| --- | --- |
| `noop` | Empty loop iteration, for transport-overhead context |
| `read3073` | Open, read and verify 3073 bytes plus EOF, close |
| `write3073` | Open/truncate, write 3073 bytes, close |
| `rename_pair` | Rename a closed file to another long name and back |
| `create_unlink` | Exclusively create a long-name file, write 73 bytes, close, delete |
| `mkdir_rmdir` | Create and remove an empty long-name directory |

The retained payload is verified after every batch, outside its timed window.
On successful completion only `Runtime File Alpha.txt` remains: 3073 bytes,
with byte `j` equal to `(j * 17 + 23) & 255`. A failure stops the workload and
retains its directory for inspection; do not automatically retry or delete it.

The Mac measures from sending `go` to receiving the complete `DONE` line.
Results include network, telnet, TTY and process scheduling overhead. The
no-op baseline is reported separately, never subtracted. Paired operations
are reported **per pair**, not per individual syscall. These are end-to-end
wall times, not PicoMEM service times or disk-only throughput.

The ELKS transaction masks interrupts while waiting for PicoMEM, so an ELKS
tick-based timer could under-report the delay. Accurate firmware service
timings will need separate instrumentation using the PicoMEM clock.

## Preparation checks

- Host sanitizer test passed all 18 batches, exact final payload, refusal to
  reuse a directory, name restrictions and abort on invalid handshake.
- Native 8086 build passed with warnings treated as errors.
- PPC native utility startup and staging readback passed. USB mounted
  read-only and both retained rename payloads matched their original bytes.
- The stack-reservation firmware is flashed and readback verified; hardware
  measurements await the ELKS reboot. The kernel is unchanged. No
  release-candidate claim is made.
