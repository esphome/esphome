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
# Env: PY (python with esphome; default DUT_TREE/venv/bin/python, else python3 on PATH; an explicit PY must exist), LTPY
#      (LibreTiny penv python with ltchiptool; default ~/.platformio/penv/.libretiny/bin/python)
# Output next to each yaml: NAME-compile.log, NAME-upload.log, NAME-serial.log, NAME-capture.err, and DUT NAME-events.txt
# (addresses redacted so the extract can go on a PR; the serial logs themselves stay local).
# Exit status: 0 RESULT PASS (no crash marker or reboot after the AP came up, the device joined the network,
# and real stimulus: saves answered and a cooldown exit); 3 RESULT FAIL (the counts show
# a failure); 2 RESULT INVALID (a precondition or the stimulus was missed); 1 when a build, flash or port
# step fails (nothing is reported).
set -u
SKILL=$(cd "$(dirname "$0")/.." && pwd)
KIND=$1; DPORT=$2; DYAML=$3; DTREE=$4; CPORT=$5; CYAML=$6; CTREE=$7; SETTLE=${8:-40}
if [ -n "${PY:-}" ]; then [ -x "$PY" ] || { echo "[hammer] FAILED: PY=$PY is not executable"; exit 1; }
else PY=$DTREE/venv/bin/python; [ -x "$PY" ] || PY=$(command -v python3) || { echo "[hammer] FAILED: no python found, set PY"; exit 1; }; fi
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
port_never_opened() { # an open failure and no line that is not a capture note: the port, not the firmware
  LC_ALL=C grep -a -q '\[capture\] open failed' "$1" && ! LC_ALL=C grep -a -q -v '^\[[0-9:]*\]\[capture\]' "$1"; }
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

: > "$DLOG"; ("$PY" "$SKILL/scripts/serial_capture.py" "$DPORT" $DUT_CAPTURE "$DLOG" $TAILOPT 2> "${DLOG%-serial.log}-capture.err" &)
if ! n=$(wait_for "$DLOG" "Captive portal\|Starting fallback AP" $AP_WAIT); then
  [ -s "$DLOG" ] || fail "the DUT capture produced nothing (the capture itself failed), see ${DLOG%-serial.log}-capture.err"
  port_never_opened "$DLOG" && fail "could not open $DPORT (held by another process, or the wrong port), see $DLOG"
  fail "DUT never raised its AP within ${n}s, see $DLOG"
fi
echo "[hammer] DUT fallback AP after ${n}s"; sleep 2
BOOT_BASE=$(count 'Log initialized' "$DLOG")  # reboots are counted from here; the first banner may predate the capture

esp_upload "$CYAML" "$CPORT" "$CTREE" || fail "client upload failed"
: > "$CLOG"; ("$PY" "$SKILL/scripts/serial_capture.py" "$CPORT" $CLIENT_CAPTURE "$CLOG" 2> "${CLOG%-serial.log}-capture.err" &)
if n=$(wait_for "$CLOG" "real save status\|posting the real" $SAVE_WAIT); then
  echo "[hammer] client posted the real credentials after ${n}s; settling ${SETTLE}s"; sleep "$SETTLE"
else
  [ -s "$CLOG" ] || fail "the client capture produced nothing (the capture itself failed), see ${CLOG%-serial.log}-capture.err"
  port_never_opened "$CLOG" && fail "could not open $CPORT (held by another process, or the wrong port), see $CLOG"
  INVALID="client never reached the real save within ${n}s"; echo "[hammer] WARNING: $INVALID"
fi

CRASHES=$(count 'Panic\|HardFault\|Fault\|abort\|Backtrace\|Exception\|Guru' "$DLOG"); BOOTS=$(count 'Log initialized' "$DLOG")
STA=$(LC_ALL=C grep -a 'IP Address:' "$DLOG" | grep -v -c '192\.168\.4\.'); REAL=$(LC_ALL=C grep -a -o 'real save status [-0-9]*' "$CLOG" | tail -1 | awk '{print $4}')
SAVES=$(count 'Requested WiFi Settings Change' "$DLOG"); EXITS=$(count 'cooldown early' "$DLOG"); ANSWERED=$(count 'bogus save status 200' "$CLOG")
REBOOTS=$((BOOTS - BOOT_BASE))
echo "[hammer] $DNAME: saves requested $SAVES, cooldown exits $EXITS, crash markers $CRASHES, reboots after the AP came up $REBOOTS, STA addresses $STA"
FAILED=$(count 'bogus save status -1' "$CLOG")
echo "[hammer] $CNAME: bogus saves answered $ANSWERED, failed $FAILED, real save status ${REAL:-none}"
# Only whole messages, matched where the message starts, so a credential line or an odd SSID never gets in
LC_ALL=C grep -a -E '^\[[0-9:]+\](\[[A-Z]\]\[[a-z_]+:[0-9]+\]: *(Exiting cooldown early|Starting fallback AP|Captive portal|Requested WiFi Settings Change|Retry phase|Connected$|IP Address: )|(Panic|Fatal exception|Exception|HardFault|Bus Fault|Guru Meditation))' "$DLOG" | LC_ALL=C tr -cd '\11\12\15\40-\176' | sed -E 's/(IP Address: )[^ ]+/\1<redacted>/' | cut -c1-110 | head -80 > "$DDIR/$DNAME-events.txt"
echo "[hammer] events: $DDIR/$DNAME-events.txt"
stop_tails
[ -z "$INVALID" ] || { echo "[hammer] RESULT INVALID: $INVALID"; exit 2; }
# Provisioning is proven by the device joining the network; the client's answer to the real save can time
# out because the device leaves the AP before replying, so its status is information only
if [ "$CRASHES" -gt 0 ] || [ "$REBOOTS" -gt 0 ] || [ "$STA" -lt 1 ]; then
  echo "[hammer] RESULT FAIL: crash markers $CRASHES, reboots $REBOOTS, STA addresses $STA, real save status ${REAL:-none}"; exit 3
fi
# A pass needs the stimulus the flow is about: at least half the configured bogus saves answered and one cooldown exit
CONFIGURED=$(LC_ALL=C grep -a -E '^[[:space:]]*bogus_saves:' "$CYAML" | head -1 | tr -dc '0-9'); CONFIGURED=${CONFIGURED:-15}
if [ "$EXITS" -lt 1 ] || [ $((ANSWERED * 2)) -lt "$CONFIGURED" ]; then
  echo "[hammer] RESULT INVALID: insufficient stimulus (bogus saves answered $ANSWERED of $CONFIGURED, cooldown exits $EXITS)"; exit 2
fi
[ "$FAILED" -eq 0 ] || echo "[hammer] WARNING: $FAILED of $CONFIGURED bogus saves got no answer"
echo "[hammer] RESULT PASS"; exit 0
