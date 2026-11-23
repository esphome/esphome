#!/bin/bash
# run.sh LABEL=TREE...: for each build, flash tts<LABEL>.yaml (written here, packaging tts-common.yaml) from TREE over
# PORT, then drive two short-intro and two long-intro announcements and one media play of the paused streams, and
# print the timings with an.py. Serve www/s1.flac (short intro) and www/s2.flac (long intro) made by mk.py.
# Run it from a scratch copy of this directory holding secrets.yaml (wifi_ssid, wifi_password, api_key),
# aux-2-pre-prod.yaml and www/.
# env: PORT, SC (path to serial_capture.py), PY (python with esphome's requirements), DRIVE (path to
# scripts/speaker_media_player/drive.py, needed from a scratch copy), HOST_IP (this host's address as the board
# sees it), API_KEY, SRV_PORT (default 18765), PAUSE (seconds the server holds the stream, default 8)
[ $# -gt 0 ] || { echo "FAIL: no builds given (LABEL=TREE...)"; exit 1; }
D=$(cd "$(dirname "$0")" && pwd); P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:?set PY}; K=${API_KEY:?set API_KEY}
SP=${SRV_PORT:-18765}; U=http://${HOST_IP:?set HOST_IP}:$SP; DRIVE=${DRIVE:-$D/../speaker_media_player/drive.py}
[ -f "$DRIVE" ] || { echo "FAIL: set DRIVE to scripts/speaker_media_player/drive.py"; exit 1; }
[ -f "$D/www/s1.flac.cut" ] && [ -f "$D/www/s2.flac.cut" ] || { echo "FAIL: make www/s1.flac and www/s2.flac with mk.py"; exit 1; }
: > "$D/srv.log"; ARGS=(); FAIL=0
pkill -f "[s]rv.py $SP" && sleep 1  # an old server keeps its own PAUSE, www and log
(cd "$D" && exec python3 srv.py "$SP" www >> srv.log 2>&1 < /dev/null) & SRV=$!; trap 'kill $SRV 2>/dev/null' EXIT; sleep 1
kill -0 $SRV 2> /dev/null || { echo "FAIL: srv.py did not start"; cat "$D/srv.log"; exit 1; }
for arg in "$@"; do
  v=${arg%%=*}; T=${arg#*=}; O=$D/out-tts$v; rm -rf "$O"; mkdir -p "$O"
  printf 'esphome:\n  name: tts%s\npackages:\n  tts: !include tts-common.yaml\n' "$v" > "$D/tts$v.yaml"
  pkill -f "[s]erial_capture.py $P"
  (cd "$T" && $PY -m esphome run --no-logs "$D/tts$v.yaml" --device "$P" > "$O/upload.log" 2>&1) ||
    { echo "FAIL: $v build or upload"; FAIL=1; continue; }
  $PY "$SC" "$P" 200 "$O/serial.txt" --ms > /dev/null 2>&1 & CAP=$!
  ip=; for _ in $(seq 60); do
    ip=$(grep -a -oE 'IP Address: [1-9][0-9.]+' "$O/serial.txt" 2>/dev/null | tail -1 | awk '{print $3}'); [ -n "$ip" ] && break; sleep 1
  done
  [ -n "$ip" ] || { echo "FAIL: $v no IP address"; kill $CAP; FAIL=1; continue; }
  sleep 15
  echo "=== $v $(date +%s)" >> "$D/srv.log"
  R=("press:Reset frames" wait:1)
  $PY "$DRIVE" "$ip" "$K" "${R[@]}" "ann:$U/s1.flac" wait:18 "${R[@]}" "ann:$U/s1.flac" wait:18 \
    "${R[@]}" "ann:$U/s2.flac" wait:18 "${R[@]}" "ann:$U/s2.flac" wait:18 "${R[@]}" "play:$U/s1.flac" wait:18 stop \
    > "$O/drive.log" 2>&1 || { echo "FAIL: $v driver"; FAIL=1; }
  kill $CAP 2>/dev/null; wait $CAP 2>/dev/null
  ARGS+=("$v=$O/serial.txt")
done
[ ${#ARGS[@]} -gt 0 ] && { python3 "$D/an.py" "$D/srv.log" "$D/www" "${ARGS[@]}" || FAIL=1; }
[ "$FAIL" = 0 ] || exit 1
echo PASS
