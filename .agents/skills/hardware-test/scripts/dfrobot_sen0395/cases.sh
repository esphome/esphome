#!/bin/sh
# cases.sh DEV_IP KEY CAPTURE LABEL : the SEN0395 flow (settings, switches, junk lines, factory reset, reset) against
# templates/dfrobot-sen0395-dut.yaml, with a serial capture of the device running into CAPTURE
D=$1; K=$2; LOG=$3; LABEL=$4; PY=${PYTHON:-python3}; CTL="$(dirname "$0")/ctl.py"
show() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a 'dfrobot\|Detected\|\[W\]\|\[E\]\|sen\]' | grep -a -v 'JYBSS\|took a long time' | cut -c1-200; }
step() { n0=$(wc -l < "$LOG"); echo "=== $LABEL: $1 ($(date +%T))"; shift; $PY "$CTL" "$D" "$K" "$@" || { rc=$?; echo "driver failed ($rc), aborting"; exit $rc; }; echo "--- device log:"; show "$n0"; }
frames() { echo "frames: $(tail -n +$(($1+1)) "$LOG" | grep -a -c 'JYBSS,1') present, $(tail -n +$(($1+1)) "$LOG" | grep -a -c 'JYBSS,0') absent"; }
n0=$(wc -l < "$LOG"); echo "=== $LABEL: 20 s of frames ($(date +%T))"; sleep 20; frames "$n0"
step "range 0 to 3 m" "press:Range 0 to 3m" wait:12
step "range 0 to 9 m" "press:Range 0 to 9m" wait:12
step "latency 1 s / 2 s" "press:Latency 1s 2s" wait:12
step "latency 0 / 0" "press:Latency 0 0" wait:12
step "sensitivity 7" "press:Sensitivity 7" wait:12
step "LED off then on" "sw:LED=off" wait:10 "sw:LED=on" wait:10
step "sensor active off then on" "sw:Sensor active=off" wait:10 "sw:Sensor active=on" wait:10
step "presence via UART off then on" "sw:Presence via UART=off" wait:10 "sw:Presence via UART=on" wait:10
step "start after boot off then on" "sw:Start after boot=off" wait:10 "sw:Start after boot=on" wait:10
for n in 100 160 200 220 240 256; do step "junk line of $n bytes" "press:Junk $n" wait:7; done
step "junk 220 then a command" "press:Junk 220" wait:6 "press:Range 0 to 9m" wait:12
step "factory reset" "press:Factory reset" wait:15
step "reset" "press:Reset" wait:15
n0=$(wc -l < "$LOG"); echo "=== $LABEL: 20 s of frames after ($(date +%T))"; sleep 20; frames "$n0"
