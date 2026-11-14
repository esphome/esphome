"""Check that a new /events client's entity listing goes only to that client (esphome#22691).

usage: sse_listing.py HOST [--rounds N] [--window S] [--out events.jsonl]
Each round opens streams 1 and 2, waits until both finished their initial listing (no `state` event with
`domain`, the detail_all marker, for 2 s after the first bare state), connects stream 3 and counts for WINDOW
seconds per stream: greeting pings (config JSON), uptime pings, sorting_group events, detail_all states and bare
states. Stream 3 must show ping, then sorting groups, then every detail_all entry before its first bare state.
Exit 0 when every round has no new detail_all on streams 1 and 2 and an ordered complete listing on stream 3.
"""

import argparse
import json
import pathlib
import socket
import sys
import threading
import time

ap = argparse.ArgumentParser()
ap.add_argument("host")
ap.add_argument("--rounds", type=int, default=3)
ap.add_argument("--window", type=float, default=10.0)
ap.add_argument("--out")
args = ap.parse_args()

REQ = b"GET /events HTTP/1.1\r\nHost: %s\r\nAccept: text/event-stream\r\n\r\n"
out = pathlib.Path(args.out).open("a", encoding="utf-8") if args.out else None  # noqa: SIM115
out_lock = threading.Lock()


def classify(ev, data):
    if ev == "ping":
        try:
            obj = json.loads(data)
        except ValueError:
            return "ping_bad"
        return "ping_uptime" if set(obj) == {"uptime"} else "ping_greeting"
    if ev == "sorting_group":
        return "sorting_group"
    if ev == "state":
        try:
            obj = json.loads(data)
        except ValueError:
            return "state_bad"
        return "detail_all" if "domain" in obj else "state"
    return ev


class Stream(threading.Thread):
    def __init__(self, rnd, idx):
        super().__init__(daemon=True)
        self.rnd, self.idx = rnd, idx
        self.events = []  # (t, kind, id)
        self.stop = False
        self.end = "?"
        self.t_start = self.t_conn = self.t_first = None

    def record(self, ev, data):
        kind = classify(ev, data)
        eid = None
        if kind in ("detail_all", "state"):
            eid = json.loads(data).get("id")
        t = time.monotonic()
        if self.t_first is None:
            self.t_first = t
        self.events.append((t, kind, eid))
        if out:
            with out_lock:
                out.write(
                    json.dumps(
                        {
                            "t": time.time(),
                            "round": self.rnd,
                            "stream": self.idx,
                            "kind": kind,
                            "id": eid,
                        }
                    )
                    + "\n"
                )

    def run(self):
        try:
            self.t_start = time.monotonic()
            s = socket.create_connection((args.host, 80), timeout=5)
            self.t_conn = time.monotonic()
            s.sendall(REQ % args.host.encode())
            s.settimeout(0.5)
            buf = body = b""
            headers = False
            cur = None
            while not self.stop:
                try:
                    data = s.recv(65536)
                except TimeoutError:
                    continue
                if not data:
                    self.end = "closed"
                    return
                buf += data
                if not headers:
                    i = buf.find(b"\r\n\r\n")
                    if i < 0:
                        continue
                    headers, buf = True, buf[i + 4 :]
                while (i := buf.find(b"\r\n")) >= 0:
                    size = int(buf[:i].split(b";")[0], 16)
                    if len(buf) < i + 4 + size:
                        break
                    body += buf[i + 2 : i + 2 + size]
                    buf = buf[i + 4 + size :]
                while b"\n" in body:
                    line, body = body.split(b"\n", 1)
                    line = line.rstrip(b"\r")
                    if line.startswith(b"event:"):
                        cur = line[6:].strip().decode()
                    elif line.startswith(b"data:"):
                        self.record(cur or "message", line[5:].strip())
                    elif not line:
                        cur = None
            self.end = "stopped"
            s.close()
        except (OSError, ValueError) as e:
            self.end = f"error: {e}"


def listing_done(st):
    last_detail = max((t for t, k, _ in st.events if k == "detail_all"), default=None)
    bare = any(k == "state" for _, k, _ in st.events)
    return last_detail is not None and bare and time.monotonic() - last_detail > 2.0


def counts(evs):
    c = {}
    for _, k, _ in evs:
        c[k] = c.get(k, 0) + 1
    return c


failed = 0
for rnd in range(1, args.rounds + 1):
    s1, s2 = Stream(rnd, 1), Stream(rnd, 2)
    s1.start()
    time.sleep(0.3)
    s2.start()
    deadline = time.monotonic() + 30
    while not (listing_done(s1) and listing_done(s2)) and time.monotonic() < deadline:
        time.sleep(0.2)
    if not (listing_done(s1) and listing_done(s2)):
        print(f"round {rnd}: streams 1/2 never finished their listing", flush=True)
        failed += 1
    expected = {i for _, k, i in s1.events if k == "detail_all"}
    s3 = Stream(rnd, 3)
    t0 = time.monotonic()
    s3.start()
    time.sleep(args.window)
    t1 = time.monotonic()
    for st in (s1, s2, s3):
        st.stop = True
    for st in (s1, s2, s3):
        st.join(3)
    ok = True
    for st in (s1, s2, s3):
        win = [e for e in st.events if t0 <= e[0] <= t1]
        print(f"round {rnd} stream {st.idx}: {counts(win)} end={st.end}", flush=True)
    for st in (s1, s2):
        win = [e for e in st.events if t0 <= e[0] <= t1]
        if any(
            k in ("detail_all", "sorting_group", "ping_greeting") for _, k, _ in win
        ):
            print(
                f"  stream {st.idx} got listing traffic after stream 3 connected",
                flush=True,
            )
            ok = False
    kinds = [k for _, k, _ in s3.events if k != "ping_uptime"]
    first_bare = kinds.index("state") if "state" in kinds else len(kinds)
    last_detail = max((i for i, k in enumerate(kinds) if k == "detail_all"), default=-1)
    groups = [i for i, k in enumerate(kinds) if k == "sorting_group"]
    got = {i for _, k, i in s3.events if k == "detail_all"}
    order_ok = (
        kinds[:1] == ["ping_greeting"]
        and groups == list(range(1, 1 + len(groups)))
        and last_detail < first_bare
        and got == expected
    )
    if s3.t_conn and s3.t_first:
        print(
            f"  stream 3 timing: TCP connect {1000 * (s3.t_conn - s3.t_start):.0f} ms, first event "
            f"{1000 * (s3.t_first - s3.t_start):.0f} ms after connect() started",
            flush=True,
        )
    print(
        f"  stream 3: first={kinds[:1]} groups={len(groups)} detail_all={len(got)}/{len(expected)} "
        f"last_detail_idx={last_detail} first_bare_idx={first_bare} order_ok={order_ok}",
        flush=True,
    )
    ok &= order_ok
    print(f"round {rnd}: {'PASS' if ok else 'FAIL'}", flush=True)
    failed += not ok
    time.sleep(3)

print(
    f"RESULT {'PASS' if not failed else 'FAIL'} ({args.rounds - failed}/{args.rounds} rounds)",
    flush=True,
)
sys.exit(1 if failed else 0)
