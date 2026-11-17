"""Rejection rate A/B: how fast the device answers and closes wrong-credential requests.

usage: rate.py HOST USER PASS [SEQ_N] [PAR_S] [THREADS]
Sequential: SEQ_N requests per path (/ and /sensor/Fast%20counter, wrong and right hash), each read to the end of
the body: ms to the first byte, and ms to the device's FIN when it comes within CLOSE_WAIT_S of the body ("no close"
counts the rest; RP2 never closes after a 401). Parallel: THREADS threads for PAR_S seconds on /, completed count.
"""

import pathlib
import re
import socket
import statistics
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import authfuzz as af  # noqa: E402

HOST, U, P = sys.argv[1:4]
SEQ = int(sys.argv[4]) if len(sys.argv) > 4 else 100
PAR = float(sys.argv[5]) if len(sys.argv) > 5 else 30
TH = int(sys.argv[6]) if len(sys.argv) > 6 else 4
CLOSE_WAIT_S = 1.0
af.configure(HOST, U, P)


def body_done(buf: bytes) -> bool:
    if b"\r\n\r\n" not in buf:
        return False
    head, body = buf.split(b"\r\n\r\n", 1)
    m = re.search(rb"(?im)^content-length:\s*(\d+)", head)
    return m is not None and len(body) >= int(m.group(1))


def one(path: str, auth: str) -> tuple:
    """(ms to the first byte, ms to the device's close or None, status)."""
    try:
        s = socket.create_connection((HOST, 80), timeout=5)
    except OSError:
        return None, None, "connect"
    t1 = time.monotonic()
    first = closed = None
    st = "?"
    buf = b""
    try:
        s.sendall(af.build("GET", path, auth))
        while True:
            d = s.recv(4096)
            if not d:
                closed = time.monotonic()
                break
            if first is None:
                first = time.monotonic()
                st = d.split(b" ", 2)[1].decode() if d.startswith(b"HTTP") else "junk"
            buf += d
            if body_done(buf):
                s.settimeout(CLOSE_WAIT_S)
                while d := s.recv(4096):
                    pass
                closed = time.monotonic()
                break
    except TimeoutError:
        if not body_done(buf):
            st += "/TimeoutError"
    except OSError as e:
        st = st + "/" + type(e).__name__
    s.close()
    return (
        (first - t1) * 1000 if first else None,
        (closed - t1) * 1000 if closed else None,
        st,
    )


def summ(v: list) -> str:
    v = [x for x in v if x is not None]
    return (
        f"n {len(v)} median {statistics.median(v):.0f} mean {statistics.mean(v):.0f} p90 {statistics.quantiles(v, n=10)[-1]:.0f}"
        if len(v) > 2
        else "n<3"
    )


fails = []
for path in ("/", "/sensor/Fast%20counter"):
    for label, auth, want in (("bad", af.BAD, "401"), ("good", af.GOOD, "200")):
        rows = [one(path, auth) for _ in range(SEQ)]
        codes: dict = {}
        for r in rows:
            codes[r[2]] = codes.get(r[2], 0) + 1
        # timings of other answers (errors, resets) are not the request under test
        if set(codes) != {want}:
            fails.append(f"seq {label} {path} {codes}")
        print(
            time.strftime("[%H:%M:%S]"),
            f"seq {label} {path}: ttfb ms {summ([r[0] for r in rows])}; "
            f"to close ms {summ([r[1] for r in rows])}, no close {sum(r[1] is None for r in rows)}; {codes}",
            flush=True,
        )

for rnd in range(2):
    if rnd:
        time.sleep(10)  # let the first round's TIME_WAIT sockets go
    stop = time.monotonic() + PAR
    codes: dict = {}
    lock = threading.Lock()

    def w(stop: float = stop, lock=lock, codes: dict = codes) -> None:
        while time.monotonic() < stop:
            _, _, st = one("/", af.BAD)
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
print("PASS" if not fails else "FAIL: " + "; ".join(fails), flush=True)
sys.exit(1 if fails else 0)
