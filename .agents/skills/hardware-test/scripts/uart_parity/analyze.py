"""analyze.py CAPTURE: classify each burst an rd.py 8E1 capture saw (bursts are RX chunks under 15 ms apart).

Knows the shelly_dimmer frames (01 seq cmd len payload sum16 04) and the stm32flash GVR request.
"""

import pathlib
import sys

from uartsim import decode_8e1, encode, rx_parse


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
# what an 8E1 reader shows for each frame sent at 8N1; some frames give the same garble
N1: dict[bytes, list[bytes]] = {}
for seq in range(256):
    for f in (frame(seq, 0x10), frame(seq, 0x11)):
        N1.setdefault(decode_8e1(encode(f, False), "edge"), []).append(f)
N1.setdefault(decode_8e1(encode(GVR, False), "edge"), []).append(GVR)


def classify(b: bytes) -> tuple[str, bool]:
    """Label of an RX burst, and whether it matched anything known (the totals use the same decision)."""
    if b.startswith(b"\r\nRPT"):
        return "8E1 report: " + b.decode("latin1").strip(), True
    if b == b"\x7f":
        return "7F sync (parity not visible)", True
    if b == GVR:
        return "8E1 GVR 01 FE (exact)", True
    if N1.get(b) == [GVR]:
        return "8N1 GVR 01 FE (matches 8N1 model)", True
    if valid(b):
        return f"8E1 frame cmd {b[2]:02x} (exact)", True
    if b in N1:
        fs = " or ".join(
            "GVR" if f == GVR else f"cmd {f[2]:02x} seq {f[1]}" for f in N1[b]
        )
        return f"8N1 frame {fs} (matches 8N1 model)", True
    return "OTHER " + b[:40].hex(" "), False


def main() -> None:
    bursts, last = [], -1.0
    for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
        t, d, h = line.split(" ", 2)
        if d == "TX" and h.startswith("framing=") and h.strip() != "framing=8E1":
            # the model reads an 8E1 capture; at 8N1 every 8N1 frame would decode as an exact 8E1 one
            sys.exit(
                f"{h.strip()}: analyze.py needs an rd.py capture made in mode e (8E1)"
            )
        if d != "RX":
            bursts.append(
                (float(t), d, h.strip())
            )  # TX, or HUP when the port went away
            last = -1.0
            continue
        # unescape a whole burst, since a doubled FF can be split across two reads
        if last >= 0 and float(t) - last < 0.015 and bursts[-1][1] == "RX":
            bursts[-1] = (bursts[-1][0], "RX", bursts[-1][2] + " " + h)
        else:
            bursts.append((float(t), "RX", h))
        last = float(t)
    known = other = marks = hups = short = 0
    for t, d, h in bursts:
        if d != "RX":
            k = f"{d} {h}"
            hups += d == "HUP"
            short += "SHORT" in h  # rd.py could not write all of an ack or feed
        else:
            b, m = rx_parse(h)
            k, ok = classify(b)
            known += ok
            other += not ok
            marks += m
            if m:
                k += f" [{m} PARMRK error marks]"
        print(f"{t:8.3f} {k}")
    # like txt.py: no data, a lost port, error marks or a short write is not a clean classification; a few OTHER
    # bursts are boot noise, more of them than classified ones means a wrong framing or a broken line
    summary = f"{known} classified, {other} OTHER, {marks} PARMRK error marks, {hups} HUP lines, {short} short writes"
    print(summary)
    if not known or other > known or marks or hups or short:
        sys.exit(summary)


if __name__ == "__main__":
    main()
