#!/bin/sh
# cli_tls_check.sh TREE [neg] YAML... : run `esphome clean-mqtt` from the checkout TREE for each config, which goes
# through the same TLS setup in esphome/mqtt.py as `esphome logs` over MQTT, without needing a board. clean-mqtt never
# exits on its own, so it runs under a 25 s timeout: status 124 after "Connected to MQTT broker!" means the CLI
# connected. With neg every config must instead fail with CERTIFICATE_VERIFY_FAILED and never connect, so a negative
# that fails for another reason (bad YAML, broker down) is reported as a failure too. A relative YAML path is taken
# from the working directory, where the cleanmqtt-NAME.log outputs also go. Exit 1 when any config missed its result.
TREE=$1; shift; PY=${PYTHON:-python3}; NEG=0
[ "$1" = neg ] && { NEG=1; shift; }
[ -d "$TREE/esphome" ] && [ -n "$1" ] || { echo "usage: cli_tls_check.sh TREE [neg] YAML..."; exit 2; }
bad=0
for y in "$@"; do
  case $y in /*) ;; *) y=$PWD/$y;; esac
  n=$(basename "$y" .yaml); log=$PWD/cleanmqtt-$n.log
  (cd "$TREE" && PYTHONUNBUFFERED=1 timeout 25 $PY -m esphome clean-mqtt "$y" > "$log" 2>&1); rc=$?
  if [ "$NEG" = 0 ] && [ "$rc" = 124 ] && grep -q "Connected to MQTT broker" "$log"; then
    echo "$n: connected"
  elif [ "$NEG" = 1 ] && grep -q "CERTIFICATE_VERIFY_FAILED" "$log" && ! grep -q "Connected to MQTT broker" "$log"; then
    echo "$n: rejected (CERTIFICATE_VERIFY_FAILED)"
  else
    echo "$n: FAILED (status $rc): $(grep -a -m1 -i 'cannot connect\|error' "$log" | cut -c1-160)"; bad=$((bad+1))
  fi
done
[ "$bad" = 0 ]
