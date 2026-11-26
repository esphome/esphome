#!/bin/bash
# keepopen.sh SECONDS: reopen the coordinator's network every 200 s (permit join lasts at most 254 s)
D=$(cd "$(dirname "$0")" && pwd); end=$((SECONDS + $1))
while [ $SECONDS -lt $end ]; do ${PY:-python3} "$D/zbdrv.py" send "network open -t 254" 0; sleep 200; done
