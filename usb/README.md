# Experimental USB file-service client

I built `pmusb`, with AI coding assistance, to test read-only USB root-directory
access from native 8086 ELKS through PicoMEM's embedded EtherDFS service.
On the PPC512, it listed ten entries from a Kingston DataTravelerMini after a
physical power cycle. [Hardware evidence and screenshot](../docs/HARDWARE-USB-FLASH-20261023.md).

This is separate from the normal `pmconfig` build. It does not mount a device,
read file contents, or create, delete, rename or write flash files. No mouse
driver is used. It requires root and the tested board-11 PPC512 profile with
ROM D000 dated 2026-06-16, primary IRQ7 and direct NE2000 IRQ5 at 300h.

```sh
TOPDIR=/path/to/prepared/elks sh usb/build.sh
```

On the PPC, the test binary is `/root/pmusb`:

```sh
/root/pmusb --check
/root/pmusb --list
```

`--check` validates the hardware/profile and DFS signature without a command.
`--list` requests at most 32 entries in the USB root, displaying sanitized DOS
8.3 names, byte sizes, type and attributes. Run only one file-service client at
a time: the firmware's directory search state is global. Mouse reporting and
other multiplexed PicoMEM interrupt consumers must remain inactive.

## Protocol and bounds

I examined PicoMEM source commit `be7b2e6031ae93bb75733c363489aa9171a7205c`,
especially `src/lib/my_etherdfs/{protocol.txt,ethersrv-pico.c,fs.c}` and
`src/pm_defines.h`. PicoMEM adapts Mateusz Viste's EtherDFS protocol to shared
memory and command `89h`; this client implements that interface directly.

The frame starts at D000:4FC4, 60 bytes before the disk buffer at D000:5000.
The actual PPC returned the service signature `12 34 AA` there. The client
accepts only logical drive U (USB), FINDFIRST `1Bh` and FINDNEXT `1Ch`. The
first search sends `\????????.???`: this firmware's mask converter does not
expand `*.*` itself. Request frames are 74/76 bytes, successful entry replies
84 bytes and end/error replies 60 bytes.

Each transaction saves FLAGS and disables interrupts, verifies command/source
idleness and unchanged configuration, saves 84 scratch bytes, sends its fixed
request, validates and copies the response, then restores scratch and FLAGS.
There are no ELKS syscalls inside that critical section, so the kernel cannot
interleave BIOS disk traffic. Polling is bounded to 262,144 status reads; this
is a count, not a calibrated time limit. Completed replies must have a changed
length, valid signature/sequence, bounded size and valid AX result. A command
error or timeout is never reset or retried, and scratch is not restored while
completion is uncertain. Stop testing and reboot after that diagnostic.

The firmware uses AX=18 both for end/no matches and directory-read failures.
An initially empty result is therefore reported as inconclusive and exits
nonzero. Even after entries are returned, the end marker alone cannot prove
there were no later read errors. No general filesystem reliability or hotplug
support is claimed.

## Validation

```sh
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
  usb/test-pmusb.c -o /tmp/pmusb-test
/tmp/pmusb-test
```

The hardware-free harness exercises the actual transaction code: 52 scenarios
cover immediate/delayed completion, find-next, end-of-list, busy/refused
profiles, every IRQ control byte, configuration changes, ignored commands,
timeouts, firmware errors, malformed lengths and sequences, and restoration
bounds. The native IA16 build was inspected for CLI/FLAGS restoration and
correct byte-port instructions, then tested on the real PPC.
