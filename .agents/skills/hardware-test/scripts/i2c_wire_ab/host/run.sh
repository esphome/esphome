#!/bin/bash
# run.sh TREE OUTFILE: copy the harness into TREE's tests/components/i2c, generate the extracted bus bodies, run the
# unit test and remove the harness again. env: PY (default TREE's venv python)
T=$(cd "${1:?tree}" && pwd); O=$(cd "$(dirname "${2:?out}")" && pwd)/$(basename "$2")
H=$(cd "$(dirname "$0")" && pwd); PY=${PY:-$T/venv/bin/python}
D=$T/tests/components/i2c
# The sources are stored as .cpp.example, .h.example and .hpp so the repo's C++ lints skip them; copy them back.
FILES=(wab_common.h wab_ard8266.cpp wab_ardpico.cpp wab_idf.cpp wab_sw.cpp test_wab.cpp benchmark.yaml)
G=$(mktemp -d)
cleanup() { for f in "${FILES[@]}"; do rm -f "$D/$f"; done; rm -rf "$G"; }
trap cleanup EXIT
trap 'exit 130' INT TERM
cp "$H/wab_common.hpp" "$D/wab_common.h"
for f in "${FILES[@]:1:5}"; do cp "$H/$f.example" "$D/$f"; done
mkdir -p "$G/gen" "$G/stubs/driver"
for f in Arduino.h Wire.h driver/i2c_master.h; do cp "$H/stub/$f.example" "$G/stubs/$f"; done
$PY "$H/extract.py" "$T" "$G/gen"
printf "esphome:\n  build_flags:\n    - -I%s\n    - -I%s\n    - -I%s\n" "$D" "$G/stubs" "$G/gen" > "$D/benchmark.yaml"
(cd "$T" && WAB_OUT=$O PYTHONPATH=$T $PY script/cpp_unit_test.py i2c) > "${O%.txt}-build.log" 2>&1
rc=$?
echo "rc=$rc"; tail -5 "${O%.txt}-build.log"
exit $rc
