#!/bin/bash
# buildall.sh YAML...: compile each config under the `name:` of its substitutions (the file name when it sets none),
# from the tree named by its suffix (`x-pr.yaml` builds from PR_TREE, any other from BASE_TREE).
# env: PR_TREE, BASE_TREE, PY, BUILD_LOCK (optional), PIO_CORE (default i2c_wire_ab/pio)
# Stops at the first failed build and exits with its status.
set -o pipefail
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
for y in "$@"; do
  b=$(basename "$y" .yaml)
  case ${b##*-} in pr) T=${PR_TREE:?set PR_TREE} ;; *) T=${BASE_TREE:?set BASE_TREE} ;; esac
  nm=$(awk '/^substitutions:/{s=1; next} /^[^ #]/{s=0} s && /^  name:/{gsub(/["\x27]/, "", $2); print $2; exit}' "$y")
  nm=${nm:-$b}
  TREE=$T PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" compile "$nm" "$y" | sed "s/^/$nm /" || exit
done
