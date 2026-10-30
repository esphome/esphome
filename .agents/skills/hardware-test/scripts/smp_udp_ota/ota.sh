#!/bin/bash
# ota.sh MODE YAML VER ADDR: build YAML (absolute path) with the substitution ver=VER from TREE and upload it over SMP UDP to ADDR,
# timed; MODE none uploads normally, lostreply/lostreq/lostreq2 go through inject.py. The build must fit MCUboot's swap
# limit (mcuboot_slot.py limit), and after the upload the device must run the new image (lostreq2: keep it pending). Give each upload a new VER so
# the image differs (an image already in the secondary slot is only marked and reset), and wait about 4 minutes
# between uploads: the device refuses a new upload until the running image has confirmed itself.
# env: TREE (esphome checkout under test), PY (checkout python with the pinned smpclient)
R=$(cd "$(dirname "$0")" && pwd); M=${1:?none, lostreply, lostreq or lostreq2}; Y=${2:?yaml}; V=${3:?ver}; A=${4:?address}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; OUT=$R/out-smp; mkdir -p "$OUT"; LOG=$OUT/ota-$(basename "$Y" .yaml)-$M-$V.log
touch "$OUT/.stamp"
(cd "$T" && PYTHONPATH=$T $PY -m esphome -s ver "$V" compile "$Y" > "$OUT/compile-$V.log" 2>&1) || { echo "$V build failed"; exit 1; }
# The image this compile wrote; MCUboot never swaps in one over the swap limit and it then wedges every later upload
BIN=$(find "$(dirname "$Y")/.esphome/build" -path "*/zephyr/app_update.bin" -newer "$OUT/.stamp" | head -1)
[ -n "$BIN" ] || { echo "no app_update.bin from this build"; exit 1; }
$PY "$R/mcuboot_slot.py" limit "$(dirname "$(dirname "$BIN")")" || exit 1
t0=$(date +%s)
if [ "$M" = none ]; then (cd "$T" && PYTHONPATH=$T PYTHONUNBUFFERED=1 $PY -m esphome -s ver "$V" upload "$Y" --device "$A") > "$LOG" 2>&1; rc=$?
else (cd "$T" && PYTHONPATH=$T PYTHONUNBUFFERED=1 $PY "$R/inject.py" "$M" -s ver "$V" upload "$Y" --device "$A") > "$LOG" 2>&1; rc=$?; fi
echo "$(date +%T) $M $V upload rc=$rc in $(( $(date +%s) - t0 )) s"
LC_ALL=C grep -a -E "INJECT|image|reset|Reset|reboot|ERROR|WARNING|Successfully" "$LOG" | tail -12
# The CLI's exit code is not proof: ask the device which image runs once it is back
expect=running; [ "$M" = lostreq2 ] && expect=pending
$PY "$R/mcuboot_slot.py" active "$A" "$BIN" --expect $expect | tee -a "$LOG"; active=${PIPESTATUS[0]}
"$R/check.sh" "$M" "$rc" "$LOG" "$active"
