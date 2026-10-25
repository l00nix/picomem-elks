#!/bin/sh
set -eu
: "${TOPDIR:?Set TOPDIR to a prepared ELKS source/build tree}"
export TOPDIR
PATH="$TOPDIR/elks/tools/bin:$TOPDIR/cross/bin:$PATH"
export PATH
cd "$(dirname "$0")"
mkdir -p build
ia16-elf-gcc -march=i8086 -mtune=i8086 -mcmodel=small -melks-libc \
    -mno-segment-relocation-stuff -Os -Wall -Wextra \
    -fno-builtin-printf -fno-builtin-fprintf \
    -maout-heap=1024 -maout-stack=2048 -Wl,-Map,build/pmconfig.map \
    pmcfg.c -o build/pmconfig
for alias in pmcfg pmdump pmmem pmstat; do
    cp build/pmconfig "build/$alias"
done
ia16-elf-gcc --version | head -1
sha256sum build/pmconfig
