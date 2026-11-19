"""neverconn.py HOST N [rst|head]: open N /events connections that never complete the SSE connect.

rst (default) sends the GET and resets at once (SO_LINGER 0), before the response head is ACKed, after one probe
GET confirms /events answers HTTP/1.1 200 (a refused path would make every reset count); head reads the
status line of the response head, then resets. Prints the counts of completed GETs (head: with a 200 status) and of
each failure; exits 1 when fewer than MIN_OK (default 90%) of the N connections completed.
env: MIN_OK (fraction)
"""

from collections import Counter
import os
import socket
import struct
import sys
import time

host, n = sys.argv[1], int(sys.argv[2])
mode = sys.argv[3] if len(sys.argv) > 3 else "rst"
ok = 0
errs: Counter[str] = Counter()
if mode == "rst":
    try:
        with socket.create_connection((host, 80), timeout=3) as p:
            p.sendall(
                b"GET /events HTTP/1.1\r\nHost: x\r\nAccept: text/event-stream\r\n\r\n"
            )
            got = b""
            while len(got) < 12 and (d := p.recv(16 - len(got))):
                got += d
    except OSError as e:
        sys.exit(f"FAIL: the /events probe failed: {e}")
    if not got.startswith(b"HTTP/1.1 200"):
        sys.exit(
            f"FAIL: /events answered {got!r}, so the resets would not reach the SSE path"
        )
for _ in range(n):
    s = socket.socket()
    s.settimeout(3)
    step = "connect"
    try:
        s.connect((host, 80))
        step = "send"
        s.sendall(
            b"GET /events HTTP/1.1\r\nHost: x\r\nAccept: text/event-stream\r\n\r\n"
        )
        if mode == "head":
            step = "recv"
            got = b""
            while len(got) < 12 and (d := s.recv(16 - len(got))):
                got += d
            if not got:
                errs["recv_eof"] += 1
                continue
            # a refused stream (401, 503) never ran the SSE path under test
            if not got.startswith(b"HTTP/1.1 200"):
                errs["status"] += 1
                continue
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        ok += 1
    except OSError:
        errs[step] += 1
    finally:
        s.close()
        time.sleep(0.15)
print(f"completed={ok}/{n} failed={dict(errs)}")
if ok < n * float(os.environ.get("MIN_OK", "0.9")):
    sys.exit(f"FAIL: only {ok}/{n} {mode} connections completed")
