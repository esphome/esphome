#!/bin/bash
# chain.sh SPEC...: SPEC is NAME:YAML:TREE:FLASHDEV:LOGPORT[:NSEC[:STEPS]]. Flashes NAME from TREE (FLASHDEV is a
# serial port or the board's current address, "-" skips the flash), resolves NAME.local and runs suite.sh NAME with
# LOGPORT (a serial port or "api"). STEPS is a comma separated suite.sh step list. env as for suite.sh, plus PY.
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1
fail=0
for spec in "$@"; do
  IFS=: read -r TAG Y W DEV LP NS ST <<< "$spec"
  echo "$(date +%T) === $TAG"
  if [ "$DEV" != "-" ]; then
    ok=0
    for t in 1 2 3 4 5; do
      TREE=$W ../pio_build.sh upload "$TAG" "$Y" "$DEV" > "flash-$TAG.log" 2>&1 && { ok=1; break; }
      echo "$(date +%T) $TAG upload try $t failed, see flash-$TAG.log"; sleep 20
    done
    [ $ok = 1 ] || { echo "$TAG flash failed"; fail=1; continue; }
  fi
  IP=$(../resolve.sh "$TAG" 100) || { echo "$TAG did not resolve"; fail=1; continue; }
  echo "$(date +%T) $TAG at $IP"
  [ "$LP" = "api" ] || pkill -f "[s]erial_capture.py $LP "
  # shellcheck disable=SC2086 # ST is a step list
  if NSEC=${NS:-600} ./suite.sh "$TAG" "$IP" "$LP" ${ST//,/ } > "suite-$TAG.log" 2>&1; then
    echo "$(date +%T) === $TAG done"
  else
    echo "$(date +%T) === $TAG FAILED, see suite-$TAG.log"; fail=1
  fi
done
exit $fail
