#!/bin/bash
# wifisave_hammer.sh: the device under test (DUT) falls back to its AP; a client board joins it and posts
# /wifisave repeatedly, then the real credentials. Compiles, flashes, captures both serial logs and
# prints counts. Configs come from templates/example-dut.yaml and templates/example-client.yaml.
#
# Usage: wifisave_hammer.sh DUT_KIND DUT_PORT DUT_YAML DUT_TREE CLIENT_PORT CLIENT_YAML CLIENT_TREE [SETTLE_S]
#   DUT_KIND    esp (esphome upload), pico (upload, then re-find the CDC port by USB serial number),
#               bw15 (flash_rtl87xx_kit.py) or bk (flash_bk72xx.py)
#   *_YAML      absolute path; the file name without .yaml must equal the config's `name`
#   *_TREE      checkout or worktree whose esphome code must build that yaml (cwd for python -m esphome)
#   SETTLE_S    seconds to keep capturing after the client reports the real save (default 40; BW15 wants 90)
# Env: PY (python with esphome; default DUT_TREE/venv/bin/python, else python3 on PATH), LTPY
#      (LibreTiny penv python with ltchiptool; default ~/.platformio/penv/.libretiny/bin/python)
# Output next to each yaml: NAME-compile.log, NAME-upload.log, NAME-serial.log, and DUT NAME-events.txt
# (addresses redacted so the extract can go on a PR; the serial logs themselves stay local).
# Exit status: 0 RESULT PASS (no crash marker, one boot, real save answered 200, a station address);
# 3 RESULT FAIL (the run completed and the counts show a failure); 2 RESULT INVALID (a precondition was
# missed, the counts are printed but mean little); 1 when a build, flash or port step fails (nothing is reported).
set -u
SKILL=$(cd "$(dirname "$0")/.." && pwd)
KIND=$1; DPORT=$2; DYAML=$3; DTREE=$4; CPORT=$5; CYAML=$6; CTREE=$7; SETTLE=${8:-40}
PY=${PY:-$DTREE/venv/bin/python}; [ -x "$PY" ] || PY=$(command -v python3)
LTPY=${LTPY:-$HOME/.platformio/penv/.libretiny/bin/python}
DDIR=$(dirname "$DYAML"); DNAME=$(basename "$DYAML" .yaml)
CDIR=$(dirname "$CYAML"); CNAME=$(basename "$CYAML" .yaml)
DLOG=$DDIR/$DNAME-serial.log; CLOG=$CDIR/$CNAME-serial.log
INVALID=""
AP_WAIT=120; UPLOAD_WAIT=300; SAVE_WAIT=240
DUT_CAPTURE=$((AP_WAIT + UPLOAD_WAIT + SAVE_WAIT + SETTLE + 60)); CLIENT_CAPTURE=$((SAVE_WAIT + SETTLE + 60))

bound() { local t=$1; shift; if command -v timeout >/dev/null; then timeout "$t" "$@"; else perl -e 'alarm shift; exec @ARGV' "$t" "$@"; fi; }
count() { LC_ALL=C grep -a -c "$1" "$2"; }
wait_for() { # wait_for FILE PATTERN SECONDS -> prints elapsed, status 1 on timeout
  local n=0; until LC_ALL=C grep -a -q "$2" "$1"; do sleep 2; n=$((n+2)); [ $n -ge "$3" ] && { echo "$n"; return 1; }; done; echo "$n"; }
# Only this run's captures: another capture on the host (a soak on a third board) must survive
stop_tails() { pkill -f "[s]erial_capture.py $DPORT " 2>/dev/null; pkill -f "[s]erial_capture.py $CPORT " 2>/dev/null; sleep 1; }
fail() { echo "[hammer] FAILED: $1"; stop_tails; exit 1; }
compile() { # compile YAML TREE
  (cd "$2" && PYTHONUNBUFFERED=1 bound 1500 "$PY" -m esphome compile "$1" > "${1%.yaml}-compile.log" 2>&1); local rc=$?
  echo "[hammer] compile $(basename "$1"): rc=$rc"; return $rc; }
esp_upload() { # esp_upload YAML PORT TREE
  (cd "$3" && bound $UPLOAD_WAIT "$PY" -m esphome upload "$1" --device "$2" > "${1%.yaml}-upload.log" 2>&1)
  local ok; ok=$(count 'Successfully uploaded' "${1%.yaml}-upload.log"); echo "[hammer] upload $(basename "$1"): ok=$ok"; [ "$ok" -ge 1 ]; }

stop_tails
compile "$DYAML" "$DTREE" || fail "DUT build failed, see $DDIR/$DNAME-compile.log"
compile "$CYAML" "$CTREE" || fail "client build failed, see $CDIR/$CNAME-compile.log"

