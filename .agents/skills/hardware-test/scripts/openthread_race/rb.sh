#!/bin/bash
# rb.sh NAME COUNT OUT [TARGET6]: COUNT restarts of NAME through its Restart button; after each, wait up to 120 s for
# the API, the OMR prefix (the device's own sensor from omr.yaml, read by omr_state.py) and, when given, a ping6 to a
# Thread device's address TARGET6, and append one "cycle N ok=..." line to OUT (restarted=0: the uptime shows no
# restart). The API key is API_KEY, else apikey.txt, else plaintext. env: PY
D=$(cd "$(dirname "$0")" && pwd); N=$1; C=$2; O=$3; T6=$4
if [ -z "$N" ] || [ -z "$O" ] || ! [[ $C =~ ^[1-9][0-9]*$ ]]; then
  echo "usage: rb.sh NAME COUNT OUT [TARGET6] (COUNT a positive number)" >&2; exit 2
fi
if [ -n "${API_KEY+x}" ]; then KEY=$API_KEY; src="API_KEY"
elif [ -f "$D/apikey.txt" ]; then KEY=$(cat "$D/apikey.txt") || exit 1; src="apikey.txt"
else KEY=; src="none, plaintext API"; fi
# an empty key from either source would silently mean plaintext
[ "$src" = "none, plaintext API" ] || [ -n "$KEY" ] || { echo "rb.sh: $src is empty" >&2; exit 1; }
echo "rb.sh: API key from $src" >&2
# GNU timeout when installed, else a perl alarm (stock macOS has no timeout)
bound() {
  local t=$1; shift
  if command -v timeout > /dev/null; then timeout "$t" "$@"; else perl -e 'alarm shift; exec @ARGV' "$t" "$@"; fi
}
PY=${PY:-python3}
# api_button.py: run it, keep its last output line in $last, and abort on errors no restart can fix
SETUP_ERR='InvalidEncryptionKey|EncryptionPlaintext|RequiresEncryption|InvalidAuth'
SETUP_ERR+='|ModuleNotFoundError|ImportError|no button'
api() {
  local out rc; out=$(bound 8 $PY "$D/../api_button.py" "$IP" "${KEY:--}" "$@" 2>&1); rc=$?
  [ $rc = 0 ] && return 0
  last=${out##*$'\n'}
  if [[ $out =~ $SETUP_ERR ]]; then
    echo "$(date +%T) aborted, setup error: $last" >> "$O"; echo "rb.sh: $last" >&2; exit 1
  fi
  return 1
}
IP=$("$D/../resolve.sh" "$N" 20) || exit 1
for i in $(seq 1 "$C"); do
  t0=$(date +%s)
  last=
  if ! api press Restart; then
    echo "$(date +%T) cycle $i press failed: $last" >> "$O"
    IP=$("$D/../resolve.sh" "$N" 20) ||
      { echo "$(date +%T) $N did not resolve, aborted at cycle $i/$C" >> "$O"; exit 1; }
    continue
  fi
  sleep 8
  ok=0; m=0; a=0; p=0; r=-
  [ -n "$T6" ] || p=1
  while (( $(date +%s) - t0 < 120 )); do
    # a failed probe looks the name up again, in case the restart got a new address
    [ $a = 1 ] || { api list && a=1; } ||
      IP=$("$D/../resolve.sh" "$N" 1 2> /dev/null || echo "$IP")
    # the OMR prefix from the device itself (omr.yaml), not the host's mDNS cache, which can predate the restart
    # its uptime also proves the restart: under the time since the press, or the press did nothing
    if [ $m = 0 ] && [ $a = 1 ]; then
      out=$(bound 25 $PY "$D/omr_state.py" "$IP" "${KEY:--}" 5 2>&1); rc=$?
      last=${out##*$'\n'}  # kept for the cycle line, so a setup error does not read as a missing prefix
      case $out in *"include omr.yaml"*)
        echo "$(date +%T) cycle $i aborted: $last" >> "$O"; echo "rb.sh: $last" >&2; exit 1 ;;
      esac
      if [ $rc = 0 ]; then
        up=${last##*uptime=}; up=${up%%.*}
        if ! [[ $up =~ ^[0-9]+$ ]]; then r="?"  # unreadable omr_state.py output, not a missed restart
        elif [ "$up" -lt $(( $(date +%s) - t0 )) ]; then m=1; r=1; else r=0; fi
      fi
    fi
    # only after the restart is proven, so a ping from before it cannot count as recovery
    [ $p = 1 ] || [ $m = 0 ] || { ping6 -c 1 -i 1 "$T6" > /dev/null 2>&1 && p=1; }
    [ $m$a$p = 111 ] && { ok=1; break; }
    sleep 1
  done
  pv=-; [ -n "$T6" ] && pv=$p  # - when no TARGET6 was given
  echo "$(date +%T) cycle $i ok=$ok restarted=$r omr=$m api=$a ping=$pv back_after=$(( $(date +%s) - t0 ))s" \
    "${last:+last: $last}" >> "$O"
done
