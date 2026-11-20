#!/bin/bash
# boots.sh NAME YAML PORT BOOTS SECS [noflash] [esphome options...]: build and upload YAML as NAME over PORT (unless
# noflash), then take BOOTS captures of SECS each from a reset into NAME-bootK.txt. Options after noflash (or "flash")
# go to esphome, e.g. -s broker <host>.
# env: TREE (checkout under test), PY (default $TREE/venv/bin/python), PIO_CORE, BUILD_LOCK (see ../../pio_build.sh)
D=$(cd "$(dirname "$0")" && pwd); S=$(cd "$D/../.." && pwd)
N=${1:?name}; Y=${2:?yaml}; P=${3:?port}; B=${4:?boots}; T=${5:?seconds}; M=${6:-flash}; shift $(($# < 6 ? $# : 6))
PY=${PY:-${TREE:?set TREE}/venv/bin/python}
if [ "$M" != noflash ]; then
  "$S/pio_build.sh" compile "$N" "$Y" "$@" || exit 1
  "$S/pio_build.sh" upload "$N" "$Y" "$P" "$@" || exit 1
fi
for k in $(seq 1 "$B"); do
  "$PY" "$S/serial_capture.py" "$P" "$T" "$D/$N-boot$k.txt" --reset --ms > /dev/null ||
    { echo "$N boot $k capture failed"; exit 1; }
done
echo "$N done"
