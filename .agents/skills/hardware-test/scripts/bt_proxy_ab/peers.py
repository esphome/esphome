"""Peer names and addresses for the bt_proxy_ab tools, read from the environment.

PEER_L      local name of the large table burst server (gen_peri.py --burst), default bpab-l
PEER_M      local name of a mid size gen_peri.py server
HK_NAME     local name prefix of a real HomeKit device (default "Eve Flare")
PEER_ADDRS  L=MAC,M=MAC,eve=MAC: report.py and stparse.py label device log lines with it, drive.py matches the
            servers by address in passive scans (no scan response, so no name)
"""

import os
import re
import sys

L = os.environ.get("PEER_L", "bpab-l")
M = os.environ.get("PEER_M", "")
HK_NAME = os.environ.get("HK_NAME", "Eve Flare")


def addrs() -> dict[str, str]:
    """Label (L, M or eve) by upper case MAC, from PEER_ADDRS."""
    out = {}
    labels = {"l": "L", "m": "M", "eve": "eve"}
    for item in filter(None, os.environ.get("PEER_ADDRS", "").split(",")):
        label, _, mac = item.partition("=")
        label, mac = labels.get(label.strip().lower()), mac.strip().upper()
        # a typo would leave that peer's stats empty with no hint why
        if not label or not re.fullmatch(r"[0-9A-F]{2}(:[0-9A-F]{2}){5}", mac):
            sys.exit(f"PEER_ADDRS entry {item!r}: use L=MAC, M=MAC or eve=MAC")
        out[mac] = label
    return out
