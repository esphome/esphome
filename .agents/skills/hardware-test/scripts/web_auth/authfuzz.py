"""Basic auth functional and security cases against an ESPHome web_server (non-ESP32 async server).

usage: authfuzz.py HOST USER PASS OUT_PREFIX [--only a,b] [--brute N] [--timing N] [--conc SECONDS]
                   [--ota FIRMWARE.bin] [--skip-heavy]
Writes OUT_PREFIX-cases.json (one row per case: status, WWW-Authenticate, raw head, pass/fail) and
OUT_PREFIX-summary.txt. Heap comes from the "Heap free" debug sensor over REST (with credentials).
Entity URLs use entity names: /sensor/Heap%20free, /switch/Test%20switch, /light/Test%20light.
"""

from __future__ import annotations

import argparse
import base64
import contextlib
import json
import pathlib
import random
import re
import socket
import statistics
import string
import sys
import threading
import time

# Set by configure(); isolate.py imports this module and calls it too.
A = argparse.Namespace()
HOST = ""
USER = ""
PASSWORD = ""
HASH = ""
GOOD = ""
OUT = None
ROWS: list[dict] = []


def configure(host: str, user: str, password: str, out: str | None = None) -> None:
    global HOST, USER, PASSWORD, HASH, GOOD, OUT  # noqa: PLW0603
    HOST, USER, PASSWORD = host, user, password
    HASH = base64.b64encode(f"{user}:{password}".encode()).decode()
    GOOD = f"Basic {HASH}"
    if out:
        OUT = pathlib.Path(out + "-summary.txt").open("w", encoding="utf-8")  # noqa: SIM115


def p(*a: object) -> None:
    s = time.strftime("[%H:%M:%S] ") + " ".join(str(x) for x in a)
    print(s, flush=True)
    if OUT is not None:
        OUT.write(s + "\n")
        OUT.flush()


def raw(
    req: bytes, timeout: float = 8.0, read_body: bool = True, max_bytes: int = 65536
) -> dict:
    """Send raw bytes, return status, headers, body, timings, error."""
    t0 = time.monotonic()
    r: dict = {
        "status": None,
        "headers": {},
        "head": "",
        "body": b"",
        "err": None,
        "closed": None,
    }
    try:
        s = socket.create_connection((HOST, 80), timeout=timeout)
    except OSError as e:
        r["err"] = f"connect {type(e).__name__}"
        return r
    try:
        s.settimeout(timeout)
        try:
            s.sendall(req)
        except OSError as e:
            r["err"] = f"send {type(e).__name__}"
        t1 = time.monotonic()
        buf = b""
        first = None
        while len(buf) < max_bytes:
            try:
                d = s.recv(4096)
            except TimeoutError:
                r["err"] = r["err"] or "timeout"
                break
            except OSError as e:
                r["err"] = r["err"] or f"recv {type(e).__name__}"
                break
            if not d:
                r["closed"] = True
                break
            if first is None:
                first = time.monotonic()
            buf += d
            if b"\r\n\r\n" in buf:
                if not read_body:
                    break
                hd, bd = buf.split(b"\r\n\r\n", 1)
                m = re.search(rb"(?im)^content-length:\s*(\d+)", hd)
                if m and len(bd) >= int(m.group(1)):
                    r["closed"] = False
                    break
        r["ttfb"] = (first - t1) if first else None
        r["total"] = time.monotonic() - t0
        if b"\r\n\r\n" in buf:
            head, body = buf.split(b"\r\n\r\n", 1)
            r["head"] = head.decode("latin1")
            r["body"] = body
            lines = r["head"].split("\r\n")
            with contextlib.suppress(IndexError, ValueError):
                r["status"] = int(lines[0].split()[1])
            for ln in lines[1:]:
                if ":" in ln:
                    k, v = ln.split(":", 1)
                    r["headers"].setdefault(k.strip().lower(), []).append(v.strip())
        elif buf:
            r["err"] = r["err"] or "partial"
            r["body"] = buf
    finally:
        s.close()
    return r


