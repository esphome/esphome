#!/bin/zsh
# fdiff.sh A.norm B.norm : diff after dropping radio, timing and hash lines that differ run to run
F='\[(wifi|wifi_esp32|wifi_esp8266|mdns|preferences|api|api\.[a-z_]+|esp8266\.preferences|dhcp|ota)\]|took [0-9]+ms|took a long time|Speaker buffer full|Received audio|Free Heap|Heap|config_hash|IP Address|uptime|signal'
diff -a <(grep -a -v -E "$F" $1) <(grep -a -v -E "$F" $2)
