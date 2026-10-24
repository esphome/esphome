#!/bin/bash
# sdrun.sh V(pr|base) [N] : flash both boards, restart each N times over MQTT, then disable, then deep sleep (ESP32)
# Copy this directory to a scratch directory (secrets.yaml with wifi_ssid, wifi_password and broker_ip next to
# sd-common.yaml) and run it there with a local mosquitto on this host.
# env: TREE (esphome checkout for V), P32 and P66 (ESP32 and ESP8266 serial ports), PY (checkout python)
S=$(cd "$(dirname "$0")" && pwd); V=$1; N=${2:-10}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
P32=${P32:?set P32}; P66=${P66:?set P66}; OUT=$S/out-$V; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P32"; pkill -f "[s]erial_capture.py $P66"
(cd "$T" && $PY -m esphome upload "$S/$V/sd32$V.yaml" --device "$P32" > "$OUT/u32.log" 2>&1) & U32=$!
(cd "$T" && $PY -m esphome upload "$S/$V/sd66$V.yaml" --device "$P66" > "$OUT/u66.log" 2>&1) & U66=$!
wait $U32 || { echo "upload32 failed"; exit 1; }; wait $U66 || { echo "upload66 failed"; exit 1; }
mosquitto_sub -h 127.0.0.1 -v -t "sd32$V/status" -t "sd66$V/status" -F '@H:@M:@S %t %p' > "$OUT/broker.txt" 2>&1 & SUB=$!
sleep 40
for i in $(seq "$N"); do
  for b in 32 66; do echo "$(date +%T) restart $b" >> "$OUT/events.txt"; mosquitto_pub -h 127.0.0.1 -t "sd$b$V/cmd" -m restart; done
  sleep 25
done
for b in 32 66; do echo "$(date +%T) disable $b" >> "$OUT/events.txt"; mosquitto_pub -h 127.0.0.1 -t "sd$b$V/cmd" -m disable; done
sleep 30
for i in 1 2 3; do echo "$(date +%T) sleep 32" >> "$OUT/events.txt"; mosquitto_pub -h 127.0.0.1 -t "sd32$V/cmd" -m sleep; sleep 30; done
kill $SUB
for b in 32 66; do echo "sd$b$V: $(grep -c "sd$b$V/status hello" "$OUT/broker.txt") hello, $(grep -c "sd$b$V/status bye" "$OUT/broker.txt") bye, $(grep -c "sd$b$V/status dead" "$OUT/broker.txt") dead"; done
