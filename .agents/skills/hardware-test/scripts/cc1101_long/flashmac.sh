#!/bin/bash
# flashmac.sh NAME MAC: find the board's port by MAC (ports.py lists it), upload NAME.yaml from TREE and wait until
# NAME.local answers on the API with that name. Needs .apikey (the base64 key from secrets.yaml) next to the configs.
# env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; MAC=${2:?mac}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; cd "$D" || exit 1
P=$($PY "$S/ports.py" | grep -i -F " $MAC " | awk '{print $1}')
[ -n "$P" ] || { echo "no port for $MAC"; exit 1; }
echo "flash $N to $MAC on $P"
PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$P" || exit 1
for _ in $(seq 1 40); do
  if $PY devinfo.py .apikey "$N.local" 2>/dev/null | grep -q "$N "; then
    $PY devinfo.py .apikey "$N.local" 2>/dev/null | grep -v Warning; exit 0
  fi
  sleep 3
done
echo "$N did not come up on the API"; exit 1
