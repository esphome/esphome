#!/bin/bash
# wifisave_hammer.sh: the device under test (DUT) falls back to its AP; a client board joins it and posts
# /wifisave repeatedly, then the real credentials. Compiles, flashes, captures both serial logs and
# prints counts. Configs come from templates/example-dut.yaml and templates/example-client.yaml.
#
# Usage: wifisave_hammer.sh DUT_KIND DUT_PORT DUT_YAML DUT_TREE CLIENT_PORT CLIENT_YAML CLIENT_TREE [SETTLE_S]
#   DUT_KIND    esp | pico | bw15 | bk
#               esp: esphome upload; pico: upload then re-find the CDC port (VID 2e8a) because it
#               re-enumerates; bw15: flash_rtl87xx_kit.py; bk: flash_bk72xx.py
#   *_YAML      absolute path; the file name without .yaml must equal the config's `name`
#   *_TREE      checkout or worktree whose esphome code must build that yaml (cwd for python -m esphome)
#   SETTLE_S    seconds to keep capturing after the client reports the real save (default 40; BW15 wants 90)
# Env: PY (python with esphome; default DUT_TREE/venv/bin/python, else python3 on PATH), LTPY (LibreTiny penv python with
#      ltchiptool; default ~/.platformio/penv/.libretiny/bin/python)
# Output next to each yaml: NAME-compile.log, NAME-upload.log, NAME-serial.log, and DUT NAME-events.txt
# (addresses redacted so the extract can go on a PR; the serial logs themselves stay local).
set -u
SKILL=$(cd "$(dirname "$0")/.." && pwd)
KIND=$1; DPORT=$2; DYAML=$3; DTREE=$4; CPORT=$5; CYAML=$6; CTREE=$7; SETTLE=${8:-40}
PY=${PY:-$DTREE/venv/bin/python}; [ -x "$PY" ] || PY=$(command -v python3)
LTPY=${LTPY:-$HOME/.platformio/penv/.libretiny/bin/python}
DDIR=$(dirname "$DYAML"); DNAME=$(basename "$DYAML" .yaml)
CDIR=$(dirname "$CYAML"); CNAME=$(basename "$CYAML" .yaml)
DLOG=$DDIR/$DNAME-serial.log; CLOG=$CDIR/$CNAME-serial.log

bound() { local t=$1; shift; if command -v timeout >/dev/null; then timeout "$t" "$@"; else perl -e 'alarm shift; exec @ARGV' "$t" "$@"; fi; }
count() { LC_ALL=C grep -a -c "$1" "$2"; }
wait_for() { # wait_for FILE PATTERN SECONDS -> prints elapsed, status 1 on timeout
  local n=0; until LC_ALL=C grep -a -q "$2" "$1"; do sleep 2; n=$((n+2)); [ $n -ge "$3" ] && { echo "$n"; return 1; }; done; echo "$n"; }
stop_tails() { pkill -f "[s]erial_capture.py" 2>/dev/null; sleep 1; }
compile() { (cd "$2" && PYTHONUNBUFFERED=1 bound 1500 "$PY" -m esphome compile "$1" > "${1%.yaml}-compile.log" 2>&1); echo "[hammer] compile $(basename "$1"): rc=$?"; }
esp_upload() { (cd "$3" && bound 300 "$PY" -m esphome upload "$1" --device "$2" > "${1%.yaml}-upload.log" 2>&1); echo "[hammer] upload $(basename "$1"): ok=$(count 'Successfully uploaded' "${1%.yaml}-upload.log")"; }

stop_tails
compile "$DYAML" "$DTREE"
compile "$CYAML" "$CTREE"

TAILOPT=""
case $KIND in
  esp) esp_upload "$DYAML" "$DPORT" "$DTREE" ;;
  pico) esp_upload "$DYAML" "$DPORT" "$DTREE"
        DPORT=$("$PY" "$SKILL/scripts/ports.py" --wait 2e8a --timeout 60); echo "[hammer] pico port after upload: $DPORT"; sleep 2 ;;
  bw15|bk)
    UF2=$(ls "$DDIR/.esphome/build/$DNAME/.pioenvs/$DNAME/"*.uf2 2>/dev/null | head -1); echo "[hammer] uf2: ${UF2##*/}"
    W=$SKILL/scripts/flash_rtl87xx_kit.py; [ "$KIND" = bk ] && W=$SKILL/scripts/flash_bk72xx.py
    (cd "$DDIR" && bound 400 "$LTPY" "$W" flash write -d "$DPORT" "$UF2" > "$DDIR/$DNAME-upload.log" 2>&1)
    echo "[hammer] $KIND flash: rc=$? errors=$(LC_ALL=C grep -a -i -c 'error\|timeout' "$DDIR/$DNAME-upload.log")"
    TAILOPT="--released" ;;
  *) echo "unknown DUT_KIND $KIND"; exit 2 ;;
esac

: > "$DLOG"; ("$PY" "$SKILL/scripts/serial_capture.py" "$DPORT" 420 "$DLOG" $TAILOPT &)
n=$(wait_for "$DLOG" "Captive portal\|Starting fallback AP" 120) || echo "[hammer] WARNING: DUT never raised its AP"
echo "[hammer] DUT fallback AP after ${n}s"; sleep 2

esp_upload "$CYAML" "$CPORT" "$CTREE"
: > "$CLOG"; ("$PY" "$SKILL/scripts/serial_capture.py" "$CPORT" 300 "$CLOG" &)
n=$(wait_for "$CLOG" "real save status\|posting the real" 240) || echo "[hammer] WARNING: client never reached the real save"
echo "[hammer] client posted the real credentials after ${n}s; settling ${SETTLE}s"; sleep "$SETTLE"

echo "[hammer] $DNAME: saves requested $(count 'Requested WiFi Settings Change' "$DLOG"), cooldown exits $(count 'cooldown early' "$DLOG"), crash markers $(count 'Panic\|HardFault\|Fault\|abort\|Backtrace\|Exception\|Guru' "$DLOG"), boots $(count 'Log initialized' "$DLOG"), STA addresses $(LC_ALL=C grep -a 'IP Address:' "$DLOG" | grep -v -c '192\.168\.4\.')"
echo "[hammer] $CNAME: bogus saves answered $(count 'bogus save status 200' "$CLOG"), failed $(count 'status -1' "$CLOG"), real save $(LC_ALL=C grep -a -o 'real save status [-0-9]*' "$CLOG" | tail -1)"
LC_ALL=C grep -a "cooldown early\|Panic\|Fault\|Exception\|Retry phase\|Requested WiFi\|Connected\|IP Address\|Captive portal\|fallback AP" "$DLOG" | LC_ALL=C tr -cd '\11\12\15\40-\176' | sed -E 's/(IP Address: )[0-9.]+/\1<redacted>/' | cut -c1-110 | head -80 > "$DDIR/$DNAME-events.txt"
echo "[hammer] events: $DDIR/$DNAME-events.txt"
stop_tails