def build(
    method: str,
    path: str,
    auth: bytes | str | None,
    extra: bytes = b"",
    body: bytes = b"",
    ctype: str | None = None,
) -> bytes:
    h = f"{method} {path} HTTP/1.1\r\nHost: {HOST}\r\nConnection: close\r\n".encode()
    if auth is not None:
        h += (
            b"Authorization: "
            + (auth.encode("latin1") if isinstance(auth, str) else auth)
            + b"\r\n"
        )
    h += extra
    if method == "POST" or body:
        if ctype:
            h += f"Content-Type: {ctype}\r\n".encode()
        h += f"Content-Length: {len(body)}\r\n".encode()
    return h + b"\r\n" + body


def heap() -> object:
    for _ in range(3):
        r = raw(build("GET", "/sensor/Heap%20free", GOOD))
        if r["status"] == 200:
            try:
                return json.loads(r["body"]).get("value")
            except ValueError:
                pass
        time.sleep(1)
    return None


def uptime() -> object:
    r = raw(build("GET", "/sensor/Uptime", GOOD))
    try:
        return json.loads(r["body"]).get("value") if r["status"] == 200 else None
    except ValueError:
        return None


def case(
    name: str, req: bytes, expect: int | str, note: str = "", **kw: object
) -> dict:
    u0 = uptime() if expect == "reject" else None
    r = raw(req, **kw)
    st = r["status"]
    if expect == "reject":
        ok = st in (400, 401, 413, 431)
        if st is None:
            # no status (closed, reset or timeout) is a rejection only when the device stayed up
            time.sleep(1)
            u1 = uptime()
            ok = u0 is not None and u1 is not None and u1 >= u0
            note = (note + " " if note else "") + f"no status, uptime {u0} -> {u1}"
    else:
        ok = st == expect
    row = {
        "case": name,
        "status": st,
        "expect": expect,
        "pass": ok,
        "err": r["err"],
        "www": r["headers"].get("www-authenticate"),
        "head": r["head"],
        "body_len": len(r["body"]),
        "body_head": r["body"][:120].decode("latin1"),
        "note": note,
        "closed": r["closed"],
    }
    ROWS.append(row)
    p(
        f"{'PASS' if ok else 'FAIL'} {name}: status {st} expect {expect} www={row['www']} err={r['err']} "
        f"body {len(r['body'])}B"
    )
    return row


def mp_body(data: bytes) -> tuple[bytes, str]:
    b = "----hwtestboundary" + "".join(random.choices(string.ascii_letters, k=8))
    body = (
        (
            f'--{b}\r\nContent-Disposition: form-data; name="update"; filename="firmware.bin"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n"
        ).encode()
        + data
        + f"\r\n--{b}--\r\n".encode()
    )
    return body, f"multipart/form-data; boundary={b}"


def flip(c: str) -> str:
    return "B" if c != "B" else "C"


def swapcase_b64(h: str) -> str:
    return "".join(c.swapcase() if c.isalpha() else c for c in h)


