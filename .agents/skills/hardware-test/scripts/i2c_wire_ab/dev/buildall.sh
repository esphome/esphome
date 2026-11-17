#!/bin/bash
# buildall.sh YAML-BASENAME...: compile each (`x-pr` from PR_TREE, `x-base` from BASE_TREE) under its file name.
# env: PR_TREE, BASE_TREE, PY, BUILD_LOCK (optional)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
for n in "$@"; do
  case ${n##*-} in pr) T=${PR_TREE:?set PR_TREE} ;; *) T=${BASE_TREE:?set BASE_TREE} ;; esac
  TREE=$T PIO_CORE=${PIO_CORE:-$D/../pio} bash "$S/pio_build.sh" compile "$n" "$n.yaml"
done
