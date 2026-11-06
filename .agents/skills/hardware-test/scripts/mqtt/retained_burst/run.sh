#!/bin/bash
# run.sh LABEL BOOTS: build the retained burst config as rb<LABEL> from TREE, seed the retained messages, flash over
# PORT, then reset the board BOOTS times with a 32 s capture each and print the RESULT and Dropped lines per boot.
# A broker must listen on BROKER_PORT on this host (mosquitto with allow_anonymous true). Run it from a scratch copy
# of this directory with a secrets.yaml (wifi_ssid, wifi_password).
# env: TREE, PORT, SC (path to serial_capture.py), PY (checkout python), BROKER (this host's address as the board
# sees it), BROKER_PORT (default 18833), BOARD (default esp32-c3-devkitm-1), VARIANT (default esp32c3),
# UART (logger hardware_uart, default USB_SERIAL_JTAG)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; N=${2:?boots}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; B=${BROKER:?set BROKER}; BP=${BROKER_PORT:-18833}
NAME=rb$L; O=$D/out-$NAME; rm -rf "$O"; mkdir -p "$O"
cat > "$D/$NAME.yaml" <<YAML
substitutions:
  broker: $B
  broker_port: "$BP"
esphome:
  name: $NAME
esp32:
  board: ${BOARD:-esp32-c3-devkitm-1}
  variant: ${VARIANT:-esp32c3}
  framework:
    type: esp-idf
logger:
  hardware_uart: ${UART:-USB_SERIAL_JTAG}
packages:
  burst: !include burst-common.yaml
YAML
"$D/seed.sh" "$BP" || exit 1
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$D/$NAME.yaml" --device "$P" > "$O/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
FAIL=0
for i in $(seq 1 "$N"); do
  $PY "$SC" "$P" 32 "$O/boot$i.txt" --reset > /dev/null 2>&1
  R=$(grep -a 'RESULT' "$O/boot$i.txt" | tail -1 | sed 's/.*RESULT //')
  echo "$NAME boot$i: ${R:-no RESULT} | $(grep -a 'Dropped' "$O/boot$i.txt" | sed 's/.*Dropped/Dropped/' | tr '\n' ' ')"
  [ -n "$R" ] || FAIL=1
  case $R in "got=25/25 "*"switches_on=15/15 "*) ;; *) FAIL=1;; esac
  grep -a -q -E 'Guru|abort\(\)|Backtrace|task_wdt' "$O/boot$i.txt" && { echo "$NAME boot$i: crash or watchdog"; FAIL=1; }
done
[ "$FAIL" = 0 ] && echo PASS || { echo "FAIL: a boot crashed, missed messages or never reported"; exit 1; }
