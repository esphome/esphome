#!/bin/sh
# cases.sh HOST KEY MEDIA_URL ANN_URL CAPTURE LABEL : three play, announce, stop cycles on a speaker media player with the
# "Task names" probe button pressed idle, during media, during the announcement, after it and after the stop.
# KEY of - is plaintext. Serve FLAC files with `python3 -m http.server 8765 --directory www` (audio_http refuses WAV and MP3).
H=$1; K=$2; MEDIA=$3; ANN=$4; LOG=$5; LABEL=$6; PY=${PYTHON:-python3}; HERE=$(dirname "$0")
[ -r "$LOG" ] || { echo "usage: cases.sh HOST KEY MEDIA_URL ANN_URL CAPTURE LABEL (CAPTURE must be a readable file)"; exit 2; }
show() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a '\[tasks:\|speaker_media_player\|speaker_mixer\|\[W\]\|\[E\]\|Guru\|abort\|Backtrace' | grep -a -v 'took a long time' | cut -c1-180; }
for cycle in 1 2 3; do
  n0=$(wc -l < "$LOG"); echo "=== $LABEL cycle $cycle ($(date +%T))"
  $PY "$HERE/drive.py" "$H" "$K" "press:Task names" wait:1 "play:$MEDIA" wait:5 "press:Task names" wait:1 "ann:$ANN" wait:1.5 "press:Task names" wait:5 "press:Task names" wait:1 stop wait:3 "press:Task names" || { rc=$?; echo "driver failed ($rc), aborting"; exit $rc; }
  sleep 1; echo "--- device log:"; show "$n0"
done
