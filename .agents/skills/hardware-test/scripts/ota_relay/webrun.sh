#!/bin/bash
# webrun.sh LABEL IMAGE COUNT LPORT CAPLOG [full|abort]: web_server uploads to dut-ap.yaml through the station relay,
# each started SYNC_DELAY s (default 3) after the DUT logs SYNC (default its fallback AP retry line) in CAPLOG, so the
# upload begins in a quiet gap and must span the next retry. abort tells the relay to cut the upload after 25 s
# (abort-ch0 flag file). Run it from the directory where relay_host.py runs.
L=$1 IMG=$2 N=$3 P=$4 CAP=$5 MODE=${6:-full}
SYNC=${SYNC:-RETRY_HIDDEN → RESTARTING}
for i in $(seq 1 "$N"); do
  B=$(LC_ALL=C grep -a -c "$SYNC" "$CAP")
  for _ in $(seq 1 120); do
    [ "$(LC_ALL=C grep -a -c "$SYNC" "$CAP")" -gt "$B" ] && break
    sleep 1
  done
  sleep "${SYNC_DELAY:-3}"
  echo "$(date +%T) $L $MODE upload $i start"
  if [ "$MODE" = abort ]; then
    curl -s -o /dev/null --max-time 120 -F "file=@$IMG" "http://127.0.0.1:$P/update" &
    CP=$!
    sleep 25
    touch abort-ch0
    echo "$(date +%T) $L relay told to cut the upload"
    wait $CP
    sleep 75
    continue
  fi
  curl -s -o /dev/null -w '%{http_code} %{size_upload} %{time_total}\n' --max-time 240 -F "file=@$IMG" \
    "http://127.0.0.1:$P/update" >"curl-$L-$i.out"
  echo "$(date +%T) $L $MODE upload $i result code/bytes/secs: $(cat "curl-$L-$i.out")"
  sleep 40
done