def run_cases() -> None:
    sw, lt = (
        "/switch/Test%20switch/toggle",
        "/light/Test%20light/turn_on?brightness=100",
    )
    p("HASH length", len(HASH))
    # Functional
    nr = case("noauth GET /", build("GET", "/", None), 401)
    ok401 = nr["www"] == ['Basic realm="Login Required"']
    p(f"{'PASS' if ok401 else 'FAIL'} 401 WWW-Authenticate exact: {nr['www']}")
    ROWS.append(
        {
            "case": "401 WWW-Authenticate exact",
            "pass": ok401,
            "www": nr["www"],
            "status": nr["status"],
            "head": nr["head"],
            "body_head": nr["body_head"],
        }
    )
    case("good GET /", build("GET", "/", GOOD), 200)
    case(
        "noauth /events",
        build("GET", "/events", None, b"Accept: text/event-stream\r\n"),
        401,
    )
    r = raw(
        build("GET", "/events", GOOD, b"Accept: text/event-stream\r\n"),
        timeout=3,
        max_bytes=4096,
    )
    ev = r["status"] == 200 and b"event:" in r["body"]
    ROWS.append(
        {
            "case": "good /events (SSE data)",
            "status": r["status"],
            "pass": ev,
            "err": r["err"],
        }
    )
    p(
        f"{'PASS' if ev else 'FAIL'} good /events: status {r['status']} event bytes {len(r['body'])}"
    )
    case("noauth POST switch", build("POST", sw, None), 401)
    case("good POST switch", build("POST", sw, GOOD), 200)
    case("noauth POST light", build("POST", lt, None), 401)
    case("good POST light", build("POST", lt, GOOD), 200)
    body, ct = mp_body(b"\x00" * 1024)
    case(
        "noauth POST /update 1KB",
        build("POST", "/update", None, body=body, ctype=ct),
        401,
    )
    case("noauth GET /sensor", build("GET", "/sensor/Fast%20counter", None), 401)
    case("good GET /sensor", build("GET", "/sensor/Fast%20counter", GOOD), 200)
    # Wrong credentials
    wp = base64.b64encode(f"{USER}:{PASSWORD}x".encode()).decode()
    wp2 = base64.b64encode(
        f"{USER}:{PASSWORD[:-1] + flip(PASSWORD[-1])}".encode()
    ).decode()
    wu = base64.b64encode(f"{USER}x:{PASSWORD}".encode()).decode()
    wu2 = base64.b64encode(f"{flip(USER[0]) + USER[1:]}:{PASSWORD}".encode()).decode()
    case("wrong password (longer)", build("GET", "/", "Basic " + wp), 401)
    case("wrong password (same length)", build("GET", "/", "Basic " + wp2), 401)
    case("wrong username (longer)", build("GET", "/", "Basic " + wu), 401)
    case("wrong username (same length)", build("GET", "/", "Basic " + wu2), 401)
    case("hash minus last char", build("GET", "/", "Basic " + HASH[:-1]), 401)
    case("hash plus one char", build("GET", "/", "Basic " + HASH + "A"), 401)
    case("hash prefix half", build("GET", "/", "Basic " + HASH[: len(HASH) // 2]), 401)
    case("empty Authorization value", build("GET", "/", ""), 401)
    # Schemes
    case(
        "Digest scheme",
        build(
            "GET",
            "/",
            f'Digest username="{USER}", realm="Login Required", nonce="0", '
            f'uri="/", response="00000000000000000000000000000000"',
        ),
        401,
    )
    case("Digest scheme with hash", build("GET", "/", "Digest " + HASH), 401)
    case("Bearer hash", build("GET", "/", "Bearer " + HASH), 401)
    case("Basic alone", build("GET", "/", "Basic"), 401)
    case("Basic plus space", build("GET", "/", "Basic "), 401)
    case("Basic plus spaces only", build("GET", "/", "Basic      "), 401)
    case("hash without scheme", build("GET", "/", HASH), 401)
    case("Basicx scheme", build("GET", "/", "Basicx " + HASH), 401)
    # Tampering
    case("hash + trailing garbage", build("GET", "/", GOOD + "garbage"), 401)
    case("hash + space + garbage", build("GET", "/", GOOD + " garbage"), 401)
    case(
        "first char changed",
        build("GET", "/", "Basic " + flip(HASH[0]) + HASH[1:]),
        401,
    )
    case(
        "last char changed",
        build("GET", "/", "Basic " + HASH[:-1] + flip(HASH[-1])),
        401,
    )
    case("swapped case base64", build("GET", "/", "Basic " + swapcase_b64(HASH)), 401)
    case(
        "hash NUL garbage",
        build("GET", "/", GOOD.encode() + b"\x00garbage"),
        "reject",
        "informational",
    )
    # Spacing and case
    case("lowercase basic", build("GET", "/", "basic " + HASH), 200)
    case("uppercase BASIC", build("GET", "/", "BASIC " + HASH), 200)
    case("Basic 3 spaces", build("GET", "/", "Basic   " + HASH), 401)
    case(
        "trailing spaces",
        build("GET", "/", GOOD + "   "),
        200,
        "library trims the header line",
    )
    case(
        "trailing tab",
        build("GET", "/", GOOD + "\t"),
        200,
        "library trims the header line",
    )
    case(
        "trailing space+tab",
        build("GET", "/", GOOD + " \t "),
        200,
        "library trims the header line",
    )
    case(
        "leading spaces in value",
        build("GET", "/", "   " + GOOD),
        "reject",
        "informational: library skips one OWS",
    )
    case(
        "lowercase header name",
        build("GET", "/", None, f"authorization: {GOOD}\r\n".encode()),
        200,
    )
    case("Basic TAB hash", build("GET", "/", "Basic\t" + HASH), 401)
    case("Basic TAB space hash", build("GET", "/", "Basic \t" + HASH), 401)
    case(
        "duplicate header bad then good",
        build("GET", "/", "Basic " + wp, f"Authorization: {GOOD}\r\n".encode()),
        "reject",
        "informational",
    )
    case(
        "duplicate header good then bad",
        build("GET", "/", GOOD, f"Authorization: Basic {wp}\r\n".encode()),
        "reject",
        "informational",
    )


def heavy() -> None:
    h0, u0 = heap(), uptime()
    p(f"heap before oversized {h0} uptime {u0}")
    case("8KB Authorization", build("GET", "/", "Basic " + "A" * 8192), "reject")
    case(
        "8KB Authorization valid prefix",
        build("GET", "/", GOOD + " " + "A" * 8192),
        "reject",
    )
    case(
        "32KB garbage header",
        build("GET", "/", None, b"X-Garbage: " + b"Z" * 32768 + b"\r\n"),
        "reject",
    )
    case(
        "64KB header no newline",
        b"GET / HTTP/1.1\r\nHost: x\r\nX-G: " + b"Q" * 65536,
        "reject",
    )
    case(
        "200 headers",
        build(
            "GET",
            "/",
            None,
            b"".join(f"X-H{i}: {'v' * 60}\r\n".encode() for i in range(200)),
        ),
        "reject",
    )
    body, ct = mp_body(random.randbytes(256 * 1024))
    case(
        "noauth POST /update 256KB",
        build("POST", "/update", None, body=body, ctype=ct),
        "reject",
        timeout=30,
    )
    body, ct = mp_body(random.randbytes(64 * 1024))
    case(
        "bad-auth POST /update 64KB",
        build(
            "POST",
            "/update",
            "Basic " + HASH[:-1] + flip(HASH[-1]),
            body=body,
            ctype=ct,
        ),
        "reject",
        timeout=30,
    )
    time.sleep(3)
    h1, u1 = heap(), uptime()
    p(f"heap after oversized {h1} uptime {u1}")
    ROWS.append(
        {
            "case": "heap oversized",
            "before": h0,
            "after": h1,
            "uptime": [u0, u1],
            "pass": h1 is not None and u1 is not None and u0 is not None and u1 > u0,
        }
    )

    # Brute force
    time.sleep(2)
    h0, u0 = heap(), uptime()
    codes: dict = {}
    t = time.monotonic()
    for i in range(A.brute):
        bad = base64.b64encode(
            f"{USER}:{''.join(random.choices(string.ascii_letters, k=len(PASSWORD)))}".encode()
        ).decode()
        r = raw(
            build("GET", "/" if i % 2 else "/sensor/Fast%20counter", "Basic " + bad),
            timeout=5,
            read_body=False,
        )
        k = r["status"] if r["status"] is not None else r["err"]
        codes[k] = codes.get(k, 0) + 1
    dt = time.monotonic() - t
    time.sleep(3)
    h1, u1 = heap(), uptime()
    bf_ok = set(codes) <= {401} and u1 is not None and u0 is not None and u1 > u0
    p(
        f"{'PASS' if bf_ok else 'FAIL'} brute {A.brute} in {dt:.1f}s ({A.brute / dt:.1f}/s): {codes}; heap {h0}->{h1}"
        f" uptime {u0}->{u1}"
    )
    ROWS.append(
        {
            "case": f"brute force {A.brute}",
            "codes": {str(k): v for k, v in codes.items()},
            "secs": dt,
            "heap": [h0, h1],
            "uptime": [u0, u1],
            "pass": bf_ok,
        }
    )

    # Concurrency: bad-cred hammer threads while a valid client streams /events
    h0, u0 = heap(), uptime()
    stop = time.monotonic() + A.conc
    stats = {
        "bad": {},
        "sse_bytes": 0,
        "sse_states": 0,
        "sse_maxgap": 0.0,
        "sse_opens": 0,
        "sse_closes": 0,
    }
    lock = threading.Lock()

    def hammer() -> None:
        while time.monotonic() < stop:
            r = raw(
                build("GET", "/", "Basic " + HASH[:-1] + flip(HASH[-1])),
                timeout=5,
                read_body=False,
            )
            k = str(r["status"] if r["status"] is not None else r["err"])
            with lock:
                stats["bad"][k] = stats["bad"].get(k, 0) + 1

    def sse() -> None:
        last = time.monotonic()
        while time.monotonic() < stop:
            try:
                s = socket.create_connection((HOST, 80), timeout=10)
                s.sendall(
                    build("GET", "/events", GOOD, b"Accept: text/event-stream\r\n")
                )
                stats["sse_opens"] += 1
                s.settimeout(1)
                while time.monotonic() < stop:
                    try:
                        d = s.recv(8192)
                    except TimeoutError:
                        continue
                    if not d:
                        stats["sse_closes"] += 1
                        break
                    now = time.monotonic()
                    n = d.count(b"event: state")
                    if n:
                        stats["sse_maxgap"] = max(stats["sse_maxgap"], now - last)
                        last = now
                    stats["sse_states"] += n
                    stats["sse_bytes"] += len(d)
                s.close()
            except OSError:
                time.sleep(0.5)

    ts = [threading.Thread(target=sse)] + [
        threading.Thread(target=hammer) for _ in range(A.conc_threads)
    ]
    for th in ts:
        th.start()
    for th in ts:
        th.join()
    time.sleep(3)
    h1, u1 = heap(), uptime()
    c_ok = (
        set(stats["bad"]) <= {"401"}
        and stats["sse_states"] > A.conc * 2
        and u1 is not None
        and u0 is not None
        and u1 > u0
    )
    p(
        f"{'PASS' if c_ok else 'FAIL'} concurrency {A.conc:.0f}s x{A.conc_threads}: bad {stats['bad']}; SSE states "
        f"{stats['sse_states']} ({stats['sse_states'] / A.conc:.1f}/s) opens {stats['sse_opens']} closes "
        f"{stats['sse_closes']} max gap {stats['sse_maxgap']:.1f}s; heap {h0}->{h1} uptime {u0}->{u1}"
    )
    ROWS.append(
        {
            "case": "concurrency",
            **stats,
            "heap": [h0, h1],
            "uptime": [u0, u1],
            "pass": c_ok,
        }
    )


def mann_whitney(a: list[float], b: list[float]) -> tuple[float, float]:
    """U and two sided normal-approximation z (no tie correction; fine for continuous timings)."""
    allv = sorted([(v, 0) for v in a] + [(v, 1) for v in b])
    ranks = [0.0] * len(allv)
    i = 0
    while i < len(allv):
        j = i
        while j + 1 < len(allv) and allv[j + 1][0] == allv[i][0]:
            j += 1
        for k in range(i, j + 1):
            ranks[k] = (i + j) / 2 + 1
        i = j + 1
    r1 = sum(r for r, (_, g) in zip(ranks, allv, strict=False) if g == 0)
    n1, n2 = len(a), len(b)
    u = r1 - n1 * (n1 + 1) / 2
    mu, sd = n1 * n2 / 2, (n1 * n2 * (n1 + n2 + 1) / 12) ** 0.5
    return u, (u - mu) / sd


def timing() -> None:
    first = "Basic " + flip(HASH[0]) + HASH[1:]
    last = "Basic " + HASH[:-1] + flip(HASH[-1])
    res: dict[str, list[float]] = {"first": [], "last": []}
    bad = 0
    for i in range(A.timing * 2):
        which = "first" if (i % 2 == 0) ^ (random.random() < 0.5) else "last"
        r = raw(
            build("GET", "/sensor/Fast%20counter", first if which == "first" else last),
            timeout=5,
            read_body=False,
        )
        if r["status"] == 401 and r.get("ttfb"):
            res[which].append(r["ttfb"] * 1000)
        else:
            bad += 1
    out = {}
    for k, v in res.items():
        out[k] = {
            "n": len(v),
            "mean_ms": statistics.mean(v),
            "median_ms": statistics.median(v),
            "p10": statistics.quantiles(v, n=10)[0],
            "p90": statistics.quantiles(v, n=10)[-1],
        }
    u, z = mann_whitney(res["first"], res["last"])
    p(
        f"timing (ms to first byte of 401): first-byte-wrong {out['first']}; last-byte-wrong {out['last']}; "
        f"Mann-Whitney U {u:.0f} z {z:.2f} (|z|<1.96: no difference at 5%); non-401 {bad}"
    )
    ROWS.append(
        {"case": "timing", **out, "u": u, "z": z, "non401": bad, "pass": abs(z) < 1.96}
    )


def ota(path: str) -> None:
    data = pathlib.Path(path).read_bytes()
    body, ct = mp_body(data)
    u0 = uptime()
    r = raw(build("POST", "/update", GOOD, body=body, ctype=ct), timeout=120)
    p(
        f"OTA with credentials: status {r['status']} body {r['body'][:60]!r} err {r['err']} uptime before {u0}"
    )
    ok = r["status"] == 200
    for _ in range(60):
        time.sleep(2)
        u1 = uptime()
        if u1 is not None and u0 is not None and u1 < u0:
            p(f"device back after OTA, uptime {u1}")
            break
    else:
        ok = False
        p("device not seen rebooted after OTA")
    ROWS.append(
        {"case": "good POST /update (real OTA)", "status": r["status"], "pass": ok}
    )


def main() -> None:
    global A  # noqa: PLW0603
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("user")
    ap.add_argument("password")
    ap.add_argument("out")
    ap.add_argument("--only", default="")
    ap.add_argument("--brute", type=int, default=600)
    ap.add_argument("--timing", type=int, default=300)
    ap.add_argument("--conc", type=float, default=60)
    ap.add_argument("--conc-threads", type=int, default=4)
    ap.add_argument("--ota", default="")
    ap.add_argument("--skip-heavy", action="store_true")
    A = ap.parse_args()
    configure(A.host, USER, PASSWORD, A.out)
    only = set(filter(None, A.only.split(",")))
    if not only or "cases" in only:
        run_cases()
    if (not only and not A.skip_heavy) or "heavy" in only:
        heavy()
    if (not only and not A.skip_heavy) or "timing" in only:
        timing()
    if A.ota:
        ota(A.ota)
    pathlib.Path(A.out + "-cases.json").write_text(
        json.dumps(ROWS, indent=1, default=str), encoding="utf-8"
    )
    fails = [r["case"] for r in ROWS if not r.get("pass")]
    p(f"TOTAL {len(ROWS)} rows, fails {len(fails)}: {fails}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
