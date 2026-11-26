#!/bin/bash
# webrep.sh IP IMAGE COUNT: COUNT web_server OTAs of IMAGE, 40 s apart, with the HTTP code and time of each. Run it
# on both builds under a serial capture and count `AUTO web end`/`abort` lines: cut uploads happen on dev too.
IP=${1:?device address}; BIN=${2:?firmware image}; C=${3:?count}
for i in $(seq "$C"); do
  t0=$(date +%s); code=$(curl -s -m 300 -o /dev/null -w "%{http_code}" -F "update=@$BIN" "http://$IP/update")
  echo "$(date +%T) web OTA $i http $code in $(( $(date +%s) - t0 )) s"; sleep 40
done
