"""Compare the listener log (JSON lines from udp_listen.py) against the udp-write-dut.yaml payload sequence.

usage: check.py LISTEN_JSONL ECHO_HEX  (ECHO_HEX is the datagram sent to the device for the echo case)
"""

import json
import pathlib
import sys

with pathlib.Path(sys.argv[1]).open(encoding="utf-8") as f:
    log = [json.loads(line) for line in f if line.strip()]
echo_in = bytes.fromhex(sys.argv[2])
small = bytes([0xAA, 0x55, 0x01, 0x02])
expected = [
    ("small 1/2", small),
    ("small 2/2", small),
    ("simple", bytes([1, 2, 3])),
    ("text", b"hello udp"),
    ("empty", b""),
    ("stack128", bytes((0x10 + i) & 0xFF for i in range(128))),
    ("heap129", bytes((0x20 + i) & 0xFF for i in range(129))),
    ("heap600", bytes((0x30 + i) & 0xFF for i in range(600))),
    ("lambda300", bytes((i * 7) & 0xFF for i in range(300))),
    ("echo", echo_in[::-1]),
]
got = [bytes.fromhex(e["hex"]) for e in log]
ok = 0
for i, (name, want) in enumerate(expected):
    have = got[i] if i < len(got) else None
    res = (
        "OK"
        if have == want
        else f"MISMATCH got {None if have is None else have[:16].hex() + '... len ' + str(len(have))}"
    )
    ok += res == "OK"
    print(f"{name:10s} {len(want):4d} B  {res}")
print(
    f"{ok}/{len(expected)} payloads byte-exact, {len(got)} datagrams received, sources {sorted({e['src'] for e in log})}"
)
sys.exit(0 if ok == len(expected) and len(got) == len(expected) else 1)
