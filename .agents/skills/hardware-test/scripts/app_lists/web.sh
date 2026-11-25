#!/bin/bash
# web.sh HOST OUT: GET / (status and size) into OUT-root.html and 8 s of /events into OUT-events.txt
curl -s -o "$2-root.html" -w "root %{http_code} %{size_download}\n" --max-time 10 "http://$1/"
curl -s -N --max-time 8 "http://$1/events" >"$2-events.txt"
echo "events bytes $(wc -c <"$2-events.txt")"
