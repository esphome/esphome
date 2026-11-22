#!/bin/bash
# meshcop.sh NAME: network name and whether omr is present in the _meshcop._udp TXT record of NAME (macOS dns-sd)
# The host's mDNS cache can answer with a record from before a restart; rb.sh uses omr_state.py instead.
# GNU timeout when installed, else a perl alarm (stock macOS has no timeout)
bound() {
  local t=$1; shift
  if command -v timeout > /dev/null; then timeout "$t" "$@"; else perl -e 'alarm shift; exec @ARGV' "$t" "$@"; fi
}
command -v dns-sd > /dev/null || { echo "meshcop.sh needs dns-sd (macOS)" >&2; exit 2; }
T=$(bound 5 dns-sd -L "$1" _meshcop._udp local. 2>&1 | LC_ALL=C tr ' ' '\n' | LC_ALL=C grep -a -E "^(nn|omr)=")
nn=$(echo "$T" | grep -a '^nn=' | head -1)
[ -n "$nn" ] || { echo "$(date +%T) $1 no record"; exit 1; }
omr=0; echo "$T" | grep -a -q '^omr=' && omr=1
echo "$(date +%T) $1 $nn omr_present=$omr"
