"""/events open/close churn with long lived observers and REST traffic in parallel (config common.yaml).

usage: churnmix.py HOST --cycles N [--workers 4] [--observers 2] [--base 20] [--settle 30] [--json OUT]

Phases: base (observers + REST), churn (workers run N cycles in total, REST keeps going), settle (observers + REST),
post (4 fresh clients for 10 s). Observers check the Counter sensor (n++ every 100 ms): every bare state event must
be the previous value + 1 (dup = same value again, back = lower value, missed = values skipped), every Relay toggle
answered 200 over REST must show up as one Relay state event, and they record Heap Free / Heap Block from the debug
sensors. Churn modes: fin_first (read to the first state event, close), rst_mid (read 0.1 to 1.5 s, RST), rst_now
(send request, RST before reading), fin_now (send request, close), half_req (half a request line, close),
stall_rst (2 KB receive buffer, read nothing for 3 s, RST), fin_read (read 1 to 3 s, close).
"""

import argparse
import contextlib
import http.client
import json
import pathlib
import random
import socket
import struct
import threading
import time

ap = argparse.ArgumentParser()
ap.add_argument("host")
ap.add_argument("--port", type=int, default=80)
ap.add_argument("--cycles", type=int, default=400)
ap.add_argument("--workers", type=int, default=4)
ap.add_argument("--observers", type=int, default=2)
ap.add_argument("--base", type=float, default=20)
ap.add_argument("--settle", type=float, default=30)
ap.add_argument("--rest-interval", type=float, default=0.25)
ap.add_argument("--json")
ap.add_argument("--no-rest", action="store_true")
ap.add_argument("--no-post", action="store_true")
ap.add_argument("--seed", type=int, default=1)
args = ap.parse_args()
H, PORT = args.host, args.port
REQ = (
    b"GET /events HTTP/1.1\r\nHost: %s\r\nAccept: text/event-stream\r\n\r\n"
    % H.encode()
)
T0 = time.monotonic()
phase = "base"
lock = threading.Lock()


def now():
    return time.monotonic() - T0


class SSEParser:
    """Feeds raw bytes; handles the HTTP head, chunked or raw bodies; yields (event, data) tuples."""

    def __init__(self):
        self.buf = b""
        self.head = None
        self.status = None
        self.chunked = None
        self.body = b""
        self.chunk_left = 0
        self.eof_chunk = False

    def feed(self, data):
        out = []
        self.buf += data
        if self.head is None:
            i = self.buf.find(b"\r\n\r\n")
            if i < 0:
                return out
            self.head = self.buf[:i].decode("latin1")
            self.buf = self.buf[i + 4 :]
            try:
                self.status = int(self.head.split(" ", 2)[1])
            except (IndexError, ValueError):
                self.status = -1
            self.chunked = "chunked" in self.head.lower()
        if self.chunked:
            while True:
                if self.chunk_left == 0:
                    j = self.buf.find(b"\r\n")
                    if j < 0:
                        break
                    line = self.buf[:j].split(b";")[0].strip()
                    if not line:  # CRLF after a chunk
                        self.buf = self.buf[j + 2 :]
                        continue
                    try:
                        size = int(line, 16)
                    except ValueError:
                        self.buf = b""
                        break
                    self.buf = self.buf[j + 2 :]
                    if size == 0:
                        self.eof_chunk = True
                        break
                    self.chunk_left = size
                take = self.buf[: self.chunk_left]
                self.body += take
                self.buf = self.buf[len(take) :]
                self.chunk_left -= len(take)
                if self.chunk_left:
                    break
        else:
            self.body += self.buf
            self.buf = b""
        while True:
            body = self.body.replace(b"\r\n", b"\n")
            k = body.find(b"\n\n")
            if k < 0:
                self.body = body
                break
            block, self.body = body[:k], body[k + 2 :]
            ev, data = "message", []
            for ln in block.split(b"\n"):
                if ln.startswith(b"event:"):
                    ev = ln[6:].strip().decode()
                elif ln.startswith(b"data:"):
                    data.append(ln[5:].strip())
            if data or ev != "message":
                out.append((ev, b"\n".join(data)))
        return out


def connect(rcvbuf=None, timeout=5):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if rcvbuf:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
    s.settimeout(timeout)
    try:
        s.connect((H, PORT))
    except OSError:
        s.close()
        raise
    return s


def rst_close(s):
    with contextlib.suppress(OSError):
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    s.close()


# ---------------- observers ----------------
rest_toggles_ok = []  # (time, phase) of toggles answered 200


