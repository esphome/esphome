"""Rejection rate A/B: how fast the device answers and closes wrong-credential requests.

usage: rate.py HOST USER PASS [SEQ_N] [PAR_S] [THREADS]
Sequential: SEQ_N requests per path (/ and /sensor/Fast%20counter, wrong hash), each read to the device's FIN:
ms to the first byte and ms to the close. Parallel: THREADS threads for PAR_S seconds on /, completed count.
"""

import base64
import socket
import statistics
import sys
import threading
import time

HOST, U, P = sys.argv[1:4]
SEQ = int(sys.argv[4]) if len(sys.argv) > 4 else 100
PAR = float(sys.argv[5]) if len(sys.argv) > 5 else 30
TH = int(sys.argv[6]) if len(sys.argv) > 6 else 4
H = base64.b64encode(f"{U}:{P}".encode()).decode()
BAD = "Basic " + H[:-1] + ("B" if H[-1] != "B" else "C")


def one(path: str, auth: str) -> tuple:
    time.monotonic()
    try:
        s = socket.create_connection((HOST, 80), timeout=5)
    except OSError:
        return None, None, "connect"
    t1 = time.monotonic()
    s.sendall(
        f"GET {path} HTTP/1.1\r\nHost: x\r\nAuthorization: {auth}\r\nConnection: close\r\n\r\n".encode()
    )
    first = None
    st = "?"
    try:
        while True:
            d = s.recv(4096)
            if not d:
                break
            if first is None:
                first = time.monotonic()
                st = d.split(b" ", 2)[1].decode() if d.startswith(b"HTTP") else "junk"
    except OSError as e:
        st = st + "/" + type(e).__name__
    end = time.monotonic()
    s.close()
    return ((first - t1) * 1000 if first else None), (end - t1) * 1000, st


def summ(v: list) -> str:
    v = [x for x in v if x is not None]
    return (
        f"n {len(v)} median {statistics.median(v):.0f} mean {statistics.mean(v):.0f} p90 {statistics.quantiles(v, n=10)[-1]:.0f}"
        if len(v) > 2
        else "n<3"
    )


for path in ("/", "/sensor/Fast%20counter"):
    for label, auth in (("bad", BAD), ("good", "Basic " + H)):
        rows = [one(path, auth) for _ in range(SEQ)]
        codes: dict = {}
        for r in rows:
            codes[r[2]] = codes.get(r[2], 0) + 1
        print(
            time.strftime("[%H:%M:%S]"),
            f"seq {label} {path}: ttfb ms {summ([r[0] for r in rows])}; "
            f"to close ms {summ([r[1] for r in rows])}; {codes}",
            flush=True,
        )

for rnd in range(2):
    stop = time.monotonic() + PAR
    codes: dict = {}
    lock = threading.Lock()

    def w(stop: float = stop, lock=lock, codes: dict = codes) -> None:
        while time.monotonic() < stop:
            _, _, st = one("/", BAD)
            with lock:
                codes[st] = codes.get(st, 0) + 1

    ts = [threading.Thread(target=w) for _ in range(TH)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    print(
        time.strftime("[%H:%M:%S]"),
        f"parallel {TH}x{PAR:.0f}s round {rnd}: {codes}",
        flush=True,
    )
    time.sleep(10)
