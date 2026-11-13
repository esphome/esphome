#!/bin/zsh
# run.sh NAME KIND PORT MODE : serial flash NAME (unless MODE has noflash), capture, authfuzz, crash counts
#   KIND esp|pico|bk|bw ; MODE full (cases+heavy+timing+OTA of its own image) | cases
#   env: PY, PR_TREE and DEV_TREE (checkouts for *-pr* and other builds); capauthb.py is a serial_capture.py copy
D=${0:A:h}; N=$1; K=$2; P=$3; M=$4
PY=${PY:?set PY}; S=$D/../..
case $N in *-pr*) T=${PR_TREE:?set PR_TREE};; *) T=${DEV_TREE:?set DEV_TREE};; esac
case $N in *-l) U=hwtest-long-username-abcdefghij; W=hwtest-long-password-0123456789xyz;; *) U=hw; W=k7q;; esac
B=$D/.esphome/build/$N/.pioenvs/$N
grep -q "$(basename $T)" $D/$N-compile.log || { echo "wrong tree in compile log"; exit 1; }
SRC=$D/.esphome/build/$N/src/esphome/components/web_server_base/web_server_base.cpp
case $N in *-pr2*) grep -q constant_time_equals $SRC || { echo "PR src check failed"; exit 1; };;
  *-dev*) grep -q "authenticate(credentials.basic_auth_hash)" $SRC || { echo "dev src check failed"; exit 1; };;
  *) echo "unknown build kind"; exit 1;; esac
pkill -f "[c]apauthb.py $P " ; sleep 1
LTPY=$D/pio/penv/.libretiny/bin/python; [ -x $LTPY ] || LTPY=~/.platformio/penv/.libretiny/bin/python
LTL=$(ls -d $D/pio/platforms/libretiny* 2>/dev/null | head -1)
OPT=""
case $K in
  esp|pico) IMG=$B/firmware.bin;;
  bk) IMG=$B/firmware.uf2;;
  bw) IMG=$B/firmware.uf2; OPT="--released";;
esac
if [[ $M != *noflash* ]]; then
case $K in
  esp|pico) $D/up.sh $N $T $P || exit 1;;
  bk) for i in 1 2 3; do timeout 400 $LTPY $S/flash_bk72xx.py -L $LTL flash write -d $P $B/firmware.uf2 > $D/$N-upload.log 2>&1 && break; done; tail -2 $D/$N-upload.log;;
  bw) timeout 600 $LTPY $S/flash_rtl87xx_kit.py -L $LTL flash write -d $P $B/firmware.uf2 > $D/$N-upload.log 2>&1; echo "flash rc=$?"; tail -2 $D/$N-upload.log;;
esac
fi
[ $K = pico ] && { sleep 3; for i in $(seq 1 30); do [ -e $P ] && break; sleep 1; done; sleep 2; }
: > $D/$N-serial.log; ($PY $D/capauthb.py $P 3000 $D/$N-serial.log $OPT 2> $D/$N-cap.err &)
HOST=""; for i in $(seq 1 60); do HOST=$(dscacheutil -q host -a name $N.local 2>/dev/null | awk '/ip_address/{print $2; exit}'); [ -n "$HOST" ] && curl -s -m 3 -o /dev/null http://$HOST/ && break; HOST=""; sleep 3; done
[ -n "$HOST" ] || { echo "no host"; exit 1; }; echo $HOST > $D/$N-host.txt; sleep 8
case $M in
  full*) $PY -I $S/web_auth/authfuzz.py $HOST $U $W $D/$N --ota $IMG;;
  cases*) $PY -I $S/web_auth/authfuzz.py $HOST $U $W $D/$N --only cases;;
esac
sleep 5; pkill -f "[c]apauthb.py $P "; sleep 1
L=$D/$N-serial.log
echo "SERIAL crash lines $(grep -a -c -E 'Panic |Exception \(|Fatal exception|HardFault|Hard Fault|CRASH|abort\(\)|Soft WDT|wdt reset|assert|Backtrace' $L), boots $(grep -a -c -E 'Log initialized|Reset reason' $L), port losses $(grep -a -c 'port lost' $L), lines $(wc -l < $L)" | tee $D/$N-serialsum.txt
grep -a "Heap free" $L | sed -n '1p;$p' | tee -a $D/$N-serialsum.txt
