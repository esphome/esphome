#!/bin/zsh
# sdiff.sh A.norm B.norm : fdiff.sh filter, then compare the sorted line sets (order free)
F='\[(wifi|wifi_esp32|wifi_esp8266|mdns|preferences|api|api\.[a-z_]+|esp8266\.preferences|dhcp|ota)\]|took [0-9]+ms|took a long time|Speaker buffer full|Received audio|Free Heap|Heap|config_hash|IP Address|uptime|ignal strength'
diff -a <(grep -a -v -E "$F" $1 | sort) <(grep -a -v -E "$F" $2 | sort)
