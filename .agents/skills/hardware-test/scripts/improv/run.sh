#!/bin/bash
# run.sh NAME [improvcli.py args...]: build NAME.yaml (next to this script, or a path) from TREE, erase the flash so no
# credentials are saved, flash it over serial and run improvcli.py on PORT. Output lands in out-NAME/.
# env: TREE (checkout under test), PORT, PY (checkout python), SECRETS (secrets.yaml with wifi_ssid, wifi_password)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; shift
W=${TREE:?set TREE}; PORT=${PORT:?set PORT}; S=${SECRETS:?set SECRETS}; PY=${PY:-$W/venv/bin/python}
Y=$N; [ -f "$Y" ] || Y=$D/$N.yaml; Y=$(cd "$(dirname "$Y")" && pwd)/$(basename "$Y"); N=$(basename "$Y" .yaml)
O=$D/out-$N; mkdir -p "$O"
echo "== $N $(git -C "$W" rev-parse --short=10 HEAD)"
(cd "$W" && PYTHONPATH=$W PYTHONUNBUFFERED=1 $PY -m esphome compile "$Y") > "$O/compile.log" 2>&1 \
  || { echo "$N compile failed"; tail -20 "$O/compile.log"; exit 2; }
grep -E "^(RAM|Flash):" "$O/compile.log"
$PY -m esptool --port "$PORT" erase_flash > "$O/erase.log" 2>&1 || { echo "$N erase failed"; exit 2; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome upload "$Y" --device "$PORT") > "$O/upload.log" 2>&1 \
  || { echo "$N upload failed"; tail -5 "$O/upload.log"; exit 2; }
sleep 1
rm -f "$O/raw.bin"
$PY "$D/improvcli.py" "$PORT" "$O/raw.bin" --secrets "$S" "$@" 2>&1 | tee "$O/improv.txt"
