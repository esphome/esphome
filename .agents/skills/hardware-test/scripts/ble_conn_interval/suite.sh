#!/bin/bash
# suite.sh TAG SCENARIO...: run the #22859 scenarios through drive.py, results in TAG-*.jsonl, stderr in TAG-*.err.
# env: HOST (proxy), PEERS=A,B,C (the three gen_peri.py peripherals; B is the large table), PY, API_KEY (else apikey.txt),
#      CHUNKS (default 8), ROUNDS, FROUNDS, HROUNDS, EROUNDS
# Scenarios: third-nocache third-cache big big-held small small-held big-free small-free small-held-free
#            big-held-free eve eve-held
T=$1; shift
cd "$(dirname "$0")" || exit 1
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
export API_KEY PEERS=${PEERS:?set PEERS=A,B,C}
IFS=, read -r PA PB PC <<< "$PEERS"
P=${PY:-python3} H=${HOST:?set HOST}
# run NAME drive-args...: repeat in chunks until the jsonl holds the wanted rounds (the API link can drop under load)
run() {
  local n=$1; shift; local want=$3 out=$4; echo "$(date +%T) start $n"
  for chunk in $(seq "${CHUNKS:-8}"); do
    # held_failed rounds never tried the measured link, so they do not count
    local have; have=$(grep '"round"' "$out" 2>/dev/null | grep -vc held_failed); have=${have:-0}
    [ "$have" -ge "$want" ] && break
    local args=("$@"); args[2]=$((want - have))
    timeout 3000 $P drive.py "$H" "${args[@]}" >> "$T-$n.err" 2>&1
    local rc=$?
    echo "$(date +%T) chunk $chunk rc=$rc rounds=$(grep '"round"' "$out" | grep -vc held_failed)"
    sleep 20
  done
  echo "$(date +%T) end $n"
}
for s in "$@"; do
  case $s in
    third-nocache) run third-nocache third nocache 12 "$T-third-nocache.jsonl" ;;
    third-cache) run third-cache third cache 12 "$T-third-cache.jsonl" ;;
    big) COLD=1 run cold-big disc "$PB" "${ROUNDS:-10}" "$T-cold-big.jsonl" ;;
    big-held) COLD=1 run cold-big-held disc "$PB" "${ROUNDS:-10}" "$T-cold-big-held.jsonl" "$PA,$PC" ;;
    small) COLD=1 run cold-small disc "$PC" "${ROUNDS:-10}" "$T-cold-small.jsonl" ;;
    small-held) COLD=1 run cold-small-held disc "$PC" "${ROUNDS:-10}" "$T-cold-small-held.jsonl" "$PA,$PB" ;;
    big-free) COLD=1 BUDGET=40 run free-big disc "$PB" "${FROUNDS:-30}" "$T-free-big.jsonl" ;;
    small-free) COLD=1 BUDGET=40 run free-small disc "$PC" "${FROUNDS:-30}" "$T-free-small.jsonl" ;;
    small-held-free) COLD=1 BUDGET=40 run free-small-held disc "$PC" "${FROUNDS:-30}" "$T-free-small-held.jsonl" "$PA,$PB" ;;
    big-held-free) COLD=1 BUDGET=60 run free-big-held disc "$PB" "${HROUNDS:-10}" "$T-free-big-held.jsonl" "$PA,$PC" ;;
    eve) COLD=1 BUDGET=40 run eve eve nocache "${EROUNDS:-10}" "$T-eve.jsonl" ;;
    eve-held) COLD=1 BUDGET=40 run eve-held eve nocache 10 "$T-eve-held.jsonl" "$PA,$PC" ;;
    *) echo "unknown scenario $s"; exit 2 ;;
  esac
done
