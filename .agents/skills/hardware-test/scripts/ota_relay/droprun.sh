#!/bin/bash
# droprun.sh LABEL IMAGE COUNT web|nat [TREE]: COUNT uploads to dut-sta.yaml through the AP relay, each with an AP drop of
# DROP_MS (default 5000) at DROP_AT s (default 20) into the upload, GAP s (default 60) apart. web posts IMAGE to
# 127.0.0.1:WEB_PORT/update (default 18081, the relay channel to port 80); nat runs `esphome upload` from TREE (the
# checkout under test) against 127.0.0.1, so map the DUT's OTA port on the other channel. PYTHON overrides the
# interpreter.
L=$1 IMG=$2 N=$3 M=$4 TREE=${5:-}
D=$(cd "$(dirname "$0")" && pwd)
IMG=$(cd "$(dirname "$IMG")" && pwd)/$(basename "$IMG")
PY=${PYTHON:-python3}
[ "$M" = web ] || [ -d "$TREE" ] || { echo "nat needs TREE, the checkout under test"; exit 1; }
for i in $(seq 1 "$N"); do
  echo "$(date +%T) $L $M upload $i start"
  if [ "$M" = web ]; then
    curl -s -w ' %{http_code} %{size_upload} %{time_total}\n' --max-time 300 -F "file=@$IMG" \
      "http://127.0.0.1:${WEB_PORT:-18081}/update" >"$D/curl-$L-$i.out" 2>&1 &
  else
    (cd "$TREE" && PYTHONPATH=$TREE PYTHONUNBUFFERED=1 timeout 300 "$PY" -m esphome upload "$D/dut-sta.yaml" \
      --device 127.0.0.1 --file "$IMG") >"$D/nat-$L-$i.log" 2>&1 &
  fi
  CP=$!
  sleep "${DROP_AT:-20}"
  echo "${DROP_MS:-5000}" >"$D/drop-ms.tmp" && mv "$D/drop-ms.tmp" "$D/drop-ms"
  echo "$(date +%T) $L drop requested"
  wait $CP
  rc=$?
  if [ "$M" = web ]; then
    r=$(cat "$D/curl-$L-$i.out")
  else
    r="rc=$rc $(grep -a -E 'OTA successful|ERROR|Error' "$D/nat-$L-$i.log" | tail -2 | tr '\n' ' ')"
  fi
  echo "$(date +%T) $L $M upload $i done: $r"
  sleep "${GAP:-60}"
done
