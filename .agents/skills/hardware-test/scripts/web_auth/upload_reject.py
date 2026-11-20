"""Unauthenticated and wrong-hash /update uploads: status seen, uptime and heap after each.

usage: upload_reject.py HOST USER PASS [N] [KB]
"""

import os
import pathlib
import socket
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import authfuzz as af  # noqa: E402

HOST, U, P = sys.argv[1:4]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 5
KB = int(sys.argv[5]) if len(sys.argv) > 5 else 256
af.configure(HOST, U, P)


# the statuses authfuzz.case(..., "reject") accepts; no status (close or reset) counts only if the device stays up
REJECT = (400, 401, 413, 431)


def upload(auth: str | None) -> tuple[int | None, str]:
    """(HTTP status or None, text for the log)."""
    body, ctype = af.mp_body(os.urandom(KB * 1024))
    h = f"POST /update HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: {ctype}\r\n"
    if auth:
        h += f"Authorization: {auth}\r\n"
    h += f"Content-Length: {len(body)}\r\n\r\n"
    s = socket.create_connection((HOST, 80), timeout=20)
    sent = 0
    status = None
    code = None
    try:
        s.sendall(h.encode())
        for i in range(0, len(body), 4096):
            s.sendall(body[i : i + 4096])
            sent = i + 4096
    except OSError as e:
        status = f"{type(e).__name__} after {sent // 1024} KB"
    try:
        d = s.recv(256)
        line = d.split(b"\r\n", 1)[0].decode("latin1") if d else "closed"
        if (
            line.startswith("HTTP/")
            and len(parts := line.split()) > 1
            and parts[1].isdigit()
        ):
            code = int(parts[1])
        status = f"{line} ({status})" if status else line
    except OSError as e:
        status = status or type(e).__name__
    s.close()
    return code, status


fails = 0
for i in range(N):
    for name, auth in (
        ("noauth", None),
        ("wronghash", af.BAD),
    ):
        u0 = af.uptime(10)
        code, st = upload(auth)
        time.sleep(3)
        u1, hp = af.uptime(10), af.heap(10)
        rb = "REBOOT" if u0 is None or u1 is None or u1 < u0 else "ok"
        accepted = code is not None and code not in REJECT
        fails += rb != "ok" or hp is None or accepted
        print(
            time.strftime("[%H:%M:%S]"),
            f"{i} {name} {KB}KB -> {st}; heap {hp} {rb}{' NOT REJECTED' if accepted else ''}",
            flush=True,
        )
print(
    "PASS"
    if not fails
    else f"FAIL: {fails} uploads not rejected, or followed by a reboot or no heap reading",
    flush=True,
)
sys.exit(1 if fails else 0)
