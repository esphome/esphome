#!/bin/bash
# run.sh V [SECS]: build and upload lzhzV.yaml to the ESP8266 and lzs3V.yaml to the S3 from TREE, then capture both
# boards from a reset for SECS (default 330) into runV-hz.log and runV-s3.log; summarise with `ana.py V`.
# env: TREE, PORT_HZ, PORT_S3, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
V=${1:?variant}; SECS=${2:-330}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
HZ=${PORT_HZ:?set PORT_HZ}; S3=${PORT_S3:?set PORT_S3}
port() { if [ "$1" = hz ]; then echo "$HZ"; else echo "$S3"; fi; }
# Both builds finish before either board is flashed; builds stay serial, they share the PlatformIO caches
for b in hz s3; do TREE=$T PY=$PY "$S/pio_build.sh" compile "lz$b$V" "$D/lz$b$V.yaml" || exit 1; done
for b in hz s3; do TREE=$T PY=$PY "$S/pio_build.sh" upload "lz$b$V" "$D/lz$b$V.yaml" "$(port $b)" || exit 1; done
PIDS=()
for b in hz s3; do
  $PY "$S/serial_capture.py" "$(port $b)" "$SECS" "$D/run$V-$b.log" --reset > "$D/run$V-$b.err" 2>&1 &
  PIDS+=($!)
done
RC=0; I=0
for b in hz s3; do
  wait "${PIDS[I++]}" || { echo "capture failed on $b:"; cat "$D/run$V-$b.err"; RC=1; }
done
exit $RC
