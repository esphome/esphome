#!/bin/zsh
# usage: run.sh NAME TREE KIND SECONDS   generate, compile (bench lock), flash the C6 and capture its log
D=${DUCO_DIR:-$PWD}; cd $D  # scratch dir holding ducosim.hpp, gen.py, secrets.yaml and capduco.py (a serial_capture.py copy)
P=${DUCO_PORT:?set DUCO_PORT to the C6 port}; PY=${PY:?set PY}
L=${BUILDLOCK:-env}  # optional build lock wrapper
$PY gen.py $1 $3 || exit 1
PYTHONPATH=$2 $PY -c 'import esphome; print(esphome.__file__)' > $1-compile.log
PYTHONUNBUFFERED=1 PYTHONPATH=$2 $L timeout 1500 $PY -m esphome compile $D/$1.yaml >> $1-compile.log 2>&1 || { echo "$1 compile failed"; exit 1; }
grep -E '^(RAM|Flash):' $1-compile.log
pkill -f "[c]apduco.py $P "
PYTHONUNBUFFERED=1 PYTHONPATH=$2 $PY -m esphome upload $D/$1.yaml --device $P > $1-upload.log 2>&1 || { echo "$1 upload failed"; tail -5 $1-upload.log; exit 1; }
$PY capduco.py $P $4 $1-log.txt --reset
echo "$1 done"
