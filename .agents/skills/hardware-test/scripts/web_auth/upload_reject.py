"""Unauthenticated and wrong-hash /update uploads: status seen, uptime and heap after each.

usage: upload_reject.py HOST USER PASS [N] [KB]
"""

import base64
import json
import os
import socket
import sys
import time

HOST, U, P = sys.argv[1:4]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 5
KB = int(sys.argv[5]) if len(sys.argv) > 5 else 256
H = base64.b64encode(f"{U}:{P}".encode()).decode()


def val(path: str) -> object:
    for _ in range(10):
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


def upload(auth: str | None) -> str:
    b = "----hwtestb"
    body = (
        (
            f'--{b}\r\nContent-Disposition: form-data; name="update"; filename="firmware.bin"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n"
        ).encode()
        + os.urandom(KB * 1024)
        + f"\r\n--{b}--\r\n".encode()
    )
    h = f"POST /update HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: multipart/form-data; boundary={b}\r\n"
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
        ("wronghash", "Basic " + H[:-1] + ("B" if H[-1] != "B" else "C")),
    ):
        u0 = val("/sensor/Uptime")
        st = upload(auth)
        time.sleep(3)
        u1, hp = val("/sensor/Uptime"), val("/sensor/Heap%20free")
        rb = "REBOOT" if u0 is None or u1 is None or u1 < u0 else "ok"
        print(
            time.strftime("[%H:%M:%S]"),
            f"{i} {name} {KB}KB -> {st}; heap {hp} {rb}",
            flush=True,
        )
