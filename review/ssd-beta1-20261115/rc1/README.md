# PicoMEM USB Beta 1 — release candidate consolidation

**Experimental RC1, 15 November 2026. Offline qualification only; not installed, published or approved upstream.** This work was developed with AI assistance and independent QA at the author's request. Maintainers still need to judge its suitability for ELKS and PicoMEM.

The candidate uses the existing ELKS SSD interface and native MINIX/FAT filesystems. PicoMEM provides individual 512-byte sector transfers through its existing command/data ports; it does not translate paths or implement an ELKS filesystem. Internal SD image boot stays on the existing BIOS path. `ssd.c` is unchanged. The Amstrad display shortcut is a separate patch.

## Scope for Beta 1

- One directly connected USB mass-storage device, present before a cold boot; 512-byte physical sectors.
- A **flat whole-device filesystem at sector zero**, accessed as `/dev/ssd`. Current hardware evidence covers an 8 MiB MINIX fixture, an 8 MiB FAT16 fixture on REDX20 and a 48 MiB FAT32 fixture on PMBLUE. These are fixture sizes, not stick capacities.
- Normal native ELKS `mount`, file operations, buffers, `sync` and `umount`. FAT uses existing short-name behavior. Full long-filename creation is not implemented.
- Windows–ELKS FAT16/FAT32 transfer evidence exists for the predecessor binaries. The exact new candidate still needs PPC qualification.

MBR partitions, `/dev/ssd1`, hot swapping, hubs, multiple devices/LUNs, exFAT, raw SD access and the earlier file-service implementation are outside this candidate. The prior nine-media results belong to a superseded implementation and do not qualify this one. The generic stick's earlier intermittent mismatch is not claimed resolved by the present two-media results.

## What is being consolidated

`dependencies.lock.json` pins the ELKS, PicoMEM, TinyUSB, Pico SDK and auxiliary sources and every patch hash. `prepare.py` reconstructs from Git objects and verifies the upstream case-sensitive USB-host path mapping. No mutable historical working-tree files are build dependencies.

The ELKS patch series separates the small SSD backend and wiring from native FAT corrections: mirrored FAT updates, FAT32 directory high-cluster clearing, root-parent encoding, FSInfo invalidation, raw-offset bounds, geometry admission and FAT32 reserved-bit preservation. The last three patches arose from this failure-path review. They reject unsupported requests or preserve existing FAT data; they do not introduce a new lifecycle or error-accounting subsystem.

The firmware retains the previously tested handshake, sector ownership, finite deadline and cache-policy behavior. Header-only cache replies use the recorded assumed-write-through compatibility policy; they do **not** prove caching is disabled. Known enabled caching requires successful flushes. The TinyUSB patch reports actual transfer completion/length and keeps failed exchanges from being reused. The old broad recovery/controller patch stack is not included. The new firmware header wording is a comment-only correction and produces identical BIN/UF2/ELF bytes to the build before that comment correction.

## Review and release gates

See [FAILURE-PATHS.md](FAILURE-PATHS.md), [BUILD.md](BUILD.md), [HARDWARE-GATE.md](HARDWARE-GATE.md) and the JSON evidence beside this file. The initial source review records findings; `qa/guard-source-review.json` records their correction. Do not read the initial review as final approval.

Host models cover errors and boundary conditions but do not simulate electrical timing. Emulator FAT tests run the actual candidate kernel on disposable BIOS disks, not a PicoMEM USB controller. Passing either does not substitute for the final PPC run and independent media check.

Before calling this Beta 1 for test use: qualify the exact frozen kernel and ON firmware on the PPC; repeat bounded FAT32 and FAT16 write/remount and independent filesystem checks; verify recovery artifacts; then request maintainer review of the small separated patches. Firmware command allocation, the TinyUSB interface change, shared PIO/READY handling and acceptance of native FAT fixes remain review questions. No maintainer approval or power-loss guarantee is implied.
