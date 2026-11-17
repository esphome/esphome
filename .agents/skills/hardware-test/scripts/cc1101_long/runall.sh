#!/bin/bash
# runall.sh A_MAC B_MAC: both roles. Phase A: board A receives (long build, then the lambda build) while board B sends
# from its long build; phase B swaps them. Configs: cc-a-long.yaml, cc-a-lambda.yaml, cc-b-long.yaml, cc-b-lambda.yaml
# (copies of the c3/s3 examples with `name:` set to the file name). Order matters, see recipes/cc1101.md.
# env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1; A=${1:?a mac}; B=${2:?b mac}
PY=${PY:-${TREE:?set TREE}/venv/bin/python}
drive() { # TXNAME RXNAME MODE TXLABEL RXLABEL
  echo "--- $3 TX=$1 RX=$2"
  timeout 1200 $PY ccdrive.py "$1.local" "$2.local" "$3" "$2-$3.log" "$4" "$5" 2>&1 |
    grep -v "Warning\|tracemalloc\|self.cli\|^\s*$"
}
fl() { ./flashmac.sh "$1" "$2" || { echo "FLASH FAIL $1"; exit 1; }; sleep 8; }
echo "===== Phase A: A receives, B sends"
fl cc-b-long "$B"
fl cc-a-long "$A"; for m in fast r2long r2mid long; do drive cc-b-long cc-a-long $m B A; done
fl cc-a-lambda "$A"; for m in lambda r2lambda; do drive cc-b-long cc-a-lambda $m B A; done
echo "===== Phase B: B receives, A sends"
fl cc-a-long "$A"
fl cc-b-long "$B"; for m in fast r2long r2mid long; do drive cc-a-long cc-b-long $m A B; done
fl cc-b-lambda "$B"; for m in lambda r2lambda; do drive cc-a-long cc-b-lambda $m A B; done
echo RUNDONE
