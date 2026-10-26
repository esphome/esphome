#!/bin/bash
# usage: run.sh N PHASE...   OTA upload timing A/B: per phase build every board, one warm-up upload per board
# (installs the phase's build, not timed), an API check that each board booted that build, then N rounds of every
# MODE on every board, interleaved so the boards share the same air. summ.py prints medians at the end.
# Layout: $D/PHASE/NAME.yaml per phase and board (with its secrets.yaml); outputs go to $D/res.txt.
# env: BOARDS ("NAME,HOST,OTA_PORT ..."; add an ESP32 on the same Wi-Fi as a network control), TREE_<PHASE>
# (esphome checkout for each phase, e.g. TREE_base, TREE_pr), PY (python with esphome's requirements),
# MODES (default "deflate-tcp raw-tcp"), SETTLE (seconds after a board answers again, default 15)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?rounds}; shift; [ $# -gt 0 ] || { echo "usage: run.sh N PHASE..."; exit 2; }
B=${BOARDS:?set BOARDS}; PY=${PY:?set PY}; MODES=${MODES:-deflate-tcp raw-tcp}; SETTLE=${SETTLE:-15}
wait_up() {  # wait_up HOST PORT: up to 5 minutes for the OTA port to answer
  local i
  for i in $(seq 60); do
    "$PY" -c "import socket,sys; socket.create_connection((sys.argv[1], int(sys.argv[2])), 2).close()" "$1" "$2" 2>/dev/null \
      && { sleep "$SETTLE"; return 0; }
    sleep 5
  done
  return 1
}
up() {  # up PHASE NAME HOST PORT MODE TAG OUTFILE
  local tree=TREE_$1
  wait_up "$3" "$4" || { echo "FAIL: $3 not answering on $4"; return 1; }
  TAG=$1-$2-$6 OTA_PORT=$4 PYTHONPATH=${!tree} timeout 900 "$PY" "$D/otatime.py" "$3" "$(cat "$D/img-$1-$2.txt")" "$5" \
    2>> "$D/stderr-$1.log" | grep RESULT | tee -a "$7"
}
for ph in "$@"; do
  tree=TREE_$ph; [ -n "${!tree}" ] || { echo "FAIL: set $tree"; exit 1; }
  checks=()
  for b in $B; do
    IFS=, read -r n h p <<< "$b"
    (cd "${!tree}" && PYTHONUNBUFFERED=1 timeout 1500 "$PY" -m esphome compile "$D/$ph/$n.yaml") > "$D/build-$ph-$n.log" 2>&1 \
      || { echo "FAIL: build $ph/$n"; exit 1; }
    # the image the CLI uploads (CORE.firmware_bin): ESP-IDF build, LibreTiny UF2, else the PlatformIO bin
    bd=$D/$ph/.esphome/build/$n; : > "$D/img-$ph-$n.txt"
    for f in "$bd/build/$n.bin" "$bd/.pioenvs/$n/firmware.uf2" "$bd/.pioenvs/$n/firmware.bin"; do
      [ -f "$f" ] && { echo "$f" > "$D/img-$ph-$n.txt"; break; }
    done
    [ -s "$D/img-$ph-$n.txt" ] || { echo "FAIL: no firmware image under $bd"; exit 1; }
    case $(cat "$D/img-$ph-$n.txt") in
      *.uf2) PYTHONPATH=${!tree} "$PY" "$D/trim_uf2.py" "$(cat "$D/img-$ph-$n.txt")" > "$D/img-$ph-$n.tmp" \
        && mv "$D/img-$ph-$n.tmp" "$D/img-$ph-$n.txt" || { echo "FAIL: trim_uf2.py $ph/$n"; exit 1; } ;;
    esac
    checks+=("$h=$D/$ph/.esphome/build/$n/build_info.json")
  done
  for b in $B; do IFS=, read -r n h p <<< "$b"; up "$ph" "$n" "$h" "$p" deflate-tcp warm "$D/warm.txt" || exit 1; done
  for b in $B; do IFS=, read -r n h p <<< "$b"; wait_up "$h" "$p"; done
  "$PY" "$D/apiinfo.py" "${checks[@]}" | tee -a "$D/api.txt"; [ "${PIPESTATUS[0]}" = 0 ] || { echo "FAIL: $ph did not boot everywhere"; exit 1; }
  for i in $(seq "$N"); do
    for m in $MODES; do
      for b in $B; do IFS=, read -r n h p <<< "$b"; up "$ph" "$n" "$h" "$p" "$m" "r$i" "$D/res.txt"; done
    done
  done
  echo "PHASE DONE $ph"
done
"$PY" "$D/summ.py" "$D/res.txt"
