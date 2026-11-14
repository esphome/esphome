"""/events stream checks for the web_server_idf stream rework (esphome#21973), against scripts/web_server/stream-dut.yaml.

usage: stream_retest.py HOST MODE [--out FILE] [--runs N]
  head      save the raw response bytes of the first 3 s of GET /events to --out (compare two builds with
            head_compare.py)
  multi     4 clients for 20 s: state events and longest gap per client
  stall     a reader plus a client with a 2 KB receive buffer that reads 3 s, stops 35 s, then drains and
            checks for EOF; the device log line gives the close time
  wifidrop  a client streams, POST /button/Wifi Drop/press (disable 8 s, enable), the client reconnects after
            5 s without data like a browser EventSource; reports how the old stream ended and when a new one worked
  churn     40 connects (every second one reset with SO_LINGER 0 after the first state event, the others closed
            normally), then a fresh client for 10 s
Each mode prints a `MODE ... ok=True/False` summary line.
"""

import argparse
import contextlib
import pathlib
import socket
import struct
import threading
import time

ap = argparse.ArgumentParser()
ap.add_argument("host")
ap.add_argument("mode", choices=["head", "multi", "stall", "wifidrop", "churn"])
ap.add_argument("--out")
ap.add_argument("--runs", type=int, default=1)
args = ap.parse_args()
H = args.host
REQ = (
    b"GET /events HTTP/1.1\r\nHost: %s\r\nAccept: text/event-stream\r\n\r\n"
    % H.encode()
)


class Reader(threading.Thread):
    """Counts `event: state` lines and the longest gap between received bytes."""

    def __init__(self, seconds, rcvbuf=None, idle_reconnect=None):
        super().__init__(daemon=True)
        self.seconds, self.rcvbuf, self.idle_reconnect = seconds, rcvbuf, idle_reconnect
        self.states = 0
        self.max_gap = 0.0
        self.end = "deadline"
        self.raw = b""
        self.sock = None

    def connect(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if self.rcvbuf:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, self.rcvbuf)
        s.settimeout(5)
        s.connect((H, 80))
        s.sendall(REQ)
        s.settimeout(0.5)
        self.sock = s
        return s

    def run(self):
        try:
            s = self.connect()
        except OSError as e:
            self.end = f"connect error {e}"
            return
        deadline = time.monotonic() + self.seconds
        last = time.monotonic()
        while time.monotonic() < deadline:
            try:
                d = s.recv(65536)
            except TimeoutError:
                if (
                    self.idle_reconnect
                    and time.monotonic() - last > self.idle_reconnect
                ):
                    self.end = "idle"
                    return
                continue
            except OSError as e:
                self.end = f"error {e.__class__.__name__}"
                return
            now = time.monotonic()
            if not d:
                self.end = "eof"
                return
            self.max_gap = max(self.max_gap, now - last)
            last = now
            if len(self.raw) < 200000:
                self.raw += d
            self.states += d.count(b"event: state")
        s.close()


def summary(mode, ok, text):
    print(f"{mode} {text} ok={ok}", flush=True)


if args.mode == "head":
    r = Reader(3)
    r.start()
    r.join()
    with pathlib.Path(args.out).open("wb") as f:
        f.write(r.raw)
    summary("head", r.states > 0, f"{len(r.raw)} B, {r.states} state events")

elif args.mode == "multi":
    for run in range(args.runs):
        rs = [Reader(20) for _ in range(4)]
        for r in rs:
            r.start()
            time.sleep(0.2)
        for r in rs:
            r.join()
        ok = all(r.end == "deadline" and r.states > 500 for r in rs)
        summary(
            "multi",
            ok,
            f"run {run + 1}: "
            + ", ".join(f"{r.states} ev gap {r.max_gap:.1f}s" for r in rs),
        )
        time.sleep(3)

elif args.mode == "stall":
    for run in range(args.runs):
        reader = Reader(45)
        reader.start()
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
        s.settimeout(5)
        s.connect((H, 80))
        s.sendall(REQ)
        s.settimeout(0.5)
        t_end = time.monotonic() + 3
        while time.monotonic() < t_end:
            with contextlib.suppress(TimeoutError):
                s.recv(4096)
        t_stop = time.time()
        print(
            f"stall run {run + 1}: stopped reading at {time.strftime('%H:%M:%S', time.localtime(t_stop))}",
            flush=True,
        )
        time.sleep(35)
        s.settimeout(2)
        closed = False
        drain_end = time.monotonic() + 5
        try:
            while time.monotonic() < drain_end:
                if not s.recv(65536):
                    closed = True
                    break
        except TimeoutError:
            pass
        except OSError:
            closed = True
        s.close()
        reader.join()
        ok = closed and reader.end == "deadline" and reader.max_gap < 5
        summary(
            "stall",
            ok,
            f"run {run + 1}: stalled closed={closed}, reader {reader.states} ev "
            f"gap {reader.max_gap:.1f}s end={reader.end}",
        )
        time.sleep(3)

elif args.mode == "wifidrop":
    r = Reader(60, idle_reconnect=5)
    r.start()
    time.sleep(3)
    pre = r.states
    try:
        b = socket.create_connection((H, 80), timeout=5)
        b.sendall(
            b"POST /button/Wifi%20Drop/press HTTP/1.1\r\nHost: "
            + H.encode()
            + b"\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
        )
        b.settimeout(3)
        print("press reply:", b.recv(200).split(b"\r\n")[0], flush=True)
        b.close()
    except OSError as e:
        print("press error", e, flush=True)
    t_press = time.monotonic()
    r.join()
    print(
        f"old stream: {pre} events before the press, ended {r.end} after {time.monotonic() - t_press:.1f} s, "
        f"{r.states} events total",
        flush=True,
    )
    t_new = None
    nr = None
    while time.monotonic() - t_press < 90:
        nr = Reader(5)
        nr.start()
        nr.join()
        if nr.states > 50:
            t_new = time.monotonic() - t_press
            break
        time.sleep(1)
    summary(
        "wifidrop",
        t_new is not None and r.end != "deadline",
        f"old end={r.end}, new stream {nr.states if nr else 0} ev in 5 s at {t_new and round(t_new, 1)} s after "
        f"the press",
    )

elif args.mode == "churn":
    made = resets = 0
    for i in range(40):
        try:
            c = socket.create_connection((H, 80), timeout=5)
            c.sendall(REQ)
            c.settimeout(3)
            got = b""
            end = time.monotonic() + 3
            while b"event: state" not in got and time.monotonic() < end:
                try:
                    got += c.recv(4096)
                except TimeoutError:
                    break
            if i % 2:
                c.setsockopt(
                    socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0)
                )
                resets += 1
            c.close()
            made += b"event: state" in got
        except OSError as e:
            print("churn error", i, e, flush=True)
        time.sleep(0.3)
    time.sleep(2)
    r = Reader(10)
    r.start()
    r.join()
    summary(
        "churn",
        made == 40 and r.states > 200 and r.end == "deadline",
        f"{made}/40 got a state event, {resets} reset; after: {r.states} ev in 10 s gap {r.max_gap:.1f}s "
        f"end={r.end}",
    )
