#!/bin/bash
# runall.sh A B: both roles; A and B are the boards' ports or USB serial numbers (see flashmac.sh). Phase A: board A
# receives (long build, then the lambda build) while board B sends from its long build; phase B swaps them.
# Configs: cc-a-long.yaml, cc-a-lambda.yaml, cc-b-long.yaml, cc-b-lambda.yaml (copies of the c3/s3 examples with
# `name:` set to the file name). Order matters, see recipes/cc1101.md.
# env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1; A=${1:?board a}; B=${2:?board b}
PY=${PY:-${TREE:?set TREE}/venv/bin/python}
export API_KEY=${API_KEY:-$(cat apikey.txt)}
FAILED=0
drive() { # TXNAME RXNAME MODE TXLABEL RXLABEL
  echo "--- $3 TX=$1 RX=$2"
  timeout 1200 $PY ccdrive.py "$1.local" "$2.local" "$3" "$2-$3.log" "$4" "$5" 2>&1 | grep -v "^\s*$"
  local rc=${PIPESTATUS[0]}
  [ "$rc" = 0 ] || { echo "MODE FAILED $3 TX=$1 RX=$2 exit=$rc (124 is the timeout)"; FAILED=$((FAILED + 1)); }
}
fl() { ./flashmac.sh "$1" "$2" || { echo "FLASH FAIL $1"; exit 1; }; }
phase() { # TX RX TXBOARD RXBOARD TXLABEL RXLABEL: RX receives (long build, then the lambda build), TX sends
  fl "cc-$1-long" "$3"
  fl "cc-$2-long" "$4"; for m in fast r2long r2mid long; do drive "cc-$1-long" "cc-$2-long" $m "$5" "$6"; done
  fl "cc-$2-lambda" "$4"; for m in lambda r2lambda; do drive "cc-$1-long" "cc-$2-lambda" $m "$5" "$6"; done
}
echo "===== Phase A: A receives, B sends"
phase b a "$B" "$A" B A
echo "===== Phase B: B receives, A sends"
phase a b "$A" "$B" A B
echo "RUNDONE failed_modes=$FAILED"
[ "$FAILED" = 0 ]
