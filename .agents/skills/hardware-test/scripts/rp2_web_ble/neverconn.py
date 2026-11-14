"""neverconn.py HOST N [rst|head]: open N /events connections that never complete the SSE connect.

rst (default) sends the GET and resets at once (SO_LINGER 0), before the response head is ACKed; head reads the
first bytes of the response head, then resets. Prints how many TCP connects succeeded.
"""

import socket
import struct
import sys
import time

host, n = sys.argv[1], int(sys.argv[2])
mode = sys.argv[3] if len(sys.argv) > 3 else "rst"
ok = 0
for _ in range(n):
    s = socket.socket()
    s.settimeout(3)
    try:
        s.connect((host, 80))
        ok += 1
        s.sendall(
            b"GET /events HTTP/1.1\r\nHost: x\r\nAccept: text/event-stream\r\n\r\n"
        )
        if mode == "head":
            s.recv(16)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    except OSError:
        pass
    finally:
        s.close()
    time.sleep(0.15)
print(f"connects={ok}/{n}")
