#!/bin/bash
# run.sh NAME TREE YAML PORT BAUD ROUNDS: flash NAME, capture serial, press each tx button ROUNDS times with
# ../press.py, then the soak button, and count the markers, triggers and binary sensor presses on serial
S=$(cd "$(dirname "$0")" && pwd); K=$(dirname "$(dirname "$S")"); N=$1; T=$2; Y=$3; P=$4; B=$5; R=$6
PY=${PY:-$T/venv/bin/python}
pkill -f "[s]erial_capture.py $P "
TREE=$T PIO_CORE=${PIO_CORE:-$S/pio} bash "$K/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
rm -f "$S/$N-serial.txt"
# boot and IP lookup (up to 160 s), 7 presses of 1.5 s per round, the 90 s soak, margin; stopped after the drive
"$PY" "$K/serial_capture.py" "$P" $((160 + R * 11 + 120)) "$S/$N-serial.txt" --baud "$B" --reset > /dev/null 2>&1 &
CAP=$!
IP=""; for _ in $(seq 60); do IP=$(grep -a -o 'IP Address: [0-9.]*' "$S/$N-serial.txt" 2>/dev/null | tail -1 | cut -d' ' -f3); [ -n "$IP" ] && break; sleep 2; done
[ -n "$IP" ] || IP=$(bash "$K/resolve.sh" "$N" 20) || { echo no ip; kill $CAP; exit 1; }
sleep 5
PRESSES=()
for _ in $(seq "$R"); do
  for b in tx_nec tx_raw21a tx_raw21b tx_raw41 tx_raw43 tx_raw121 tx_raw245; do PRESSES+=("$b:1.5"); done
done
"$PY" "$S/../press.py" "$IP" "${PRESSES[@]}" soak:90 > "$S/$N-drive.txt" 2>&1
sleep 3; kill $CAP; wait $CAP 2>/dev/null
cat "$S/$N-drive.txt"
echo "MARK tx $(grep -a -c 'MARK tx' "$S/$N-serial.txt") triggers $(grep -a -c 'TRIG len=' "$S/$N-serial.txt")" \
  "binary sensor on_press $(grep -a -c 'BS pronto a ON' "$S/$N-serial.txt")"
