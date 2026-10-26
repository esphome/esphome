#!/bin/bash
# usage: run.sh BUILD   OTA the CB3S, serial flash the BK7238, then switch the upstream AP off and on three times
# while both DUTs send alive lines over syslog. BUILD is a label (pr, base) for the output files.
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password, wisp_up_ssid,
# wisp_up_password, fallback_ap_password, syslog_ip, api_key) and run it there. wisp-upstream-wpa3.yaml runs on the
# ethernet ESP32-S3 upstream; syslog_ip is this host's address as the DUTs reach it through the bridge's NAT.
# env: TREE (esphome checkout for the DUTs), BK_PORT (BK7238 serial), CB_PORT (CB3S log only serial, optional),
# CB_HOST (CB3S address to OTA), UPSTREAM (bridge address), SC (path to serial_capture.py), FLASH_BK (path to
# flash_bk72xx.py), LT (LibreTiny penv python), PY (checkout python), UP_TREE (checkout for the bridge's API logs)
D=$(cd "$(dirname "$0")" && pwd); b=${1:?build label}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
BK=${BK_PORT:?set BK_PORT}; CBH=${CB_HOST:?set CB_HOST}; UP=${UPSTREAM:?set UPSTREAM}; SC=${SC:?set SC}
FB=${FLASH_BK:?set FLASH_BK}; LT=${LT:-$HOME/.platformio/penv/.libretiny/bin/python}; CB=${CB_PORT:-}
ap() { curl -s -m 5 -X POST -d "" -o /dev/null -w "%{http_code}" "http://$UP/button/ap%20$1/press"; }
# retry N PAUSE CMD...: run CMD up to N times, PAUSE seconds apart; fails when every attempt failed
retry() { local n=$1 pause=$2 i; shift 2; for i in $(seq "$n"); do "$@" && return 0; [ "$i" -lt "$n" ] && sleep "$pause"; done; return 1; }
build() { (cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome "$@"); }
# only this run's ports: a global pkill stops other runs' captures
pkill -f "[s]erial_capture.py $BK"; [ -n "$CB" ] && pkill -f "[s]erial_capture.py $CB"
[ "$(ap on)" = 200 ] || { echo "FAIL: upstream bridge did not take ap on"; exit 1; }
cb_upload() { build run --no-logs "$D/recb.yaml" --device "$CBH" > "$D/cb-$b-upload.log" 2>&1; }
retry 3 15 cb_upload || { echo "FAIL: CB3S build or OTA after 3 tries"; exit 1; }
build compile "$D/rebk.yaml" > "$D/bk-$b-build.log" 2>&1 || { echo "FAIL: BK7238 build"; exit 1; }
UF2=$D/.esphome/build/rebk/.pioenvs/rebk/firmware.uf2
bk_flash() { $LT "$FB" flash write -d "$BK" "$UF2" > "$D/bk-$b-flash.log" 2>&1; }
retry 4 5 bk_flash || { echo "FAIL: BK7238 flash after 4 tries"; exit 1; }
PIDS=()
$PY - > "$D/$b-syslog.txt" 2>&1 << 'PYEOF' &
import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("0.0.0.0", 5514))
s.settimeout(1)
end = time.time() + 1250
while time.time() < end:
    try:
        d, a = s.recvfrom(2048)
        print(time.strftime("%H:%M:%S"), a[0], d.decode(errors="replace").strip(), flush=True)
    except TimeoutError:
        pass
PYEOF
PIDS+=($!)
$PY "$SC" "$BK" 1240 "$D/$b-bk.txt" --released > /dev/null 2>&1 & PIDS+=($!)
[ -n "$CB" ] && { $PY "$SC" "$CB" 1240 "$D/$b-cb.txt" --released > /dev/null 2>&1 & PIDS+=($!); }
(cd "${UP_TREE:-$T}" && PYTHONUNBUFFERED=1 exec timeout 1240 $PY -m esphome logs "$D/wisp-upstream-wpa3.yaml" --device "$UP" > "$D/$b-up.txt" 2>&1) & PIDS+=($!)
trap 'kill "${PIDS[@]}" 2>/dev/null' EXIT
t0=$(date +%s); : > "$D/$b-marks.txt"
for step in 180:off 330:on 480:off 630:on 780:off 930:on; do
  t=${step%%:*}; st=${step#*:}
  while [ $(($(date +%s) - t0)) -lt "$t" ]; do sleep 1; done
  code=$(ap "$st")
  echo "$(date +%H:%M:%S) t=$t ap $st $code" | tee -a "$D/$b-marks.txt"
  [ "$code" = 200 ] || { echo "FAIL: upstream bridge returned $code for ap $st"; exit 1; }
done
wait "${PIDS[@]}"; trap - EXIT
$PY "$D/an.py" "$D" "$b"
