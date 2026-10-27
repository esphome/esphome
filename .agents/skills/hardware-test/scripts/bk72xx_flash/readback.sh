#!/bin/bash
# usage: readback.sh TAG [START LENGTH]   read flash from a BK72xx over its adapter into TAG.bin (whole chip by default)
# env: PORT (adapter), FAMILY (ltchiptool family, default beken-7238), FLASH_BK (path to flash_bk72xx.py),
# LT (LibreTiny penv python, default ~/.platformio/penv/.libretiny/bin/python). A whole 2 MB chip takes about 190 s.
T=${1:?tag}; P=${PORT:?set PORT}; FB=${FLASH_BK:?set FLASH_BK}; LT=${LT:-$HOME/.platformio/penv/.libretiny/bin/python}
ARGS=(); [ -n "$2" ] && ARGS=(-s "$2" -l "${3:?length}")
ok=
for i in 1 2 3; do
  timeout 900 "$LT" "$FB" flash read -d "$P" "${ARGS[@]}" "${FAMILY:-beken-7238}" "$T.bin" > "$T.log" 2>&1 && ok=1 && break
  sleep 3
done
[ -n "$ok" ] || { echo "FAIL: read $T"; exit 1; }
shasum -a 256 "$T.bin"
