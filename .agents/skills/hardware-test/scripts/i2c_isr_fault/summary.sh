#!/bin/bash
# summary.sh CAPTURE...: crashes, watchdogs, timeout kinds and the read counters per capture
for f in "$@"; do
  first=$(grep -a -m1 'rst:0x' "$f" | cut -c2-9); last=$(tail -1 "$f" | cut -c2-9)
  echo "== $(basename "$f") $first..$last boots=$(grep -a -c 'rst:0x' "$f") guru=$(grep -a -c Guru "$f") twdt=$(grep -a -c 'task_wdt: Task watchdog' "$f") diag=$(grep -a -c 'DIAG stuck' "$f")"
  echo "   sw=$(grep -a -c 'software timeout' "$f") hw=$(grep -a -c 'hardware timeout detected' "$f") trans=$(grep -a -c 'transaction timeout detected' "$f") busy=$(grep -a -c 'still busy' "$f") ina_garbage_lines=$(grep -a -c 'INA bus voltage garbage' "$f")"
  grep -a 'MASTER' "$f" | tail -1 | sed 's/^.*MASTER/   MASTER/'
  grep -a 'SLAVE' "$f" | tail -1 | sed 's/^.*SLAVE/   SLAVE/'
  echo "   ok per 30s window (min over windows after arm): $(grep -a MASTER "$f" | sed 's/.* ok=\([0-9]*\).*/\1/' | awk 'NR>2{d=$1-p; if(min==""||d<min)min=d} {p=$1} END{print (min == "" ? "n/a (fewer than 3 MASTER windows)" : min)}')"
  echo "   harness: install_errors=$(grep -a -c -E 'task creation failed|slave ISR install failed|slave install did not finish|selects no fault' "$f") $(grep -a -o 'selftest1 write=[-0-9]* read=[-0-9]* val=[0-9A-F]*' "$f" | head -1)"
done
