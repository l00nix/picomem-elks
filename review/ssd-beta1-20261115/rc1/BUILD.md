# Reconstruct and build

Use a case-sensitive filesystem, Python 3.12+, Git, CMake, make, a native C/C++ compiler and the ARM GNU toolchain. Pins, patches and four exact upstream USB-host path mappings are in `dependencies.lock.json`. New source destinations are required.

```sh
python3 prepare.py --destination /absolute/case-sensitive/source
python3 build.py --source /absolute/case-sensitive/source --toolchain /absolute/arm-toolchain/bin
python3 qualify.py --source /absolute/case-sensitive/source --toolchain /absolute/arm-toolchain/bin
```

Offline `prepare.py --repositories /absolute/map.json` accepts local Git repositories containing the pinned commits. It reads Git objects, not their working trees. Workstation maps, credentials, device backups and hardware-control scripts are not public dependencies.

Firmware builds were performed on macOS with ARM GCC 15.3.1 (15.3.Rel2), CMake 4.4.4, GNU Make 3.81, pinned picotool and SDK, `SOURCE_DATE_EPOCH=1794571200`, UTC. ON and OFF are both checked; only ON implements USB SSD. OFF is a build control, not an officially released stock image. No Linux firmware build or byte-identical cross-host reproducibility is claimed.

Build the ELKS kernel on Linux using an existing IA16 ELKS toolchain:

```sh
bash build-elks.sh /absolute/case-sensitive/source/elks /absolute/elks-toolchain/cross
```

The supplied config keeps the display shortcut. The tested compiler is `ia16-elf-gcc (GCC) 6.3.0`; the kernel is built from freshly reconstructed source. The toolchain is an external prerequisite, not rebuilt by this packet. No modified ELKS userspace is required. `build-elks.sh` generates the upstream keyboard-map prerequisite before configuration; an initial packaging attempt that omitted this step is retained separately in the local build evidence.

The resulting kernel is `elks/arch/i86/boot/Image`. Build hashes, source hashes and command logs identify the exact candidate; timestamps mean byte-identical rebuilds are not promised. The firmware build and source tree must be retained together with their manifest. Do not install an unrecorded rebuild as if it were the qualified binary.

The release packet contains patches and pinned reconstruction instructions, not a new bootable hard-disk image. Installation and flashing require the verified recovery copies and the supervised hardware gate described separately.

Portable kernel source checks can be rerun with `python3 verify-kernel.py /absolute/case-sensitive/source/elks` from the review packet. They require Clang with ASan/UBSan and use mocked storage. The archived emulator evidence records target-kernel checks; replaying it also requires the retained disposable root/fixture images, which are not part of this source-and-binary review archive.
