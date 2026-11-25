#!/bin/bash
# cycle.sh NAME YAML N TAG ota|restart [fast|spaced]: N OTA uploads (or Restart button presses) of a USB NCM build,
# then per cycle: seconds until the API answers again, whether the OTA port was open while the API was still closed
# 60 s or more after the reboot (safe mode), 10 pings, and the previous boot lines of the next boot's log (boot_recorder
# verdict, alt 1 time, count of alt 0 requests in the event ring from `evring.py count`). `spaced` waits until the boot
# is 65 s old before the next OTA: OTAs less than 60 s apart count as failed boots and the tenth puts the device in safe
# mode for 300 s. Writes TAG-*.log next to the script and TAG-times.txt (wall clock per OTA, for hostlog.sh).
# env: TREE, PY, IP (the device's link-local address), API_KEY (the config's api_key, for api_button.py),
#      EXTRA (more esphome options, e.g. `-s ncm_libs DIR/libraries` for the patched and hammer configs)
# macOS: `ping -b IFACE` sends one ping out the USB interface first so the ARP entry lands there (see the recipe).
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; C=${3:?count}; G=${4:?tag}; M=${5:?ota|restart}
P=${6:-fast}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; IP=${IP:?set IP}
Y=$(cd "$(dirname "${2:?yaml}")" && pwd)/$(basename "$2")
iface() {
  local f
  for f in $(ifconfig -l); do
    ifconfig "$f" 2>/dev/null | grep -q 'inet 169.254' && ping -b "$f" -c1 -t1 "$IP" >/dev/null 2>&1 && { echo "$f"; return 0; }
  done
  return 1
}
port() { nc -z -G 1 "$IP" "$1" >/dev/null 2>&1; }
for i in $(seq 1 "$C"); do
  echo "$(date +%H:%M:%S) start $M $i" >> "$D/$G-times.txt"
  if [ "$M" = ota ]; then
    r=$("$D/../pio_build.sh" upload "$N" "$Y" "$IP" -s ip "$IP" $EXTRA 2>&1 | grep -ac "OTA successful")
  else
    $PY "$D/../api_button.py" "$IP" "${API_KEY:--}" press Restart >/dev/null && r=pressed || r=failed
  fi
  # the USB interface is looked up again only after a ping on the last one fails
  t0=$(date +%s); sleep 4; ota_only=0; f=
  while e=$(($(date +%s) - t0)); [ $e -lt 420 ]; do
    { [ -n "$f" ] && ping -b "$f" -c1 -t1 "$IP" >/dev/null 2>&1; } || f=$(iface)
    if [ -n "$f" ]; then port 6053 && break; [ $e -ge 60 ] && port 2040 && ota_only=1; fi
    sleep 1
  done
  tup=$(date +%s); back=$((tup - t0))
  [ -n "$f" ] || f=$(iface); p=$(ping -b "$f" -c 10 -i 0.2 "$IP" 2>/dev/null | grep -o '[0-9]* packets received' | cut -d' ' -f1)
  (cd "$T" && PYTHONPATH=$T timeout 24 $PY -m esphome -s name "$N" -s ip "$IP" $EXTRA logs "$Y" --device "$IP" > "$D/$G-$i-logs.txt" 2>&1)
  L=$(sed 's/\x1b\[[0-9;]*m//g' "$D/$G-$i-logs.txt")
  pv=$(echo "$L" | grep -a -m1 -o "previous boot[: ].*")
  usb=$(echo "$L" | grep -a -m1 -o "alt1 at.*")
  a0=$($PY "$D/evring.py" "$D/$G-$i-logs.txt" count) || a0=unavailable
  echo "$(date +%H:%M:%S) $M $i: ok=$r back=${back}s ota_port_open_while_api_closed=$ota_only ping=${p:-0}/10 :: $pv :: $usb :: prev_alt0_lines=$a0"
  if [ "$P" = spaced ]; then w=$((tup + 65 - $(date +%s))); [ $w -gt 0 ] && sleep $w; fi
done
