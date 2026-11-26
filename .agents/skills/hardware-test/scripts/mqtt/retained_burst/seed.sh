#!/bin/bash
# seed.sh BROKER_PORT: leave the 25 hwburst/in/N messages and the 15 switch ON commands retained on the local broker.
# seed.sh BROKER_PORT clear: remove them again.
B=${1:?broker port}; M=1; [ "$2" = clear ] && M=
pub() { if [ -n "$M" ]; then mosquitto_pub -h 127.0.0.1 -p "$B" -r -t "$1" -m "$2"; else mosquitto_pub -h 127.0.0.1 -p "$B" -r -n -t "$1"; fi; }
for i in $(seq 0 24); do pub "hwburst/in/$i" "m$i" || { echo "FAIL: mosquitto_pub on port $B"; exit 1; }; done
for i in $(seq 0 14); do pub "hwburst/switch/sw$i/command" ON || exit 1; done
