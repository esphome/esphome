#!/bin/bash
# buildall.sh YAML-BASENAME...: compile each with the name from its substitutions, from the tree named by its suffix
# (`x-pr.yaml` builds from PR_TREE, `x-base.yaml` from BASE_TREE). env: PR_TREE, BASE_TREE, PY, BUILD_LOCK (optional)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
for n in "$@"; do
  case ${n##*-} in pr) T=${PR_TREE:?set PR_TREE} ;; *) T=${BASE_TREE:?set BASE_TREE} ;; esac
  nm=$(awk '/^  name:/{print $2; exit}' "$n.yaml")
  TREE=$T PIO_CORE=${PIO_CORE:-$D/../pio} bash "$S/pio_build.sh" compile "$nm" "$n.yaml" | sed "s/^/$nm /"
done
