"""bench.py HOST MODE [arg]: rx SECS | tx SECS | tcpburst COUNT [KB] | udpburst ROUNDS [PKTS]. Prints one RESULT line.

rx sends to the tcpbench sink (device receive), tx reads from its source (device send), tcpburst makes COUNT
connections of KB each, udpburst sends ROUNDS bursts of PKTS 1400 byte datagrams and asks the device how many arrived.
"""

import contextlib
import errno
import socket
import statistics
import struct
import sys
import time

host, mode = sys.argv[1], sys.argv[2]
arg = int(sys.argv[3]) if len(sys.argv) > 3 else 20


def rx_once(nbytes: int | None = None, secs: float | None = None) -> tuple[int, int]:
    s = socket.create_connection((host, 5001), timeout=30)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1 << 20)
    buf = memoryview(b"\xa5" * 65536)
    sent = 0
    t0 = time.monotonic()
    while True:
        if secs is not None and time.monotonic() - t0 >= secs:
            break
        if nbytes is not None and sent >= nbytes:
            break
        chunk = buf if nbytes is None else buf[: min(len(buf), nbytes - sent)]
        s.sendall(chunk)
        sent += len(chunk)
    s.shutdown(socket.SHUT_WR)
    rep = b""
    while len(rep) < 12:
        d = s.recv(12 - len(rep))
        if not d:
            break
        rep += d
    s.close()
    return struct.unpack("<QI", rep)


def tx_once(secs: int) -> tuple[int, float]:
    s = socket.create_connection((host, 5002), timeout=30)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    s.sendall(struct.pack("<I", secs))
    total = 0
    t0 = t1 = None
    while d := s.recv(65536):
        if t0 is None:
            t0 = time.monotonic()
        total += len(d)
        t1 = time.monotonic()
    return total, ((t1 - t0) * 1000) if t0 is not None else 0.0


def udp_burst(rounds: int, pkts: int) -> None:
    u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    u.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 << 20)
    u.settimeout(2)
    payload = b"\x55" * 1400
    got = []
    enobufs = 0
    u.sendto(b"Q", (host, 5003))  # reset the device counter
    with contextlib.suppress(TimeoutError):
        u.recvfrom(16)
    for _ in range(rounds):
        for _ in range(pkts):
            while True:
                try:
                    u.sendto(payload, (host, 5003))
                    break
                except OSError as e:
                    if e.errno != errno.ENOBUFS:
                        raise
                    enobufs += 1
                    time.sleep(0.0005)
        time.sleep(0.5)
        u.sendto(b"Q", (host, 5003))
        try:
            d, _ = u.recvfrom(16)
        except TimeoutError:  # the next count would span several bursts
            sys.exit("udpburst: count query timed out, run discarded")
        got.append(struct.unpack("<I", d[:4])[0])
        time.sleep(0.3)
    print(
        f"RESULT udpburst rounds={rounds} pkts={pkts} median={statistics.median(got)} "
        f"min={min(got)} max={max(got)} enobufs={enobufs}"
    )


if mode == "rx":
    total, ms = rx_once(secs=arg)
    print(f"RESULT rx bytes={total} ms={ms} KBps={total / 1.024 / max(ms, 1):.1f}")
elif mode == "tx":
    total, ms = tx_once(arg)
    if not total:
        sys.exit("tx: the device sent nothing")
    print(f"RESULT tx bytes={total} ms={ms:.0f} KBps={total / 1.024 / max(ms, 1):.1f}")
elif mode == "tcpburst":
    kb = int(sys.argv[4]) if len(sys.argv) > 4 else 512
    rates = []
    for _ in range(arg):
        total, ms = rx_once(nbytes=kb * 1024)
        rates.append(total / 1.024 / max(ms, 1))
        time.sleep(0.3)
    print(
        f"RESULT tcpburst n={arg} kb={kb} median={statistics.median(rates):.1f} "
        f"min={min(rates):.1f} max={max(rates):.1f}"
    )
elif mode == "udpburst":
    udp_burst(arg, int(sys.argv[4]) if len(sys.argv) > 4 else 300)
else:
    sys.exit(f"unknown mode {mode}")
