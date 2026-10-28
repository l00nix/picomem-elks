# PPC512 long-name creation evidence

These files record the actual PPC512 creation/write tests on 28 October 2026.
The screenshot uses 4,000 bytes of native B800 video memory and the captured
BIOS font. Its shell script prints commands and runs the native programs; no
listing or filesystem result was synthesized.

`hardware.log` contains the native creation probe, mount checks and reboot
checks, with terminal CR bytes and trailing whitespace normalized. The log
manifest records the original and published hashes. `old-firmware-compat.log` records the initial refusal with the older
metadata-only firmware. The 32 ROM bytes contain the exact metadata and creation
descriptors observed from ELKS after flashing. The three readback manifests
record independent host comparisons of the complete expected payloads, downloaded
through ELKS immediately, after unmount/remount and after normal reboot.

`direct-mac-readback.json` records matching full contents and exact directory
entries through macOS after moving the unmounted stick. The FAT check in
`mac-verify-volume.log` ran `fsck_msdos -n` and returned 0. The Mac-to-PPC roundtrip manifest and `ppc-alias-readback.json` record full-byte
matches through all three long paths and their short aliases after returning the
stick. The second native screen shows both paths resolving to the same ELKS
file. DOS verification of this creation build remains pending. No full firmware backup, host credentials or personal host paths are
included here. The protected recovery copies remain local.
