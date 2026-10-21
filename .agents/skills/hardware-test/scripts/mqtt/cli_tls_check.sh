#!/bin/sh
# cli_tls_check.sh TREE YAML... : run `esphome clean-mqtt` from the checkout TREE for each config, which goes through
# the same TLS setup in esphome/mqtt.py as `esphome logs` over MQTT, without needing a board. clean-mqtt never exits on
# its own, so it runs under a 25 s timeout: status 124 after "Connected to MQTT broker!" means the CLI connected; any
# other status with a "Cannot connect" line is a failure (expected for a negative case). Outputs cleanmqtt-NAME.log go
# to the working directory. Exit 1 when a config failed to connect, so negatives are run separately.
TREE=$1; shift; PY=${PYTHON:-python3}
[ -d "$TREE/esphome" ] && [ -n "$1" ] || { echo "usage: cli_tls_check.sh TREE YAML..."; exit 2; }
bad=0
for y in "$@"; do
  n=$(basename "$y" .yaml); log=$PWD/cleanmqtt-$n.log
  (cd "$TREE" && PYTHONUNBUFFERED=1 timeout 25 $PY -m esphome clean-mqtt "$y" > "$log" 2>&1); rc=$?
  if [ "$rc" = 124 ] && grep -q "Connected to MQTT broker" "$log"; then
    echo "$n: connected"
  else
    echo "$n: FAILED (status $rc): $(grep -a -m1 -i 'cannot connect\|error' "$log" | cut -c1-160)"; bad=$((bad+1))
  fi
done
[ "$bad" = 0 ]
