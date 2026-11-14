#!/bin/bash
# run.sh NAME [improvcli.py args...]: build NAME.yaml (next to this script, or a path) from TREE, erase the flash so no
# credentials are saved, flash it over serial and run improvcli.py on PORT. Output lands in out-NAME/.
# env: TREE (checkout under test), PORT, PY (checkout python), SECRETS (secrets.yaml with wifi_ssid, wifi_password),
# BUILD_LOCK (optional); builds go through ../pio_build.sh with a private PlatformIO directory
# ESP chips only (esptool erase). On LibreTiny or RP2040 erase and flash with that platform's tools, then run
# improvcli.py directly. Exit status is improvcli.py's: 0 pass, 1 fail, 2 setup failed.
set -o pipefail
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; shift
W=${TREE:?set TREE}; PORT=${PORT:?set PORT}; S=${SECRETS:?set SECRETS}; PY=${PY:-$W/venv/bin/python}
Y=$N; [ -f "$Y" ] || Y=$D/$N.yaml; Y=$(cd "$(dirname "$Y")" && pwd)/$(basename "$Y"); N=$(basename "$Y" .yaml)
O=$D/out-$N; mkdir -p "$O"
REV=$(git -C "$W" rev-parse --short=10 HEAD) || { echo "TREE is not a git checkout: $W"; exit 2; }
echo "== $N $REV"
# the erase runs while the firmware compiles
$PY -m esptool --port "$PORT" erase_flash > "$O/erase.log" 2>&1 &
ERASE=$!
"$(dirname "$D")/pio_build.sh" compile "$N" "$Y" || { wait $ERASE; exit 2; }
wait $ERASE || { echo "$N erase failed"; exit 2; }
"$(dirname "$D")/pio_build.sh" upload "$N" "$Y" "$PORT" || exit 2
sleep 1
rm -f "$O/raw.bin"
$PY "$D/improvcli.py" "$PORT" "$O/raw.bin" --secrets "$S" "$@" 2>&1 | tee "$O/improv.txt"