TAILOPT=""
case $KIND in
  esp) esp_upload "$DYAML" "$DPORT" "$DTREE" || fail "DUT upload failed" ;;
  pico)
    SN=$("$PY" "$SKILL/scripts/ports.py" --serial-of "$DPORT") || fail "no USB serial number for $DPORT"
    esp_upload "$DYAML" "$DPORT" "$DTREE" || fail "DUT upload failed"
    # The Pico drops its CDC port on reboot and may already be back: take whatever path this board has now;
    # the capture follows the serial number if the path changes again
    DPORT=$("$PY" "$SKILL/scripts/ports.py" --wait 2e8a --serial "$SN" --timeout 60) || fail "pico did not re-enumerate"
    echo "[hammer] pico port after upload: $DPORT"; sleep 2 ;;
  bw15|bk)
    UF2=$(ls "$DDIR/.esphome/build/$DNAME/.pioenvs/$DNAME/"*.uf2 2>/dev/null | head -1)
    [ -n "$UF2" ] || fail "no uf2 under $DDIR/.esphome/build/$DNAME"
    echo "[hammer] uf2: ${UF2##*/}"
    W=$SKILL/scripts/flash_rtl87xx_kit.py; [ "$KIND" = bk ] && W=$SKILL/scripts/flash_bk72xx.py
    (cd "$DDIR" && bound 400 "$LTPY" "$W" flash write -d "$DPORT" "$UF2" > "$DDIR/$DNAME-upload.log" 2>&1); rc=$?
    echo "[hammer] $KIND flash: rc=$rc"; [ $rc -eq 0 ] || fail "DUT flash failed, see $DDIR/$DNAME-upload.log"
    TAILOPT="--released" ;;
  *) echo "unknown DUT_KIND $KIND"; exit 1 ;;
esac

: > "$DLOG"; ("$PY" "$SKILL/scripts/serial_capture.py" "$DPORT" $DUT_CAPTURE "$DLOG" $TAILOPT &)
if ! n=$(wait_for "$DLOG" "Captive portal\|Starting fallback AP" $AP_WAIT); then
  if LC_ALL=C grep -a -q '\[capture\] open failed' "$DLOG" && ! LC_ALL=C grep -a -q '^\[[0-9:]*\]\[^\[]' "$DLOG"; then
    fail "could not open $DPORT (held by another process, or the wrong port), see $DLOG"
  fi
  fail "DUT never raised its AP within ${n}s, see $DLOG"
fi
echo "[hammer] DUT fallback AP after ${n}s"; sleep 2

esp_upload "$CYAML" "$CPORT" "$CTREE" || fail "client upload failed"
: > "$CLOG"; ("$PY" "$SKILL/scripts/serial_capture.py" "$CPORT" $CLIENT_CAPTURE "$CLOG" &)
if n=$(wait_for "$CLOG" "real save status\|posting the real" $SAVE_WAIT); then
  echo "[hammer] client posted the real credentials after ${n}s; settling ${SETTLE}s"; sleep "$SETTLE"
else
  INVALID="client never reached the real save within ${n}s"; echo "[hammer] WARNING: $INVALID"
fi

CRASHES=$(count 'Panic\|HardFault\|Fault\|abort\|Backtrace\|Exception\|Guru' "$DLOG"); BOOTS=$(count 'Log initialized' "$DLOG")
STA=$(LC_ALL=C grep -a 'IP Address:' "$DLOG" | grep -v -c '192\.168\.4\.'); REAL=$(LC_ALL=C grep -a -o 'real save status [-0-9]*' "$CLOG" | tail -1 | awk '{print $4}')
echo "[hammer] $DNAME: saves requested $(count 'Requested WiFi Settings Change' "$DLOG"), cooldown exits $(count 'cooldown early' "$DLOG"), crash markers $CRASHES, boots $BOOTS, STA addresses $STA"
echo "[hammer] $CNAME: bogus saves answered $(count 'bogus save status 200' "$CLOG"), failed $(count 'status -1' "$CLOG"), real save status ${REAL:-none}"
LC_ALL=C grep -a "cooldown early\|Panic\|Fault\|Exception\|Retry phase\|Requested WiFi\|Connected\|IP Address\|Captive portal\|fallback AP" "$DLOG" | LC_ALL=C tr -cd '\11\12\15\40-\176' | sed -E 's/(IP Address: )[^ ]+/\1<redacted>/' | cut -c1-110 | head -80 > "$DDIR/$DNAME-events.txt"
echo "[hammer] events: $DDIR/$DNAME-events.txt"
stop_tails
[ -z "$INVALID" ] || { echo "[hammer] RESULT INVALID: $INVALID"; exit 2; }
if [ "$CRASHES" -eq 0 ] && [ "$BOOTS" -eq 1 ] && [ "${REAL:-0}" = 200 ] && [ "$STA" -ge 1 ]; then echo "[hammer] RESULT PASS"; exit 0; fi
echo "[hammer] RESULT FAIL: crash markers $CRASHES, boots $BOOTS, real save status ${REAL:-none}, STA addresses $STA"; exit 3
