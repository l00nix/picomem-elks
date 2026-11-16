# USB Beta 1 review updates — 15 November 2026

The [published source-review snapshot](https://github.com/l00nix/picomem-elks/tree/8267942179e9cf06400c6b6c7ad60db781c71a75/review/ssd-beta1-20261115) contains the minimal SSD backend, matching PicoMEM transport, separate TinyUSB dependency, native FAT corrections, orderly-hotplug extension and optional probe-only fdisk patch. It is an experimental review candidate; no new binary release or upstream approval is claimed.

- [ELKS discussion: revised architecture and MBR question](https://github.com/ghaerr/elks/discussions/2841#discussioncomment-19644274). Clarifies that current FAT fixtures start at device LBA 0 (superfloppy layout), mounts use /dev/ssd, and MBR partitions are not exposed. Asks how best to reuse existing ELKS partition code, with the core backend reviewed first and other changes separately.
- [PicoMEM PR #168 progress update](https://github.com/FreddyVRetro/ISA-PicoMEM/pull/168#issuecomment-6905500239). The PR remains the earlier read-only draft at 25566f2; its branch has not been replaced with the new candidate. The new implementation is available in the source snapshot for review of command allocation, I/O/READY behavior, exclusive USB ownership and cache policy.
- [TinyUSB MSC issue #3985 contribution](https://github.com/hathach/tinyusb/issues/3985#issuecomment-6905503808). Describes the downstream completion-result/actual-length and validation changes, and asks how they should align with overlapping upstream work. No new TinyUSB PR has been opened.

The original FAT32 network interruption remains unresolved despite a separately recorded successful continuation and independent integrity checks. A slow df scan is also documented. Historical nine-media/file-service and earlier Windows-transfer results must not be used to qualify the exact current hotplug pair.

The repository update and all three comments were read back and verified after publication. The current review packet is source and evidence only; it excludes credentials, private device images and recovery backups.
