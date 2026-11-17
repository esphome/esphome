"""web_server flow: page loads + REST GETs, SSE soak, REST control. usage: flow.py HOST OUT_PREFIX [PAGE_S] [SSE_S]"""

import pathlib
import socket
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import rest  # noqa: E402
from rest import heap, jget, req  # noqa: E402

HOST, OUT = sys.argv[1], sys.argv[2]
rest.HOST = HOST
PAGE_S = float(sys.argv[3]) if len(sys.argv) > 3 else 180
SSE_S = float(sys.argv[4]) if len(sys.argv) > 4 else 180
log = pathlib.Path(OUT + "-flow.txt").open("w", encoding="utf-8")  # noqa: SIM115


def p(*a) -> None:
    s = " ".join(str(x) for x in a)
    print(s, flush=True)
    log.write(s + "\n")
    log.flush()


p("heap start", heap(), "uptime", rest.uptime())

# 1. pages
gets = [
    "/sensor/Fast%20counter",
    "/switch/Test%20switch",
    "/number/Test%20number",
    "/light/Test%20light",
    "/metrics",
]
res: dict[str, list] = {}
end = time.monotonic() + PAGE_S
i = 0
while time.monotonic() < end:
    for path in ("/", gets[i % len(gets)]):
        code, body, dt = req(path)
        res.setdefault(path, []).append((code, len(body), dt))
    i += 1
    time.sleep(max(0.0, PAGE_S / 36 - 0.5))
for path, rows in res.items():
    ok = sum(1 for c, _, _ in rows if c == 200)
    p(
        f"page {path}: {ok}/{len(rows)} ok, bytes {max(b for _, b, _ in rows)}, max {max(d for _, _, d in rows):.2f}s, "
        f"codes {sorted({c for c, _, _ in rows})}"
    )
pages_ok = sum(1 for rows in res.values() for c, _, _ in rows if c == 200)
pages_n = sum(len(rows) for rows in res.values())
p(f"PAGES {pages_ok}/{pages_n}")

# 2. SSE
states = pings = closes = opens = 0
gaps: list[float] = []
last = None
maxgap = 0.0
end = time.monotonic() + SSE_S
while time.monotonic() < end:
    try:
        s = socket.create_connection((HOST, 80), timeout=10)
        s.sendall(
            f"GET /events HTTP/1.1\r\nHost: {HOST}\r\nAccept: text/event-stream\r\n\r\n".encode()
        )
        opens += 1
        s.settimeout(1.0)
        buf = b""
        while time.monotonic() < end:
            try:
                d = s.recv(8192)
            except TimeoutError:
                d = None
            now = time.monotonic()
            if d == b"":
                closes += 1
                p(f"sse closed by device after {now - (end - SSE_S):.1f}s")
                break
            if d:
                buf += d
                n = buf.count(b"event: state")
                if n:
                    if last is not None:
                        gap = now - last
                        maxgap = max(maxgap, gap)
                        if gap > 3:
                            gaps.append(round(gap, 1))
                    last = now
                    states += n
                pings += buf.count(b"event: ping")
                buf = buf[buf.rfind(b"\n") + 1 :] if b"\n" in buf else buf[-64:]
        s.close()
    except Exception as e:  # noqa: BLE001
        p("sse error", type(e).__name__)
        time.sleep(1)
p(
    f"SSE {SSE_S:.0f}s: states {states} ({states / SSE_S:.1f}/s), pings {pings}, opens {opens}, device closes {closes}, "
    f"max gap {maxgap:.1f}s, gaps>3s {len(gaps)} {gaps[:10]}"
)

# 3. REST control
before = jget("/switch/Test%20switch").get("value")
c1 = req("/switch/Test%20switch/toggle", "POST")[0]
time.sleep(0.5)
after = jget("/switch/Test%20switch").get("value")
c2 = req("/number/Test%20number/set?value=42", "POST")[0]
time.sleep(0.5)
num = jget("/number/Test%20number").get("value")
c3 = req("/light/Test%20light/turn_on?brightness=128", "POST")[0]
time.sleep(0.5)
lt = jget("/light/Test%20light")
ctl_ok = (
    before != after
    and c1 == 200
    and c2 == 200
    and float(num or -1) == 42
    and c3 == 200
    and lt.get("state") == "ON"
    and lt.get("brightness") == 128
)
p(
    f"CONTROL switch {before}->{after} ({c1}), number {num} ({c2}), light {lt.get('state')} b={lt.get('brightness')} ({c3}) "
    f"=> {'OK' if ctl_ok else 'FAIL'}"
)
p("heap end", heap(), "uptime", rest.uptime())
