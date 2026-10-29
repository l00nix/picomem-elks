# USB cache flush and timeout containment candidate

I have prepared the next firmware-only step toward reliable PicoMEM USB writes.
**Host tests and ARM builds pass; real PPC512/Kingston and DOS tests are pending.**
The existing diagnostics kernel and utilities are sufficient. SD remains read-only.

The preceding [file-service error milestone](FILE-SERVICE-ERRORS.md) reports
failures, but its USB timeout could leave the controller pointing at a caller's
old buffer. This candidate gives USB a permanently allocated 512-byte buffer.
After an uncertain operation, it stops USB I/O until PicoMEM restarts and keeps
that buffer reserved, including when a completion arrives late. It also checks
USB transfer lengths, completion status and command identity more strictly.

Filesystem synchronization now sends a real device-cache flush command. Before
the first sector write, the firmware tests whether the drive supports that flush.
Unsupported or failed flushes stop the operation; a failed close cannot silently
report success. This does not prove protection against power loss or provide
transactional writes. The Kingston's support and flush timing are not yet known.

The candidate is default-off on `codex/usb-sync-lifetime`. See the
[firmware design, build instructions and test plan](https://github.com/l00nix/ISA-PicoMEM/blob/codex/usb-sync-lifetime/USB-SYNC-LIFETIME.md)
and [artifact/test manifest](https://github.com/l00nix/ISA-PicoMEM/blob/codex/usb-sync-lifetime/USB-SYNC-LIFETIME.json).

Local validation covers 578 cases in the actual TinyUSB state machine, 56 adapter
scenarios with memory-error sanitizers, five real FAT32/mount scenarios, and the
existing LFN/create/rename/diagnostic/error regressions. A normal long-name file
was independently read with mtools; a simulated unsupported flush caused zero
sector writes. Firmware RAM use increases by 512 bytes, with the 4 KiB stack
reservation intact. Runtime stack usage and the performance cost of one-sector
transfers still need hardware measurements.

Next I will validate the current baseline in ELKS, save and verify a fresh
full-flash recovery backup, then test detection and a small write on the new
firmware. If those pass, I will repeat the bounded workload, clean remount,
independent Mac checks, ELKS return trip and DOS 8.3 compatibility tests. No
real-media failure injection is planned. The upstream metadata-only draft PR
remains unchanged.

This candidate provides safe buffer containment on timeout. Automatic USB
controller cancellation/reset recovery remains unfinished; restarting the PC
alone is not assumed to restart PicoMEM.
