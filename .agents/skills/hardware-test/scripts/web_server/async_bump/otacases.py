"""otacases.py HOST KIND OUT [REPS]: web OTA cases on the D1 mini. KIND dev|pr picks img-KIND-{a,b}.bin.
case1 happy uploads, case2 uploads cut at 30%/80%, case3 uploads with a live /events client plus a reconnecting one."""

import json
import pathlib
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import rest  # noqa: E402
from rest import jv, uptime  # noqa: E402

HOST, KIND, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
rest.HOST = HOST
REPS = int(sys.argv[4]) if len(sys.argv) > 4 else 5
D = pathlib.Path(__file__).resolve().parent
IMG = {v: str(D / f"img-{KIND}-{v}.bin") for v in "ab"}
log = pathlib.Path(OUT).open("a", encoding="utf-8")  # noqa: SIM115


def p(*a):
    s = time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a)
    print(s, flush=True)
    log.write(s + "\n")
    log.flush()


def ver():
    return jv("/text_sensor/Build%20ver", "value") or jv(
        "/text_sensor/Build%20ver", "state"
    )


def heap():
    return rest.heap(), jv("/sensor/Heap%20max%20block")


def events_ok(t=8):
    """open /events, True when a state event arrives within t s"""
    try:
        s = socket.create_connection((HOST, 80), timeout=t)
        s.sendall(f"GET /events HTTP/1.1\r\nHost: {HOST}\r\n\r\n".encode())
        s.settimeout(t)
        buf = b""
        end = time.monotonic() + t
        while time.monotonic() < end and b"event: state" not in buf:
            d = s.recv(4096)
            if not d:
                break
            buf += d
        s.close()
        return b"event: state" in buf
    except Exception:  # noqa: BLE001
        return False


def current_ver():
    for _ in range(40):
        v = ver()
        if v:
            return v
        time.sleep(2)
    return None


def wait_boot(expect, before_up, cap=120):
    t0 = time.monotonic()
    while time.monotonic() - t0 < cap:
        u = uptime()
        if u is not None and u < before_up and ver() == expect:
            return time.monotonic() - t0
        time.sleep(1)
    return None


