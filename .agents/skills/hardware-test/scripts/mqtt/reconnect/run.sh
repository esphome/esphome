#!/bin/bash
# run.sh LABEL CYCLES: build the reconnect config as rk<LABEL> from TREE, seed the retained burst, flash over PORT,
# capture the serial log and run kick.py for CYCLES kick and leave-alone pairs. Prints one RESULT per connection and
# PASS when every left-alone connection got the whole burst after the forced reconnect, FAIL otherwise
# (judge.py) or on a reset after the first connect.
# A mosquitto must listen on BROKER_PORT with allow_anonymous true, log_type all and log_dest file BROKER_LOG.
# Run it from a scratch copy of this directory with a secrets.yaml (wifi_ssid, wifi_password).
# env: TREE, PORT, SC (path to serial_capture.py), PY (checkout python), BROKER (this host's address as the board
# sees it), BROKER_PORT (default 18833), BROKER_LOG, BOARD (default esp32-s3-devkitc-1), VARIANT (default esp32s3),
# UART (logger hardware_uart, default USB_SERIAL_JTAG)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; N=${2:?cycles}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; B=${BROKER:?set BROKER}; BP=${BROKER_PORT:-18833}; BL=${BROKER_LOG:?set BROKER_LOG}
NAME=rk$L; O=$D/out-$NAME; rm -rf "$O"; mkdir -p "$O"
cat > "$D/$NAME.yaml" <<YAML
substitutions:
  broker: $B
  broker_port: "$BP"
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
  kick: !include kick-common.yaml
YAML
"$D/../retained_burst/seed.sh" "$BP" || exit 1
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$D/$NAME.yaml" --device "$P" > "$O/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" $((60 + N * 25)) "$O/serial.txt" --reset > /dev/null 2>&1 & CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
for _ in $(seq 60); do grep -a -q 'EV CONNECTED' "$O/serial.txt" 2>/dev/null && break; sleep 1; done
grep -a -q 'EV CONNECTED' "$O/serial.txt" || { echo "FAIL: no MQTT connect within 60 s"; exit 1; }
sleep 8
timeout $((N * 25 + 30)) $PY "$D/kick.py" "$BL" "$BP" "$NAME" "$N" | tee "$O/kick.txt"
rc=${PIPESTATUS[0]}
[ "$rc" = 0 ] || { echo "FAIL: kick.py exited $rc"; exit 1; }
sleep 8
grep -a -E 'CONN [0-9]+|RESULT' "$O/serial.txt" | sed 's/.*\]: //' | paste - - | tee "$O/results.txt"
# only after the first connect: the capture starts with a reset of its own
R=$(sed -n '/EV CONNECTED/,$p' "$O/serial.txt" | grep -a -c -E 'Guru|abort\(\)|Backtrace|rst:0x|task_wdt')
[ "$R" = 0 ] || { echo "FAIL: $R reset or task watchdog lines (a reboot also restarts the CONN count)"; exit 1; }
# kick.py prints each left-alone connection with its time and the device's CONN number; judge.py pairs them with the
# RESULT lines (retained switch commands only fire on the first connection, so switches_on is not judged)
python3 "$D/judge.py" "$O/kick.txt" "$O/serial.txt" "$N"
