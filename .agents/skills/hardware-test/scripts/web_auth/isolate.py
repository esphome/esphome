"""Send one hostile request at a time and check uptime afterwards, to tie a reboot to a request.

usage: isolate.py HOST USER PASS [ROUNDS]   (prints one line per case: uptime before/after, rebooted yes/no)
"""

import base64
import json
import socket
import sys
import time

HOST, U, P = sys.argv[1:4]
ROUNDS = int(sys.argv[4]) if len(sys.argv) > 4 else 1
GOOD = "Basic " + base64.b64encode(f"{U}:{P}".encode()).decode()


def send(req: bytes, timeout: float = 6) -> str:
    try:
        s = socket.create_connection((HOST, 80), timeout=timeout)
    except OSError as e:
        return f"connect {type(e).__name__}"
    try:
        s.sendall(req)
        d = s.recv(256)
        return d.split(b"\r\n", 1)[0].decode("latin1") if d else "closed"
    except OSError as e:
        return type(e).__name__
    finally:
        s.close()


def uptime() -> float | None:
    for _ in range(25):
        try:
            s = socket.create_connection((HOST, 80), timeout=3)
            s.sendall(
                f"GET /sensor/Uptime HTTP/1.1\r\nHost: x\r\nAuthorization: {GOOD}\r\nConnection: close\r\n\r\n".encode()
            )
            buf = b""
            while b"}" not in buf:
                d = s.recv(512)
                if not d:
                    break
                buf += d
            s.close()
            return float(json.loads(buf.split(b"\r\n\r\n", 1)[1])["value"])
        except Exception:  # noqa: BLE001
            time.sleep(2)
    return None


def req(extra: bytes, auth: bytes | None = None) -> bytes:
    h = b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
    if auth is not None:
        h += b"Authorization: " + auth + b"\r\n"
    return h + extra + b"\r\n"


CASES = {
    "nul_in_auth": req(b"", GOOD.encode() + b"\x00garbage"),
    "nul_in_other_header": req(b"X-A: a\x00b\r\n"),
    "nul_in_request_line": b"GET /\x00 HTTP/1.1\r\nHost: x\r\n\r\n",
    "auth_8k": req(b"", b"Basic " + b"A" * 8192),
    "garbage_32k": req(b"X-Garbage: " + b"Z" * 32768 + b"\r\n"),
    "nonl_64k": b"GET / HTTP/1.1\r\nHost: x\r\nX-G: " + b"Q" * 65536,
    "headers_200": req(
        b"".join(f"X-H{i}: {'v' * 60}\r\n".encode() for i in range(200))
    ),
    "control": req(b"", GOOD.encode()),
}
only = sys.argv[5].split(",") if len(sys.argv) > 5 else list(CASES)
for r in range(ROUNDS):
    for name in only:
        u0 = uptime()
        t = time.strftime("%H:%M:%S")
        res = send(CASES[name])
        time.sleep(4)
        u1 = uptime()
        rb = "REBOOT" if (u0 is None or u1 is None or u1 < u0) else "ok"
        print(
            f"[{t}] round {r} {name:22} -> {res[:40]:40} uptime {u0} -> {u1} {rb}",
            flush=True,
        )
