#!/bin/bash
# rb.sh NAME COUNT OUT [TARGET6]: COUNT restarts of NAME through its Restart button; after each, wait up to 120 s for
# the meshcop omr field, the API and, when given, a ping6 to a Thread device's address TARGET6, and append one
# "cycle N ok=..." line to OUT. The API key is API_KEY, else apikey.txt, else plaintext. env: PY
D=$(cd "$(dirname "$0")" && pwd); N=$1; C=$2; O=$3; T6=$4
KEY=${API_KEY:-$(cat "$D/apikey.txt" 2>/dev/null)}
PY=${PY:-python3}
IP=$("$D/../resolve.sh" "$N" 20) || exit 1
for i in $(seq 1 "$C"); do
  t0=$(date +%s)
  if ! $PY "$D/../api_button.py" "$IP" "${KEY:--}" press Restart > /dev/null 2>&1; then
    echo "$(date +%T) cycle $i press failed" >> "$O"
    IP=$("$D/../resolve.sh" "$N" 20) || exit 1
  fi
  sleep 8
  ok=0; m=0; a=0; p=0
  [ -n "$T6" ] || p=1
  while (( $(date +%s) - t0 < 120 )); do
    [ $a = 1 ] || { timeout 8 $PY "$D/../api_button.py" "$IP" "${KEY:--}" list > /dev/null 2>&1 && a=1; }
    # only after the API is back, so a TXT record cached from before the restart cannot count
    [ $m = 1 ] || [ $a = 0 ] || { "$D/meshcop.sh" "$N" | grep -q "omr_present=1" && m=1; }
    [ $p = 1 ] || { ping6 -c 1 -i 1 "$T6" > /dev/null 2>&1 && p=1; }
    [ $m$a$p = 111 ] && { ok=1; break; }
    sleep 1
  done
  echo "$(date +%T) cycle $i ok=$ok meshcop=$m api=$a ping=$p back_after=$(( $(date +%s) - t0 ))s" >> "$O"
done
