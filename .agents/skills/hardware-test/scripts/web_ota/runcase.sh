#!/bin/bash
# runcase.sh LOG HOST IMAGE LABEL MODE [CUT] [OFFSET]: one shortpost.py run against HOST while serial_capture.py writes
# LOG; prints the device lines the upload caused and a liveness check. PYTHON overrides the interpreter.
LOG=$1 HOST=$2 IMG=$3 L=$4
shift 4
D=$(cd "$(dirname "$0")" && pwd)
PY=${PYTHON:-python3}
for try in 1 2 3 4 5 6; do
  N=$(wc -l <"$LOG")
  timeout 700 "$PY" "$D/shortpost.py" "$HOST" "$IMG" "$@" >"$D/case-$L.out" 2>&1
  rc=$?
  sleep 8
  NEW=$(tail -n +$((N + 1)) "$LOG")
  # Retry only a case the network cut before the final chunk; close and hold cut it on purpose
  if [ "$1" != close ] && [ "$1" != hold ] && ! grep -aq "final chunk" <<<"$NEW" && grep -aq "interrupted" <<<"$NEW"; then
    [ "$try" = 6 ] && echo "$L RETRIES EXHAUSTED: every try was cut by the network, no result for this case" && break
    echo "$L try $try interrupted by the network, retrying"
    sleep 10
    continue
  fi
  break
done
echo "=== $L ($*) try $try client rc=$rc"
[ "$rc" = 0 ] || tail -3 "$D/case-$L.out"
grep -a "mode=\|server reply\|closed" "$D/case-$L.out" | sed 's/Access-Control.*\\r\\n\\r\\n/ ... /'
grep -a "final chunk\|Image incomplete\|end failed\|staged\|successful\|interrupted\|OTAEV\|rst cause\|@cp\|e:3\|BOOTMARK\|Exception\|ota:[0-9]*\]: [^O]" <<<"$NEW" | grep -v "in progress"
C=$(curl -s -o /dev/null -w '%{http_code}' --max-time 10 "http://$HOST/")
echo "alive check: GET / -> $C; last alive: $(tail -n 2000 "$LOG" | grep -a 'alive build' | tail -1)"