class Observer(threading.Thread):
    def __init__(self, idx, until):
        super().__init__(daemon=True)
        self.idx, self.until = idx, until
        self.c_prev = None
        self.dup = self.back = self.missed = self.bare_counter = 0
        self.detail_counter = (
            0  # detail_all entries for Counter after this observer's own listing
        )
        self.listing_done = False
        self.relay_events = 0
        self.heap = []  # (t, phase, free)
        self.block = []
        self.max_gap = 0.0
        self.reconnects = 0
        self.events = 0
        self.by_phase_missed = {}
        self.uptime_prev = None
        self.relay_last = None
        self.dup_log = []
        self.last_ev_age = None
        self.t_last_counter = None
        self.reboots = 0
        self.bad_json = 0

    def handle(self, ev, data):
        self.events += 1
        if ev != "state":
            return
        try:
            j = json.loads(data)
        except ValueError:
            self.bad_json += 1
            return
        i = j.get("id", "")
        detail = "domain" in j
        if i == "sensor/Counter":
            if detail:
                if self.listing_done:
                    self.detail_counter += 1
                # a listing entry carries the current value; do not count it in the bare sequence
                return
            self.listing_done = True
            v = j.get("value")
            if not isinstance(v, (int, float)):
                return
            v = int(round(v))
            tn = time.monotonic()
            self.last_ev_age = (
                round(tn - self.t_last_counter, 3) if self.t_last_counter else None
            )
            self.t_last_counter = tn
            self.bare_counter += 1
            if self.c_prev is not None:
                if v == self.c_prev:
                    self.dup += 1
                    if len(self.dup_log) < 20:
                        self.dup_log.append(
                            (round(now(), 2), phase, v, self.last_ev_age)
                        )
                elif v < self.c_prev:
                    self.back += 1
                elif v > self.c_prev + 1:
                    gap = v - self.c_prev - 1
                    self.missed += gap
                    self.by_phase_missed[phase] = (
                        self.by_phase_missed.get(phase, 0) + gap
                    )
            self.c_prev = v
        elif i == "switch/Relay":
            self.relay_last = j.get("value")
            if detail:
                return
            self.relay_events += 1
        elif i == "sensor/Uptime" and not detail:
            u = j.get("value")
            if isinstance(u, (int, float)):
                if self.uptime_prev is not None and u + 5 < self.uptime_prev:
                    self.reboots += 1
                self.uptime_prev = u
        elif i == "sensor/Heap Free" and not detail:
            self.heap.append((round(now(), 1), phase, j.get("value")))
        elif i == "sensor/Heap Block" and not detail:
            self.block.append((round(now(), 1), phase, j.get("value")))

    def run(self):
        while now() < self.until:
            try:
                s = connect(timeout=10)
                s.sendall(REQ)
            except OSError:
                self.reconnects += 1
                time.sleep(1)
                continue
            p = SSEParser()
            last = time.monotonic()
            first = True
            try:
                while now() < self.until:
                    d = s.recv(8192)
                    t = time.monotonic()
                    if not first:
                        self.max_gap = max(self.max_gap, t - last)
                    first = False
                    last = t
                    if not d:
                        break
                    for ev, data in p.feed(d):
                        self.handle(ev, data)
            except OSError:
                pass
            s.close()
            if now() < self.until:
                self.reconnects += 1
                self.c_prev = None
                self.listing_done = False
                time.sleep(1)


# ---------------- churn ----------------
MODES = [
    "fin_first",
    "rst_mid",
    "rst_now",
    "fin_now",
    "half_req",
    "stall_rst",
    "fin_read",
]
churn_stats = {
    m: {"n": 0, "connect_fail": 0, "no_200": 0, "no_state": 0, "ok": 0} for m in MODES
}
churn_first_ms = []
cycle_counter = [0]
churn_reasons = {}


def note_reason(m, why):
    with lock:
        k = f"{m}:{why}"
        churn_reasons[k] = churn_reasons.get(k, 0) + 1


def read_until(s, p, deadline, want_state, m=None):
    gotstate = False
    why = "deadline"
    t0 = time.monotonic()
    while time.monotonic() < deadline:
        s.settimeout(max(0.05, deadline - time.monotonic()))
        try:
            d = s.recv(4096)
        except TimeoutError:
            why = "timeout"
            break
        except OSError as e:
            why = f"oserror{e.errno}"
            break
        if not d:
            why = "eof"
            break
        for ev, _ in p.feed(d):
            if ev == "state":
                gotstate = True
        if want_state and gotstate:
            break
    if m and not (want_state and gotstate):
        note_reason(
            m, f"{why}{'' if p.status else '_nohead'}@{time.monotonic() - t0:.1f}s"[:40]
        )
    return p.status == 200, gotstate


