#!/bin/zsh
# restchurn.sh HOST SECS OUT: 4 parallel loops of REST POSTs (Sw toggle, Lt on with a brightness, Num set, Btn press
# from stress.yaml) for SECS; writes "time code" lines to OUT and prints the count per HTTP code.
H=$1; S=$2; O=$3; : > $O; : > $O.1; : > $O.2; : > $O.3; : > $O.4; end=$(( $(date +%s) + S ))
# $1 is expanded per request outside the command substitution, so each $RANDOM in it is a new value
loop() { while (( $(date +%s) < end )); do u=${(e)1}; echo "$(date +%s) $(curl -s -o /dev/null -m 3 -w '%{http_code}' -X POST "http://$H$u")"; done >> $O.$2; }
loop '/switch/Sw/toggle' 1 & loop '/light/Lt/turn_on?brightness=$((RANDOM % 255))' 2 &
loop '/number/Num/set?value=$((RANDOM % 100))' 3 & loop '/button/Btn/press' 4 & wait
cat $O.1 $O.2 $O.3 $O.4 | sort -n > $O; rm -f $O.1 $O.2 $O.3 $O.4
echo "REST $(awk '{c[$2]++} END {for (k in c) printf "%s:%d ", k, c[k]}' $O)"
