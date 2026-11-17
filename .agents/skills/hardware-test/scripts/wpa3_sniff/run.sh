#!/bin/bash
# usage: run.sh BUILDNAME TREE SECONDS   flash BUILDNAME.yaml (a station config built on st-common.yaml or
# stearly-common.yaml) from TREE to the station, then capture station and AP logs with millisecond stamps.
# env: ST (station port), AP (access point port), PY
W=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$W"); cd "$W" || exit 1; PY=${PY:-$2/venv/bin/python}
ST=${ST:?station port}; AP=${AP:?access point port}
pkill -f "[s]erial_capture.py $ST"; pkill -f "[s]erial_capture.py $AP"
TREE=$2 bash "$S/pio_build.sh" upload "$1" "$W/$1.yaml" "$ST" || exit 1
$PY "$S/serial_capture.py" "$AP" "$3" "$1-ap.txt" --ms > /dev/null 2>&1 &
$PY "$S/serial_capture.py" "$ST" "$3" "$1-st.txt" --ms > /dev/null 2>&1 &
wait
echo "$1 done"
