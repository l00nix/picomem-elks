# Amstrad PPC512 hardware test — 21 October 2026

## Machine and build

- Amstrad PPC512, reporting 735 KiB RAM with PicoMEM memory expansion.
- ELKS 0.9.2, kernel `e7a56a43`, i8086, built 5 September 2026.
- PicoMEM 2 board ID 11, BIOS 2026-06-16, ROM D000, management port 02A0h.
- COM1 serial login at 9600 baud, retained across warm reboots.
- Native executable provenance: [dist/README.md](../dist/README.md).

## Editor checks

I ran the host suite: all 33 checks passed, covering 10 viewer tests, 14 staging
tests, and 9 transaction scenarios using fake hardware. On the physical machine:

1. Staging IRQ3 → IRQ5 changed no hardware.
2. IRQ12 was rejected without creating a proposal.
3. An existing backup filename was rejected before hardware access.
4. Fast boot was saved off → on with firmware acknowledgment.
5. An earlier IRQ proposal was rejected as stale after that configuration change.
6. A warm reboot retained the fast-boot setting and returned the serial login.
7. Rollback staged and saved fast boot back to off.
8. After a second warm reboot, all 768 configuration bytes exactly matched the
   original baseline. ELKS `/bootopts` remained unchanged in this phase.

## IRQ5 and networking

I then saved NIC IRQ3 → IRQ5 and installed this ELKS boot option:

```text
ne0=5,0x300,,0x92
```

The firmware acknowledged save. After reboot, `pmcfg` reported IRQ5 and the ELKS
driver reported port 0300h, IRQ5, a 16 KiB buffer and flags 0x92. The serial login
returned and `ktcp -b -p ne0` opened the adapter successfully.

| Test | Result |
| --- | --- |
| DHCP | Address 10.0.0.67, /24 mask, gateway 10.0.0.1 |
| Gateway ICMP | 3/3 replies, 0% loss; min/avg/max 30/30/30 ms |
| HTTP by hostname | `urlget -h http://frogfind.8088retro.com/` exited 0 |
| HTTP response | `200 OK`; 1,620 bytes including headers, 1,273-byte body |
| TCP after download | 0 retransmissions, 0 dropped packets, 0 bad checksums |
| Final configuration comparison | Only byte 215 changed: IRQ3 → IRQ5 |

I left the machine on IRQ5 with `ktcp` running and network startup set to manual.
I preserved the original ELKS boot settings and PicoMEM rollback records on the
machine. I did not start any application servers.

## What this does not establish

- SD-file persistence across removal of power: PicoMEM remains powered across
  the warm reboot used here. No independent SD readback or cold-cycle test ran.
- Compatibility with other PicoMEM boards, BIOS dates or modified firmware.
- Live IRQ switching, long-duration throughput, or browser UI behavior.
- Stock 512 KiB memory requirements: this PPC had expanded RAM.

## Screenshots

[Staged IRQ preview](screenshots/staged-irq-ppc512.png) was captured before the
IRQ5 proposal was applied. [Network results](screenshots/network-ppc512.png)
show `ifconfig`, the downloaded HTTP status line, and `netstat` after the test.
Counters differ from earlier readings because traffic continued between captures.

Both images come from the 4,000 character/attribute bytes at physical B8000h,
read via `/dev/kmem`, rendered with the 1,024-byte ASCII font captured at BIOS
FFA6Eh. Scanlines were doubled for the CGA display aspect ratio. Cursor/blink
phase is not reconstructed. The renderer is [tools/render-text.py](../tools/render-text.py).

Raw memory, configuration backups, BIOS font dumps and full serial logs remain
local; they are not part of this repository. The PNGs contain the real test
machine's LAN address and MAC address.

## Complete information readout

I captured the [complete `pmcfg -v` output](examples/pmcfg-verbose.txt) over
serial after the IRQ5 test. Its 40 lines include all fields currently decoded
by the viewer and the raw 256-byte configuration prefix. Extended configuration
fields are not decoded.
