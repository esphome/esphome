#!/bin/zsh
# up.sh NAME TREE DEVICE : esphome upload with the private PlatformIO core dir (env PY)
D=${0:A:h}; PY=${PY:?set PY}
cd $D && PLATFORMIO_CORE_DIR=$D/pio PYTHONPATH=$2 PYTHONUNBUFFERED=1 timeout 300 $PY -m esphome upload $D/$1.yaml --device $3 > $D/$1-upload.log 2>&1; echo "rc=$?"
grep -a -E "OTA successful|Successfully|ERROR|error" $D/$1-upload.log | tail -3
