"""Model: bytes sent back to back at 8N1 or 8E1, decoded by an 8E1 receiver.

Run it to print the garble an 8E1 reader shows for the test pattern sent at 8N1.
"""


def encode(data: bytes, parity: bool) -> list[int]:
    bits = [1, 1]
    for b in data:
        bits.append(0)
        d = [(b >> i) & 1 for i in range(8)]
        bits += d
        if parity:
            bits.append(sum(d) & 1)
        bits.append(1)
    return bits + [1] * 12


def decode_8e1(bits: list[int], mode: str) -> bytes:
    """mode edge: a start bit needs a 1 before it; level: any 0 starts a byte."""
    out, i, n = [], 1, len(bits)
    while i < n - 11:
        if bits[i] == 0 and (mode == "level" or bits[i - 1] == 1):
            out.append(sum(v << k for k, v in enumerate(bits[i + 1 : i + 9])))
            i += 11
        else:
            i += 1
    return bytes(out)


def rx_parse(hexes: str) -> tuple[bytes, int]:
    """Bytes of RX hex and the number of PARMRK error marks: FF FF is a real 0xFF, FF 00 X a parity or framing
    error on X (kept as X). A cut off escape or a lone FF also counts as a mark, with no data byte."""
    raw, out, marks, i = bytes.fromhex(hexes), bytearray(), 0, 0
    while i < len(raw):
        if raw[i] == 0xFF and raw[i + 1 : i + 2] == b"\xff":
            out.append(0xFF)
            i += 2
        elif raw[i] == 0xFF and raw[i + 1 : i + 2] == b"\x00" and i + 2 < len(raw):
            out.append(raw[i + 2])
            marks += 1
            i += 3
        elif raw[i] == 0xFF and (i + 1 == len(raw) or raw[i + 1 :] == b"\x00"):
            marks += 1  # truncated escape: a real 0xFF would be doubled
            break
        elif raw[i] == 0xFF:
            marks += 1  # a lone 0xFF breaks the escaping; not kept as data
            i += 1
        else:
            out.append(raw[i])
            i += 1
    return bytes(out), marks


def missing_stimulus(header: str, tx: list[str], syncs: int, reports: int) -> list[str]:
    """Options in the rd.py header (options=ack,feed) whose writes fell short: rd.py acks each lone 7F sync and
    feeds after each RPT report line, so a split or merged read skips one; only the last may go unanswered."""
    opts = (
        header.rsplit("options=", maxsplit=1)[-1].split(",")
        if "options=" in header
        else []
    )
    out = []
    for o, p, want in (("ack", "79", syncs), ("feed", "55", reports)):
        got = sum(h.startswith(p) for h in tx)
        if o in opts and (not got or got < want - 1):
            out.append(f"{o} {got}/{want}")
    return out


def rx_bytes(hexes: str) -> bytes:
    """Bytes of RX hex, without the PARMRK escapes."""
    return rx_parse(hexes)[0]


if __name__ == "__main__":
    P = (
        bytes([0x01, 0x00, 0x11, 0x00, 0x00, 0x11, 0x04, 0x00, 0x00, 0x55, 0xAA, 0xFF])
        + b"HELLO"
    )
    for m in ("edge", "level"):
        n_to_e = decode_8e1(encode(P, False), m).hex(" ")
        e_to_e = decode_8e1(encode(P, True), m).hex(" ")
        print(m, "N->E", n_to_e, "| E->E", e_to_e)
