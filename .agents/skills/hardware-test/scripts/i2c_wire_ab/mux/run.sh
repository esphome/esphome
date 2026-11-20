#!/bin/bash
# run.sh TREE OUT [freq]: run the host harness in TREE's tests/components/tca9548a and remove it again
# (freq: build with I2C_PORT_FREQUENCY_COUNT=4). env: PY (default TREE's venv python)
T=$(cd "${1:?tree}" && pwd); O=$(cd "$(dirname "${2:?out}")" && pwd)/$(basename "$2")
H=$(cd "$(dirname "$0")" && pwd); PY=${PY:-$T/venv/bin/python}; D=$T/tests/components/tca9548a
for f in test_muxab.cpp benchmark.yaml; do
  [ -e "$D/$f" ] && { echo "$D/$f already exists, not overwriting it" >&2; exit 1; }
done
cleanup() { rm -f "$D/test_muxab.cpp" "$D/benchmark.yaml"; }
trap cleanup EXIT
trap 'exit 130' INT TERM
# stored as .cpp.example so the repo's C++ lints skip it
cp "$H/test_muxab.cpp.example" "$D/test_muxab.cpp"
[ "${3:-}" = freq ] && printf "esphome:\n  build_flags:\n    - -DI2C_PORT_FREQUENCY_COUNT=4\n" > "$D/benchmark.yaml"
(cd "$T" && MUXAB_OUT=$O PYTHONPATH=$T $PY script/cpp_unit_test.py tca9548a) > "${O%.txt}-build.log" 2>&1
rc=$?
echo "rc=$rc"; grep -a -E "PASSED|FAILED|error:" "${O%.txt}-build.log" | head -5
exit $rc
