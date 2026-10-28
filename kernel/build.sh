#!/bin/bash
# Compile in an isolated ELKS checkout; never install to the PPC.
set -euo pipefail
kit=$(cd "$(dirname "$0")" && pwd)
: "${ELKS_TREE:?Set ELKS_TREE to a dedicated ELKS e7a56a43 checkout}"
: "${ELKS_TOOLCHAIN:?Set ELKS_TOOLCHAIN to a prepared ELKS cross directory}"
sh "$kit/test.sh"
python3 "$kit/apply.py" "$ELKS_TREE"
python3 "$kit/tests/test-mount.py" "$ELKS_TREE/elkscmd/sys_utils/mount.c"
cd "$ELKS_TREE"
if [ ! -e cross ]; then ln -s "$ELKS_TOOLCHAIN" cross; fi
export TOPDIR="$PWD" CROSSDIR="$PWD/cross"
export PATH="$PWD/elks/tools/bin:$PWD/cross/bin:$PATH"
export MAKEFLAGS="-j${JOBS:-2}"
make elks/arch/i86/drivers/char/KeyMaps/config.in kconfig
cp "${KERNEL_CONFIG:-$kit/ppc512.config}" .config
make include/autoconf.h
make kclean
make kernel
mkdir -p "$kit/build"
cp elks/arch/i86/boot/Image "$kit/build/linux"
cp elks/arch/i86/boot/system.map "$kit/build/system.map"
cp .config "$kit/build/kernel.config"
for app in pmmount pmfstest pmrwtest pmlfnrw pmlfnnew; do
  ia16-elf-gcc -march=i8086 -mtune=i8086 -mcmodel=small -melks-libc \
    -mno-segment-relocation-stuff -Os -Wall -Wextra \
    -maout-heap=1024 -maout-stack=2048 "$kit/$app.c" -o "$kit/build/$app"
done
ia16-elf-gcc -march=i8086 -mtune=i8086 -mcmodel=small -melks-libc \
  -mno-segment-relocation-stuff -Os -Wall -Wextra \
  -I"$ELKS_TREE/elks/include" -maout-heap=4096 -maout-stack=2048 \
  "$ELKS_TREE/elkscmd/sys_utils/mount.c" -o "$kit/build/mount"
TOPDIR="$ELKS_TREE" sh "$kit/../mouse/build.sh"
cp "$kit/../mouse/build/pmmouse" "$kit/build/pmmouse"
python3 "$kit/manifest.py" "$ELKS_TREE" "$kit/build"
