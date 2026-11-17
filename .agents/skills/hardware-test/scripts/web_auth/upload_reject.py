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


def upload(auth: str | None) -> str:
    body, ctype = af.mp_body(os.urandom(KB * 1024))
    h = f"POST /update HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: {ctype}\r\n"
    if auth:
        h += f"Authorization: {auth}\r\n"
    h += f"Content-Length: {len(body)}\r\n\r\n"
    s = socket.create_connection((HOST, 80), timeout=20)
    sent = 0
    status = None
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
        status = f"{line} ({status})" if status else line
    except OSError as e:
        status = status or type(e).__name__
    s.close()
    return status


for i in range(N):
    for name, auth in (
        ("noauth", None),
        ("wronghash", af.BAD),
    ):
        u0 = af.uptime(10)
        st = upload(auth)
        time.sleep(3)
        u1, hp = af.uptime(10), af.heap(10)
        rb = "REBOOT" if u0 is None or u1 is None or u1 < u0 else "ok"
        print(
            time.strftime("[%H:%M:%S]"),
            f"{i} {name} {KB}KB -> {st}; heap {hp} {rb}",
            flush=True,
        )
