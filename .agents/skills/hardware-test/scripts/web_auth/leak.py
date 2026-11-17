"""Heap leak check: rounds of bad-credential load, heap read after a settle each round.

usage: leak.py HOST USER PASS [ROUNDS] [SETTLE_S] [CONC_S] [THREADS] [RATE]
Each round: THREADS threads send wrong-hash requests for CONC_S seconds while one authenticated /events
stream is open, then everything is closed and the heap is read after SETTLE_S seconds (lwIP TIME_WAIT
and the async server's queued sockets hold heap for a while after a flood).
"""

import base64
import json
import socket
import sys
import threading
import time

HOST, U, P = sys.argv[1:4]
ROUNDS = int(sys.argv[4]) if len(sys.argv) > 4 else 4
SETTLE = float(sys.argv[5]) if len(sys.argv) > 5 else 30
CONC = float(sys.argv[6]) if len(sys.argv) > 6 else 30
THREADS = int(sys.argv[7]) if len(sys.argv) > 7 else 4
# RATE > 0 paces the wrong-hash requests to RATE per second in total, so builds that answer at different
# speeds see the same load
RATE = float(sys.argv[8]) if len(sys.argv) > 8 else 0.0
H = base64.b64encode(f"{U}:{P}".encode()).decode()
BAD = "Basic " + H[:-1] + ("B" if H[-1] != "B" else "C")


def get(path: str) -> object:
    for _ in range(5):
        try:
            s = socket.create_connection((HOST, 80), timeout=4)
            s.sendall(
                f"GET {path} HTTP/1.1\r\nHost: x\r\nAuthorization: Basic {H}\r\nConnection: close\r\n\r\n".encode()
            )
            buf = b""
            while b"}" not in buf:
                d = s.recv(512)
                if not d:
                    break
                buf += d
            s.close()
            return json.loads(buf.split(b"\r\n\r\n", 1)[1])["value"]
        except Exception:  # noqa: BLE001
            time.sleep(2)
    return None


def heap_settled() -> tuple:
    time.sleep(SETTLE)
    return get("/sensor/Heap%20free"), get("/sensor/Uptime")


def bad_once() -> str:
    try:
        s = socket.create_connection((HOST, 80), timeout=5)
        s.sendall(
            f"GET / HTTP/1.1\r\nHost: x\r\nAuthorization: {BAD}\r\nConnection: close\r\n\r\n".encode()
        )
        d = s.recv(64)
        s.close()
        return d.split(b" ", 2)[1].decode() if d.startswith(b"HTTP") else "closed"
    except OSError as e:
        return type(e).__name__


h, u = heap_settled()
print(time.strftime("[%H:%M:%S]"), f"idle heap {h} uptime {u}", flush=True)
for r in range(ROUNDS):
    stop = time.monotonic() + CONC
    codes: dict = {}
    lock = threading.Lock()
    sse = {"states": 0, "opens": 0, "closes": 0, "errors": 0, "maxgap": 0.0}

    def hammer(idx: int, stop: float = stop, lock=lock, codes: dict = codes) -> None:
        step = THREADS / RATE if RATE > 0 else 0.0
        nxt = time.monotonic() + (idx * step / THREADS if step else 0.0)
        while time.monotonic() < stop:
            if step:
                if (wait := nxt - time.monotonic()) > 0:
                    time.sleep(wait)
                nxt += step
                if time.monotonic() >= stop:
                    break
            k = bad_once()
            with lock:
                codes[k] = codes.get(k, 0) + 1

    def stream(stop: float = stop, sse: dict = sse) -> None:
        last = time.monotonic()
        while time.monotonic() < stop:
            try:
                s = socket.create_connection((HOST, 80), timeout=10)
                s.sendall(
                    f"GET /events HTTP/1.1\r\nHost: x\r\nAuthorization: Basic {H}\r\n\r\n".encode()
                )
                sse["opens"] += 1
                s.settimeout(1)
                while time.monotonic() < stop:
                    try:
                        d = s.recv(4096)
                    except TimeoutError:
                        continue
                    if not d:
                        sse["closes"] += 1
                        break
                    n = d.count(b"event: state")
                    if n:
                        now = time.monotonic()
                        sse["maxgap"] = max(sse["maxgap"], now - last)
                        last = now
                    sse["states"] += n
                s.close()
            except OSError:
                sse["errors"] += 1
                time.sleep(0.5)

    ts = [threading.Thread(target=stream)] + [
        threading.Thread(target=hammer, args=(i,)) for i in range(THREADS)
    ]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    h, u = heap_settled()
    print(
        time.strftime("[%H:%M:%S]"),
        f"round {r}: bad {codes} sse {sse['states']} states, opens {sse['opens']}, closes {sse['closes']}, errors {sse['errors']}, max gap {sse['maxgap']:.1f}s; heap after {SETTLE:.0f}s "
        f"settle {h} uptime {u}",
        flush=True,
    )
