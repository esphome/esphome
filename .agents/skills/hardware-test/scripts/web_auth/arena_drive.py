"""Drive the web_server event source while sampling the main task stack minimum and free heap (esphome#22699).

usage: arena_drive.py HOST USER PASS [PHASE_S]
Needs the "Stack min" and "Heap free" template sensors and the "Big select" select of arena/arena-common.yaml,
with the logger at VERY_VERBOSE (logs flood the /events streams).
Phases: idle, two /events streams, plus a new /events client every 3 s (greeting and full listing), plus select
changes every 0.5 s with a detail=all read of the select (a state over 1 KB), then all at once.
Prints per phase (the load counters cover that phase only): lowest stack minimum, heap range, uptime (resets), SSE
bytes and the connect, stream, select and detail errors. Exits 1 when a phase had a reset (uptime compared across
phases), no stack reading, a stack minimum of 0 (used to its end), or a load that mostly failed to reach the device
(marked NOT LOADED).
"""

import base64
import json
import socket
import sys
import threading
import time
import urllib.parse

HOST, U, P = sys.argv[1:4]
PHASE = float(sys.argv[4]) if len(sys.argv) > 4 else 60
H = base64.b64encode(f"{U}:{P}".encode()).decode()
stats: dict = {}
last_up: list[float | None] = [None]  # the last uptime reading, carried across phases
OPTS = [
    f"Option {i:02d} with a deliberately long label to grow the listing"
    for i in range(40)
]


def req(method: str, path: str, timeout: float = 5) -> tuple[int | None, bytes]:
    try:
        s = socket.create_connection((HOST, 80), timeout=timeout)
        s.sendall(
            f"{method} {path} HTTP/1.1\r\nHost: x\r\nAuthorization: Basic {H}\r\nConnection: close\r\n"
            f"Content-Length: 0\r\n\r\n".encode()
        )
        buf = b""
        while True:
            try:
                d = s.recv(4096)
            except TimeoutError:
                break
            if not d:
                break
            buf += d
            if b"\r\n\r\n" in buf:
                hd, bd = buf.split(b"\r\n\r\n", 1)
                cl = [
                    ln
                    for ln in hd.split(b"\r\n")
                    if ln.lower().startswith(b"content-length:")
                ]
                if cl and len(bd) >= int(cl[0].split(b":")[1]):
                    break
        s.close()
        st = int(buf.split(b" ", 2)[1]) if buf.startswith(b"HTTP") else None
        return st, buf.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in buf else b""
    # ValueError, IndexError: a truncated or malformed reply counts as a failed request, not a dead load thread
    except (OSError, ValueError, IndexError):
        return None, b""


def val(name: str) -> float | None:
    st, body = req("GET", "/sensor/" + urllib.parse.quote(name))
    # TypeError: an unknown (NaN) state is null
    try:
        return float(json.loads(body)["value"]) if st == 200 else None
    except (ValueError, KeyError, TypeError):
        return None


def stream(stop: threading.Event) -> None:
    while not stop.is_set():
        try:
            s = socket.create_connection((HOST, 80), timeout=10)
            s.sendall(
                f"GET /events HTTP/1.1\r\nHost: x\r\nAuthorization: Basic {H}\r\n\r\n".encode()
            )
            stats["stream_opens"] += 1
            s.settimeout(1)
            head = b""
            tail = b""
            while not stop.is_set():
                try:
                    d = s.recv(8192)
                except TimeoutError:
                    continue
                if not d:
                    stats["sse_closes"] += 1
                    break
                # collect the whole response head first; TCP may split even the status line
                if head is not None:
                    head += d
                    if b"\r\n\r\n" not in head:
                        continue
                    hd, _, d = head.partition(b"\r\n\r\n")
                    head = None
                    # a refused stream (401, 503) is an error, not load
                    if not hd.startswith(b"HTTP/1.1 200"):
                        stats["stream_errors"] += 1
                        time.sleep(1)
                        break
                stats["sse_bytes"] += len(d)
                # keep the tail so an event line split across two reads is still counted
                buf = tail + d
                stats["sse_states"] += buf.count(b"event: state")
                tail = buf[-11:]
            s.close()
        except OSError:
            stats["stream_errors"] += 1
            time.sleep(1)


