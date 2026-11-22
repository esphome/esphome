#!/bin/zsh
# httpscheck.sh HOST [ROUNDS]: over HTTPS (self-signed, -k) /events must answer 501 every time, REST GET/POST and the
# page must answer 200; then prints the counts per kind, the counter and the heap sensor.
W=${0:A:h}
H=$1; R=${2:-30}; typeset -A C
for i in $(seq 1 $R); do
  e=$(curl -sk -m 8 -o $W/evbody.txt -w '%{http_code}' -H 'Accept: text/event-stream' https://$H/events); C[events_$e]=$((${C[events_$e]:-0}+1))
  [[ $i == 1 ]] && echo "events body: $(head -c 120 $W/evbody.txt)"
  g=$(curl -sk -m 8 -o /dev/null -w '%{http_code}' "https://$H/sensor/Counter"); C[get_$g]=$((${C[get_$g]:-0}+1))
  t=$(curl -sk -m 8 -o /dev/null -w '%{http_code}' -X POST -H 'Content-Length: 0' "https://$H/switch/Relay/toggle"); C[toggle_$t]=$((${C[toggle_$t]:-0}+1))
  (( i % 10 == 0 )) && { p=$(curl -sk -m 15 -o /dev/null -w '%{http_code}' "https://$H/"); C[page_$p]=$((${C[page_$p]:-0}+1)); }
done

for k in ${(ko)C}; do echo "$k ${C[$k]}"; done
echo "counter: $(curl -sk -m 8 https://$H/sensor/Counter)"
echo "heap: $(curl -sk -m 8 "https://$H/sensor/Heap%20Free")"
echo "plain http /events on port 80: $(curl -s -m 4 -o /dev/null -w '%{http_code}' http://$H/events)"
