#!/bin/bash
set -eu
packet=$(cd -- "$(dirname -- "$0")" && pwd)
source_tree=$(cd -- "$1" && pwd)
cross_tools=$(cd -- "$2" && pwd)
test -x "$cross_tools/bin/ia16-elf-gcc"
cd "$source_tree"
if [ -e cross ] || [ -L cross ]; then
    test "$(cd cross && pwd -P)" = "$cross_tools"
else
    ln -s "$cross_tools" cross
fi
cp "$packet/elks.config" .config
. ./env.sh
make elks/arch/i86/drivers/char/KeyMaps/config.in
make include/autoconf.h
make -j2 kernel
