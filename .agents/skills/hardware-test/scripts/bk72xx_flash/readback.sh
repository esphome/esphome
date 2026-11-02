#!/bin/bash
# usage: readback.sh TAG [START LENGTH]   read flash from a BK72xx over its adapter into TAG.bin (whole chip by default)
# env: PORT (adapter), FAMILY (ltchiptool family, default beken-7238), FLASH_BK (path to flash_bk72xx.py),
# LT (LibreTiny penv python, default ~/.platformio/penv/.libretiny/bin/python), SIZE (whole chip bytes, default
# 2097152). A whole 2 MB chip takes about 190 s. The read must come back at the expected size.
T=${1:?tag}; P=${PORT:?set PORT}; FB=${FLASH_BK:?set FLASH_BK}; LT=${LT:-$HOME/.platformio/penv/.libretiny/bin/python}
ARGS=(); [ -n "$2" ] && ARGS=(-s "$2" -l "${3:?length}")
WANT=$(( ${3:-${SIZE:-2097152}} ))
: > "$T.log"
ok=
for i in 1 2 3; do
  echo "### try $i" >> "$T.log"
  rm -f "$T.bin"
  timeout 900 "$LT" "$FB" flash read -d "$P" "${ARGS[@]}" "${FAMILY:-beken-7238}" "$T.bin" >> "$T.log" 2>&1 &&
    [ "$(wc -c < "$T.bin" | tr -d ' ')" = "$WANT" ] && ok=1 && break
  sleep 3
done
[ -n "$ok" ] || { echo "FAIL: read $T (expected $WANT bytes, see $T.log)"; exit 1; }
shasum -a 256 "$T.bin"
