#!/bin/bash
# pio_build.sh compile|upload NAME YAML [DEVICE] [esphome options...]: esphome compile or upload of YAML with
# `-s name NAME`, built from TREE with a private PlatformIO core directory. Logs to <yaml dir>/NAME-<action>.log and
# prints the RAM and Flash lines (compile) or the upload result. Exits 1 on a failure or when esphome would not import
# from TREE. A config that sets `name: ${name}` builds one copy per NAME, so a PR and a dev build of the same file
# never share a build directory.
# env: TREE (checkout under test), PY (default $TREE/venv/bin/python), PIO_CORE (default <yaml dir>/pio),
#      BUILD_LOCK (optional command prefix that serialises builds on a shared machine; compile only)
A=${1:?compile|upload}; N=${2:?name}; Y=${3:?yaml}; shift 3
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
Y=$(cd "$(dirname "$Y")" && pwd)/$(basename "$Y"); D=$(dirname "$Y")
export PLATFORMIO_CORE_DIR=${PIO_CORE:-$D/pio} PYTHONPATH=$T PYTHONUNBUFFERED=1
L=$D/$N-$A.log
F=$(cd "$T" && $PY -c 'import esphome, os; print(os.path.dirname(esphome.__file__))')
[ "$F" = "$(cd "$T" && pwd)/esphome" ] || { echo "esphome imports from $F, not $T"; exit 1; }
echo "esphome from $F" > "$L"
case $A in
  compile)
    (cd "$T" && ${BUILD_LOCK:-} timeout 1500 $PY -m esphome -s name "$N" "$@" compile "$Y") >> "$L" 2>&1
    RC=$?; grep -a -E "^(RAM|Flash):" "$L"; [ $RC = 0 ] || { echo "$N compile failed (see $L)"; exit 1; } ;;
  upload)
    DEV=${1:?device}; shift
    (cd "$T" && timeout 600 $PY -m esphome -s name "$N" "$@" upload "$Y" --device "$DEV") >> "$L" 2>&1 ||
      { echo "$N upload failed (see $L)"; tail -3 "$L"; exit 1; }
    grep -a -E "OTA successful|Successfully uploaded" "$L" | tail -1 ;;
  *) echo "unknown action $A"; exit 1 ;;
esac
