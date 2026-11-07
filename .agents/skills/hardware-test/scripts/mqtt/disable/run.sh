#!/bin/bash
# run.sh LABEL outage|drop SECONDS: build the mqtt.disable config as md<LABEL> from TREE, flash over PORT, start a
# private mosquitto on BROKER_PORT (its log and config go to out-md<LABEL>/), capture the serial log for SECONDS and
#   outage: publish md<LABEL>/in every second, stop the broker 45 s after CYCLE 3 and start it 25 s later (inside a
#           disabled window), then count per disabled window with analyze.py
#   drop:   stop the broker 6 s after every EV ENABLE for 15 s (a drop while the client reconnects)
# Exits 1 on a reset or task watchdog, when no cycle ran, the device never connected, the broker was never stopped or
# did not restart (start means the port answers a publish), or in outage mode when no inbound message arrived, the
# device did not reconnect after the restart, or analyze.py found a connect or an inbound message inside a disabled
# window. Run it from a scratch copy of this directory with a secrets.yaml (wifi_ssid,
# wifi_password).
# env: TREE, PORT, SC (path to serial_capture.py), PY (checkout python), BROKER (this host's address as the board
# sees it: a stale one gives esp-tls select() timeout), BROKER_PORT (default 18831), ASYNC (idf_send_async, default
# false), BOARD (default esp32-s3-devkitc-1), VARIANT (default esp32s3), UART (default USB_SERIAL_JTAG), MOSQUITTO
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; MODE=${2:?outage or drop}; S=${3:?seconds}; T=${TREE:?set TREE}
P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; B=${BROKER:?set BROKER}; BP=${BROKER_PORT:-18831}
MQ=${MOSQUITTO:-mosquitto}; NAME=md$L; O=$D/out-$NAME; rm -rf "$O"; mkdir -p "$O"
case $MODE in outage|drop) ;; *) echo "MODE must be outage or drop"; exit 2;; esac
cat > "$D/$NAME.yaml" <<YAML
substitutions:
  name: $NAME
  broker: $B
  port: "$BP"
  async: "${ASYNC:-false}"
esphome:
  name: $NAME
esp32:
  board: ${BOARD:-esp32-s3-devkitc-1}
  variant: ${VARIANT:-esp32s3}
  framework:
    type: esp-idf
logger:
  hardware_uart: ${UART:-USB_SERIAL_JTAG}
packages:
  common: !include disable-common.yaml
YAML
printf 'listener %s\nallow_anonymous true\nlog_type all\nlog_timestamp_format %%H:%%M:%%S\nlog_dest file %s\n' "$BP" "$O/mosq.log" > "$O/mosq.conf"
# -d forks before the listener binds, so only a publish that gets through proves the broker is up
broker_up() {
  "$MQ" -c "$O/mosq.conf" -d || return 1
  for _ in $(seq 20); do
    mosquitto_pub -p "$BP" -t probe -n 2>/dev/null && { echo "$(date +%T) BROKER START" >> "$O/driver.txt"; return 0; }
    sleep 0.5
  done
  return 1
}
broker_down() {
  pkill -f "[m]osquitto -c $O/mosq.conf" || { echo "FAIL: mosquitto was not running when it was to be stopped"; exit 1; }
  echo "$(date +%T) BROKER STOP" >> "$O/driver.txt"
}
broker_up || { echo "FAIL: mosquitto did not start on $BP"; exit 1; }
trap 'kill $CAP $SUB $PUB 2>/dev/null; pkill -f "[m]osquitto -c $O/mosq.conf"' EXIT
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$D/$NAME.yaml" --device "$P" > "$O/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" "$S" "$O/serial.txt" --ms > /dev/null 2>&1 & CAP=$!
sub() { mosquitto_sub -p "$BP" -v -t "$NAME/#" -F '@H:@M:@S %t %p' >> "$O/sub.txt" 2>/dev/null & SUB=$!; }
sub
if [ "$MODE" = outage ]; then
  (end=$((SECONDS + S)); i=0; while [ $SECONDS -lt $end ]; do i=$((i + 1)); mosquitto_pub -p "$BP" -t "$NAME/in" -m "p$i" 2>/dev/null; sleep 1; done) & PUB=$!
  until grep -a -q "CYCLE 3 " "$O/serial.txt" 2>/dev/null; do sleep 1; kill -0 $CAP 2>/dev/null || break; done
  grep -a -q "CYCLE 3 " "$O/serial.txt" 2>/dev/null || { echo "FAIL: CYCLE 3 never appeared"; exit 1; }
  sleep 45; broker_down; sleep 25; broker_up || { echo "FAIL: mosquitto did not restart"; exit 1; }; sleep 1; kill $SUB 2>/dev/null; sub
  CONNECTS_AT_RESTART=$(grep -a -c 'EV CONNECT' "$O/serial.txt")
else
  seen=0
  while kill -0 $CAP 2>/dev/null; do
    sleep 1; k=$(grep -a -c 'EV ENABLE' "$O/serial.txt" 2>/dev/null); [ "${k:-0}" -gt "$seen" ] || continue
    seen=$k; sleep 6; broker_down; sleep 15; broker_up || { echo "FAIL: mosquitto did not restart"; exit 1; }
  done
fi
wait $CAP
c() { grep -a -c "$1" "$O/serial.txt"; }
echo "$NAME $MODE: cycles $(c 'CYCLE '), connects $(c 'EV CONNECT'), disconnects $(c 'EV DISCONNECT'), msgs $(c 'EV MSG')," \
  "task wdt $(c 'task_wdt'), resets $(grep -a -c -E 'Guru|abort\(\)|Backtrace|rst:0x' "$O/serial.txt")"
grep -a 'mqtt took a long time' "$O/serial.txt" | sed 's/.*took/took/' | sort | uniq -c
if [ "$MODE" = outage ]; then python3 "$D/analyze.py" "$O" "$NAME" || { echo "FAIL: activity inside a disabled window"; exit 1; }; fi
[ "$(c 'CYCLE ')" -gt 0 ] || { echo "FAIL: no disable cycle ran"; exit 1; }
[ "$(c 'EV CONNECT')" -gt 0 ] || { echo "FAIL: the device never connected to the broker"; exit 1; }
[ "$MODE" = drop ] || [ "$(c 'EV MSG')" -gt 0 ] || { echo "FAIL: no inbound message arrived"; exit 1; }
[ "$MODE" = drop ] || [ "$(c 'EV CONNECT')" -gt "$CONNECTS_AT_RESTART" ] || { echo "FAIL: no connect after the broker restart"; exit 1; }
grep -q 'BROKER STOP' "$O/driver.txt" 2> /dev/null || { echo "FAIL: the broker was never stopped"; exit 1; }
[ "$(c task_wdt)" = 0 ] && [ "$(grep -a -c -E 'Guru|abort\(\)|Backtrace|rst:0x' "$O/serial.txt")" = 0 ] || { echo "FAIL: reset or task watchdog"; exit 1; }
echo PASS
