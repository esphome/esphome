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


def rx_bytes(hexes: str) -> bytes:
    """Bytes of an RX line; PARMRK doubles a real 0xFF."""
    return bytes.fromhex(hexes).replace(b"\xff\xff", b"\xff")


if __name__ == "__main__":
    P = (
        bytes([0x01, 0x00, 0x11, 0x00, 0x00, 0x11, 0x04, 0x00, 0x00, 0x55, 0xAA, 0xFF])
        + b"HELLO"
    )
    for m in ("edge", "level"):
        n_to_e = decode_8e1(encode(P, False), m).hex(" ")
        e_to_e = decode_8e1(encode(P, True), m).hex(" ")
        print(m, "N->E", n_to_e, "| E->E", e_to_e)
