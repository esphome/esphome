#!/bin/zsh
# bleseq.sh NAME [PICO_SN] : 180 s GATT hold through the proxy, then 4 short connects 10 s apart; serial captured
# env: PY, PEER_MAC (the GATT server), PICO_SN when not given
D=${0:A:h}; N=$1; H=$(cat $D/$N-host.txt); PY=${PY:?set PY}
export API_KEY=$(cat $D/apikey.txt)
PORT=$($PY $D/../ports.py --wait 2e8a --serial ${2:-${PICO_SN:?set PICO_SN}} --timeout 10)
: > $D/$N-bleseq-serial.log; ($PY $D/cappico22671.py $PORT 420 $D/$N-bleseq-serial.log 2>/dev/null &)
sleep 2; timeout 300 $PY -I $D/gatthold.py $H ${PEER_MAC:?set PEER_MAC} 180 | grep -v '^uuids'
for i in 1 2 3 4; do sleep 10; timeout 90 $PY -I $D/gatthold.py $H $PEER_MAC 15 2>&1 | grep -E "PASS|FAIL|held" | tr '\n' ' '; echo; done
sleep 3; pkill -f "[c]appico22671.py"
echo "serial: crash/assert $(grep -a -c -i -E 'CRASH|panic|HardFault|assert' $D/$N-bleseq-serial.log) boots $(grep -a -c 'ESPHome version' $D/$N-bleseq-serial.log) notify-response warnings $(grep -a -c 'Failed to send notify' $D/$N-bleseq-serial.log) connects $(grep -a -c 'Connecting v3' $D/$N-bleseq-serial.log)"
grep -a -E "Disconnected, reason|Connection failed|status" $D/$N-bleseq-serial.log | cut -c1-120 | sort | uniq -c | sort -rn | head -5
