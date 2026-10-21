#!/bin/sh
# flash_and_drive.sh TREE YAML PORT KEY BROKER_HOST MODE [SUB_PORT] : flash YAML from the checkout TREE over the serial
# PORT, capture the serial log (cap-NAME.txt in the working directory), wait for the first MQTT connect or TLS error,
# then drive the device with cases.sh (KEY as for cases.sh, - for a plaintext API). MODE local runs cases.sh with a
# broker restart (the working directory must hold mosquitto-ws.conf and mosquitto.pid), public runs it without a
# restart, neg only watches for 100 s (a build that must never connect). SUB_PORT is the plain listener cases.sh
# subscribes on (default 1883). Ends with connect, disconnect, handshake error and crash counts, the config dump's
# TLS line, the probe heap lines and the broker listener each connection used.
TREE=$1; YAML=$2; PORT=$3; KEY=$4; B=$5; MODE=$6; SP=${7:-1883}
case $YAML in /*) ;; *) YAML=$PWD/$YAML;; esac
HERE=$(cd "$(dirname "$0")" && pwd); PY=${PYTHON:-python3}; N=$(basename "$YAML" .yaml); CAP=$PWD/cap-$N.txt; ULOG=$PWD/upload-$N.log
[ -d "$TREE/esphome" ] && [ -e "$PORT" ] && [ -n "$KEY" ] && [ -n "$B" ] || { echo "usage: flash_and_drive.sh TREE YAML PORT KEY BROKER_HOST local|public|neg [SUB_PORT]"; exit 2; }
case $MODE in local|public|neg) ;; *) echo "MODE must be local, public or neg"; exit 2;; esac
pkill -f "[s]erial_capture.py $PORT"; sleep 1
echo "=== $N: flash ($(date +%T))"
(cd "$TREE" && PYTHONUNBUFFERED=1 timeout 300 $PY -m esphome upload "$YAML" --device "$PORT" > "$ULOG" 2>&1) || { echo "upload failed"; tail -5 "$ULOG"; exit 1; }
SECS=200; [ "$MODE" = neg ] && SECS=100
$PY "$HERE/../serial_capture.py" "$PORT" $SECS "$CAP" > "$PWD/cap-$N.stderr" 2>&1 &
cap_pid=$!
i=0; until grep -a -q "MQTT_EVENT_CONNECTED\|\[mqtt:[0-9]*\]: Connected\|mbedtls_ssl_handshake returned\|Failed to verify" "$CAP" 2>/dev/null || [ $i -ge 45 ]; do sleep 2; i=$((i+1)); done
DEV=$(grep -a -o 'IP Address: [0-9.]*' "$CAP" | head -1 | awk '{print $3}')
echo "=== $N: first MQTT event after $((i*2)) s, device $DEV ($(date +%T))"
grep -a 'ESPHome version\|\]:   Transport\|\]:   TLS:' "$CAP" | sed 's/\x1b\[[0-9;]*m//g' | cut -c1-120
rc=0
case $MODE in
  local)  PYTHON=$PY sh "$HERE/cases.sh" "$N" "$KEY" "$B" "$SP" "$CAP" "$DEV" restart; rc=$?;;
  public) PYTHON=$PY sh "$HERE/cases.sh" "$N" "$KEY" "$B" "$SP" "$CAP" "$DEV"; rc=$?;;
  neg)    sleep 70;;
esac
wait $cap_pid
strip() { sed 's/\x1b\[[0-9;]*m//g'; }
connects=$(grep -a -c 'MQTT on_connect' "$CAP"); errors=$(grep -a -c 'mbedtls_ssl_handshake returned\|Failed to verify certificate' "$CAP")
echo "=== $N: connects $connects, disconnects $(grep -a -c 'MQTT on_disconnect' "$CAP"), handshake errors $errors, crashes $(grep -a -c 'Guru\|abort()\|Backtrace' "$CAP")"
grep -a 'mbedtls_ssl_handshake returned\|Failed to verify\|esp-tls' "$CAP" | strip | sed 's/^\[[^]]*\]//' | sort | uniq -c | sort -rn | head -4 | cut -c1-150
grep -a '\[probe:' "$CAP" | strip | sed 's/.*\[probe:[0-9]*\]: //' | sed -n '1p;$p'
[ -f "$PWD/mosquitto.log" ] && [ -n "$DEV" ] && echo "broker listeners used by $DEV: $(grep "New connection from $DEV" "$PWD/mosquitto.log" | sed 's/.*on port //' | sort | uniq -c | tr '\n' ';')"
if [ "$MODE" = neg ]; then
  [ "$connects" = 0 ] && [ "$errors" -gt 0 ] || { echo "negative case: expected no connect and a handshake error"; exit 1; }
else
  [ "$rc" = 0 ] || { echo "cases.sh failed with status $rc"; exit 1; }
fi
