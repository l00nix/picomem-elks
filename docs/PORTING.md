# PicoMEM tools worth adapting to ELKS

I reviewed the thirteen Pascal utilities in
[CH375USBTools/PicoMEM](https://github.com/jdredd87/CH375USBTools/tree/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM)
and its network/EMS driver projects on 22 October 2026. This assessment is
pinned to commit `5d40d93c`; firmware handler references use PicoMEM commit
`be7b2e60`. These sources document interfaces and reported experiments, not
proof that every installed firmware behaves identically.

My recommendation is to keep building this small native C utility for ELKS.
Forking the entire DOS collection would not make its BIOS calls, interrupt
handlers, packet-driver interface or memory managers usable by ELKS. I can
adapt the useful functions individually and retain attribution to StevenC,
Claude and the underlying PicoMEM/driver authors where applicable.

## Priorities

| Source tool | Value on the PPC512 | ELKS approach and status |
| --- | --- | --- |
| [PMCFG][pmcfg] | Full configuration inspection and next-boot edits | Expanded inspection tested on the PPC512 in 0.4-dev. The existing v0.2 save/rollback path already handles NIC IRQ and fast boot; upstream PMCFG does not write settings. |
| [PMINFO][pminfo] | Board identity, firmware date, initialization and BIOS counts | Its memory-only information is now included in `pmconfig --details`. No DOS BIOS detection or parameter-location query is needed. |
| [PMDUMP][pmdump] | Repeatable bug reports and comparison of firmware layouts | Implemented and hardware-tested in 0.4-dev: a 964-byte support bundle containing allowed ROM/configuration ranges, CRC32, exclusive creation, private permissions and verified readback. |
| [PMSTAT][pmstat] | Claimed USB devices, disk descriptions and optional WiFi status | Passive summary and explicit bounded USB/disk queries implemented and hardware-tested in 0.5-dev. The receiver reports Logitech Mouse. WiFi queries remain excluded. |
| PicoMEM PMDFS / embedded EtherDFS | Direct USB and SD file service | Experimental [`pmusb`](../usb/README.md) now retrieves up to 32 USB-root directory entries from ELKS using the shared-memory protocol. Ten entries were read from a Kingston drive after a physical power cycle. The separate optional [kernel filesystem](HARDWARE-KERNEL-FS-20261023.md) now also mounts USB and SD read-only and passed native file-read/copy tests. The user-space client itself remains listing-only. [Evidence](HARDWARE-USB-FLASH-20261023.md). |
| [PMMEM][pmmem] | Compare the configured memory map with the card's active map | Implemented and hardware-tested: guarded `05h` queries compare all 64 active blocks with configuration. No `/W` write probes or mapping changes. A BIOS-only probe is available first. |
| [PMDEV][pmdev] | Check configured devices and selected responses | Reuse configuration reporting here and existing ELKS network status. Its DOS packet-driver scan is inapplicable; OPL timer detection writes registers and needs an explicit probe mode. Medium priority. |
| [PMWATCH][pmwatch] | Diagnose which documented status fields change | Later, a bounded watcher with a sleep interval, allowed offsets and redaction before output. The upstream whole-8-KB watcher can expose parameter data and generates noise from disk traffic; keyboard enable and query self-tests are active operations. |
| [PMBENCH][pmbench] | Quantify port, ROM and RAM access costs | Later. Use ELKS timing and matched access loops. Timing `/dev/kmem` system calls against DOS direct memory reads would measure different work. Command stress needs separate treatment. |
| [PMOPL][pmopl] | Audible check of PicoMEM audio | Optional later utility. Even timer-only detection writes OPL registers; allow it only when audio is idle and make cleanup explicit. Useful for an audio project, less urgent than network/storage visibility. |
| [PMMTEST][pmmtest] | Investigate USB mouse input | Kernel/input-driver project, not a direct user-space port. It enables firmware mouse reporting and polls shared event bytes; polling loses overwritten motion. First establish IRQ ownership and acknowledgment in ELKS. The receiver is detected, and an [experimental kernel driver/capture program](../mouse/README.md) now boots and passes passive validation on the PPC; an active capture received one zero-motion report and stopped cleanly, while movement/button validation remains incomplete. See the [integration audit](ELKS-MOUSE.md). |
| [PMIRQ][pmirq] | Understand PicoMEM's multiplexed IRQ | Reference material for the same kernel project. Do not port DOS interrupt-vector chaining into an ELKS utility. |
| [PMTICK][pmtick] | Diagnose command stalls and interrupt state | Developer-only work later. It relies on BIOS ticks and reads the PIC using command-port writes. ELKS interrupt handling/timing must be accounted for. |
| [PMCMDT][pmcmdt] | Reproduce command-interface timing problems | Low priority outside a controlled diagnostic session. Repeated USB commands stress the same card serving the boot disk; this is not normal status inspection. |

## Why status queries need their own design

The firmware's [command handlers][commands] show why a command called “status”
cannot automatically be treated as passive:

- `05h`, `CMD_GetMEMType`, returns one active memory-block type through command
  registers. It still needs exclusive command-port access.
- `61h`, USB status, and `62h`, disk status, write their answers to shared
  command parameters. The answer must be copied while the transaction owns that
  workspace, before console or filesystem activity can trigger competing I/O.
- `60h`, WiFi information, can retry the connection and copies a structure
  containing WiFi credentials into shared memory. It must be opt-in, with key
  fields omitted from both output and support dumps.

The DOS tools have observed parameter offsets 374 and 886 on different firmware
layouts. I would use explicitly identified, validated ELKS profiles, not assume
one offset or issue a discovery query during ordinary inspection. The existing
save transaction is useful groundwork, but it is not automatically a suitable
transport for every command: handler latency, response length, timeout behavior
and interrupt requirements need review for each query. Holding interrupts off
for an unbounded USB or WiFi operation is not acceptable.

For a query implementation, completion criteria are: fake-hardware tests for
busy/error/timeout and competing commands, bounded response parsing, generated
8086 inspection of the critical section, and hardware tests with boot-disk I/O
and networking. The 0.4 memory query and 0.5 bounded USB/disk transport now meet these criteria
for the tested profile. See the [USB receiver hardware results](HARDWARE-USB-20261023.md). The disk command is **62h** in
`pm_defines.h`; the **64h** comment in the handler is stale and was the source of
the earlier incorrect number in this document. [Transport design](TOOLS.md).

## Driver projects and other useful findings

The [network driver work][netdrv] is more useful as an ELKS NE2000 driver audit
than as a port of `PM2000.COM`. ELKS already has a working driver on this PPC512.
Upstream reports improved NEC V20/V30 throughput from byte-wide string I/O and
a packet-buffer over-read fix. I would check ELKS's existing copy loops and
bounds before changing anything; the NEC optimization must not be enabled on
the generic 8086 target. The DOS throughput gains are not ELKS measurements.

Upstream also reports dropped frames sent close together on PicoMEM 2 with
BIOS 2026-09-27, and a separate timing-related lockup investigation on a 386.
Those are useful test leads, not established faults on this PPC512's 2026-06-16
firmware. A controlled LAN test comparing paced and back-to-back traffic, with
checksummed downloads and ELKS error/retransmission counters, would be more
informative than removing port delays speculatively.

The [EMS project][emm] offers useful page-boundary, overlap, disabled-window
and interrupt-latency tests. Its DOS `INT 67h` driver and `CONFIG.SYS` installation
cannot be carried directly into ELKS. Any EMS-backed memory or storage feature
needs kernel ownership of page registers and windows, plus context-switch and
interrupt rules. I would borrow those test cases when such a project exists.
`UMBSC`, now maintained in DOSBridge, manages DOS upper memory and has no direct
ELKS porting value. The modified DOS network driver also has its own licensing
and attribution history; copying its implementation would need a separate
compatibility review, not this repository's blanket license assumption.

The [upstream investigation notes][next] report no working USB keyboard delivery
path on the two firmware versions they tested. They also explain that an
unclaimed USB device does not gain an OS driver merely because PicoMEM lists it.
The collection's CH375-specific tools target a different USB controller; they
are not automatically PicoMEM tools. Generic USB control/endpoint passthrough
would require a suitable PicoMEM firmware interface before those could be
adapted. These observations are version-specific, not claims about every future
firmware release.

## Hardware validation

The machine was available again on 23 October 2026. I ran 0.3 and 0.4 natively
over telnet/FTP, captured six real console screens, verified support bundles
and all 64 memory queries, and checked configuration equality and a checksummed
HTTP download afterward. [Complete results](HARDWARE-TEST-20261023.md).

The following persistence checks remain pending; none were performed in this
inspection/query round:

1. Preserve a verified stage/rollback record and copies of the SD configuration
   file before testing persistence. Record the initial fast-boot value. Stage
   only its opposite value, review the proposal and save with a fresh backup.
   Fast boot avoids altering the already working NIC or disk mapping.
2. Shut down ELKS cleanly. With someone at the machine, remove PicoMEM power
   fully, then start it and check the selected value, serial login, disk access
   and networking. A normal PC reboot alone is insufficient because the card
   can remain powered. Keep serial recovery and the original SD configuration
   available before the power test.
3. Restore the original fast-boot setting through a staged rollback with another
   backup. Repeat the cold-start check. If inspecting the SD directly instead,
   do so while powered down and compare the persistent file through a verified
   file-layout decoder; do not treat the shared 768 bytes as the entire SD file.

I would only add another editable setting after documenting its firmware
semantics, validation, next-boot effects and rollback, then testing it on the
identified hardware profile. Memory-map, boot-image and live NIC changes remain
outside this development step.

[pmcfg]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmcfg.pas
[pminfo]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pminfo.pas
[pmdump]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmdump.pas
[pmstat]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmstat.pas
[pmmem]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmmem.pas
[pmdev]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmdev.pas
[pmwatch]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmwatch.pas
[pmbench]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmbench.pas
[pmopl]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmopl.pas
[pmmtest]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmmtest.pas
[pmirq]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmirq.pas
[pmtick]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmtick.pas
[pmcmdt]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/src/pmcmdt.pas
[commands]: https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_cmd.cpp
[netdrv]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/netdrv/README.md
[emm]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/emm/README.md
[next]: https://github.com/jdredd87/CH375USBTools/blob/5d40d93c1a7b1c8b4497c3879e30e607d7cac1d7/PicoMEM/NEXT.md
