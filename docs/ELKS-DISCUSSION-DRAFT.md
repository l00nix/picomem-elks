# ELKS discussion: PicoMEM configuration tools for ELKS

Posted on 22 October 2026 in Show and tell:
https://github.com/ghaerr/elks/discussions/2841

The screenshots are GitHub discussion attachments. The source repository
was private at the time of that post. The text below is a historical record;
the current public snapshot is [picomem-elks-public](https://github.com/l00nix/picomem-elks-public).

---

For anyone using ELKS with [PicoMEM](https://github.com/FreddyVRetro/ISA-PicoMEM), I'm developing `pmcfg`, a native 8086 PicoMEM configuration utility for ELKS, tested on an Amstrad PPC512.

It currently provides read-only inspection and guarded next-boot NIC IRQ and fast-boot changes, with backups and rollback. I'm also investigating 512/640 KB profiles for testing ELKSWeb on real hardware with different amounts of RAM.

I'm using [jdredd87's PicoMEM tools](https://github.com/jdredd87/CH375USBTools/tree/main/PicoMEM) and PicoMEM's published firmware interfaces as references for this separate C implementation for ELKS. I'm also investigating which other tools from that collection would be useful to port to ELKS, potentially forming a small suite.

On the PPC512, I used `pmcfg` to change the NIC from IRQ3 to IRQ5, updated the ELKS driver setting and rebooted. I then obtained a DHCP lease and downloaded a web page with HTTP `200 OK`.

The screenshots below are framebuffer captures from the actual PPC512.

**Staging the NIC IRQ3 to IRQ5 change before saving:**

![PPC512 showing the staged PicoMEM NIC IRQ change](https://github.com/user-attachments/assets/f296399c-ec66-4c8c-8bba-fbf3125ddd75)

**Working ELKS networking after saving, matching the driver setting and rebooting:**

![PPC512 showing DHCP-assigned network settings, HTTP 200 OK and network statistics](https://github.com/user-attachments/assets/8747f368-bf59-4f11-a77d-7c82981ee85a)

The configuration editor is experimental and currently supports the tested PicoMEM 2 / BIOS 2026-06-16 profile. Warm-reboot behavior has been tested; independent SD persistence and full power-cycle verification are still pending. I developed it with AI coding assistance and tested it on the real hardware.

Project: [l00nix/picomem-elks](https://github.com/l00nix/picomem-elks) (currently private while I prepare it for sharing).

Would an optional external application, potentially growing into a small suite, integrated through `buildext.sh` following ELKSWeb's approach be useful? Would you prefer that arrangement or eventual inclusion in `elkscmd`?

l00nix
