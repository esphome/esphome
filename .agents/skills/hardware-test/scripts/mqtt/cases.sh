#!/bin/sh
# cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart] : subscribe to NAME/# on the broker, send a
# switch command and an on_message topic, press the 3000 byte publish button over the API (KEY, - for plaintext),
# optionally restart the local broker started from mosquitto-ws.conf in this directory, and show the device log.
N=$1; KEY=$2; B=$3; SP=$4; LOG=$5; DEV=${6:-$1.local}; RESTART=$7; HERE=$(cd "$(dirname "$0")" && pwd); PY=${PYTHON:-python3}
BTN=$HERE/../api_button.py
[ -n "$KEY" ] && [ -n "$LOG" ] || { echo "usage: cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart]"; exit 2; }
show() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a 'mqtt\|\[probe:\|cmd payload\|Relay\|\[W\]\|\[E\]\|Guru\|abort\|Backtrace\|ESPHome version' | grep -a -v 'took a long time\|Publish.*Counter\|sensor/counter' | cut -c1-170; }
pub() { mosquitto_pub -h "$B" -p "$SP" -t "$1" -m "$2" || { echo "mosquitto_pub to $1 failed"; exit 1; }; }
n0=$(($(wc -l < "$LOG")+0))
timeout 75 mosquitto_sub -h "$B" -p "$SP" -v -t "$N/#" > "$HERE/sub-$N.txt" 2>&1 &
sleep 20
echo "=== $N: command to the switch and the 3000 byte publish ($(date +%T))"
pub "$N/switch/relay/command" ON; sleep 2
pub "$N/cmd" "hello-over-$N"; sleep 2
$PY "$BTN" "$DEV" "$KEY" press "Publish big" || { echo "button press failed"; exit 1; }; sleep 3
pub "$N/switch/relay/command" OFF; sleep 2
if [ "$RESTART" = restart ]; then
  echo "=== $N: broker restart ($(date +%T))"; pkill -f "[m]osquitto -c"; sleep 8
  (cd "$HERE" && timeout 10800 mosquitto -c mosquitto-ws.conf > mosquitto.out 2>&1 &) ; sleep 25
  for port in 1883 9001 9002; do nc -z -w 3 "$B" "$port" || { echo "broker port $port not listening after the restart"; exit 1; }; done
else
  echo "=== $N: no broker restart (pass 'restart' as the 7th argument for a local broker)"
fi
wait
echo "--- broker saw ($(grep -c . "$HERE/sub-$N.txt") messages): $(cut -c1-60 "$HERE/sub-$N.txt" | awk '{print $1}' | sort | uniq -c | tr '\n' ';')"
echo "--- big payload bytes seen: $(grep "^$N/big " "$HERE/sub-$N.txt" | awk '{print length($2)}' | tr '\n' ' ')"
echo "--- device log:"; show "$n0"
