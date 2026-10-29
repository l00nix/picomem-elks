# File-service error candidate

I am preparing a firmware-only correctness update after the successful
[runtime diagnostics tests](RUNTIME-DIAGNOSTICS.md). **Host tests and the firmware
build pass; PPC512 testing of this candidate is pending.** This is another step
toward a USB read/write release candidate, not a release-candidate announcement.

The current firmware can hide both a failed file close and a failed USB sector
transfer. Independent readback catches many problems, but cannot make those
success responses reliable. The new default-off firmware option:

* Reports real close errors and refuses malformed close requests.
* Keeps failed file handles unavailable, so retrying a close cannot mask the
  original failure or reuse an uncertain handle.
* Checks metadata lookup after open and cleans up a failed open operation.
* Reports USB submission/completion errors and the existing transfer timeout.
  USB I/O stays stopped after a transfer failure until PicoMEM firmware restarts.

The existing ELKS kernel already rejects these CLOSE error responses, stops
further file-service operations after an uncertain write, and does not advance
the file position or cached size. I added tests for five DOS error codes to
verify this; no production kernel code or utility binary changed. The existing
full-byte readback checks remain enabled. SD remains read-only.

The USB error latch also affects DOS USB disk-image access because that uses the
same firmware sector adapter. That makes a fresh DOS compatibility test part of
this candidate's validation, even though no DOS long-filename extension is needed.

## Evidence and limits

Five disposable FAT32-image scenarios exercise the real firmware dispatcher,
handle table and FatFs: normal close/remount/readback, write failure, flush
failure, metadata failure after open, and failed cleanup. The normal result also
passes independent `mcopy` readback. Fourteen transport-model cases exercise the
actual USB disk adapter, including rejected transfers, SCSI errors, short
completion, timeout across timer wrap, disconnect, and late completion. The
ELKS host suite and existing firmware LFN/create/rename/diagnostics tests pass.

The ARM build and 4 KiB stack-layout check pass. Turning the new option off
reproduces the currently installed diagnostics firmware byte-for-byte.
[Candidate artifact hashes and validation](validation/file-errors-preflight-20261029.json).

This preserves `f_close`/`f_sync`; it does not remove flushes to improve timing.
USB `CTRL_SYNC` checks current fault/readiness state but does not send SCSI
SYNCHRONIZE CACHE. A timeout still does not cancel an outstanding TinyUSB transfer;
late-transfer buffer ownership and transport recovery need further engineering.
Power-loss persistence is not established. Reported errors do not undo previous
writes. Failure injection is confined to disposable host images, not the Kingston.

The [firmware implementation and recovery limits](https://github.com/l00nix/ISA-PicoMEM/blob/codex/file-service-errors/FILE-SERVICE-ERRORS.md)
include build instructions. Build with `PICOMEM_EXPERIMENTAL_FILE_ERRORS=ON`
in addition to the existing LFN/create/rename/stack/diagnostics options. Keep the
currently tested diagnostics kernel and utilities.

## Next physical test

1. Cleanly unmount USB and shut down ELKS. Save and verify a fresh full PicoMEM
   flash backup in BOOT mode, then flash and verify the candidate.
2. Boot ELKS and run the bounded workload in a new `PMERR29` USB directory.
   Capture native mount/diagnostics output and check every retained file byte.
3. Cleanly unmount/remount and repeat the byte checks. Follow with direct Mac
   filename/content/FAT checks and a DOS legacy file-operation compatibility run.

I will keep SD unmounted for these tests. Open/close accounted for 57.4% of the
preceding workload's firmware time, but neither timing result justifies removing
synchronization. Performance work follows correctness and separate measurement.
