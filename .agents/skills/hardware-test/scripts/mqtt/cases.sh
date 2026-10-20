#!/bin/sh
# cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart] : subscribe to NAME/# on the broker, send a
# switch command and an on_message topic, press the 3000 byte publish button over the API (KEY, - for plaintext),
# optionally restart the local broker from the mosquitto-ws.conf copy in the working directory, and show the device
# log. Outputs (sub-NAME.txt, mosquitto.out) go to the working directory, which should be the scratch directory.
N=$1; KEY=$2; B=$3; SP=$4; LOG=$5; DEV=${6:-$1.local}; RESTART=$7; HERE=$(cd "$(dirname "$0")" && pwd); PY=${PYTHON:-python3}; WORK=$PWD
BTN=$HERE/../api_button.py
[ -n "$KEY" ] && [ -n "$LOG" ] || { echo "usage: cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart]"; exit 2; }
show() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a 'mqtt\|\[probe:\|cmd payload\|Relay\|\[W\]\|\[E\]\|Guru\|abort\|Backtrace\|ESPHome version' | grep -a -v 'took a long time\|Publish.*Counter\|sensor/counter' | cut -c1-170; }
pub() { mosquitto_pub -h "$B" -p "$SP" -t "$1" -m "$2" || { echo "mosquitto_pub to $1 failed"; exit 1; }; }
n0=$(($(wc -l < "$LOG")+0))
timeout 75 mosquitto_sub -h "$B" -p "$SP" -v -t "$N/#" > "$WORK/sub-$N.txt" 2> "$WORK/sub-$N.err" &
sub_pid=$!
sleep 20
echo "=== $N: command to the switch and the 3000 byte publish ($(date +%T))"
pub "$N/switch/relay/command" ON; sleep 2
pub "$N/cmd" "hello-over-$N"; sleep 2
$PY "$BTN" "$DEV" "$KEY" press "Publish big" || { echo "button press failed"; exit 1; }; sleep 3
pub "$N/switch/relay/command" OFF; sleep 2
if [ "$RESTART" = restart ]; then
  [ -f "$WORK/mosquitto-ws.conf" ] || { echo "no mosquitto-ws.conf in $WORK, cannot restart the broker"; exit 1; }
  echo "=== $N: broker restart ($(date +%T))"; n1=$(($(wc -l < "$LOG")+0))
  pkill -f "[m]osquitto -c" || { echo "no running mosquitto matched, nothing to restart"; exit 1; }; sleep 8
  nc -z -w 3 "$B" 1883 && { echo "the old broker still answers on 1883 after pkill"; exit 1; }
  (timeout 10800 mosquitto -c "$WORK/mosquitto-ws.conf" > "$WORK/mosquitto.out" 2>&1 &) ; sleep 25
  for port in 1883 9001 9002; do nc -z -w 3 "$B" "$port" || { echo "broker port $port not listening after the restart"; exit 1; }; done
  tail -n +$((n1+1)) "$LOG" | grep -a -q "MQTT_EVENT_CONNECTED\|\[mqtt:[0-9]*\]: Connected" || { echo "the device did not reconnect after the broker restart"; exit 1; }
else
  echo "=== $N: no broker restart (pass 'restart' as the 7th argument for a local broker)"
fi
wait $sub_pid; sub_rc=$?
[ "$sub_rc" = 124 ] || { echo "subscriber ended with status $sub_rc: $(cat "$WORK/sub-$N.err")"; exit 1; }
echo "--- broker saw ($(grep -c . "$WORK/sub-$N.txt") messages): $(cut -c1-60 "$WORK/sub-$N.txt" | awk '{print $1}' | sort | uniq -c | tr '\n' ';')"
big=$(grep "^$N/big " "$WORK/sub-$N.txt" | awk '{print length($2)}' | tr '\n' ' ')
echo "--- big payload bytes seen: $big"
echo "--- device log:"; show "$n0"
case " $big " in *" 3000 "*) ;; *) echo "the 3000 byte payload did not reach the broker"; exit 1;; esac
