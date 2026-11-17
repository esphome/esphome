#!/bin/bash
# webota.sh HOST IMAGE OUTPREFIX : web_server /update upload, wait for reboot, then a restart button press
H=$1; IMG=$2; O=$3
up() { curl -s -m 5 http://$H/sensor/Uptime | python3 -c 'import json,sys; print(json.load(sys.stdin)["value"])' 2>/dev/null; }
waitup() { for i in $(seq 1 90); do u=$(up); [ -n "$u" ] && [ "${u%.*}" -lt 120 ] && { echo "$u"; return 0; }; sleep 2; done; echo none; return 1; }
echo "uptime before $(up)" | tee $O-ota.txt
T0=$(date +%s)
curl -s -m 600 -o $O-ota-resp.txt -w "ota http %{http_code} size %{size_upload} time %{time_total}\n" -F "update=@$IMG" http://$H/update | tee -a $O-ota.txt
echo "resp: $(head -c 120 $O-ota-resp.txt)" | tee -a $O-ota.txt
sleep 8; echo "back after OTA (uptime $(waitup)) $(( $(date +%s) - T0 ))s" | tee -a $O-ota.txt
sleep 10; curl -s -m 10 -X POST -o /dev/null -w "restart press %{http_code}\n" http://$H/button/Restart/press | tee -a $O-ota.txt
sleep 8; echo "back after restart (uptime $(waitup))" | tee -a $O-ota.txt
sleep 5; echo "heap after reboot $(curl -s -m 5 http://$H/sensor/Heap%20free)$(curl -s -m 5 http://$H/sensor/Heap%20Free)" | tee -a $O-ota.txt
