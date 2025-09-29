#!/bin/bash
set -euo pipefail

sudo apt update && sudo apt install -y hugo

pip3 install -r requirements_test.txt
pre-commit install

mkdir -p ~/.local/bin

ARCH="$(uname -m)"
case "$ARCH" in
  x86_64) PAGEFIND_ARCH="x86_64" ;;
  arm64|aarch64) PAGEFIND_ARCH="aarch64" ;;
  *)
    echo "Unsupported architecture: $ARCH" >&2
    exit 1
    ;;
esac
curl -L "https://github.com/CloudCannon/pagefind/releases/download/v1.3.0/pagefind-v1.3.0-${PAGEFIND_ARCH}-unknown-linux-musl.tar.gz" \
  | tar -xz -C ~/.local/bin