def connector(stop: threading.Event) -> None:
    while not stop.is_set():
        try:
            s = socket.create_connection((HOST, 80), timeout=10)
            s.sendall(
                f"GET /events HTTP/1.1\r\nHost: x\r\nAuthorization: Basic {H}\r\n\r\n".encode()
            )
            s.settimeout(0.5)
            n = 0
            head = b""
            end = time.monotonic() + 2.5
            while time.monotonic() < end:
                try:
                    d = s.recv(8192)
                except TimeoutError:
                    continue
                if not d:
                    break
                # keep fragments until the status line is complete
                if b"\r\n" not in head:
                    head += d
                n += len(d)
            s.close()
            # A refused or empty answer did not run the new client listing
            if head.split(b" ", 2)[1:2] == [b"200"]:
                stats["connects"] += 1
                stats["listing_bytes"].append(n)
            else:
                stats["connect_errors"] += 1
        except OSError:
            stats["connect_errors"] += 1
        stop.wait(0.5)


def selector(stop: threading.Event) -> None:
    i = 0
    while not stop.is_set():
        st, _ = req(
            "POST",
            "/select/Big%20select/set?option=" + urllib.parse.quote(OPTS[i % 40]),
        )
        stats["select_ok" if st == 200 else "select_fail"] += 1
        if i % 4 == 0:
            st, body = req("GET", "/select/Big%20select?detail=all")
            stats["detail_bytes"] = max(stats["detail_bytes"], len(body))
            stats["detail_ok" if st == 200 and len(body) > 1024 else "detail_fail"] += 1
        i += 1
        stop.wait(0.5)


def phase(name: str, workers: list) -> bool:
    stats.update(
        sse_bytes=0,
        sse_states=0,
        sse_closes=0,
        stream_opens=0,
        stream_errors=0,
        connects=0,
        connect_errors=0,
        listing_bytes=[],
        select_ok=0,
        select_fail=0,
        detail_bytes=0,
        detail_ok=0,
        detail_fail=0,
    )
    stop = threading.Event()
    ts = [threading.Thread(target=w, args=(stop,)) for w in workers]
    for t in ts:
        t.start()
    stacks, heaps, ups = [], [], []
    end = time.monotonic() + PHASE
    while time.monotonic() < end:
        time.sleep(5)
        s, h, u = val("Stack min"), val("Heap free"), val("Uptime")
        if s is not None:
            stacks.append(s)
        if h is not None:
            heaps.append(h)
        if u is not None:
            ups.append(u)
    stop.set()
    for t in ts:
        t.join()
    chain = ([last_up[0]] if last_up[0] is not None else []) + ups
    resets = sum(1 for a, b in zip(chain, chain[1:], strict=False) if b < a)
    if ups:
        last_up[0] = ups[-1]
    lb = stats["listing_bytes"]
    loaded = not (
        (
            stream in workers
            and (
                stats["sse_states"] == 0
                or stats["stream_errors"] * 2 > stats["stream_opens"]
            )
        )
        or (connector in workers and stats["connect_errors"] >= stats["connects"])
        or (
            selector in workers
            and (
                stats["select_fail"] >= stats["select_ok"]
                or stats["detail_fail"] >= stats["detail_ok"]
            )
        )
    )
    print(
        time.strftime("[%H:%M:%S]"),
        f"PHASE {name}{'' if loaded else ' NOT LOADED'}: stack min {min(stacks) if stacks else None} B, heap "
        f"{min(heaps) if heaps else None}..{max(heaps) if heaps else None}, uptime {ups[:1]}..{ups[-1:]}, resets {resets}; "
        f"sse bytes {stats['sse_bytes']} states {stats['sse_states']}, sse closes {stats['sse_closes']}, stream opens {stats['stream_opens']} "
        f"errors {stats['stream_errors']}, connects {stats['connects']} errors {stats['connect_errors']} "
        f"(listing {min(lb) if lb else 0}..{max(lb) if lb else 0} B), select ok {stats['select_ok']} fail "
        f"{stats['select_fail']}, detail {stats['detail_bytes']} B ok {stats['detail_ok']} fail {stats['detail_fail']}",
        flush=True,
    )
    # fewer than two uptime readings cannot rule out a reset; a minimum of 0 is a stack used to its end
    return (
        loaded and bool(stacks) and min(stacks) > 0 and len(chain) >= 2 and resets == 0
    )


last_up[0] = val("Uptime")
print(
    time.strftime("[%H:%M:%S]"),
    f"start: stack min {val('Stack min')} heap {val('Heap free')} uptime {last_up[0]}",
    flush=True,
)
ok = [
    phase("idle", []),
    phase("two streams", [stream, stream]),
    phase("two streams + new clients", [stream, stream, connector]),
    phase("two streams + select over 1 KB", [stream, stream, selector]),
    phase("all", [stream, stream, connector, selector]),
]
print(
    "PASS"
    if all(ok)
    else f"FAIL: {ok.count(False)} phases with a reset, no or a 0 stack reading, or NOT LOADED",
    flush=True,
)
sys.exit(0 if all(ok) else 1)
