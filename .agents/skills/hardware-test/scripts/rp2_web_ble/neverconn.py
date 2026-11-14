"""neverconn.py HOST N [rst|head]: open N /events connections that never complete the SSE connect.

rst (default) sends the GET and resets at once (SO_LINGER 0), before the response head is ACKed; head reads the
first bytes of the response head, then resets. Prints the counts of completed GETs (head: with response bytes) and of
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
            if not s.recv(16):
                errs["recv_eof"] += 1
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
