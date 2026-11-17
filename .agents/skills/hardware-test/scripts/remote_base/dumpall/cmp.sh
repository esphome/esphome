#!/bin/bash
# cmp.sh BASE_DEBUG PR_DEBUG BASE_VERBOSE PR_VERBOSE: compare host harness outputs from the first INPUT line on,
# with colours, timestamps and line numbers removed (../../lognorm.py). Writes db/dp/vb/vp.txt here.
PY=${PY:-python3}; LN=$(cd "$(dirname "$0")/../.." && pwd)/lognorm.py
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
i=0; for f in "$1" "$2" "$3" "$4"; do i=$((i + 1)); "$PY" "$LN" "$f" | awk '/^INPUT /{f=1} f' > "$TMP/$i"; done
NSYM='s/^\[[DV]\]\[remote.beo4\]: Beo4: n_sym/[*][remote.beo4]: Beo4: n_sym/'
sed -E "$NSYM" "$TMP/3" > vb.txt
sed -E "$NSYM" "$TMP/4" > vp.txt
echo "VERBOSE diff (n_sym level mapped): $(diff vb.txt vp.txt | grep -c '^[<>]') of $(wc -l < vp.txt)"
grep -v 'Beo4: n_sym' "$TMP/1" > db.txt; cp "$TMP/2" dp.txt
echo "DEBUG diff (base n_sym dropped): $(diff db.txt dp.txt | grep -c '^[<>]') of $(wc -l < dp.txt)"
echo "n_sym: DEBUG base $(grep -c '\[D\]\[remote.beo4\]: Beo4: n_sym' "$TMP/1") pr $(grep -c 'Beo4: n_sym' dp.txt);" \
  "VERBOSE base D $(grep -c '\[D\]\[remote.beo4\]: Beo4: n_sym' "$TMP/3") pr V $(grep -c '\[V\]\[remote.beo4\]: Beo4: n_sym' "$TMP/4")"
