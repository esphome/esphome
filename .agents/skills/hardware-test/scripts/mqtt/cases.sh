#!/bin/sh
# cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart] : subscribe to NAME/# on the broker, send a
# switch command and an on_message topic, press the 3000 byte publish button over the API (KEY, - for plaintext),
# optionally restart the local broker (its PID from mosquitto.pid in the working directory, written when it was started
# with `mosquitto -c mosquitto-ws.conf & echo $! > mosquitto.pid`), and show the device log. Outputs (sub-NAME.txt, mosquitto.out) go to the working directory, which should be the scratch directory.
N=$1; KEY=$2; B=$3; SP=$4; LOG=$5; DEV=${6:-$1.local}; RESTART=$7; HERE=$(cd "$(dirname "$0")" && pwd); PY=${PYTHON:-python3}; WORK=$PWD
BTN=$HERE/../api_button.py
[ -n "$KEY" ] && [ -r "$LOG" ] || { echo "usage: cases.sh NAME KEY BROKER_HOST SUB_PORT CAPTURE [DEV_HOST] [restart] (CAPTURE must be a readable file)"; exit 2; }
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
  # Only this run's broker is signalled: its PID comes from mosquitto.pid, written when it was started
  pid=$(cat "$WORK/mosquitto.pid" 2>/dev/null); [ -n "$pid" ] || { echo "no mosquitto.pid in $WORK, cannot restart the broker"; exit 1; }
  kill "$pid" || { echo "broker PID $pid is not running, nothing to restart"; exit 1; }; sleep 8
  nc -z -w 3 "$B" 1883 && { echo "the old broker still answers on 1883 after the kill"; exit 1; }
  # exec so the PID is the broker's own and no shell stays behind holding the caller's output pipe open
  (cd "$WORK" && exec mosquitto -c mosquitto-ws.conf > mosquitto.out 2>&1 < /dev/null) &
  echo $! > "$WORK/mosquitto.pid"; sleep 25
  for port in $(awk '/^listener/ {print $2}' "$WORK/mosquitto-ws.conf"); do nc -z -w 3 "$B" "$port" || { echo "broker port $port not listening after the restart"; exit 1; }; done
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
for want in "Relay ON" "cmd payload: hello-over-$N" "Relay OFF"; do
  tail -n +$((n0+1)) "$LOG" | grep -a -q "$want" || { echo "the device never logged '$want': the broker to device path failed"; exit 1; }
done
