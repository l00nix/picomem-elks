# Public configuration-tool scope

This repository maintains native ELKS configuration and diagnostic utilities based on published PicoMEM interfaces and examples from [FreddyV's PicoMEM](https://github.com/FreddyVRetro/ISA-PicoMEM) and [jdredd87's PicoMEM tools](https://github.com/jdredd87/CH375USBTools/tree/main/PicoMEM).

Current commands are `pmconfig`/`pmcfg`, `pmdump`, `pmstat` and `pmmem`. Their implementation is separate C code for ELKS; the DOS tools are references, not a directly compiled Pascal port. Configuration editing is limited to the documented next-boot NIC IRQ and fast-boot settings. Device status is metadata, not an operating-system driver or a storage mount.

See [interface and transaction notes](DESIGN.md), [support/query protocol](TOOLS.md), [native hardware evidence](HARDWARE-TEST-20261023.md) and [binary provenance](../dist/README.md). Known limits, firmware-profile checks and pending persistence tests remain applicable.

Public kernel, USB filesystem and mouse integration work is outside this repository's current scope. See the [discontinuation notice](USB-INTEGRATION-STATUS.md). The earlier broader roadmap is preserved in Git history.

## Hardware validation

The machine was available again on 23 October 2026. I ran 0.3 and 0.4 natively
over telnet/FTP, captured six real console screens, verified support bundles
and all 64 memory queries, and checked configuration equality and a checksummed
HTTP download afterward. [Complete results](HARDWARE-TEST-20261023.md).

The following persistence checks remain pending; none were performed in this
inspection/query round:

1. Preserve a verified stage/rollback record and copies of the SD configuration
   file before testing persistence. Record the initial fast-boot value. Stage
   only its opposite value, review the proposal and save with a fresh backup.
   Fast boot avoids altering the already working NIC or disk mapping.
2. Shut down ELKS cleanly. With someone at the machine, remove PicoMEM power
   fully, then start it and check the selected value, serial login, disk access
   and networking. A normal PC reboot alone is insufficient because the card
   can remain powered. Keep serial recovery and the original SD configuration
   available before the power test.
3. Restore the original fast-boot setting through a staged rollback with another
   backup. Repeat the cold-start check. If inspecting the SD directly instead,
   do so while powered down and compare the persistent file through a verified
   file-layout decoder; do not treat the shared 768 bytes as the entire SD file.

I would only add another editable setting after documenting its firmware
semantics, validation, next-boot effects and rollback, then testing it on the
identified hardware profile. Memory-map, boot-image and live NIC changes remain
outside this development step.
