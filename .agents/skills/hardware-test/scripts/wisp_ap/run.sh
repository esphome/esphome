#!/bin/bash
# usage: run.sh DUT START SECONDS T:STATE...   e.g. run.sh dut-wisp on 600 120:off 300:on
# Builds and flashes DUT.yaml (dut-wisp or dut-always-on) over serial, sets the upstream AP to START, captures the
# DUT and the client, and switches the upstream AP at each T seconds; sum.py summarises the client per phase.
# Copy this directory into a scratch directory with a secrets.yaml (wisp_up_ssid, wisp_up_password, dut_ap_ssid,
# dut_ap_password, lan_probe_url, api_key) and run it there. wisp-upstream.yaml runs on an ethernet ESP32-S3 and
# wispcli.yaml on a D1 mini; flash both once (CLI_FLASH=1 builds and flashes the client too).
# env: TREE (esphome checkout for the DUT), PORT (DUT serial), CLI_PORT (client serial), UPSTREAM (upstream bridge
# address), SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); n=${1:?dut yaml name}; start=${2:?on or off}; dur=${3:?seconds}; shift 3
T=${TREE:?set TREE}; P=${PORT:?set PORT}; CP=${CLI_PORT:?set CLI_PORT}; UP=${UPSTREAM:?set UPSTREAM}
SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}
ap() { curl -s -m 5 -X POST -d "" -o /dev/null -w "%{http_code}" "http://$UP/button/ap%20$1/press"; }
pkill -f "[s]erial_capture.py ($P|$CP)"
[ "$(ap "$start")" = 200 ] || { echo "FAIL: upstream bridge did not take ap $start"; exit 1; }
if [ "${CLI_FLASH:-0}" = 1 ]; then
  (cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$D/wispcli.yaml" --device "$CP" > "$D/wispcli-upload.log" 2>&1) \
    || { echo "FAIL: client build or upload"; exit 1; }
fi
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$D/$n.yaml" --device "$P" > "$D/$n-upload.log" 2>&1) \
  || { echo "FAIL: DUT build or upload"; exit 1; }
: > "$D/$n-marks.txt"
$PY "$SC" "$P" "$dur" "$D/$n-dut.txt" --reset > /dev/null 2>&1 & C1=$!
# no --reset on the client: a CH340 D1 mini floods garbage after an RTS reset
$PY "$SC" "$CP" "$dur" "$D/$n-cli.txt" > /dev/null 2>&1 & C2=$!
trap 'kill $C1 $C2 2>/dev/null' EXIT
t0=$(date +%s)
for step in "$@"; do
  t=${step%%:*}; st=${step#*:}
  while [ $(($(date +%s) - t0)) -lt "$t" ]; do sleep 1; done
  code=$(ap "$st")
  echo "$(date +%H:%M:%S) t=$t upstream $st $code" | tee -a "$D/$n-marks.txt"
  [ "$code" = 200 ] || { echo "FAIL: upstream bridge returned $code for ap $st"; exit 1; }
done
wait $C1 $C2; trap - EXIT
$PY "$D/sum.py" "$D" "$n"
