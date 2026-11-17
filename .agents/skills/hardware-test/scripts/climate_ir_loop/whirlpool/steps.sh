PRE=("W TX:mode=off,t=24,fan=auto,swing=off")
STEPS=()
for m in heat_cool heat cool dry fan_only off; do STEPS+=("W TX:mode=${m}"); done
STEPS+=("W TX:mode=cool")
for f in low medium high auto; do STEPS+=("W TX:fan=${f}"); done
STEPS+=("W TX:mode=off")
