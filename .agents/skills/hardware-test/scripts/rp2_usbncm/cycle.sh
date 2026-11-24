#!/bin/bash
# cycle.sh NAME YAML N TAG ota|restart [fast|spaced]: N OTA uploads (or Restart button presses) of a USB NCM build,
# then per cycle: seconds until the API answers again, whether only the OTA port was open meanwhile (safe mode), 10
# pings, and the previous boot lines of the next boot's log (panic_capture or boot_recorder verdict, alt 1 time, count
# of alt 0 requests in the event ring). `spaced` waits until the boot is 65 s old before the next OTA: OTAs less than
# 60 s apart count as failed boots and the tenth puts the device in safe mode for 300 s. Writes TAG-*.log next to the
# script and TAG-times.txt (wall clock per OTA, to line up with hostlog.sh).
# env: TREE, PY, IP (the device's link-local address), API_KEY (the config's api_key, for press.py)
# macOS: `ping -b IFACE` sends one ping out the USB interface first so the ARP entry lands there (see the recipe).
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; Y=${2:?yaml}; C=${3:?count}; G=${4:?tag}; M=${5:?ota|restart}
P=${6:-fast}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; IP=${IP:?set IP}
iface() {
  local f
  for f in $(ifconfig -l); do
    ifconfig "$f" 2>/dev/null | grep -q 'inet 169.254' && ping -b "$f" -c1 -t1 "$IP" >/dev/null 2>&1 && { echo "$f"; return 0; }
  done
  return 1
}
port() { $PY -c 'import socket, sys; socket.create_connection((sys.argv[1], int(sys.argv[2])), 1).close()' "$IP" "$1" 2>/dev/null; }
for i in $(seq 1 "$C"); do
  echo "$(date +%H:%M:%S) start $M $i" >> "$D/$G-times.txt"
  if [ "$M" = ota ]; then
    r=$("$D/../pio_build.sh" upload "$N" "$Y" "$IP" 2>&1 | grep -ac "OTA successful")
  else
    PYTHONPATH=$D/.. $PY "$D/press.py" "$IP" Restart && r=pressed || r=failed
  fi
  t0=$(date +%s); sleep 4; t=0; ota_only=0
  until { f=$(iface) && port 6053; } || [ $t -ge 420 ]; do
    if [ $t = 60 ] || [ $t = 200 ]; then f=$(iface) && port 2040 && ota_only=$((ota_only + 1)); fi
    sleep 1; t=$((t + 1))
  done
  tup=$(date +%s); back=$((tup - t0))
  f=$(iface); p=$(ping -b "$f" -c 10 -i 0.2 "$IP" 2>/dev/null | grep -o '[0-9]* packets received' | cut -d' ' -f1)
  (cd "$T" && PYTHONPATH=$T timeout 24 $PY -m esphome -s name "$N" logs "$Y" --device "$IP" > "$D/$G-$i-logs.txt" 2>&1)
  L=$(sed 's/\x1b\[[0-9;]*m//g' "$D/$G-$i-logs.txt")
  pv=$(echo "$L" | grep -a -m1 -o "previous boot[: ].*")
  usb=$(echo "$L" | grep -a -m1 -o "alt1 at.*"); a0=$(echo "$L" | grep -a -c "code=48 a=0B b=0000")
  echo "$(date +%H:%M:%S) $M $i: ok=$r back=${back}s ota_port_open_while_api_closed=$ota_only ping=${p:-0}/10 :: $pv :: $usb :: prev_alt0_lines=$a0"
  if [ "$P" = spaced ]; then w=$((tup + 65 - $(date +%s))); [ $w -gt 0 ] && sleep $w; fi
done
