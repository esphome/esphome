#!/bin/bash
# check.sh BUILD CAPTURE: judge one tcp-probe.yaml run. BUILD is .esphome/build/<name>/.pioenvs/<name>, CAPTURE the
# device log. A build without CONFIG_NET_TCP must log "tcp null udp ok" twice and the "TCP stack not built" error
# exactly once; a build with it must log "tcp ok udp ok" twice and no such error.
B=${1:?build dir}; C=${2:?capture}; CFG=$B/zephyr/zephyr/.config
[ -f "$CFG" ] || { echo "FAIL: no $CFG"; exit 1; }
LC_ALL=C grep -E '^CONFIG_NET_TCP=|^CONFIG_NET_BUF_(RX|TX)_COUNT=|^CONFIG_NET_PKT_(RX|TX)_COUNT=' "$CFG" | tr '\n' ' '; echo
err=$(LC_ALL=C grep -a -c "TCP stack not built" "$C")
if LC_ALL=C grep -q '^CONFIG_NET_TCP=y' "$CFG"; then want="tcp ok udp ok"; want_err=0; else want="tcp null udp ok"; want_err=1; fi
tries=$(LC_ALL=C grep -a -c "try [12]: $want" "$C")
echo "try lines '$want': $tries of 2, 'TCP stack not built': $err (want $want_err)"
[ "$tries" = 2 ] && [ "$err" = "$want_err" ] && echo PASS || { echo FAIL; exit 1; }
