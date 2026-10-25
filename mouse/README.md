# Experimental PicoMEM mouse driver

I am developing an optional ELKS kernel driver and a bounded `pmmouse` capture
program, with AI coding assistance. This is experimental code, separate from
`pmconfig` and its configuration editor. It is not enabled by the normal build.

The first supported profile is deliberately narrow: the PPC512's PicoMEM 2,
board 11, ROM at D000 dated 2026-06-16, primary IRQ7, and direct NE2000 IRQ5 at
300h. A CRC16 of the installed ROM's IRQ routine must match. USB must be enabled;
Sound Blaster and joystick must be disabled. Unknown profiles are refused.

DOS PMMOUSE is a resident interrupt driver. This ELKS implementation requires
an optional kernel build because ELKS does not provide an equivalent loadable
interrupt-driver mechanism. No serial mouse or COM port is emulated.

[Hardware status and screenshot](../docs/HARDWARE-MOUSE-20261023.md).
For the combined mouse and USB/SD filesystem build, use the
[kernel integration](../kernel/README.md). Unmount both PicoMEM volumes before
opening the mouse device, including for a passive check.

The first active capture received one zero-motion, no-button report and
restored IRQ7 cleanly. Movement, button delivery and sustained input have not
yet passed hardware validation.

## Build and test

The patch installer requires a checkout of ELKS commit `e7a56a43`. It does not
change boot files on a running machine.

```
python3 mouse/apply.py /path/to/elks
```

Configure `CONFIG_MOUSE_PICOMEM=y`, keeping the appropriate machine settings.
Generate the keymap configuration before configuring a fresh tree. Finish
configuration before compiling; do not run configuration and compilation as
parallel make targets. Clean the kernel after changing configuration.

```
. ./env.sh
make elks/arch/i86/drivers/char/KeyMaps/config.in kconfig
make menuconfig
make kclean
make kernel
```

Build the user program with a prepared ELKS compiler/libc tree:

```
TOPDIR=/path/to/prepared/elks sh mouse/build.sh
```

Run the real driver's logic under host stubs and sanitizers:

```
cc -std=c99 -Wall -Wextra -Werror -Wno-sign-compare \
  -fsanitize=address,undefined mouse/tests/test-driver.c -o /tmp/pmmouse-test
/tmp/pmmouse-test
```

The tests cover 30 driver scenarios and all 524,288 combinations of X, Y and
three mouse buttons. They exercise passive open, profile rejection, IRQ
ownership and verified BIOS handoff, queue wrap/overflow, acknowledgment races, interrupt-state preservation, flood bounding, command
errors/timeouts and repeated arm/stop. These are synthetic tests, not proof of
physical event delivery.

## Runtime

Create a root-only character device on the test system:

```
mknod /dev/pmouse c 4 33
chmod 600 /dev/pmouse
pmmouse --check
pmmouse --run
```

`--check` only validates the profile and opens/closes the queue. `--run` claims
IRQ7, enables reports, captures for at most 30 seconds, then disables reporting
and releases IRQ7, restoring its previous BIOS vector and mask bit. Ctrl-C, termination and hangup also request cleanup. Capture
output is limited to the first 100 packets; counters cover the capture period.

The device emits three-byte PS/2-style packets. USB Y is inverted; the -128 USB
Y case is split across two packets for ELKS readers that decode signed 8-bit
movement. Whole reports are dropped if the queue lacks space. Only three
buttons and X/Y are supported. Wheel/extra buttons are not implemented.

IRQ acknowledgment uses the fast sideband command 01h, which does not overwrite
an in-progress disk/management command. Command 31h is not used. The driver
preserves other interrupt requests; an unexpected source or an ambiguous
command failure masks IRQ7 and retains ownership. A reported fault requires
a reboot; it must not be worked around by repeatedly enabling the mouse.

Do not run PicoMEM management/configuration commands, DOS BIOS mouse drivers,
or another PicoMEM interrupt consumer during a capture. Disk I/O and the
separately routed NE2000 are intended to remain usable, pending hardware tests.
The 16-bit diagnostic counters wrap after 65,535.

A saved `/linux` alone is not a recovery boot menu. Keep access to the SD image
or a working recovery disk before replacing a kernel. Preserve the original
kernel and boot options, and verify all transfers. This tree does not install
a kernel automatically.

## Interface audit

The installed ROM was read without enabling the mouse. Its IRQ entry is at
D000:2C1E. The mouse branch is at 2D4B, and calls `INT 33h, AX=0060h` at 2D7E.
CRC16-CCITT (initial FFFF) over [2C1E,2D91) is 596F. The public BIOS assembly
contains an older handshake; it is not sufficient by itself for this firmware.
The firmware C++ and installed ROM agree on the current shared IRQ structure.

This is a dedicated ELKS input driver, not a port of the entire DOS CuteMouse
resident driver. Application integration and graphical cursor testing are
separate follow-up work.
