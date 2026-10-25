# PicoMEM mouse integration for ELKS

## What is confirmed

On 23 October 2026, with a wireless USB mouse receiver plugged into the PPC512's
PicoMEM, `pmstat --usb` returned one device described as **Logitech Mouse**.
This establishes firmware detection/classification. It does not establish
pairing with the handheld mouse, delivery of movement/buttons, or ELKS input.
No mouse-enable command was sent in this test.

[Hardware results and screenshot](HARDWARE-USB-20261023.md).

## Firmware interface

The examined PicoMEM source uses `CMD_Mouse_Enable` (52h) to set a runtime
boolean, and `CMD_Mouse_Disable` (53h) to clear it. USB HID reports update the
PicoMEM-side mouse state. When reporting is enabled, the main loop copies X/Y
and button bytes into the shared IRQ structure and raises multiplexed source
`IRQ_R_MOUSE` (3). This is independent of NE2000's direct hardware interrupt.

For the supported board-11/768-byte profile, `IRQ_VAR_OFFS` is 854 relative to
the shared area (`ROM segment:4000`). The 14-byte IRQ structure is:

| Relative offset | Field |
| --- | --- |
| 854–859 | Six source request bytes; mouse uses byte 857 |
| 860–861 | In-service bitmask |
| 862 | PicoMEM IRQ requested |
| 863 | PicoMEM IRQ in service |
| 864 | IRQ nesting/count byte |
| 865–867 | Mouse X, Y and button bytes |

These are **profile-specific offsets**, not a general discovery mechanism.
They precede the status-query parameter region at 886. Reading three bytes
periodically would lose reports; the firmware does not expose a mouse event
queue there. Enabling reports without an interrupt owner is not an input driver.

The multiplexed IRQ acknowledgment is more than an ISA PIC end-of-interrupt.
The host must manage source requests/in-service state and the PicoMEM
acknowledgment path. `isa_pm_irq_ack()` checks `PM_IRR` before lowering the IRQ,
then clears `PM_ISR`. Blindly sending command 01h or clearing the whole request
table can lose another source's interrupt. The public BIOS assembly contains an older layout. I also inspected the
installed ROM: the current IRQ routine at D000:2C1E and mouse branch at 2D4B
match the firmware C++ shared-memory layout. The experimental driver checks
a CRC of that routine before accepting the profile.

On this machine, PicoMEM reports primary IRQ7 and NE2000 uses IRQ5. This avoids
sharing those two IRQ numbers, but it does not prove IRQ7 is free in ELKS. The
kernel must claim it through `request_irq()` and fail cleanly if occupied.
On the PPC, IRQ7 starts unmasked with the PicoMEM BIOS vector D000:2C1E.
The driver permits handoff from this verified BIOS handler and restores its
vector and mask on successful close; an unknown unmasked handler is refused.

## ELKS implementation path

I inspected ELKS source checkout `7e0d8ea8`, particularly
`elks/arch/i86/drivers/char/mouse-ps2.c`, `ntty.c`, `kernel/irq.c` and
`elkscmd/gui/mouse.c`. The running PPC512 kernel is a different recorded build,
`e7a56a43`; a driver patch must be checked against the actual build tree.

The existing PS/2 driver supplies a useful queue/open/close pattern, but its
8042 ports and IRQ12 are not the PicoMEM interface. My proposed implementation
is an optional `CONFIG_MOUSE_PICOMEM` driver with a dedicated input device,
for example `/dev/pmouse`:

1. At open, validate the exact firmware profile and IRQ routing, allocate the
   input queue, and claim the IRQ before enabling reports. Do not change the
   firmware, NIC IRQ, RAM map or persistent configuration.
2. Implement and test the multiplexed IRQ handshake. Copy mouse data before
   acknowledgment; handle or explicitly exclude other multiplexed users.
   BIOS disk I/O and command-port access need coordinated ownership, including
   a documented rule for running the user-space inspection tools while open.
3. Queue complete mouse packets and wake readers through the ELKS input/TTY
   facilities. Never enqueue half a packet on overflow. Include queue-overflow
   and unexpected-source counters for diagnosis.
4. Supply a byte stream understood by ELKS applications. A PS/2-compatible
   three-byte stream can reuse their parser, but USB and PS/2 Y directions
   differ. The signed -128 edge case needs splitting or other explicit handling;
   a naive eight-bit negation loses motion information. Initially document
   wheel/extra-button limitations rather than implying full HID support.
5. Let applications select `/dev/pmouse`. The inspected user-mode driver
   currently hardcodes `/dev/psaux` for `MOUSE_TYPE=ps2`; allowing `MOUSE_PORT`
   to override that default would preserve existing users and permit the new
   device. Audit other consumers separately rather than assuming all use this
   helper.
6. On final close or failed open, disable only the mouse reporting this driver
   owns, finish pending acknowledgment, release the IRQ and free the queue.
   Preserve other PicoMEM sources. Bound all waits and handle failure paths.

An experimental optional driver and bounded capture program now live in
[`mouse/`](../mouse/README.md). I built the kernel against the PPC512's exact
ELKS source commit, tested its logic under host sanitizers, and booted it on
the PPC. Passive profile validation passes. A 30-second capture received one
zero-motion, no-button report and stopped cleanly; sustained movement and
button delivery remain unverified. See the [hardware evidence](HARDWARE-MOUSE-20261023.md).
This is not yet general application mouse support.

## Validation before a hardware driver trial

Start with host tests for sign/button conversion, whole-packet queue overflow,
IRQ request/ack races, busy command ports, failed enable/disable and another
owner of IRQ7. Build an optional kernel candidate and keep the working boot
image/recovery path. On hardware, verify movement and buttons first, then input
while disk and network traffic run. Repeated open/close and application exit
must release the input and IRQ correctly. Only after that should mouse input
be exposed to a graphical application or ELKSWeb.

## Sources

- [PicoMEM mouse commands and event dispatch](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_cmd.cpp)
- [PicoMEM shared IRQ structure](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_libs/isa_irq.h)
- [PicoMEM IRQ signaling and acknowledgment](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/pm_libs/isa_irq.cpp)
- [PicoMEM HID mouse reporting](https://github.com/FreddyVRetro/ISA-PicoMEM/blob/be7b2e6031ae93bb75733c363489aa9171a7205c/src/USB_Host_driver/hid_app.c)
- [ELKS PS/2 input driver](https://github.com/ghaerr/elks/blob/7e0d8ea8ea87dbd4da768284887ad953be4bbaa0/elks/arch/i86/drivers/char/mouse-ps2.c)
- [ELKS application mouse reader](https://github.com/ghaerr/elks/blob/7e0d8ea8ea87dbd4da768284887ad953be4bbaa0/elkscmd/gui/mouse.c)
