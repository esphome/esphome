#!/bin/sh
# compare.sh NAME_A NAME_B : diff the readback.sh outputs of two device names (api, web, mqttn and, when present, dump)
# after replacing each name with DEV. Exit 1 when any pair differs; the diffs stay in cmp-KIND.diff.
A=$1; B=$2; rc=0
[ -n "$A" ] && [ -n "$B" ] || { echo "usage: compare.sh NAME_A NAME_B"; exit 2; }
for k in api web mqttn dump; do
  ext=jsonl; [ $k = dump ] && ext=txt
  [ -f "$PWD/$k-$A.$ext" ] && [ -f "$PWD/$k-$B.$ext" ] || { echo "$k: missing"; [ $k = dump ] || rc=1; continue; }
  sed "s/$A/DEV/g; s/$(echo "$A" | tr - _)/DEV/g" "$PWD/$k-$A.$ext" > "$PWD/cmp-$k-a.txt"
  sed "s/$B/DEV/g; s/$(echo "$B" | tr - _)/DEV/g" "$PWD/$k-$B.$ext" > "$PWD/cmp-$k-b.txt"
  if diff -u "$PWD/cmp-$k-a.txt" "$PWD/cmp-$k-b.txt" > "$PWD/cmp-$k.diff"; then echo "$k: identical ($(wc -l < "$PWD/cmp-$k-b.txt" | tr -d ' ') lines)"; else echo "$k: DIFFERS"; head -20 "$PWD/cmp-$k.diff"; rc=1; fi
done
exit $rc
