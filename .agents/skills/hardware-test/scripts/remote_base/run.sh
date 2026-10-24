#!/bin/bash
# run.sh LABEL B(32|66) [N] : build rb<B><LABEL> from templates/remote_base, flash it, run rbdrive.py for N rounds
# Copy this directory and templates/remote_base into one scratch directory (secrets.yaml next to rb-common.yaml).
# env: TREE (esphome checkout to build from), P32 or P66 (serial port of the board), SC (path to
# serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); V=$1; B=$2; N=${3:-3}; NAME=rb$B$V; T=${TREE:?set TREE}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
if [ "$B" = 32 ]; then P=${P32:?set P32}; else P=${P66:?set P66}; fi
mkdir -p "$R/$V"; [ -f "$R/$V/$NAME.yaml" ] || sed "s/@V@/$V/g" "$R/rb$B.tmpl" > "$R/$V/$NAME.yaml"
OUT=$R/out-$V-$B; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome upload "$R/$V/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "upload failed"; exit 1; }
$PY "$SC" "$P" 150 "$OUT/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
for i in $(seq 60); do IP=$(grep -a -o 'IP Address: [0-9.]*' "$OUT/serial.txt" 2>/dev/null | tail -1 | cut -d' ' -f3); [ -z "$IP" ] && IP=$(python3 -c "import socket;print(socket.gethostbyname('$NAME.local'))" 2>/dev/null); [ -n "$IP" ] && break; sleep 2; done
[ -n "$IP" ] || { echo "device not found"; exit 1; }
$PY "$R/rbdrive.py" "$IP" "$N" > "$OUT/drive.txt" 2>&1
pkill -f "[s]erial_capture.py $P"
cat "$OUT/drive.txt"; echo "serial: $(grep -a -c 'ext dumper register called' "$OUT/serial.txt") register calls, $(grep -a -c 'No dumper slot' "$OUT/serial.txt") rejected, $(grep -a -c -i 'guru\|panic\|exception\|rst cause' "$OUT/serial.txt") crash lines"
