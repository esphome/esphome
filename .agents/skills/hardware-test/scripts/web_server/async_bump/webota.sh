#!/bin/bash
# webota.sh HOST IMAGE OUTPREFIX : web_server /update upload, wait for reboot, then a restart button press. A reboot
# counts only when the uptime drops below the reading taken before the operation; exits 1 when one is not seen.
H=$1; IMG=$2; O=$3; D=$(cd "$(dirname "$0")" && pwd); PY=${PY:-python3}
up() { $PY -I "$D/rest.py" uptime "$H"; }
# waitreset BEFORE AT: the uptime once the board booted after the reading BEFORE taken at epoch second AT (uptime now
# 5 s or more below BEFORE plus the time since); "none" and status 1 after 180 s or with no BEFORE
waitreset() {
  for i in $(seq 1 90); do
    u=$(up); [ -n "$u" ] && [ -n "$1" ] && [ "$u" -lt $(($1 + $(date +%s) - $2 - 5)) ] && { echo "$u"; return 0; }
    sleep 2
  done
  echo none; return 1
}
RC=0
U0=$(up); T0=$(date +%s); echo "uptime before $U0" | tee $O-ota.txt
curl -s -m 600 -o $O-ota-resp.txt -w "ota http %{http_code} size %{size_upload} time %{time_total}\n" -F "update=@$IMG" http://$H/update | tee -a $O-ota.txt
echo "resp: $(head -c 120 $O-ota-resp.txt)" | tee -a $O-ota.txt
sleep 8; U=$(waitreset "$U0" $T0) || RC=1
echo "back after OTA (uptime $U) $(( $(date +%s) - T0 ))s" | tee -a $O-ota.txt
sleep 10; U1=$(up); T1=$(date +%s)
curl -s -m 10 -X POST -o /dev/null -w "restart press %{http_code}\n" http://$H/button/Restart/press | tee -a $O-ota.txt
sleep 8; U=$(waitreset "$U1" $T1) || RC=1
echo "back after restart (uptime $U, was $U1)" | tee -a $O-ota.txt
sleep 5; echo "heap after reboot $($PY -I "$D/rest.py" heap "$H")" | tee -a $O-ota.txt
[ $RC = 0 ] || echo "FAIL: no reset seen" | tee -a $O-ota.txt
exit $RC
