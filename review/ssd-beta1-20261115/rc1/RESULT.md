# RC1 offline qualification — 15 November 2026

The candidate is consolidated for controlled hardware qualification and maintainer review. It is not yet Beta 1 qualified on the PPC, installed, flashed, published or approved upstream.

- Fresh pinned ELKS source build: **117,680 bytes**, SHA-256 `f19bbdbe1e71dc89e038e51a972dc64f63da5bb81dcf6ba0b678bf4b28a67e7a`. This is 560 bytes larger than the current tested FSInfo kernel.
- Fresh PicoMEM ON/OFF builds passed. ON UF2 SHA-256: `1a1834b2fd12f0b27904d80558cc2da215966a19185c2be244ffe6be99410ba4`; ON BIN: `951083146ca8c856bebfe47c11654afc4e50ee3f183881e1911a9d59173c6487`. The two comment corrections do not change behavior; the final firmware-header comment alone was also proven byte-neutral across all six ON/OFF artifacts.
- Firmware host checks: 106 enabled and 96 disabled handshake cases, 38 MSC completion cases, 11 PIO acceptance cases, 532 cache-policy cases. Both UF2/BIN conversions and stack layouts passed.
- Kernel host checks: 436 FAT-entry/mirror cases, 80 raw-boundary cases, 31 FSInfo cases, 91 geometry cases and 143 backend port/fault cases. ASan/UBSan passed. Packaged source-argument runners were executed successfully on the final reconstruction.
- Actual candidate kernel: **19 emulator cases, 277 command checks**. FAT16/FAT32 write/remount and read-only filesystem checks passed; nine malformed geometry cases left their images byte-exact; invalid FSInfo RW admission was rejected; FAT32 upper nibbles were preserved. A raw crossing transfer changed only the last valid byte below 64 MiB, with no block-zero alias. Exact-boundary reads/writes returned EFBIG.
- Independent source/limits/evidence audit passed. The audit records hardware and upstream-review gates separately.

The three new functional patches total +49/-3 lines (46 net): raw offset guard +10, geometry guard +34, FAT32 reserved bits +5/-3. They address four failure findings without widening the block interface, adding partition handling or replacing native ELKS writeback. A separate firmware header-comment patch removes an unconditional-flush implication.

`harness-note.md` preserves the failed test-driver attempts: a redirection/status-marker issue, a missing newline after a displayed raw marker, and an incorrectly calculated crossing offset. Images were unchanged in these incomplete attempts; they are not counted as passed cases. The corrected tests exercise the same unchanged candidate code.

The predecessor's latest FAT32 Windows round trip independently verified eight test files and exactly 13,800,448 free bytes. Its final shop image audit found only two access-date bytes changed by Windows, with all other filesystem/guard bytes unchanged. This is evidence for the predecessor pair, not a replacement for the exact RC1 hardware gate.

See `HARDWARE-GATE.md` for the pending frozen-binary test and `FAILURE-PATHS.md` for inherited buffered-error, cache-policy, lifecycle and durability limits. `qa/rc-audit.json` is the independent offline audit; any kernel staging is recorded separately and does not authorize installation or shutdown.
