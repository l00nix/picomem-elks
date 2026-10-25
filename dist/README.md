# ELKS executables

`pmcfg` is the exact executable installed and tested on the Amstrad PPC512.
It is an ELKS i8086 small-model executable, not a Linux x86-64 or DOS program.

- Version: 0.2
- Size: 11,824 bytes
- SHA-256: `48357c11242a06e219a862215a46597f31a6107a5c6592dd1abec3b509219be5`
- MD5, for comparison with ELKS `md5sum`: `03c3f450610fa602626545fb6268f787`
- Compiler: ia16-elf-gcc 6.3.0
- Build tree: prepared ELKS `e1469b64` libc/toolchain
- Target tested: ELKS 0.9.2 `e7a56a43`, i8086, on a PPC512

The v0.2 source is available at commit `13b14bd`. The current source builds
`pmconfig` 0.5-dev below. Host tests exercise logic; they do not run the native
executables. Each recorded native build completed with `-Wall -Wextra` without
diagnostics.

The IRQ5 save, reboot, DHCP and HTTP test used this exact binary. A previous
fast-boot save/rollback test used an otherwise identical build with a different
success-message string.

## Development candidate: 0.3-dev

`pmcfg-0.3-dev` includes the expanded `--details` report. I ran it successfully
on the PPC512 on 23 October 2026 before testing 0.4-dev. It does not replace the original `pmcfg` above.

- Size: 15,008 bytes
- SHA-256: `e8ed3e427217a1e4e04fea58b9488437c00d87acd4461a73b3f9fa163a25b2a6`
- MD5: `7b1fcd3373f864e1ffbe9880db69ee18`
- Built 22 October 2026 with the same IA16 compiler and prepared ELKS tree.
- Source at commit `ec14d81`: `pmcfg.c`, `editor.c`, `details.c`.
- Validation: all 58 host tests/scenarios pass; native build without warnings;
  generated native transaction inspected with no calls between CLI and POPF.
- Existing 1,700-byte `PMCFG02` stage/backup files remain compatible.

The earlier offline replay has now been followed by native PPC512 execution.
The newer 0.4 capture set below includes the same expanded information.
SD persistence verification remains pending.

## Tested development build: 0.4-dev

`pmconfig-0.4-dev` preserves the expanded report, bounded support dump, passive status
and active memory-map query. The names `pmdump`, `pmstat` and `pmmem` select
companion modes of this same executable.

- Size: 19,744 bytes
- SHA-256: `3688c9a4c39ae04559c495d296ad1b3dc39fbd70f3d0d2d23bc39935d47500f9`
- MD5: `56cdd47ffca7eb0d5b16a6bbf95d30df`
- Built 23 October 2026 with ia16-elf-gcc 6.3.0 and the prepared ELKS `e1469b64` tree.
- Target: `-march=i8086 -mtune=i8086 -mcmodel=small -melks-libc -Os`,
  1,024-byte heap and 2,048-byte stack; `-Wall -Wextra` produced no diagnostics.
- Sources: `pmcfg.c`, `editor.c`, `details.c`, `dump.c`, `query.c`.
- All 85 host tests/scenarios passed; native query/save critical sections
  inspected for compiler-inserted calls.
- Verified by FTP readback and ELKS MD5 before native execution.
- Hardware tests: summary/details/raw report, passive status, exclusive support
  file creation and readback, BIOS-only query, all 64 memory blocks, stage/show
  with the existing IRQ5, and HTTP/download checksum after the queries.
- No save, reboot, RAM write probe or configuration change in this round.
- `PMCFG02` staging/backup records remain compatible.

[Results, screenshots and limitations](../docs/HARDWARE-TEST-20261023.md).
The original v0.2 executable remains unchanged.

## Tested development build: 0.5-dev

`pmconfig` is the current binary. It adds explicit `--usb` and `--disks` status
queries; plain `pmstat` remains passive. The existing names still select modes
of the same executable. No mouse-enable or WiFi command is implemented.

- Size: 21,088 bytes
- SHA-256: `62b4d9dadbd00c9ba517b765b11887afb310038a953b31732d6d9065cbcfca18`
- MD5: `ab0a42b5cd55a5757004ddc32f636c97`
- Built 23 October 2026 with the same IA16 compiler, ELKS toolchain, flags,
  heap and stack settings recorded above; no compiler warnings.
- Adds `status.c` to the existing source files; build with `build.sh`.
- All 114 host tests/scenarios pass, including 28 status-transaction cases.
- Generated assembly: all three CLI-to-POPF paths checked, including failure
  branches; no function calls in the critical sections.
- Hardware: repeated USB/disk queries completed after 2–3 polls; Logitech Mouse
  detected; correct disk image/geometry; unchanged configuration and a
  checksummed HTTP download afterward.
- Installed under the existing four `/bin` names after FTP readback and MD5
  verification. The 0.4 binary is retained as `/root/pmold04` on the PPC512;
  the original `/bin/pmcfg` v0.2 remains unchanged.

[Hardware evidence and screenshots](../docs/HARDWARE-USB-20261023.md).
