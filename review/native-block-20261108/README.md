# Native ELKS/PicoMEM block-device review — 8 November 2026

This is a source review snapshot of the experimental implementation tested on an Amstrad PPC512 with PicoMEM 2. It replaces the earlier `picofs` file-service architecture. It is not a release candidate, a request to merge the complete series, or evidence of maintainer approval. The source includes AI-assisted development and physical-machine testing.

## Architecture

```text
ELKS applications / normal mount and umount
  -> ELKS VFS, native msdos filesystem and buffer cache
  -> existing SSD block driver and partition scanner
  -> optional PicoMEM SSD backend / versioned raw-sector mailbox
  -> PicoMEM USB transport -> TinyUSB -> USB mass storage
```

ELKS owns FAT interpretation and filename handling. PicoMEM transfers sectors and manages USB ownership, media generation, completion checks, write preparation and faults. Firmware FatFs is excluded from the USB medium while this raw mode is enabled. Boot remains on PicoMEM's internal SD image. USB-backed DOS file services and BIOS images are not concurrently available in this configuration.

The current supported/test scope is **one directly connected USB device, LUN 0, 512-byte sectors, with an MBR primary FAT16 or FAT32 partition**. GPT, exFAT, hubs/multiple mounted drives, surprise removal and other sector sizes are outside this snapshot's qualification. This is a prototype boundary, not a general claim about upstream ELKS or PicoMEM support.

The whole device is `/dev/ssd`; partitions are `/dev/ssd1` through `/dev/ssd4`. The normal command is `mount -t msdos /dev/ssd1 /mnt/usb` (create the mount directory first). Use `mount -r -t msdos /dev/ssd1 /mnt/usb` for read-only inspection. A separate userspace change lets bare `fdisk -l` try `/dev/ssd`, silently skipping absent media but reporting actual I/O errors. Interactive partition editing is not established by that listing change.

**Long-name creation/full long-name read/write support is deferred pending architectural feedback.** Existing native bounded long-name lookup and 8.3 aliases are present, with corrections for alias matching and invalid deletion names. New names remain 8.3; `mv` uses copy/delete where native rename is unavailable. This is not the earlier firmware-managed long-name implementation.

## Source and review boundaries

| Area | Source | Review scope |
| --- | --- | --- |
| ELKS | [Ordered patches](elks/series) | Generic FAT/VFS fixes, SSD backend/partitions, write-error lifetime, allocation scan and fdisk listing |
| PicoMEM | [ABI 2 writer and integration](firmware/01-current-abi2-writer-and-build-integration.patch) | Raw-sector ownership, read/write/flush, preparation, completion checks and sticky faults |
| TinyUSB | [Pinned dependency patch](tinyusb/01-msc-completion-recovery-and-control-length.patch) | Transfer-completion evidence, bounded cache-query CSW recovery, control-transfer length correction and controller selection wrappers |
| RP2 controller | [Imported controller plus local hook](tinyusb-controller/01-rp2-controller-import-with-local-clear-stall-hook.patch) | Upstream source import with a separately documented, guarded clear-stall/DATA0 hook |

ELKS patches 01–08 are generic FAT changes; 09 is mount handling; 10–11 add partition/SSD integration; 12 is the unrelated optional Amstrad display shortcut; 13 is test support. Later patches 14–19 capture write-error/lifecycle corrections, alias/deletion validation, raw address units, FAT allocation scanning, diagnostic naming and optional USB listing. These are review groups reconstructed from tested source, not nineteen independently qualified upstream PRs. Apply the complete series in order to reproduce this snapshot; dependencies and tests span groups.

