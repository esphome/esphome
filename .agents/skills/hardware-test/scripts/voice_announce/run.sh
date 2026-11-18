#!/bin/bash
# run.sh DEVICE LABEL=CONFIG=TREE...: for each build, compile CONFIG from TREE, OTA it to DEVICE (address or .local
# name), wait 45 s, then run two late-start announcements with drive.py and print the playback timeline.
# CONFIG is the device's own config with slow-announce.yaml added as a package (and its own API key, so Home
# Assistant cannot hold the voice assistant subscription).
# env: PY (python with esphome's requirements), API_KEY, DELAY (seconds before the first byte, default 3.5), FLAC
D=$(cd "$(dirname "$0")" && pwd); DEV=${1:?device}; shift; PY=${PY:?set PY}; K=${API_KEY:?set API_KEY}; FAIL=0
[ $# -gt 0 ] || { echo "FAIL: no builds given (LABEL=CONFIG=TREE...)"; exit 1; }
for arg in "$@"; do
  v=${arg%%=*}; rest=${arg#*=}; C=${rest%%=*}; T=${rest#*=}; O=$D/out-va$v; rm -rf "$O"; mkdir -p "$O"
  case $C in /*) ;; *) C=$PWD/$C;; esac
  (cd "$T" && PYTHONUNBUFFERED=1 timeout 3000 $PY -m esphome run --no-logs "$C" --device "$DEV" > "$O/upload.log" 2>&1) ||
    { echo "FAIL: $v build or upload"; exit 1; }
  sleep 45
  for r in 1 2; do
    echo "== $v run $r"
    $PY "$D/drive.py" "$DEV" "$K" "${DELAY:-3.5}" "$O/run$r.txt" > "$O/drive$r.log" 2>&1 ||
      { echo "FAIL: drive.py exited $?:"; tail -5 "$O/drive$r.log"; FAIL=1; }
    [ -f "$O/run$r.txt" ] && grep -v "sensor:\|wifi\|Free Memory\|api.connection\|Event Type\|Desired" "$O/run$r.txt" |
      grep -E "PRESS|HTTP|VA |State changed from (STREAMING_RESPONSE|IDLE|RESPONSE_FINISHED)|STREAMING_MICROPHONE$|micro_decoder|media_player|announc|Error" |
      cut -c1-140 | head -24
  done
done
[ "$FAIL" = 0 ] || { echo "FAIL: a run failed, see out-va*/drive*.log"; exit 1; }
echo PASS
