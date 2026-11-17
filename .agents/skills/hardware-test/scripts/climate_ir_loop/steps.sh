# prelude puts every sender in the same state on both builds; frames before the first MARK step are not compared
PRE=("SAM TX:mode=off,t=24,fan=auto,swing=off" "FR TX:mode=off,t=72,fan=auto,swing=off" "SI TX:mode=off,t=21,fan=auto,preset=comfort" "MI TX:mode=off,t=24,fan=auto")
STEPS=()
for p in "SAM TX" "FR TX" "MI TX"; do
  for m in heat_cool cool dry fan_only heat off; do STEPS+=("${p}:mode=$m"); done
  STEPS+=("${p}:mode=cool")
  if [ "$p" = "FR TX" ]; then fans=(high medium low quiet auto); else fans=(low medium high auto); fi
  for f in "${fans[@]}"; do STEPS+=("${p}:fan=$f"); done
  [ "$p" = "SAM TX" ] && for s in vertical horizontal both off; do STEPS+=("${p}:swing=$s"); done
  STEPS+=("${p}:mode=off")
done
STEPS+=("SI TX:mode=heat_cool" "SI TX:preset=eco" "SI TX:preset=comfort")
for f in low medium high auto; do STEPS+=("SI TX:fan=$f"); done
STEPS+=("SI TX:mode=off")
