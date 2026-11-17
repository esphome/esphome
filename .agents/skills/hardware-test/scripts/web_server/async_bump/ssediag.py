"""ssediag.py HOST SECONDS [PAGE_S]: optional page-load phase, then SSE with a REST poll every 10 s.
Prints per 10 s: SSE fast-counter events, latest SSE counter value vs the REST value (lag), REST uptime."""

import json
import pathlib
import socket
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import rest  # noqa: E402

host, secs = sys.argv[1], float(sys.argv[2])
page_s = float(sys.argv[3]) if len(sys.argv) > 3 else 0
rest.HOST = host


end = time.monotonic() + page_s
while time.monotonic() < end:
    for p in ("/", "/metrics", "/sensor/Fast%20counter", "/light/Test%20light"):
        rest.req(p, timeout=8)
    time.sleep(4)
state = {"n": 0, "last": None, "ended": False}


def sse():
    """Reader thread; an error or a device close is printed and flagged, so it never reads as a stall."""
    try:
        sse_read()
    except Exception as e:  # noqa: BLE001
        print(f"SSE FAILED {type(e).__name__}: {e}", flush=True)
        state["ended"] = True


def sse_read():
    s = socket.create_connection((host, 80), timeout=10)
    s.sendall(
        f"GET /events HTTP/1.1\r\nHost: {host}\r\nAccept: text/event-stream\r\n\r\n".encode()
    )
    s.settimeout(1)
    buf = b""
    ev = None
    stop = time.monotonic() + secs
    while time.monotonic() < stop:
        try:
            d = s.recv(8192)
        except TimeoutError:
            continue
        if not d:
            print("closed", flush=True)
            state["ended"] = True
            return
        buf += d
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.rstrip(b"\r")
            if line.startswith(b"event: "):
                ev = line[7:]
            elif (
                line.startswith(b"data: ")
                and ev == b"state"
                and b"Fast counter" in line
            ):
                state["n"] += 1
                state["last"] = json.loads(line[6:]).get("value")


t = threading.Thread(target=sse, daemon=True)
t.start()
t0 = time.monotonic()
prev = 0
tot = []
while time.monotonic() - t0 < secs:
    time.sleep(10)
    rv = rest.jv("/sensor/Fast%20counter", timeout=8)
    up = rest.jv("/sensor/Uptime", timeout=8)
    n = state["n"] - prev
    prev = state["n"]
    tot.append(n)
    lag = (rv - state["last"]) if rv is not None and state["last"] is not None else None
    print(
        f"t={time.monotonic() - t0:4.0f}s sse fast events {n:3d}  sse last {state['last']}  rest {rv}  lag {lag}  uptime {up}{'  SSE ENDED' if state['ended'] else ''}",
        flush=True,
    )
print(
    f"TOTAL sse fast events {sum(tot)} in {secs:.0f}s ({sum(tot) / secs:.1f}/s)"
    + (" (SSE ended early)" if state["ended"] else "")
)
# an SSE stream that ended early or carried nothing is not a measurement
sys.exit(1 if state["ended"] or not sum(tot) else 0)
