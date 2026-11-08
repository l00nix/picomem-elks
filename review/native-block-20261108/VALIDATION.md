# Validation status — 8 November 2026

This records evidence scope, not a release certification. The earlier nine-media ELKS–Mac–ELKS and DOS tests exercised the superseded firmware file service. They do not qualify this native block path or its current USB ownership configuration.

## Native-path hardware milestones

- REDX20 (MBR/FAT16) and PMBLUE (MBR/FAT32) passed bounded create/copy/append/truncate/delete operations, unmount/remount, independent Mac file hashes and read-only FAT checks, and read-only return checks. PMBLUE's complete 600 MiB filler was checked on the Mac, not transferred in full in every PPC pass.
- Alias append/truncate and invalid deletion-name checks passed on both media, preserving existing long-name entries. These do not implement new long-name creation.
- The sector-wise FAT scan passed 6,000 host cases, two 8086 builds and 158 emulator checks. On the same PMBLUE workload, first allocation fell from 140.57 to 22.18 seconds. No general speedup guarantee or matched FAT16 timing is claimed.
- The latest label-only kernel was installed and readback verified. Optional USB `fdisk -l` listing passed host fixtures, emulator checks and a live listing. The user subsequently reported successful manual mount/read-write tests on three FAT media; those are user observations, not a new independently hashed three-media qualification round.

These tests occurred across successive native-path kernel/firmware revisions. Earlier successful writes on REDX20/PMBLUE are not silently promoted to write qualification of the newest controller/recovered-write firmware.

## Current installed source identifiers

| Artifact | SHA-256 |
| --- | --- |
| Kernel, 120,240 bytes | `065b709fa39d471533cb5a92d417405c2a6da60bda3c14967aff3231e9c6e766` |
| fdisk, 7,248 bytes | `cea44d02f8f5fb0c133c957ddfb3af115929a8c61b61eb4dcf0d6ed3655c192b` |
| Firmware binary, 4,804,496 bytes | `12e9bca3b5acc7d60c60c3b4429ec7cbd2ef7bf981ae4c2a3553847134aba5eb` |
| Firmware UF2 | `1172a9bd1731d3a7d0c6e50b9f250a1aaa4686d383a96a79aa84ca0a08016aa3` |

Build timestamps can change binary hashes on rebuilding. The source manifest establishes the reviewed source bytes, not a promise of bit-for-bit reproducible timestamps/toolchain output.

## Unresolved observations

1. On the current write-enabled firmware, the original generic stick produced one full-length read with an incorrect hash. The initial harness retained the hash/state but not the differing bytes. Two repeats matched. Independent Mac checks passed all 20 reference files and the read-only FAT32 check. Subsequent PPC-local checksums, retained FTP captures, cold-start checks and raw-sector reconstructions matched; PMBLUE also passed the same-payload comparison. The original failure remains unexplained. Successful repeats are not a fix; the next recurrence needs its first differing bytes preserved before retries. Assistant-driven write qualification on this firmware stopped before writes.
2. After a user-reported `shutdown -r`, `fdisk -l` returned `Error opening /dev/ssd (5)`. The rebooted kernel's first INFO response already contained the firmware fault flag, before partition reads; the kernel then refused further I/O. A single listing replay left the guarded state unchanged. The user later reported that full power cycling worked. Warm reboot can preserve MCU state, but the original fault trigger is unknown. Do not present this as filesystem damage or a solved recovery bug.
3. The explicitly enabled empty-cache policy assumes write-through only after two valid cache responses omit cache settings. That is a compatibility assumption, not proof of device durability. Explicit write-back caching requires checked flushes. Acknowledged writes need not survive power loss.
4. Buffered-write-error visibility and release ordering have local corrections and fault-injection coverage, but still require maintainer review. Successful application exit or `sync` alone is not proof of durable storage. Ambiguous writes/flushes are not automatically retried.

All full media captures, private recovery backups and detailed hardware logs remain local. This public snapshot publishes code, reproducible host harnesses, source hashes and an evidence summary, not private media data. No firmware flash, new PPC test or hardware reboot was performed to publish it.
