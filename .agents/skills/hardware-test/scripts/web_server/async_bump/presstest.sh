#!/bin/bash
# presstest.sh IP : restart presses at uptime ~20 s and ~60 s; prints the HTTP code and whether the board restarted
IP=$1; D=$(cd "$(dirname "$0")" && pwd); PY=${PY:-python3}
up() { $PY -I "$D/rest.py" uptime "$IP" || echo -1; }
waitfor() { for i in $(seq 1 60); do u=$(up); [ $u -ge $1 ] && return; sleep 1; done; }
RC=0
for target in 20 20 20 60 60 60; do
  waitfor $target; b=$(up)
  r=$(curl -s -m 10 -X POST -o /dev/null -w "%{http_code} %{time_total}" http://$IP/button/Restart/press)
  sleep 4; for i in $(seq 1 30); do a=$(up); [ $a -ge 0 ] && break; sleep 1; done
  if [ $a -lt 0 ]; then v=NOT-back; elif [ $b -ge 0 ] && [ $a -lt $b ]; then v=restarted; else v=NOT-restarted; fi
  echo "uptime $b press $r -> uptime $a $v"
  [ $v = restarted ] || RC=1
done
exit $RC
