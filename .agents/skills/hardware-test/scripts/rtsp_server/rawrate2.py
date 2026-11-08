import contextlib
import socket
import sys
import time

IP = sys.argv[1]
mode = sys.argv[2]
secs = 8
s = socket.create_connection((IP, 554), timeout=5)
if mode == "bigbuf":
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
cseq = 0


buf = b""


def read_response() -> bytes:
    """One complete RTSP response: headers through the blank line plus a Content-Length body."""
    global buf  # noqa: PLW0603
    while (end := buf.find(b"\r\n\r\n")) < 0:
        chunk = s.recv(4096)
        if not chunk:
            raise ConnectionError("closed")
        buf += chunk
    head = buf[:end].decode(errors="replace")
    body_len = 0
    for line in head.split("\r\n"):
        if line.lower().startswith("content-length:"):
            body_len = int(line.split(":", 1)[1])
    while len(buf) < end + 4 + body_len:
        chunk = s.recv(4096)
        if not chunk:
            raise ConnectionError("closed")
        buf += chunk
    resp, buf = buf[: end + 4 + body_len], buf[end + 4 + body_len :]
    return resp


def req(m, extra="", wait=True):
    global cseq  # noqa: PLW0603
    cseq += 1
    url = f"rtsp://{IP}:554/trackID=0" if m == "SETUP" else f"rtsp://{IP}:554/"
    s.sendall(f"{m} {url} RTSP/1.0\r\nCSeq: {cseq}\r\n{extra}\r\n".encode())
    return read_response() if wait else b""


req("OPTIONS")
req("DESCRIBE", "Accept: application/sdp\r\n")
r = req("SETUP", "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n")
sess = (
    [x for x in r.decode(errors="replace").split("\r\n") if x.startswith("Session")][0]
    .split()[1]
    .split(";")[0]
)
req("PLAY", f"Session: {sess}\r\n")
t0 = time.monotonic()
total = 0
chunks = 0
last_chat = t0
s.settimeout(2)
while time.monotonic() - t0 < secs:
    now = time.monotonic()
    if mode == "chatty" and now - last_chat > 0.2:
        req("GET_PARAMETER", f"Session: {sess}\r\n", wait=False)
        last_chat = now
    if mode == "rtcp" and now - last_chat > 0.2:
        s.sendall(b"$\x01\x00\x08" + b"\x80\xc9\x00\x01\x00\x00\x00\x01")
        last_chat = now
    try:
        d = s.recv(65536)
    except TimeoutError:
        continue
    if not d:
        break
    total += len(d)
    chunks += 1
print(
    f"{mode:7s}: {total / 1024 / secs:5.1f} KiB/s, {chunks} recvs, avg {total // max(chunks, 1)} B/recv"
)
with contextlib.suppress(OSError):
    req("TEARDOWN", f"Session: {sess}\r\n")
s.close()
