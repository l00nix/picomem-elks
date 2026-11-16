# Hotplug candidate FAT16 qualification passed

REDX20 was read once, unmounted, on shop after the hotplug candidate's bounded native FAT16 writes. Full-device extent: 252,182,528 bytes. Local and remote capture SHA256 agree: b812813ccf0b949db611db2bcb072dee71ca36288ed560c9eb7c49588fe0db5e. No physical-device writes, repair or formatting were requested during this check.

Independent QA verified all ten live files (eight test/reference files and two Windows metadata files). All nine pre-existing files, directory entries, allocation chains, and full cluster contents including slack are unchanged. The only retained additions are hp1115 and its 28-byte proof file, using two clusters. Both FAT tables agree, with no leaks, crosslinks or cycles; 16,149 clusters remain free. Directory parent links and existing long-name entries are valid. The 83 changed freed clusters are accounted for by the bounded temporary workload; identical pre-existing residue was not miscounted as newly written.

Boot/reserved bytes, the 16 KiB guard, and the entire remaining physical-device tail match the baseline exactly. Read-only fsck and mtools checks on the captured regular-file image passed and left it unchanged. REDX20 remains unmounted on shop.

This closes FAT16 qualification of the hotplug pair following empty boot, late insertion, same-device reconnect and bidirectional media swaps without reboot. FAT32 writes on this pair remain pending. Surprise removal while mounted, power-loss recovery and full-capacity addressing are not qualified by these checks. No production code or binary changed during qualification; the sealed source review archive remains unchanged and unpublished.

Evidence: fat16-return/, qa/fat16-final-image-audit.json and the saved PPC/QA write evidence.
