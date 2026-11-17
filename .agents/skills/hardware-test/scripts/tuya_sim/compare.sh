#!/bin/bash
# compare.sh A B: diff two run.sh results (NAME prefixes) in this directory: API states, entity dump block, the
# re-dump 1 s after init, hub trigger vs entity publish order, and the command frames the simulator received.
# Timestamps (with or without milliseconds) and line numbers are removed with ../lognorm.py.
cd "$(dirname "$0")" || exit 1; A=${1:?name}; B=${2:?name}; PY=${PY:-python3}
for n in $A $B; do
  $PY -c "
import json
for e in json.load(open('$n-states.json')): print(e['phase'], e['entity'], json.dumps(e['value'], sort_keys=True))" > $n-states.txt
  $PY ../lognorm.py $n-log.txt > $n-norm.txt
  awk '/^\[C\]\[logger\]: Logger:/{on=1} on' $n-norm.txt | grep -av "^\[[DIVW]\]" > $n-dumpfull.txt
  awk '/Tuya Binary Sensor:/{on=1} on{print} /Current Temperature has datapoint ID 22/{exit}' $n-dumpfull.txt > $n-ent.txt
  awk '/\[C\]\[tuya\]: Tuya:/{c++} c>=2 && /\[C\]\[tuya\]/' $n-dumpfull.txt > $n-redump.txt
  grep -a "\[ord\|SIM \(STEP\|TX\|DELIVER\)\|Datapoint [0-9]* update to" $n-norm.txt > $n-ord.txt
  grep -a "SIM RX" $n-norm.txt | grep -v "cmd 00" | sed -E 's/#[0-9]+ //' > $n-rx.txt
done
for k in states ent redump ord rx; do
  if diff -q $A-$k.txt $B-$k.txt > /dev/null; then echo "$k: identical ($(wc -l < $A-$k.txt) lines)"; else echo "$k: DIFFERS"; fi
done
