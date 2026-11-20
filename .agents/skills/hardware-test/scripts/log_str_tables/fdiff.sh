#!/bin/bash
# fdiff.sh A.norm B.norm [--sorted]: diff after dropping radio, timing and hash lines that differ run to run;
# --sorted compares the sorted line sets instead, for independent timers that interleave.
F='\[(wifi|wifi_esp32|wifi_esp8266|mdns|preferences|api|api\.[a-z_]+|esp8266\.preferences|dhcp|ota)\]|took [0-9]+ms|took a long time|Speaker buffer full|Received audio|Free Heap|Heap|config_hash|IP Address|uptime|[Ss]ignal'
keep() { grep -a -v -E "$F" "$1" | if [ "$2" = --sorted ]; then sort; else cat; fi; }
for f in "$1" "$2"; do [ -r "$f" ] || { echo "cannot read $f" >&2; exit 2; }; done
diff -a <(keep "$1" "$3") <(keep "$2" "$3")
