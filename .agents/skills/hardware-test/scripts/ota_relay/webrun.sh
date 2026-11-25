#!/bin/bash
# webrun.sh LABEL IMAGE COUNT LPORT CAPLOG [full|abort]: web_server uploads to dut-ap.yaml through the station relay,
# each started SYNC_DELAY s (default 3) after the DUT logs SYNC (default its fallback AP retry line) in CAPLOG, so the
# upload begins in a quiet gap and must span the next retry. abort tells the relay to cut the upload after 25 s
# (abort-ch0 flag file next to relay_host.py). For plain repeated uploads without the sync use
# ota_automations/webrep.sh.
L=$1 IMG=$2 N=$3 P=$4 CAP=$5 MODE=${6:-full}
D=$(cd "$(dirname "$0")" && pwd)
SYNC=${SYNC:-RETRY_HIDDEN → RESTARTING}
for i in $(seq 1 "$N"); do
  B=$(wc -c <"$CAP")
  ok=
  for _ in $(seq 1 120); do
    tail -c +$((B + 1)) "$CAP" | LC_ALL=C grep -aq "$SYNC" && ok=1 && break
    sleep 1
  done
  [ -n "$ok" ] || { echo "$(date +%T) $L no new SYNC line in $CAP within 120 s, stopping"; exit 1; }
  sleep "${SYNC_DELAY:-3}"
  echo "$(date +%T) $L $MODE upload $i start"
  if [ "$MODE" = abort ]; then
    curl -s -w ' %{http_code} %{size_upload} %{time_total}\n' --max-time 120 -F "file=@$IMG" \
      "http://127.0.0.1:$P/update" >"$D/curl-$L-$i.out" &
    CP=$!
    sleep 25
    if kill -0 $CP 2>/dev/null; then
      touch "$D/abort-ch0"
      echo "$(date +%T) $L relay told to cut the upload"
    else
      echo "$(date +%T) $L upload $i ended before the cut, NOT ABORTED"
    fi
    wait $CP
    rm -f "$D/abort-ch0"
    echo "$(date +%T) $L abort upload $i result reply code/bytes/secs: $(cat "$D/curl-$L-$i.out")"
    sleep 75
    continue
  fi
  curl -s -w ' %{http_code} %{size_upload} %{time_total}\n' --max-time 240 -F "file=@$IMG" \
    "http://127.0.0.1:$P/update" >"$D/curl-$L-$i.out"
  echo "$(date +%T) $L $MODE upload $i result reply code/bytes/secs: $(cat "$D/curl-$L-$i.out")"
  sleep 40
done
