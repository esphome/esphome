#!/bin/bash
# ota.sh MODE YAML VER ADDR: build YAML (absolute path) with the substitution ver=VER from TREE and upload it over SMP UDP to ADDR,
# timed; MODE none uploads normally, lostreply/lostreq/lostreq2 go through inject.py. Give each upload a new VER so
# the image differs (an image already in the secondary slot is only marked and reset), and wait about 4 minutes
# between uploads: the device refuses a new upload until the running image has confirmed itself.
# env: TREE (esphome checkout under test), PY (checkout python with the pinned smpclient)
R=$(cd "$(dirname "$0")" && pwd); M=${1:?none, lostreply, lostreq or lostreq2}; Y=${2:?yaml}; V=${3:?ver}; A=${4:?address}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; OUT=$R/out-smp; mkdir -p "$OUT"; LOG=$OUT/ota-$(basename "$Y" .yaml)-$M-$V.log
(cd "$T" && PYTHONPATH=$T $PY -m esphome -s ver "$V" compile "$Y" > "$OUT/compile-$V.log" 2>&1) || { echo "$V build failed"; exit 1; }
t0=$(date +%s)
if [ "$M" = none ]; then (cd "$T" && PYTHONPATH=$T PYTHONUNBUFFERED=1 $PY -m esphome -s ver "$V" upload "$Y" --device "$A") > "$LOG" 2>&1; rc=$?
else (cd "$T" && PYTHONPATH=$T PYTHONUNBUFFERED=1 $PY "$R/inject.py" "$M" -s ver "$V" upload "$Y" --device "$A") > "$LOG" 2>&1; rc=$?; fi
echo "$(date +%T) $M $V upload rc=$rc in $(( $(date +%s) - t0 )) s"
LC_ALL=C grep -a -E "INJECT|image|reset|Reset|reboot|ERROR|WARNING|Successfully" "$LOG" | tail -12
