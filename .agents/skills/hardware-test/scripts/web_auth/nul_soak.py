"""NUL byte in request headers, N requests per case, uptime checked after each request.

usage: nul_soak.py HOST USER PASS [N] [CASE,...] [MAXREB]
Cases: auth (Basic <hash>\\0garbage), other (NUL in another header), reqline (NUL in the request line),
split (header sent in two TCP segments, the NUL first in the second). Cases run in blocks, so a reboot that
lands a second late still counts against the right case. Prints one line per reboot and a summary per case.
"""

import pathlib
import socket
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import authfuzz as af  # noqa: E402

HOST, U, P = sys.argv[1:4]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 200
# A case stops after MAXREB reboots: a build that reboots every time would otherwise take hours
MAXREB = int(sys.argv[6]) if len(sys.argv) > 6 else 10
af.configure(HOST, U, P)
GOOD = af.GOOD.encode()


def uptime(tries: int = 30) -> float | None:
    return None if (u := af.uptime(tries)) is None else float(u)


def send(parts: list[bytes]) -> str:
    try:
        s = socket.create_connection((HOST, 80), timeout=4)
    except OSError as e:
        return f"connect {type(e).__name__}"
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    try:
        for i, part in enumerate(parts):
            if i:
                time.sleep(0.08)
            s.sendall(part)
        d = s.recv(128)
        return d.split(b"\r\n", 1)[0].decode("latin1") if d else "closed"
    except OSError as e:
        return type(e).__name__
    finally:
        s.close()


CASES = {
    "auth": [
        b"GET / HTTP/1.1\r\nHost: x\r\nAuthorization: "
        + GOOD
        + b"\x00garbage\r\nConnection: close\r\n\r\n"
    ],
    "other": [b"GET / HTTP/1.1\r\nHost: x\r\nX-A: a\x00b\r\nConnection: close\r\n\r\n"],
    "reqline": [b"GET /\x00x HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"],
    "split": [
        b"GET / HTTP/1.1\r\nHost: x\r\nAuthorization: " + GOOD,
        b"\x00garbage\r\nConnection: close\r\n\r\n",
    ],
}
names = sys.argv[5].split(",") if len(sys.argv) > 5 else list(CASES)
for name in names:
    reboots = 0
    results: dict = {}
    u0 = uptime()
    t0 = time.monotonic()
    for i in range(N):
        r = send(CASES[name])
        results[r] = results.get(r, 0) + 1
        time.sleep(0.3)
        u1 = uptime(3)
        if u1 is None or u0 is None or u1 < u0:
            time.sleep(2)
            u1 = uptime()
            if u1 is None or u0 is None or u1 < u0:
                reboots += 1
                print(
                    time.strftime("[%H:%M:%S]"),
                    f"REBOOT {name} request {i}: uptime {u0} -> {u1}",
                    flush=True,
                )
        u0 = u1
        if reboots >= MAXREB:
            break
    sent = i + 1
    print(
        time.strftime("[%H:%M:%S]"),
        f"CASE {name}: {sent} requests in {time.monotonic() - t0:.0f}s, reboots {reboots}, "
        f"answers {results}",
        flush=True,
    )