The RP2 files were imported from TinyUSB commit `871b843245666786857f54b7ffc1454d4acad7a0` ([upstream PR #3561](https://github.com/hathach/tinyusb/pull/3561)). That substantial upstream import is distinct from our local fixes. Its original copyright/MIT notices remain. The current `hcd_rp2040.c` includes a local guarded idle bulk-IN toggle reset after CLEAR_FEATURE; the other two imported files remain byte-identical. [Provenance](tinyusb-controller/provenance.json).

PicoMEM's starting point is the published **read-only** raw-block proposal, [PR #168](https://github.com/FreddyVRetro/ISA-PicoMEM/pull/168), at the pinned commit below. This snapshot publishes the subsequent write/controller work separately; it does not update that upstream PR or submit the TinyUSB changes upstream.

## Exact bases and reproduction

| Component | Base revision |
| --- | --- |
| [ELKS](https://github.com/ghaerr/elks) | `052b4be698d9fb780ebdd9c9510ecdc4c2f9dd33` |
| [PicoMEM fork](https://github.com/l00nix/ISA-PicoMEM) | `25566f256af1013e20e2035027a641d79e17fa94` |
| [TinyUSB](https://github.com/hathach/tinyusb) | `331c263400dd5ccf77264cab1fa6bbd9f374b44e` |

[Source manifest](source-manifest.json) gives the SHA-256 of every changed file. Fresh index replay verifies all 52 changed ELKS files, 7 PicoMEM files, 3 imported controller files and 6 TinyUSB files against the working sources. The firmware base already contains the read-only transport and dependency setup; the firmware patch here is incremental to it. Historical base documentation describes ABI 1, not this ABI 2 snapshot; use the current protocol headers/source and this document for the new candidate.

Use disposable clean checkouts at those exact bases. Set `PACKAGE` to this folder's absolute path. For ELKS, from its checkout root:

```sh
while IFS= read -r patch; do
    git apply --check "$PACKAGE/elks/$patch" || exit 1
    git apply "$PACKAGE/elks/$patch" || exit 1
done < "$PACKAGE/elks/series"
```

Apply the `firmware/series` and then `tinyusb-controller/series` to the **firmware root** in the same way. Prepare its pinned dependencies with `./prepare-raw-deps.sh`. That helper applies an older TinyUSB patch: reverse it before applying the cumulative TinyUSB snapshot, to avoid applying it twice:

```sh
git -C src/lib/tinyusb apply -R "$PWD/patches/raw-usb-tinyusb.patch"
while IFS= read -r patch; do
    git -C src/lib/tinyusb apply --check "$PACKAGE/tinyusb/$patch" || exit 1
    git -C src/lib/tinyusb apply "$PACKAGE/tinyusb/$patch" || exit 1
done < "$PACKAGE/tinyusb/series"
```

Alternatively start with a pristine TinyUSB checkout at its pinned base, apply only this cumulative patch, and place it at firmware `src/lib/tinyusb`. The controller include wrappers assume that layout. Do not run the older local `prepare-rw-lab-deps.sh` or apply historical completion patches on top.

The tested ELKS configuration is [configs/elks-ppc.config](configs/elks-ppc.config); use the ELKS base's normal toolchain/build process. It retains the display shortcut. Firmware uses Pico SDK `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` (2.2.0), an Arm bare-metal toolchain, board `pimoroni_pico_plus2_rp2350`, platform `rp2350-arm-s`, and these experimental options:

```text
PM_RAW_USB_RW_LAB=ON
PM_RAW_USB_EMPTY_CACHE_WT=ON
PM_RAW_USB_RP2_COMPARE=ON
PM_RAW_USB_CSW_RECOVERY=ON
PM_RAW_USB_RECOVERED_WRITE=ON
```

The options default OFF and invalid combinations are rejected. This is build provenance, not a recommendation to flash a production system. Firmware reserves 4096/2048-byte main/core1 stacks; that layout check is not a measured bound on every call chain.

## Checks and limitations

[Validation and hardware status](VALIDATION.md) distinguishes the previously tested revisions from the current firmware and records unresolved failures. [Fresh packaging verification](verification-results.json) records patch replay and host checks rerun against reconstructed sources. These tests do not contact hardware or model all USB interrupt timing.

Portable regression commands after reconstruction:

```sh
# In the ELKS tree, run each tests/*/run.py supplied by this series.
python3 "$PACKAGE/tests/fat-scan/run.py" /path/to/elks
python3 "$PACKAGE/tests/transport/run.py" /path/to/firmware /path/to/firmware/src/lib/tinyusb
```

The transport harness extracts the actual host-control functions and combines actual MSC/writer code with simulated completions. Source hashes and `SHA256SUMS` cover the publication payload. No media contents, full flash backups, credentials or ready-to-flash binaries are included.

The next review gate is agreement on the native SSD/block architecture, provisional mailbox interface and ownership/error policies. Generic FAT fixes can be reviewed independently where dependencies permit. Full native long-filename work and release qualification remain follow-ups.

This work builds on [FreddyV's PicoMEM](https://github.com/FreddyVRetro/ISA-PicoMEM), [ELKS](https://github.com/ghaerr/elks), [TinyUSB](https://github.com/hathach/tinyusb) and the earlier utility references from [jdredd87's PicoMEM tools](https://github.com/jdredd87/CH375USBTools/tree/main/PicoMEM). Modified and imported code retains its originating project's licensing; this repository's GPL notice does not replace third-party license terms.
