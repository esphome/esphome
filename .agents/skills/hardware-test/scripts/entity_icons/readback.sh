#!/bin/sh
# readback.sh DEVICE_IP KEY NAME BROKER_HOST [CAPTURE] : record how a device built from templates/entity-icon-dut.yaml
# reports its entity icons and device classes: the native API listing (api-NAME.jsonl), the retained MQTT discovery
# payloads and states (mqtt-NAME.txt, normalised to mqttn-NAME.jsonl with the device name replaced and the debug topic
# dropped), the web_server /events listing plus every REST endpoint with ?detail=all (web-NAME.jsonl) and, when a
# serial CAPTURE is given, the dump_config Icon and Device Class lines (dump-NAME.txt, timestamps and line numbers
# removed). KEY as for api_button.py. Outputs go to the working directory; compare two names with compare.sh.
HERE=$(cd "$(dirname "$0")" && pwd); PY=${PYTHON:-python3}; DEV=$1; KEY=$2; N=$3; B=$4; CAP=$5; rc=0; TAB=$(printf '\t')
[ -n "$DEV" ] && [ -n "$KEY" ] && [ -n "$N" ] && [ -n "$B" ] || { echo "usage: readback.sh DEVICE_IP KEY NAME BROKER_HOST [CAPTURE]"; exit 2; }
timeout 12 mosquitto_sub -h "$B" -v -t 'homeassistant/#' -t "$N/#" -F "%t$TAB%p" > "$PWD/mqtt-$N.txt" 2> "$PWD/mqtt-$N.err"
$PY "$HERE/mqtt_norm.py" "$PWD/mqtt-$N.txt" "$N" "$PWD/mqttn-$N.jsonl" || rc=1
$PY "$HERE/api_list.py" "$DEV" "$KEY" "$PWD/api-$N.jsonl" || { echo "API listing failed"; rc=1; }
$PY "$HERE/web_dump.py" "$DEV" "$PWD/web-$N.jsonl" || { echo "web_server dump failed"; rc=1; }
if [ -n "$CAP" ]; then
  LC_ALL=C grep -a '\]:   Icon: \|\]:   Device Class: ' "$CAP" | LC_ALL=C sed 's/\x1b\[[0-9;]*m//g; s/^\[[^]]*\]//; s/:[0-9]*\]:/]:/' | sort > "$PWD/dump-$N.txt"
  echo "dump_config icon lines $(grep -c 'Icon:' "$PWD/dump-$N.txt"), device class lines $(grep -c 'Device Class:' "$PWD/dump-$N.txt")"
fi
exit $rc
