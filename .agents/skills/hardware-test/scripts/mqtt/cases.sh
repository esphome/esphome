#!/bin/sh
# cases.sh NAME BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] : subscribe to NAME/# on the broker, send a command, press the big publish
# button over the API, restart the local broker (local only), and show the device log. KEY from secrets.yaml next to this script; broker_ip.txt holds the local broker address.
N=$1; B=$2; SP=$3; LOG=$4; DEV=${5:-$1.local}; HERE=$(dirname "$0"); PY=${PYTHON:-python3}
KEY=$(grep api_key "$HERE/secrets.yaml" | awk '{print $2}' | tr -d '"'); BTN=$HERE/../api_button.py
show() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a 'mqtt\|\[probe:\|cmd payload\|Relay\|\[W\]\|\[E\]\|Guru\|abort\|Backtrace\|ESPHome version' | grep -a -v 'took a long time\|Publish.*Counter\|sensor/counter' | cut -c1-170; }
n0=$(wc -l < "$LOG")
timeout 75 mosquitto_sub -h "$B" -p "$SP" -v -t "$N/#" > "$HERE/sub-$N.txt" 2>&1 &
sleep 20
echo "=== $N: command to the switch and the 3000 byte publish ($(date +%T))"
mosquitto_pub -h "$B" -p "$SP" -t "$N/switch/relay/command" -m ON; sleep 2
mosquitto_pub -h "$B" -p "$SP" -t "$N/cmd" -m "hello-over-$N"; sleep 2
$PY "$BTN" "$DEV" "$KEY" press "Publish big" 2>&1 | grep -v '^$'; sleep 3
mosquitto_pub -h "$B" -p "$SP" -t "$N/switch/relay/command" -m OFF; sleep 2
if [ "$B" = "$(cat "$HERE/broker_ip.txt")" ]; then
  echo "=== $N: broker restart ($(date +%T))"; pkill -f "[m]osquitto -c"; sleep 8
  (timeout 10800 mosquitto -c "$HERE/mosquitto-ws.conf" > "$HERE/mosquitto.out" 2>&1 &) ; sleep 25
fi
wait
echo "--- broker saw ($(grep -c . "$HERE/sub-$N.txt") messages): $(cut -c1-60 "$HERE/sub-$N.txt" | awk '{print $1}' | sort | uniq -c | tr '\n' ';')"
echo "--- big payload bytes seen: $(grep "$N/big" "$HERE/sub-$N.txt" | awk '{print length($2)}' | tr '\n' ' ')"
echo "--- device log:"; show "$n0"