def churn_worker(rng):
    while True:
        with lock:
            if cycle_counter[0] >= args.cycles:
                return
            cycle_counter[0] += 1
        m = rng.choice(MODES)
        st = churn_stats[m]
        with lock:
            st["n"] += 1
        try:
            s = connect(rcvbuf=2048 if m == "stall_rst" else None)
        except OSError:
            with lock:
                st["connect_fail"] += 1
            time.sleep(0.5)
            continue
        try:
            t0 = time.monotonic()
            if m == "half_req":
                s.sendall(b"GET /eve")
                s.close()
                ok = True
            elif m == "rst_now":
                s.sendall(REQ)
                rst_close(s)
                ok = True
            elif m == "fin_now":
                s.sendall(REQ)
                s.close()
                ok = True
            elif m == "stall_rst":
                s.sendall(REQ)
                time.sleep(3)
                rst_close(s)
                ok = True
            else:
                s.sendall(REQ)
                p = SSEParser()
                if m == "fin_first":
                    g200, gst = read_until(s, p, t0 + 6, True, m)
                    if gst:
                        churn_first_ms.append((time.monotonic() - t0) * 1000)
                    s.close()
                elif m == "rst_mid":
                    g200, gst = read_until(s, p, t0 + rng.uniform(0.1, 1.5), False)
                    rst_close(s)
                else:  # fin_read
                    g200, gst = read_until(s, p, t0 + rng.uniform(1, 3), False)
                    s.close()
                # short rst_mid reads may end before the first state; judge those on 200 only
                ok = g200 and (gst or m == "rst_mid")
                with lock:
                    if not g200:
                        st["no_200"] += 1
                    elif not ok:
                        st["no_state"] += 1
        except OSError as e:
            ok = False
            note_reason(m, f"outer_oserror{e.errno}")
            with lock:
                st["no_200"] += 1
            with contextlib.suppress(OSError):
                s.close()
        if ok:
            with lock:
                st["ok"] += 1
        time.sleep(rng.uniform(0, 0.15))


# ---------------- REST ----------------
rest = {}
rest_stop = [False]


def rest_req(method, path, kind):
    t = time.monotonic()
    r = rest.setdefault(phase, {}).setdefault(
        kind, {"ok": 0, "fail": 0, "max_ms": 0, "codes": {}}
    )
    try:
        c = http.client.HTTPConnection(H, PORT, timeout=8)
        c.request(
            method,
            path,
            headers={"Connection": "close"}
            if method == "GET"
            else {"Content-Length": "0"},
        )
        resp = c.getresponse()
        body = resp.read()
        c.close()
        code = resp.status
    except (OSError, http.client.HTTPException) as e:
        code = type(e).__name__
        body = b""
    ms = (time.monotonic() - t) * 1000
    with lock:
        r["codes"][str(code)] = r["codes"].get(str(code), 0) + 1
        if code == 200:
            r["ok"] += 1
            r["max_ms"] = max(r["max_ms"], round(ms))
        else:
            r["fail"] += 1
    return code, body


def rest_worker():
    i = 0
    while not rest_stop[0]:
        i += 1
        k = i % 10
        if k in (1, 4, 7):
            rest_req("GET", "/sensor/Counter", "get_sensor")
        elif k in (2, 6):
            code, _ = rest_req("POST", "/switch/Relay/toggle", "toggle")
            if code == 200:
                with lock:
                    rest_toggles_ok.append((now(), phase))
        elif k == 9:
            rest_req("GET", "/", "page")
        else:
            rest_req("GET", "/switch/Relay", "get_switch")
        time.sleep(args.rest_interval)


# ---------------- post check ----------------
def post_client(res, idx):
    try:
        s = connect(timeout=5)
        s.sendall(REQ)
    except OSError:
        res[idx] = {"connect": False}
        return
    p = SSEParser()
    t0 = time.monotonic()
    first = None
    states = 0
    details = set()
    while time.monotonic() < t0 + 10:
        s.settimeout(0.5)
        try:
            d = s.recv(8192)
        except TimeoutError:
            continue
        except OSError:
            break
        if not d:
            break
        for ev, data in p.feed(d):
            if ev == "state":
                states += 1
                if first is None:
                    first = (time.monotonic() - t0) * 1000
                try:
                    j = json.loads(data)
                    if "domain" in j:
                        details.add(j["id"])
                except ValueError:
                    pass
    s.close()
    res[idx] = {
        "status": p.status,
        "states": states,
        "first_ms": round(first or -1),
        "listed": len(details),
    }


