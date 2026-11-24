#!/bin/bash
# coordsetup.sh: factory reset the H2 console coordinator and form a centralized network on channel 25 with client
# clusters on endpoint 1 and an On/Off, Level and Color client on endpoint 2, then open it. Run it after every reset of
# the H2 (opening its console port resets it). Needs zbcon.py running.
D=$(cd "$(dirname "$0")" && pwd); PY=${PY:-python3}
send() { $PY "$D/zbdrv.py" send "$1" "${2:-1.5}"; }
send factoryreset 12
for c in "role zc" "channel 25" "network type c" "zha add 1 configuration_tool" "dm add -e 1 -c 0x000c -r C" \
  "dm add -e 1 -c 0x000d -r C" "dm add -e 1 -c 0x000f -r C" "dm add -e 1 -c 0x0010 -r C" "dm add -e 1 -c 0x0006 -r C" \
  "dm add -e 1 -c 0x000a -r S" "zha add 2 color_dimmer_switch" "dm register" "bdb_comm start form"; do send "$c"; done
sleep 5; send "network type" 1; send "address -x" 1; send "network open -t 254"
