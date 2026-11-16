# Reconstruct the current review candidate

Start here rather than following the historical RC1 README's hardware gate. The root review index and current evidence describe subsequent hotplug qualification. Production patches are unchanged from the recorded candidate.

Use a case-sensitive filesystem and Python 3.12+, Git, Clang with ASan/UBSan, CMake, make and the toolchains described in [the RC1 build notes](rc1/BUILD.md). From this directory:

```sh
python3 prepare-current.py --destination /absolute/case-sensitive/source
python3 hotplug/build.py --source /absolute/case-sensitive/source --toolchain /absolute/arm-toolchain/bin
python3 hotplug/qualify.py --source /absolute/case-sensitive/source --toolchain /absolute/arm-toolchain/bin
bash rc1/build-elks.sh /absolute/case-sensitive/source/elks /absolute/elks-toolchain/cross
```

`prepare-current.py` reconstructs all pinned upstream components, applies the RC1 patch series in lockfile order, then verifies and applies exactly the two hotplug deltas. For offline reconstruction, `--repositories /absolute/map.json` accepts a local map of component names to repositories containing the pinned commits. That workstation-specific map is not published. An alternative Git cache directory can be supplied with `--cache`.

The RC1 config retains the separate Amstrad display shortcut. That patch is not required for USB. No production filesystem code is replaced with host-side code. The current bridge requires the matching kernel and firmware; prior mailbox and file-service versions are incompatible.

The exported hotplug host runners accept reconstructed source explicitly:

```sh
python3 hotplug/qa/check-firmware-output.py /absolute/case-sensitive/source/firmware
python3 hotplug/qa/test-lifecycle.py /absolute/case-sensitive/source/elks
python3 hotplug/qa/backend/run.py /absolute/case-sensitive/source/elks /absolute/case-sensitive/source/firmware
```

They exercise actual driver functions against mocks. `rc1/verify-kernel.py` includes the historical RC1 backend test and is for the RC1 source before hotplug deltas, not the changed claim/release protocol. Its filesystem tests remain separately available. Emulator evidence is archived; replay also needs disposable root/fixture images, which are not included here.

The optional [fdisk probe](fdisk/fdisk-probe.patch) can be applied separately from the reconstructed ELKS root with `git apply /absolute/path/to/fdisk/fdisk-probe.patch`. It is not required for mounting and is not included by `prepare-current.py`. Its source, original source and recorded validation are included for review.

This snapshot deliberately does not publish flash images or device backups. The firmware build uses the recorded ARM toolchain and generated PIO checks; ELKS uses IA16 GCC 6.3.0. The complete dependency lock and patch hashes are provided, but cross-host byte-identical builds are not claimed. Build tools write their local logs/results next to their scripts; preserve the committed evidence before rerunning them.