def main():
    global phase  # noqa: PLW0603
    obs = [Observer(i, 10**9) for i in range(args.observers)]
    for o in obs:
        o.start()
    rt = threading.Thread(target=rest_worker, daemon=True)
    time.sleep(2)
    if not args.no_rest:
        rt.start()
    time.sleep(args.base)
    phase = "churn"
    tc = now()
    rng = random.Random(args.seed)
    ws = [
        threading.Thread(
            target=churn_worker, args=(random.Random(rng.random()),), daemon=True
        )
        for _ in range(args.workers)
    ]
    for w in ws:
        w.start()
    for w in ws:
        w.join()
    churn_s = now() - tc
    phase = "settle"
    time.sleep(args.settle)
    rest_stop[0] = True
    if not args.no_rest:
        rt.join(15)
    time.sleep(2)
    for o in obs:
        o.until = 0
    phase = "post"
    pres = {}
    pts = [
        threading.Thread(target=post_client, args=(pres, i))
        for i in range(0 if args.no_post else 4)
    ]
    for t in pts:
        t.start()
    for t in pts:
        t.join()
    toggles = len(rest_toggles_ok)
    _, rb = rest_req("GET", "/switch/Relay", "final_switch")
    try:
        relay_now = json.loads(rb).get("value")
    except ValueError:
        relay_now = None
    fm = sorted(churn_first_ms)
    out = {
        "churn_seconds": round(churn_s, 1),
        "cycles": cycle_counter[0],
        "churn": churn_stats,
        "reasons": churn_reasons,
        "first_state_ms": [round(fm[0]), round(fm[len(fm) // 2]), round(fm[-1])]
        if fm
        else [None] * 3,
        "rest": rest,
        "toggles_ok": toggles,
        "observers": [
            {
                "events": o.events,
                "bare_counter": o.bare_counter,
                "dup": o.dup,
                "back": o.back,
                "missed": o.missed,
                "missed_by_phase": o.by_phase_missed,
                "relay_events": o.relay_events,
                "relisted_counter": o.detail_counter,
                "reconnects": o.reconnects,
                "reboots": o.reboots,
                "bad_json": o.bad_json,
                "relay_final_ok": o.relay_last == relay_now,
                "dup_log": o.dup_log,
                "uptime_last": o.uptime_prev,
                "max_gap_s": round(o.max_gap, 2),
                "heap": o.heap,
                "block": o.block,
            }
            for o in obs
        ],
        "post": pres,
    }
    if args.json:
        with pathlib.Path(args.json).open("w", encoding="utf-8") as f:
            json.dump(out, f)
    # summary
    print(
        f"churn: {out['cycles']} cycles in {out['churn_seconds']} s; first state ms min/med/max {out['first_state_ms']}"
    )
    for m, st in churn_stats.items():
        print(
            f"  {m:10s} n={st['n']:4d} ok={st['ok']:4d} connect_fail={st['connect_fail']} no_200={st['no_200']} no_state={st['no_state']}"
        )
    print("churn failure reasons:", dict(sorted(churn_reasons.items())))
    for ph, kinds in rest.items():
        print(
            f"rest {ph}: "
            + "; ".join(
                f"{k} ok={v['ok']} fail={v['fail']} max={v['max_ms']}ms {v['codes'] if v['fail'] else ''}"
                for k, v in kinds.items()
            )
        )
    print(f"toggles answered 200: {toggles}")
    for i, o in enumerate(out["observers"]):
        hp = {}
        for _, ph, v in o["heap"]:
            hp.setdefault(ph, []).append(v)
        hs = " ".join(
            f"{ph}:{int(min(v))}/{int(v[-1])}"
            for ph, v in hp.items()
            if v and all(isinstance(x, (int, float)) for x in v)
        )
        print(
            f"observer {i}: events={o['events']} counter={o['bare_counter']} dup={o['dup']} back={o['back']} missed={o['missed']} {o['missed_by_phase']} relay_events={o['relay_events']} relisted={o['relisted_counter']} reconnects={o['reconnects']} reboots={o['reboots']} bad_json={o['bad_json']} relay_final_ok={o['relay_final_ok']} uptime={o['uptime_last']} max_gap={o['max_gap_s']}s heap(min/last) {hs}"
        )
    for i, r in sorted(pres.items()):
        print(f"post client {i}: {r}")
    bad = []
    for o in out["observers"]:
        if o["dup"] or o["back"]:
            bad.append("dup/back")
        if not o["relay_final_ok"]:
            bad.append("relay final state")
        if o["relay_events"] != toggles:
            bad.append(f"relay {o['relay_events']} != {toggles}")
        if o["reconnects"]:
            bad.append("observer reconnect")
        if o["bad_json"]:
            bad.append("bad state json")
    bad.extend(
        f"post {r}"
        for r in pres.values()
        if r.get("status") != 200 or r.get("states", 0) < 50
    )
    # rst_mid may reset before the head arrives, so only its missing states count
    if churn := sum(
        st["connect_fail"] + st["no_state"] + (st["no_200"] if m != "rst_mid" else 0)
        for m, st in churn_stats.items()
    ):
        bad.append(f"churn failures {churn}")
    if rest_fail := sum(v["fail"] for kinds in rest.values() for v in kinds.values()):
        bad.append(f"rest failures {rest_fail}")
    print("RESULT", "PASS" if not bad else "CHECK " + "; ".join(sorted(set(bad))))


main()
