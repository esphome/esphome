"""analyze.py CAPTURE: classify each burst an rd.py 8E1 capture saw (bursts are RX chunks under 15 ms apart).

Knows the shelly_dimmer frames (01 seq cmd len payload sum16 04) and the stm32flash GVR request.
"""

import pathlib
import sys

from uartsim import decode_8e1, encode, rx_bytes


def frame(seq: int, cmd: int, payload: bytes = b"") -> bytes:
    body = bytes([seq, cmd, len(payload)]) + payload
    c = sum(body) & 0xFFFF
    return b"\x01" + body + bytes([c >> 8, c & 0xFF, 0x04])


def valid(b: bytes) -> bool:
    if len(b) >= 7 and b[0] == 1 and b[-1] == 4 and b[3] == len(b) - 7:
        c = sum(b[1:-3]) & 0xFFFF
        return b[-3:-1] == bytes([c >> 8, c & 0xFF])
    return False


GVR = b"\x01\xfe"
# what an 8E1 reader shows for each frame sent at 8N1
N1 = {
    decode_8e1(encode(f, False), "edge"): f
    for seq in range(256)
    for f in (frame(seq, 0x10), frame(seq, 0x11))
}
N1[decode_8e1(encode(GVR, False), "edge")] = GVR


def main() -> None:
    bursts, last = [], -1.0
    for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
        t, d, h = line.split(" ", 2)
        if d != "RX":
            bursts.append((float(t), "TX", h.strip()))
            last = -1.0
            continue
        b = rx_bytes(h)
        if last >= 0 and float(t) - last < 0.015 and bursts[-1][1] == "RX":
            bursts[-1] = (bursts[-1][0], "RX", bursts[-1][2] + b)
        else:
            bursts.append((float(t), "RX", b))
        last = float(t)
    for t, d, b in bursts:
        if d == "TX":
            k = "TX " + b
        elif b.startswith(b"\r\nRPT"):
            k = "8E1 report: " + b.decode().strip()
        elif b == b"\x7f":
            k = "7F sync (parity not visible)"
        elif b == GVR:
            k = "8E1 GVR 01 FE (exact)"
        elif valid(b):
            k = f"8E1 frame cmd {b[2]:02x} (exact)"
        elif b in N1:
            k = f"8N1 frame cmd {N1[b][2]:02x} seq {N1[b][1]} (matches 8N1 model)"
        else:
            k = "OTHER " + b[:40].hex(" ")
        print(f"{t:8.3f} {k}")


if __name__ == "__main__":
    main()
