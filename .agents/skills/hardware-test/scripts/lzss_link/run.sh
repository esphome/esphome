#!/bin/bash
# run.sh V [SECS]: upload lzhzV.yaml to the ESP8266 and lzs3V.yaml to the S3 from TREE, then capture both boards from a
# reset for SECS (default 330) into runV-hz.log and runV-s3.log; summarise with `ana.py V`.
# env: TREE, PORT_HZ, PORT_S3, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
V=${1:?variant}; SECS=${2:-330}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
HZ=${PORT_HZ:?set PORT_HZ}; S3=${PORT_S3:?set PORT_S3}
TREE=$T PY=$PY "$S/pio_build.sh" upload "lzhz$V" "$D/lzhz$V.yaml" "$HZ" || exit 1
TREE=$T PY=$PY "$S/pio_build.sh" upload "lzs3$V" "$D/lzs3$V.yaml" "$S3" || exit 1
$PY "$S/serial_capture.py" "$HZ" "$SECS" "$D/run$V-hz.log" --reset > /dev/null 2>&1 &
$PY "$S/serial_capture.py" "$S3" "$SECS" "$D/run$V-s3.log" --reset > /dev/null 2>&1 &
wait
