#!/bin/bash
# flashmac.sh NAME BOARD: upload NAME.yaml from TREE to BOARD (a port such as /dev/cu.usbserial-0001, or the board's
# USB serial number, `ports.py` lists it, so a port that moved after a replug is still found), then wait until
# NAME.local answers on the API with that name. Needs apikey.txt (the api_key secret) next to the configs.
# env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; BOARD=${2:?port or USB serial number}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; cd "$D" || exit 1
export API_KEY=${API_KEY:-$(cat apikey.txt)}
case $BOARD in
  /dev/*) P=$BOARD ;;
  *) P=$($PY "$S/ports.py" --serial "$BOARD" --timeout 10) || { echo "no port for $BOARD"; exit 1; } ;;
esac
echo "flash $N to $BOARD on $P"
PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$P" || exit 1
IP=$(bash "$S/resolve.sh" "$N" 40) || { echo "$N did not come up"; exit 1; }
$PY "$S/ota_timing/apiinfo.py" "$IP" | grep " $N " || { echo "$N did not answer on the API"; exit 1; }
