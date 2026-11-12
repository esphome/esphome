#!/bin/bash
# run.sh DUT_YAML SECONDS: erase and flash DUT_YAML from TREE, let it boot, then reset the client (already flashed with
# client.yaml) and capture both for SECONDS. Prints the generated scan window, the client's first JOIN and last STAT
# lines, its disconnect reasons and the DUT's resets. One AP with this SSID at a time: ports in PARK are held in their
# ROM bootloader first. The yaml file name must equal its `name:` (the build directory is found by it).
# env: TREE, DUT_PORT, CLIENT_PORT, PARK (space separated ports, optional), SC (path to serial_capture.py), PY
Y=$(cd "$(dirname "${1:?dut yaml}")" && pwd)/$(basename "$1"); T=${2:?seconds}; N=$(basename "$Y" .yaml)
W=${TREE:?set TREE}; DUT=${DUT_PORT:?set DUT_PORT}; CLI=${CLIENT_PORT:?set CLIENT_PORT}; SC=${SC:?set SC}
PY=${PY:-$W/venv/bin/python}; O=$(dirname "$Y")/out-$N; mkdir -p "$O"
for p in $PARK; do $PY -m esptool --port "$p" --after no-reset chip-id > /dev/null 2>&1 || { echo "park $p failed"; exit 1; }; done
(cd "$W" && PYTHONPATH=$W $PY -m esphome compile "$Y") > "$O/compile.log" 2>&1 || { echo "$N compile failed"; exit 1; }
$PY -m esptool --port "$DUT" erase_flash > "$O/erase.log" 2>&1 || { echo "$N erase failed"; exit 1; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome upload "$Y" --device "$DUT") > "$O/upload.log" 2>&1 || { echo "$N upload failed"; exit 1; }
$PY "$SC" "$DUT" $((T + 40)) "$O/dut.txt" --reset > /dev/null 2>&1 &
sleep 30
$PY "$SC" "$CLI" "$T" "$O/cli.txt" --reset > /dev/null 2>&1
wait
B=$(dirname "$Y")/.esphome/build/$N/src/main.cpp
[ -f "$B" ] || { echo "$N: $B not found (file name differs from name:?)"; exit 1; }
echo "== $N window=$(grep -o 'set_scan_window([0-9]*)' "$B" || echo none)"
grep -a -m1 "JOIN 1" "$O/cli.txt" | cut -c1-100
grep -a "STAT" "$O/cli.txt" | tail -1 | cut -c1-140
echo "client disconnect reasons: $(grep -a -o "reason='[^']*'" "$O/cli.txt" | sort | uniq -c | tr '\n' ' ')"
echo "dut resets: $(grep -a -c 'rst:' "$O/dut.txt")"
