# Optional SSD probe for stock fdisk listing

This replaces the broader filesystem-detection proposal with the user's requested minimal change. Only elkscmd/disk_utils/fdisk.c changes: +11/-6 lines (net 5). The stock table renderer, argument handling and interactive editor are unchanged.

Default `fdisk -l` lists /dev/hda as before, then attempts a read-only listing of /dev/ssd. Missing/unavailable optional SSD responses (ENOENT, ENODEV, ENODATA, ENXIO) are skipped. Other open failures and short/read failures retain stock messages and failure behavior. An explicit operand lists only that operand and retains stock errors. Each successful listing reads only the first 512 bytes and closes the device; no filesystem scan or USB writes are introduced.

There is no filesystem recognition or new formatting. Consequently stock's interpretation of flat FAT/MINIX boot bytes as partition entries remains, including potentially misleading values. Printed /dev/ssd1..4 entries do not create device nodes or add partition-mounting support. The existing SSD driver can return ENODATA for failed initialization as well as no medium; an omitted SSD is therefore not proof of physical absence. These limitations are documented rather than expanding the patch.

The source is based on frozen ELKS commit a872443c7303d4572b7af27c55f8fc3f3dfe8175. No kernel, PicoMEM firmware or TinyUSB changes are included. The separate expanded classifier patch remains archived and is superseded for review; no public release or posting occurred.

Validation: 23 host stock-output/error comparisons and 31 native ELKS emulator checks passed. The present-SSD emulator case uses an ordinary fixture temporarily at /dev/ssd inside QEMU, not physical USB. The tested native binary is 7,264 bytes versus 7,184 stock (+80), SHA256 ad094766643cca4d1ed674f7283b62a4b4ef556cda4b41a516be493fab98ff43. QA independently extracted it from both prepared and tested root images. See tests/result.json, emulator/result.json and qa/final-review.json. Physical deployment and output comparisons are recorded separately in ppc/.

Patch: fdisk-probe.patch. Source: fdisk.c. Original: fdisk-before.c. The reviewed minimal patch applies independently of the driver work.

Live qualification: REDX20 was connected without reboot. Default output matches stock hard-disk plus stock USB tables exactly; explicit USB output also matches stock. USB remained unmounted. A logging-helper assertion about changing internal-root free space is preserved separately and resolved by offline evidence validation, not by repeating the test. See RESULT.md and ppc-redx20/validation.json.
