"""Recompute the Z-Wave checksum (XOR of LENGTH..payload, initial 0xFF) over every frame the proxy logged."""

from functools import reduce
import pathlib
import re
import sys


def zw_checksum(frame: bytes) -> int:
    return reduce(lambda a, b: a ^ b, frame[1:-1], 0xFF)


def complete(frame: bytes) -> bool:
    """A data frame is SOF, LENGTH, then LENGTH bytes; the proxy logs at most 168 bytes, so longer ones are cut."""
    return len(frame) >= 3 and frame[0] == 0x01 and len(frame) == frame[1] + 2


for path in sys.argv[1:]:
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    sent = re.findall(r"Sending: ((?:[0-9A-F]{2}:)+[0-9A-F]{2})", text)
    recv = re.findall(r"Received frame: ((?:[0-9A-F]{2}:)+[0-9A-F]{2})", text)
    calc = re.findall(
        r"CHECKSUM Received: 0x([0-9A-F]{2}) - Calculated: 0x([0-9A-F]{2})", text
    )
    bad = len(re.findall(r"Bad checksum", text))
    acks_sent = len(re.findall(r"Sending ACK", text))
    naks_sent = len(re.findall(r"Sending NAK/CAN", text))
    acks_recv = len(re.findall(r"Received ACK", text))
    naks_recv = len(re.findall(r"Received NAK", text))
    reboots = len(re.findall(r"Boot count|Reset reason|rst:0x", text))
    long_ops = len(re.findall(r"zwave_proxy took a long time", text))
    rx_frames = [bytes.fromhex(f.replace(":", "")) for f in recv]
    tx_frames = [bytes.fromhex(f.replace(":", "")) for f in sent]
    rx_cut = sum(1 for b in rx_frames if not complete(b))
    tx_cut = sum(1 for b in tx_frames if len(b) > 1 and not complete(b))
    rx_ok = sum(1 for b in rx_frames if complete(b) and zw_checksum(b) == b[-1])
    tx_ok = sum(1 for b in tx_frames if complete(b) and zw_checksum(b) == b[-1])
    calc_ok = sum(1 for r, c in calc if r == c)
    cmds = sorted({f.split(":")[3] for f in recv})
    print(
        f"{path.rsplit('/', 1)[-1]}: tx frames {len(sent)} (checksum ok {tx_ok}, truncated in log {tx_cut}), "
        f"rx frames {len(recv)} (recomputed ok {rx_ok}, truncated in log {rx_cut}), "
        f"device calc==received {calc_ok}/{len(calc)}, bad checksum warnings {bad}, ACK sent {acks_sent}, NAK sent {naks_sent}, "
        f"ACK received {acks_recv}, NAK received {naks_recv}, long ops {long_ops}, rx cmd ids {cmds}"
    )
