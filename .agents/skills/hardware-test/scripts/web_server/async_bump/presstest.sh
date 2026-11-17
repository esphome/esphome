#!/bin/bash
# presstest.sh IP : restart presses at uptime ~20 s and ~60 s; prints the HTTP code and whether the board restarted
IP=$1
up() { curl -s -m 5 http://$IP/sensor/Uptime | python3 -c 'import json,sys; print(int(json.load(sys.stdin)["value"]))' 2>/dev/null || echo -1; }
waitfor() { for i in $(seq 1 60); do u=$(up); [ $u -ge $1 ] && return; sleep 1; done; }
for target in 20 20 20 60 60 60; do
  waitfor $target; b=$(up)
  r=$(curl -s -m 10 -X POST -o /dev/null -w "%{http_code} %{time_total}" http://$IP/button/Restart/press)
  sleep 4; for i in $(seq 1 30); do a=$(up); [ $a -ge 0 ] && break; sleep 1; done
  echo "uptime $b press $r -> uptime $a $([ $a -lt $b ] && echo restarted || echo NOT-restarted)"
done
