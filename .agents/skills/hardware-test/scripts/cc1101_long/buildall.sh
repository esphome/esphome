#!/bin/bash
# buildall.sh NAME...: compile each NAME.yaml in this directory from TREE and show whether the long packet define and
# the GDO0 drain made it into the build. env: TREE, PY, BUILD_LOCK (optional)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
export PIO_CORE=${PIO_CORE:-$D/pio}
for n in "$@"; do
  echo "== $n $(git -C "${TREE:?set TREE}" rev-parse --short=10 HEAD)"
  bash "$S/pio_build.sh" compile "$n" "$D/$n.yaml" || continue
  echo "long define: $(grep -c USE_CC1101_LONG_PACKET "$D/.esphome/build/$n/src/esphome/core/defines.h")" \
    "gdo0 drain: $(grep -c RX_FIFO_THRESHOLD "$D/.esphome/build/$n/src/esphome/components/cc1101/cc1101.cpp")"
done
