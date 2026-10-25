# Draft message for FreddyV

Historical, unsent configuration-tool draft. The public source snapshot is now
linked below. For the current USB-mount/long-filename question, see
[X-LFN-DRAFT.md](X-LFN-DRAFT.md).

---

Hi FreddyV,

I've been running ELKS on an Amstrad PPC512 with a PicoMEM 2 and put together a
small native 8086 utility, `pmcfg`, to inspect and manage selected PicoMEM settings
from ELKS. Your shared-memory layout and firmware command interfaces made this
possible.

It can display the configuration, stage and preview NIC IRQ or fast-boot changes,
save through the firmware with a required verified backup, and stage a rollback.
Changes are applied through a separate reboot; it doesn't attempt live device
reinitialization.

On the PPC512 I used it to change the NIC from IRQ3 to IRQ5, matched ELKS's
boot option, rebooted, then obtained DHCP and downloaded a page with HTTP 200.
The repository includes screenshots from the actual machine's framebuffer,
source, tests and the tested ELKS binary.

It's experimental and currently restricts editing to the tested PicoMEM 2 / BIOS
2026-06-16 profile. I verified firmware acknowledgment and warm-reboot behavior,
but haven't independently read back the SD configuration or tested a full power
cycle. I developed it with AI coding assistance and tested it on the real hardware.

Project: https://github.com/l00nix/picomem-elks-public

I'd welcome your thoughts on the supported way to identify configuration layouts,
save settings and verify persistence from an OS like ELKS, and whether there are
firmware-version details I should account for before expanding support.

Thanks for PicoMEM!
l00nix
