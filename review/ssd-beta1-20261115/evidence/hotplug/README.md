# Beta 1 orderly-hotplug candidate

This is an installed, unpublished extension to the frozen RC1 pair. The matching hotplug binaries passed the read-only lifecycle checks and independent FAT16 media verification. FAT32 data integrity passed after a separately verified continuation; the initial mkdir lost its network response and its exit status remains unknown. Development and QA used AI assistance. Maintainer approval remains pending. See HARDWARE-STATUS.md and KNOWN-ISSUES.md before assessing readiness.

The supported target is one directly attached drive with a flat native ELKS filesystem: boot without USB, insert a drive, mount normally, successfully unmount and close all raw-device handles, remove it, insert another, and mount again without rebooting. Automatic mounting, hubs, partitions, multiple devices and recovery from removal while mounted remain outside scope.

## Small lifecycle change

ELKS's existing SSD framework now discovers the PicoMEM medium at first open rather than boot. Its existing last-close flush and cache invalidation run before a new release hook. A closing flag rejects new opens while cleanup may yield. Capacity is cleared after final close. The native FAT/MINIX implementations and ordinary mount utilities are unchanged from RC1.

The PicoMEM bridge accepts claim/geometry operation 4 and release operation 3. Sector operations 1/2 require a claimed medium. Legacy operation 0 is rejected, so old kernels cannot silently use the new firmware without ownership. A zero-capacity geometry reply is normal when no supported medium is present. A clean release allows subsequent removal/reselection and resets the removed drive's cache policy.

Removing a claimed drive, any uncertain transport, or a failed write permanently blocks the session until power-cycle. Neither release, late completion nor a new insertion clears that fault. This is necessary because controller abort/retirement is not established. It prevents old requests from reaching replacement media. No additional TinyUSB change is included.

ELKS release and fsync return no storage status to umount. A successful umount alone is not proof of persistence; check diagnostics. A failed release prints `ssd: USB not released; power cycle required`. No automatic error recovery or power-loss guarantee is claimed.

Increment beyond RC1: ELKS +55/-7 across ssd.c, ssd.h and ssd-picomem.c (net48); firmware +29/-12 in pm_usb_ssd.cpp (net17). Kernel image117752 bytes,72 bytes larger than RC1. USB-disabled firmware BIN/UF2 are byte-identical to RC1.

## Validation

- Actual kernel backend:181 host checks; actual SSD lifecycle:110 checks in four configurations.
- Actual firmware transport/cache/ownership:574 checks, plus two independent output-removal cases (576 total, not an additional576).
- Existing command handshake202 cases, TinyUSB MSC38 cases and PIO11 checks passed.
- Native ARM builds with USB option OFF/ON passed; UF2/BIN, generated PIO and stack-layout checks passed. One initial build used the incomplete Homebrew ARM toolchain and failed for missing nosys.specs; logs are preserved. Successful builds use the same complete ARM15.3 toolchain as RC1.
- Native IA16 kernel build passed;95 emulator checks across FAT16, FAT32 and remount workloads passed, with no PicoMEM attached. Repeated missing-device mount attempts failed normally. Emulator images passed independent read-only FAT checks.

Host mocks do not reproduce physical timing. Physical tests now cover empty boot, two absent-media probes, late insertion, same-device reconnection and FAT16/FAT32 swaps in both directions without reboot. FAT16 writes and independent full-device verification passed. FAT32 continuation, release/remount and independent full-filesystem verification passed, including the full 34 MiB filler. The initial network interruption remains unresolved; this is not an uninterrupted FAT32 pass. Deliberate removal during writes and power-loss recovery were not tested.

## Reconstruct and review

Start with the frozen RC1 review archive SHA256 `c2cfb14f24840168230922109987b9ff2b0439849d7fffc39dca66a795898d6c` and follow its pinned-source reconstruction. Apply `patches/hotplug-elks.patch` from the reconstructed ELKS root and `patches/hotplug-picomem.patch` from its firmware root. Use RC1's configuration, separate display patch, dependencies and build instructions. These two delta patches are the entire production-code extension; `source-delta.json` records their hashes.

Kernel SHA256: `ae68c867c964ba0a895a5be7bc2d12a43e698551f11a8a67dbf9ac95919a2b52`.
Enabled UF2 SHA256: `ab48031e32ecdc944f4a4f85876d62a8eb04eecd52da88d353e11e407000cf0d`.

Install this kernel and firmware as a matching pair only. The current RC1 kernel/firmware and full-flash recovery backup must be retained. No shutdown, installation or flash is authorized by an offline test result; the user must be ready for the hardware handoff.

## Read-only timing follow-up

A single native df scan on read-only PMBLUE completed in 179 seconds with the exact expected free-space count and a successful telnet response. The passive trace lacks completion traffic, so continuous network responsiveness remains unmeasured. This does not resolve the original mkdir interruption. See df-timing-hardware/RESULT.md; production sources and sealed archives are unchanged.
