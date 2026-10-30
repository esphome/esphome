#!/bin/bash
# check.sh MODE RC LOG ACTIVE: PASS when an ota.sh upload ended the way MODE requires, FAIL (exit 1) otherwise.
# none, lostreply and lostreq must succeed and the device must then run the new image; lostreq2 must fail with the old
# image still running and the new one pending. ACTIVE is the exit code of mcuboot_slot.py active.
M=${1:?mode}; RC=${2:?rc}; LOG=${3:?log}; ACTIVE=${4:?active rc}
fail() { echo "FAIL: $M $*"; exit 1; }
if [ "$M" != none ]; then
  LC_ALL=C grep -a -q "INJECT FAIL" "$LOG" && fail "no reset was faulted"
  LC_ALL=C grep -a -q "^INJECT reset" "$LOG" || fail "no INJECT line in the log"
fi
case $M in
  none | lostreply | lostreq)
    [ "$RC" = 0 ] || fail "upload exited $RC"
    LC_ALL=C grep -a -q "Successfully uploaded" "$LOG" || fail "no 'Successfully uploaded' line"
    [ "$ACTIVE" = 0 ] || fail "the device does not run the new image" ;;
  lostreq2)
    [ "$RC" != 0 ] || fail "upload exited 0, it must fail when no reset reaches the device"
    LC_ALL=C grep -a -q "still runs the old image" "$LOG" || fail "no 'still runs the old image' error"
    [ "$ACTIVE" = 0 ] || fail "the new image is not pending behind the old one" ;;
  *) fail "unknown mode" ;;
esac
echo "PASS: $M"
