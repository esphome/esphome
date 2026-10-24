#!/bin/bash
# run.sh X BUILD : flash variant X (a..d) of BUILD (pr|base) on both boards, record broker traffic, restart, hard reset
# Copy this directory to a scratch directory and run it there; secrets.yaml (wifi_ssid, wifi_password, broker_ip)
# goes next to mq-common.yaml and in every board folder.
# env: TREE (esphome checkout for BUILD), P32 and P66 (ESP32 and ESP8266 serial ports), SC (path to
# serial_capture.py), PY (checkout python)
M=$(cd "$(dirname "$0")" && pwd); X=$1; V=$2; T=${TREE:?set TREE to the checkout under test}
P32=${P32:?set P32}; P66=${P66:?set P66}; SC=${SC:?set SC}
PY=${PY:-$T/venv/bin/python}; OUT=$M/out-$X-$V; mkdir -p $OUT
pkill -f "[s]erial_capture.py $P32"; pkill -f "[s]erial_capture.py $P66"; pkill -f "[m]osquitto_sub -h 127.0.0.1"
(cd $T && $PY -m esphome upload $M/$V/32/mq${X}32$V.yaml --device $P32 > $OUT/u32.log 2>&1) & U32=$!
(cd $T && $PY -m esphome upload $M/$V/66/mq${X}66$V.yaml --device $P66 > $OUT/u66.log 2>&1) & U66=$!
wait $U32 || { echo "upload32 failed, see $OUT/u32.log"; exit 1; }
wait $U66 || { echo "upload66 failed, see $OUT/u66.log"; exit 1; }
mosquitto_sub -h 127.0.0.1 -v -t '#' -F '@H:@M:@S r=%r %t %p' > $OUT/broker.txt 2>&1 &
SUB=$!
$PY $SC $P32 170 $OUT/s32.txt --reset > /dev/null 2>&1 &
$PY $SC $P66 170 $OUT/s66.txt > /dev/null 2>&1 &
sleep 60
TOPICS=$(grep "homeassistant/button/" $OUT/broker.txt | grep "\"name\":\"mq${X}" | grep -o '"cmd_t":"[^"]*"' | cut -d'"' -f4 | sort -u)
ERR=0
[ "$(echo "$TOPICS" | grep -c .)" -ge 2 ] || { echo "ERROR: expected a Restart command topic per board, found: ${TOPICS:-none}"; ERR=1; }
for t in $TOPICS; do
  mosquitto_pub -h 127.0.0.1 -t "$t" -m PRESS && echo "$(date +%T) press $t" || { echo "$(date +%T) ERROR: publish to $t failed"; ERR=1; }
done
sleep 45
pkill -f "[s]erial_capture.py $P32"; pkill -f "[s]erial_capture.py $P66"; sleep 1
echo "$(date +%T) hold both in the bootloader for 40 s (will test)"
$PY -m esptool --port $P32 --after no-reset chip-id > $OUT/hold32.log 2>&1 & H32=$!
$PY -m esptool --port $P66 --after no-reset chip-id > $OUT/hold66.log 2>&1 & H66=$!
wait $H32 || { echo "ERROR: ESP32 not held in the bootloader, no will expected (see $OUT/hold32.log)"; ERR=1; }
wait $H66 || { echo "ERROR: ESP8266 not held in the bootloader, no will expected (see $OUT/hold66.log)"; ERR=1; }
sleep 40
echo "$(date +%T) release both"
$PY $SC $P32 40 $OUT/s32b.txt --reset > /dev/null 2>&1 &
$PY $SC $P66 40 $OUT/s66b.txt --reset > /dev/null 2>&1 &
sleep 42
kill $SUB
for f in s32 s66; do echo "== $f"; grep -a -E "Topic Prefix|Log Topic|Availability|Client ID" $OUT/$f.txt | head -4 | sed 's/^\[[0-9:]*\]//'; done
echo "== broker (excluding discovery and entity state)"
grep -v -E " homeassistant/| [^ ]*/(sensor|button|binary_sensor)/| esphome/discover/" $OUT/broker.txt | awk '$3 ~ /(debug|log)$/ { if (seen[$3]++) next; print $1, $2, $3, "(log lines:", "see file)"; next } { print }' | head -40
echo "== log line counts per topic"; awk '$3 ~ /(debug|log)$/ {c[$3]++} END {for (t in c) print t, c[t]}' $OUT/broker.txt
exit $ERR
