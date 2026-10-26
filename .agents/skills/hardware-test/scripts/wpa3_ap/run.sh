#!/bin/bash
# usage: run.sh BUILD   flash the PMF AP, OTA the CB3S, serial flash the BK7238, then capture one AP window
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password, wd_ap_password) and
# run it there. BUILD is a label (pr, base) for the output files.
# env: TREE (esphome checkout for the DUTs), AP_PORT (classic devkit running wdap.yaml), BK_PORT (BK7238 serial),
# CB_HOST (CB3S address on the house network), SC (path to serial_capture.py), FLASH_BK (path to flash_bk72xx.py),
# LT (LibreTiny penv python, default ~/.platformio/penv/.libretiny/bin/python), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); b=${1:?build label}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
AP=${AP_PORT:?set AP_PORT}; BK=${BK_PORT:?set BK_PORT}; CB=${CB_HOST:?set CB_HOST}; SC=${SC:?set SC}
FB=${FLASH_BK:?set FLASH_BK}; LT=${LT:-$HOME/.platformio/penv/.libretiny/bin/python}
pkill -f "[s]erial_capture.py ($AP|$BK)"
build() { (cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome "$@"); }
build run --no-logs "$D/wdap.yaml" --device "$AP" > "$D/ap-$b-upload.log" 2>&1 || { echo "FAIL: AP build or upload"; exit 1; }
# The DUT reboots 150 s after joining the house network; retry when it reboots mid-upload.
for t in 1 2 3; do build run --no-logs "$D/wdcb.yaml" --device "$CB" > "$D/cb-$b-upload.log" 2>&1 && break; sleep 20; done \
  || { echo "FAIL: CB3S build or OTA after 3 tries"; exit 1; }
build compile "$D/wdbk.yaml" > "$D/bk-$b-build.log" 2>&1 || { echo "FAIL: BK7238 build"; exit 1; }
# The build directory has two .uf2 files; firmware.uf2 is the one to flash.
UF2=$D/.esphome/build/wdbk/.pioenvs/wdbk/firmware.uf2
for t in 1 2 3 4; do $LT "$FB" flash write -d "$BK" "$UF2" > "$D/bk-$b-flash.log" 2>&1 && break; sleep 5; done \
  || { echo "FAIL: BK7238 flash after 4 tries"; exit 1; }
sleep 20
# The reset restarts the AP's 420 s window.
$PY "$SC" "$AP" 450 "$D/ap-$b.txt" --reset > /dev/null 2>&1 & C1=$!
sleep 8
$PY "$SC" "$BK" 440 "$D/bk-$b.txt" > /dev/null 2>&1 & C2=$!
trap 'kill $C1 $C2 2>/dev/null' EXIT
wait $C1 $C2; trap - EXIT
$PY "$D/an.py" "$D" "$b"
