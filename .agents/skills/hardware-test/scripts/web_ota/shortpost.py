"""Multipart POST of a firmware image to http://HOST/update, shaped like `curl -F file=@IMAGE`, with damage.
Usage: shortpost.py HOST[:PORT] IMAGE MODE [CUT] [OFFSET]

MODE full     whole body, correct Content-Length
     close    correct Content-Length, stop CUT bytes before the end of the file part, close at once
     hold     same as close but keep the socket open, sending nothing, until the server closes (max 600 s)
     dropmid  correct (full) Content-Length, CUT bytes removed from the file at OFFSET, closing boundary still
              sent: what a lossy relay produces
     flip     same length, one bit flipped at OFFSET
     shortcl  as dropmid, but Content-Length matches the shorter body (a truncated file uploaded cleanly)
CUT defaults to 7168 bytes, OFFSET to the middle of the image; a negative OFFSET counts from the end."""

from pathlib import Path
import socket
import sys
import time

host, img, mode = sys.argv[1], sys.argv[2], sys.argv[3]
addr, _, port = host.partition(":")
cut = int(sys.argv[4]) if len(sys.argv) > 4 else 7168
off = int(sys.argv[5], 0) if len(sys.argv) > 5 else None
data = Path(img).read_bytes()
pos = len(data) // 2 if off is None else off % len(data)
b = "------------------------e3b0c44298fc1c14"
head = (
    f'--{b}\r\nContent-Disposition: form-data; name="file"; filename="firmware.ota.bin"\r\n'
    f"Content-Type: application/octet-stream\r\n\r\n"
).encode()
tail = f"\r\n--{b}--\r\n".encode()
full_len = len(head) + len(data) + len(tail)
if mode == "full":
    body, clen = head + data + tail, full_len
elif mode in ("close", "hold"):
    body, clen = head + data[: len(data) - cut], full_len
elif mode in ("dropmid", "shortcl"):
    body = head + data[:pos] + data[pos + cut :] + tail
    clen = len(body) if mode == "shortcl" else full_len
elif mode == "flip":
    bad = bytearray(data)
    bad[pos] ^= 0x01
    body, clen = head + bytes(bad) + tail, full_len
else:
    sys.exit(f"bad mode {mode}")
req = (
    f"POST /update HTTP/1.1\r\nHost: {addr}\r\nUser-Agent: curl/8.7.1\r\nAccept: */*\r\n"
    f"Content-Length: {clen}\r\nContent-Type: multipart/form-data; boundary={b}\r\n\r\n"
).encode()
file_sent = len(body) - len(head) - (len(tail) if mode not in ("close", "hold") else 0)
print(
    f"{time.strftime('%T')} mode={mode} image={len(data)} file_bytes_sent={file_sent} "
    f"body_sent={len(body)} content_length={clen} cut={cut} offset={off}",
    flush=True,
)
s = socket.create_connection((addr, int(port or 80)), timeout=30)
s.sendall(req + body)
print(f"{time.strftime('%T')} body sent", flush=True)
if mode == "close":
    s.close()
    print(f"{time.strftime('%T')} closed", flush=True)
    sys.exit(0)
s.settimeout(600 if mode == "hold" else 120)
t0 = time.time()
resp = b""
try:
    while r := s.recv(4096):
        resp += r
except OSError as e:
    print(f"{time.strftime('%T')} recv ended: {e!r}", flush=True)
print(
    f"{time.strftime('%T')} after {time.time() - t0:.1f}s server reply: {resp!r}",
    flush=True,
)
