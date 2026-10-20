"""SSE client(s) for a web_server /events stream: counts state events and checks each is valid JSON.

usage: sse.py HOST SECONDS [--clients N] [--stall] [--churn CYCLES]
--stall opens one extra connection that never reads (the device's socket buffer fills); --churn opens
and abruptly closes a connection CYCLES times, 2 s apart, next to the reading clients.
"""

import argparse
from collections import Counter
import contextlib
import json
import socket
import sys
import threading
import time

ap = argparse.ArgumentParser()
ap.add_argument("host")
ap.add_argument("seconds", type=float)
ap.add_argument("--clients", type=int, default=1)
ap.add_argument("--stall", action="store_true")
ap.add_argument("--churn", type=int, default=0)
args = ap.parse_args()

REQ = b"GET /events HTTP/1.1\r\nHost: %s\r\nAccept: text/event-stream\r\nConnection: keep-alive\r\n\r\n"


def read_chunked(sock, deadline, on_line):
    """Decode HTTP chunked transfer encoding and feed complete event-stream lines to on_line."""
    buf = b""
    body = b""
    headers_done = False
    sock.settimeout(1.0)
    while time.monotonic() < deadline:
        try:
            data = sock.recv(65536)
        except TimeoutError:
            continue
        if not data:
            return "closed"
        buf += data
        if not headers_done:
            i = buf.find(b"\r\n\r\n")
            if i < 0:
                continue
            headers_done = True
            buf = buf[i + 4 :]
        while True:
            i = buf.find(b"\r\n")
            if i < 0:
                break
            size = int(buf[:i].split(b";")[0], 16)
            if len(buf) < i + 2 + size + 2:
                break
            body += buf[i + 2 : i + 2 + size]
            buf = buf[i + 2 + size + 2 :]
            if size == 0:
                return "closed"
        while b"\n" in body:
            line, body = body.split(b"\n", 1)
            on_line(line.rstrip(b"\r"))
    return "deadline"


class Client(threading.Thread):
    def __init__(self, idx, seconds):
        super().__init__(daemon=True)
        self.idx = idx
        self.seconds = seconds
        self.events = Counter()
        self.ids = Counter()
        self.invalid = 0
        self.max_data = 0
        self.bytes = 0
        self.end = "?"
        self.cur_event = None

    def on_line(self, line):
        self.bytes += len(line) + 1
        if line.startswith(b"event:"):
            self.cur_event = line[6:].strip().decode()
        elif line.startswith(b"data:"):
            ev = self.cur_event or "message"
            self.events[ev] += 1
            data = line[5:].strip()
            self.max_data = max(self.max_data, len(data))
            if ev in ("state", "state_detail_all"):
                try:
                    obj = json.loads(data)
                    self.ids[obj.get("id", "?")] += 1
                except ValueError:
                    self.invalid += 1
        elif line == b"":
            self.cur_event = None

    def run(self):
        try:
            s = socket.create_connection((args.host, 80), timeout=5)
            s.sendall(REQ % args.host.encode())
            self.end = read_chunked(s, time.monotonic() + self.seconds, self.on_line)
            s.close()
        except (OSError, ValueError) as e:
            self.end = f"error: {e}"


clients = [Client(i, args.seconds) for i in range(args.clients)]
for c in clients:
    c.start()
    time.sleep(0.3)

stall_sock = None
if args.stall:
    # The window scale is negotiated in the SYN, so the small buffer must be set before connect()
    stall_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    stall_sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
    stall_sock.settimeout(5)
    stall_sock.connect((args.host, 80))
    stall_sock.sendall(REQ % args.host.encode())
    print("stalled client opened (never reads)", flush=True)

churn_done = 0
for _ in range(args.churn):
    try:
        cs = socket.create_connection((args.host, 80), timeout=5)
        cs.sendall(REQ % args.host.encode())
        cs.settimeout(1.0)
        with contextlib.suppress(TimeoutError):
            cs.recv(4096)
        cs.setsockopt(
            socket.SOL_SOCKET, socket.SO_LINGER, b"\x01\x00\x00\x00\x00\x00\x00\x00"
        )  # RST on close
        cs.close()
        churn_done += 1
    except OSError as e:
        print("churn error", e, flush=True)
    time.sleep(2)
if args.churn:
    print(f"churn: {churn_done}/{args.churn} abrupt closes", flush=True)

for c in clients:
    c.join()
if stall_sock is not None:
    try:
        stall_sock.settimeout(0.5)
        peek = stall_sock.recv(1)
        print(
            "stalled client: still open, had data"
            if peek
            else "stalled client: closed by device",
            flush=True,
        )
    except TimeoutError:
        print("stalled client: still open, no data read", flush=True)
    except OSError as e:
        print("stalled client: closed by device", e, flush=True)
    stall_sock.close()

for c in clients:
    states = c.events["state"]
    print(
        f"client {c.idx}: {c.end}, {states} state events, {c.invalid} invalid JSON, max data {c.max_data} B, "
        f"{c.bytes} B, other events { {k: v for k, v in c.events.items() if k != 'state'} }",
        flush=True,
    )
    print("  per id:", dict(sorted(c.ids.items())), flush=True)

failed = [
    c for c in clients if c.end != "deadline" or c.invalid or not c.events["state"]
]
if failed:
    sys.exit(
        f"{len(failed)} client(s) did not run clean to the deadline with valid state events"
    )
