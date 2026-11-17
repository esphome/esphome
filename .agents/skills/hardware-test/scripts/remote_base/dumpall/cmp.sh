#!/bin/bash
# cmp.sh BASE_DEBUG PR_DEBUG BASE_VERBOSE PR_VERBOSE: compare host harness outputs
norm() { sed -E 's/\x1b\[[0-9;]*m//g; s/^\[[0-9:.]*\]//; s/\[([A-Z]+)\]\[([a-zA-Z0-9._]+):[0-9]+\]/[\1][\2]/' $1 | awk '/^INPUT /{f=1} f'; }
norm $3 | sed -E 's/^\[[DV]\]\[remote.beo4\]: Beo4: n_sym/[*][remote.beo4]: Beo4: n_sym/' > vb.txt
norm $4 | sed -E 's/^\[[DV]\]\[remote.beo4\]: Beo4: n_sym/[*][remote.beo4]: Beo4: n_sym/' > vp.txt
echo "VERBOSE diff (n_sym level mapped): $(diff vb.txt vp.txt | grep -c '^[<>]') of $(wc -l < vp.txt)"
norm $1 | grep -v 'Beo4: n_sym' > db.txt; norm $2 > dp.txt
echo "DEBUG diff (base n_sym dropped): $(diff db.txt dp.txt | grep -c '^[<>]') of $(wc -l < dp.txt)"
echo "n_sym: DEBUG base $(norm $1 | grep -c '\[D\]\[remote.beo4\]: Beo4: n_sym') pr $(grep -c 'Beo4: n_sym' dp.txt); VERBOSE base D $(norm $3 | grep -c '\[D\]\[remote.beo4\]: Beo4: n_sym') pr V $(norm $4 | grep -c '\[V\]\[remote.beo4\]: Beo4: n_sym')"
