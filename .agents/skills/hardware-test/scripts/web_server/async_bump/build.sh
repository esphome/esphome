#!/bin/bash
# build.sh dev|pr VER: compile ota8266-<kind> from TREE_BASE-<kind> with `-s ver VER` and copy firmware.bin to
# img-<kind>-<VER>.bin. env: TREE_BASE (two checkouts named <base>-dev and <base>-pr), PY, BUILD_LOCK (optional)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); K=${1:?dev|pr}; V=${2:?ver}; N=ota8266-$K
T=${TREE_BASE:?set TREE_BASE}-$K
TREE=$T PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" compile "$N" "$D/ota8266.yaml" -s ver "$V" || exit 1
cp "$D/.esphome/build/$N/.pioenvs/$N/firmware.bin" "$D/img-$K-$V.bin" && ls -l "$D/img-$K-$V.bin"
