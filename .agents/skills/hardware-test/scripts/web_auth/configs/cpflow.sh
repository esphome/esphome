#!/bin/bash
# cpflow.sh DUT KIND DUT_PORT CLIENT_PORT [flashclient]: captive portal with web_server Basic auth configured. Flash the
# AP DUT (DUT.yaml) from TREE, wait for "Starting fallback AP", and only then reset the client (CLIENT.yaml, flashed
# from CLIENT_TREE with `flashclient`), or the client joins the real network first and never sees the AP.
# KIND pico waits for the re-enumerated port. env: TREE, CLIENT_TREE, CLIENT (default cpclient), PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); N=${1:?dut}; K=${2:?kind}; P=${3:?dut port}
CP=${4:?client port}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; C=${CLIENT:-cpclient}
# up NAME TREE PORT: upload NAME.yaml built from TREE with `-s name NAME`
up() { TREE=$2 PY=$PY PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$1" "$D/$1.yaml" "$3"; }
pkill -f "[s]erial_capture.py $P "; pkill -f "[s]erial_capture.py $CP "; sleep 1
up "$N" "$T" "$P" || exit 1
if [ "$K" = pico ]; then sleep 3; for _ in $(seq 1 30); do [ -e "$P" ] && break; sleep 1; done; sleep 2; fi
: > "$D/$N-cp-serial.log"; ($PY "$S/serial_capture.py" "$P" 300 "$D/$N-cp-serial.log" 2> /dev/null &)
if [ "$5" = flashclient ]; then up "$C" "${CLIENT_TREE:?set CLIENT_TREE}" "$CP" || exit 1; fi
for _ in $(seq 1 90); do grep -a -q "Starting fallback AP" "$D/$N-cp-serial.log" && break; sleep 2; done; sleep 3
: > "$D/$N-cpclient-serial.log"
($PY "$S/serial_capture.py" "$CP" 240 "$D/$N-cpclient-serial.log" --reset 2> /dev/null &)
for _ in $(seq 1 120); do grep -a -q "real save status" "$D/$N-cpclient-serial.log" && break; sleep 2; done
sleep 30; pkill -f "[s]erial_capture.py $P "; pkill -f "[s]erial_capture.py $CP "
echo "== $N client:"
grep -a -E "probe|real save|connected, ip" "$D/$N-cpclient-serial.log" | sed 's/ip [0-9.]*/ip <ip>/' | cut -c1-160
echo "== $N DUT: wifi saves $(grep -a -c 'Requested WiFi Settings Change' "$D/$N-cp-serial.log")," \
  "crash lines $(grep -a -c -E 'Exception|Panic|HardFault|abort\(\)|WDT' "$D/$N-cp-serial.log")," \
  "joined $(grep -a -c -E 'WiFi Connected|Connected to' "$D/$N-cp-serial.log")"
