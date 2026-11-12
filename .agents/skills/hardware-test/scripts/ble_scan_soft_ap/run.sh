#!/bin/bash
# run.sh DUT_YAML SECONDS: erase and flash DUT_YAML from TREE, let it boot, then reset the client (already flashed with
# client.yaml) and capture both for SECONDS. Prints the generated scan window, the client's first JOIN and last STAT
# lines, its disconnect reasons and the DUT's resets. One AP with this SSID at a time: ports in PARK are held in their
# ROM bootloader first. The yaml file name must equal its `name:` (the build directory is found by it).
# Exit 0: pass; 1: the run finished and failed the checks; 2: setup or capture failed.
# env: TREE, DUT_PORT, CLIENT_PORT, PARK (space separated ports, optional), SC (path to serial_capture.py), PY
Y=$(cd "$(dirname "${1:?dut yaml}")" && pwd)/$(basename "$1"); T=${2:?seconds}; N=$(basename "$Y" .yaml)
W=${TREE:?set TREE}; DUT=${DUT_PORT:?set DUT_PORT}; CLI=${CLIENT_PORT:?set CLIENT_PORT}; SC=${SC:?set SC}
PY=${PY:-$W/venv/bin/python}; O=$(dirname "$Y")/out-$N; mkdir -p "$O"
for p in $PARK; do $PY -m esptool --port "$p" --after no-reset chip-id > /dev/null 2>&1 || { echo "park $p failed"; exit 2; }; done
(cd "$W" && PYTHONPATH=$W $PY -m esphome compile "$Y") > "$O/compile.log" 2>&1 || { echo "$N compile failed"; exit 2; }
$PY -m esptool --port "$DUT" erase_flash > "$O/erase.log" 2>&1 || { echo "$N erase failed"; exit 2; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome upload "$Y" --device "$DUT") > "$O/upload.log" 2>&1 || { echo "$N upload failed"; exit 2; }
$PY "$SC" "$DUT" $((T + 40)) "$O/dut.txt" --reset > "$O/dut-capture.log" 2>&1 &
CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
sleep 30
$PY "$SC" "$CLI" "$T" "$O/cli.txt" --reset > "$O/cli-capture.log" 2>&1 || { echo "$N client capture failed"; exit 2; }
wait "$CAP" || { echo "$N DUT capture failed"; exit 2; }
B=$(dirname "$Y")/.esphome/build/$N/src/main.cpp
[ -f "$B" ] || { echo "$N: $B not found (file name differs from name:?)"; exit 2; }
echo "== $N window=$(grep -o 'set_scan_window([0-9]*)' "$B" || echo none)"
J=$(grep -a -m1 "JOIN 1" "$O/cli.txt" | cut -c1-100); echo "${J:-$N NO JOIN}"
STAT=$(grep -a "STAT" "$O/cli.txt" | tail -1 | cut -c1-140); echo "$STAT"
echo "client disconnect reasons: $(grep -a -o "reason='[^']*'" "$O/cli.txt" | sort | uniq -c | tr '\n' ' ')"
echo "dut resets: $(grep -a -c 'rst:' "$O/dut.txt")"
# pass: the client joined and fetched pages, and the DUT reset exactly once (the capture's own reset)
[ -n "$J" ] && [ -n "$STAT" ] && [[ "$STAT" != *"pages_ok=0 "* ]] && [ "$(grep -a -c 'rst:' "$O/dut.txt")" = 1 ]
