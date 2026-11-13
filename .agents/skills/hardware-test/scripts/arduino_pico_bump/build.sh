#!/bin/zsh
# build.sh NAME TREE : compile NAME.yaml from TREE with a private PlatformIO core dir
# env: PY, LOCK (optional wrapper that serialises builds on a shared machine)
D=${0:A:h}; N=$1; T=$2; PY=${PY:?set PY}
L=$D/$N-compile.log
${=LOCK} zsh -c "cd $D && export PLATFORMIO_CORE_DIR=$D/pio PYTHONPATH=$T PYTHONUNBUFFERED=1 && $PY -c 'import esphome; print(\"esphome from\", esphome.__file__)' > $L 2>&1 && timeout 1500 $PY -m esphome compile $D/$N.yaml >> $L 2>&1; echo rc=\$? >> $L"
grep -a -E "esphome from|RAM:|Flash:|rc=|Deleting|ERROR" $L | sed 's/\x1b\[[0-9;]*m//g' | tail -8
