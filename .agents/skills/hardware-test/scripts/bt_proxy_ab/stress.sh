#!/bin/bash
# stress.sh HOST SECONDS: 8 parallel pings with 1400 byte payloads every 0.1 s (about 110 KB/s each way)
for _ in 1 2 3 4 5 6 7 8; do ping -q -s 1400 -i 0.1 -t "$2" "$1" > /dev/null 2>&1 & done
wait