def full_upload(tag):
    cur = current_ver()
    nxt = "b" if cur == "a" else "a"
    up = uptime() or 1e9
    t0 = time.monotonic()
    r = subprocess.run(
        [
            "curl",
            "-s",
            "-m",
            "300",
            "-o",
            "/dev/null",
            "-w",
            "%{http_code} %{size_upload} %{time_total}",
            "-F",
            f"update=@{IMG[nxt]}",
            f"http://{HOST}/update",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    up_s = time.monotonic() - t0
    time.sleep(3)
    back = wait_boot(nxt, up + up_s + 5, 60 if not r.stdout.startswith("200") else 120)
    if back is not None and r.stdout.startswith("200"):
        st = "OK"
    elif back is not None:
        st = "REPLY_LOST"  # new image booted but the HTTP reply never arrived
    elif (
        back is None
        and ver() == cur
        and rest.req("/", timeout=8)[0] == 200
        and (uptime() or 0) > up
    ):
        st = "INTERRUPTED"  # upload cut on the network, old image still running and serving
    else:
        st = "BAD"
    p(
        f"{tag} upload {cur}->{nxt} curl '{r.stdout}' upload {up_s:.1f}s, booted {nxt} after {back if back is None else round(back, 1)}s => {st}"
    )
    return st


def upload_ok(tag, tries=4):
    sts = []
    for k in range(tries):
        st = full_upload(f"{tag} try{k}")
        sts.append(st)
        if st != "INTERRUPTED":
            break
        time.sleep(15)
    return sts


def cut_upload(frac, tag):
    cur = current_ver()
    nxt = "b" if cur == "a" else "a"
    data = pathlib.Path(IMG[nxt]).read_bytes()
    bnd = "----otacut1234"
    head = (
        f'--{bnd}\r\nContent-Disposition: form-data; name="update"; filename="firmware.bin"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode()
    tail = f"\r\n--{bnd}--\r\n".encode()
    body_len = len(head) + len(data) + len(tail)
    h0 = heap()
    up0 = uptime()
    s = socket.create_connection((HOST, 80), timeout=20)
    s.sendall(
        (
            f"POST /update HTTP/1.1\r\nHost: {HOST}\r\nContent-Type: multipart/form-data; boundary={bnd}\r\n"
            f"Content-Length: {body_len}\r\n\r\n"
        ).encode()
        + head
    )
    n = int(len(data) * frac)
    sent = 0
    try:
        while sent < n:
            k = s.send(data[sent : min(n, sent + 1460)])
            sent += k
    except Exception as e:  # noqa: BLE001
        p(f"{tag} send error {type(e).__name__} at {sent}")
    time.sleep(0.5)
    s.close()
    # serving right after the cut
    g = rest.req("/", timeout=8)[0]
    ev = events_ok()
    time.sleep(20)
    up1 = uptime()
    v1 = ver()
    h1 = heap()
    g2 = rest.req("/", timeout=8)[0]
    ev2 = events_ok()
    rebooted = up1 is None or up0 is None or up1 < up0
    p(
        f"{tag} cut {int(frac * 100)}% sent {sent}/{len(data)}: right after GET / {g} events {ev}; +20s GET / {g2} events {ev2}, "
        f"ver {v1} (was {cur}), uptime {up0}->{up1} {'REBOOTED' if rebooted else 'no reboot'}, heap {h0}->{h1}"
    )
    return not rebooted and v1 == cur and g2 == 200 and ev2


class Sse(threading.Thread):
    def __init__(self, reconnect):
        super().__init__(daemon=True)
        self.reconnect = reconnect
        self.stop = False
        self.states = 0
        self.closes = 0
        self.silent = 0
        self.opens = 0
        self.errors = 0
        self.first_after_close = None
        self.closed_at = None

    def run(self):
        while not self.stop:
            try:
                s = socket.create_connection((HOST, 80), timeout=3)
                s.sendall(f"GET /events HTTP/1.1\r\nHost: {HOST}\r\n\r\n".encode())
                self.opens += 1
                s.settimeout(1)
                last = time.monotonic()
                while not self.stop:
                    try:
                        d = s.recv(4096)
                    except TimeoutError:
                        if time.monotonic() - last > 6:
                            self.silent += 1
                            self.closed_at = self.closed_at or time.monotonic()
                            break
                        continue
                    last = time.monotonic()
                    if not d:
                        self.closes += 1
                        self.closed_at = time.monotonic()
                        break
                    c = d.count(b"event: state")
                    self.states += c
                    if c and self.closed_at and self.first_after_close is None:
                        self.first_after_close = time.monotonic() - self.closed_at
                s.close()
            except (ConnectionResetError, BrokenPipeError):
                self.closes += 1
                self.closed_at = self.closed_at or time.monotonic()
            except Exception:  # noqa: BLE001
                self.errors += 1
            if not self.reconnect and (self.closes or self.silent):
                return
            time.sleep(1)


def sse_upload(tag):
    a = Sse(False)
    b = Sse(True)
    a.start()
    b.start()
    time.sleep(5)
    st0 = (a.states, b.states)
    sts = upload_ok(tag + " (sse)")
    ok = sts[-1] == "OK"
    time.sleep(10)
    a.stop = b.stop = True
    time.sleep(2)
    c = Sse(True)
    c.start()
    time.sleep(8)
    c.stop = True
    p(
        f"{tag} sse: held client states before OTA {st0[0]}, total {a.states}, FIN/RST {a.closes}, went silent {a.silent}; "
        f"reconnecting client opens {b.opens}, FIN/RST {b.closes}, silent {b.silent}, errors {b.errors}, events again {b.first_after_close if b.first_after_close is None else round(b.first_after_close, 1)}s after close; "
        f"fresh client states in 8s {c.states}"
    )
    return (
        ok
        and (a.closes + a.silent) >= 1
        and b.first_after_close is not None
        and c.states > 0
    )


case = sys.argv[5] if len(sys.argv) > 5 else "all"
res = {}
if case in ("all", "1"):
    sts = [full_upload(f"c1#{i}") for i in range(REPS)]
    while sts.count("OK") < REPS and len(sts) < 3 * REPS:
        time.sleep(15)
        sts.append(full_upload(f"c1#{len(sts)}"))
    res["c1"] = {k: sts.count(k) for k in ("OK", "REPLY_LOST", "INTERRUPTED", "BAD")}
if case in ("all", "2"):
    okn = 0
    h_start = heap()
    for i in range(REPS):
        for f in (0.3, 0.8):
            okn += cut_upload(f, f"c2#{i}")
    time.sleep(10)
    h_end = heap()
    p(f"c2 heap start {h_start} end {h_end} over {2 * REPS} cuts")
    after = upload_ok("c2 follow-up")
    res["c2"] = okn
    res["c2_followup"] = after
    res["c2_heap"] = (h_start, h_end)
if case in ("all", "3"):
    res["c3"] = sum(sse_upload(f"c3#{i}") for i in range(REPS))
p("RESULT", KIND, json.dumps(res))
