"""json_ab.py HOST OUT : record every JSON document web_server builds, normalized, for a byte-level A/B.

1. /events for 5 s: greeting ping (config), sorting groups, every detail_all listing entry; all must parse.
2. GET /<domain>/<Name>?detail=all for every listed entity, and the plain GET.
3. Drive commands (select, number, text, switch, light, fan, cover, lock) with a /events stream open and
   record the state event each one produced.
Varying values (uptime, counters, the clock based text and the door) are masked. Writes OUT (one line per document,
sorted keys) and prints counts. Exit 1 on any unparsable document.
"""

import json
import pathlib
import socket
import sys
import threading
import time
import urllib.parse
import urllib.request

host, out = sys.argv[1], sys.argv[2]
bad = 0
docs = []
VARY = ("Fast counter", "Long text", "Door")  # entities whose value changes on its own


def varies(oid):
    return any(s in oid for s in VARY)


def mask(obj):
    o = dict(obj)
    if varies(o.get("id", "")):
        for k in ("value", "state"):
            if k in o:
                o[k] = "<masked>"
    if "uptime" in o:
        o["uptime"] = "<masked>"
    return o


def sse(seconds, stop=None):
    s = socket.create_connection((host, 80), timeout=3)
    s.sendall(
        f"GET /events HTTP/1.1\r\nHost: {host}\r\nAccept: text/event-stream\r\n\r\n".encode()
    )
    buf = b""
    end = time.time() + seconds
    s.settimeout(0.3)
    while time.time() < end and not (stop and stop.is_set()):
        try:
            d = s.recv(4096)
        except TimeoutError:
            continue
        except OSError:
            break
        if not d:
            break
        buf += d
    s.close()
    head, _, body = buf.partition(b"\r\n\r\n")
    if b"chunked" in head.lower():
        raw = b""
        while body:
            ln, _, rest = body.partition(b"\r\n")
            try:
                n = int(ln, 16)
            except ValueError:
                break
            raw += rest[:n]
            body = rest[n + 2 :]
        body = raw
    events = []
    for block in (
        body.decode(errors="replace")
        .replace("\r\n", "\n")
        .replace("\r", "\n")
        .split("\n\n")[:-1]
    ):
        ev, data = None, []
        for line in block.split("\n"):
            if line.startswith("event:"):
                ev = line[6:].strip()
            elif line.startswith("data:"):
                data.append(line[5:].lstrip())
        if ev:
            events.append((ev, "\n".join(data)))
    return events


def parse(tag, text):
    global bad  # noqa: PLW0603
    try:
        obj = json.loads(text)
    except ValueError:
        bad += 1
        docs.append(f"{tag} BAD {text[:120]!r}")
        return None
    return obj


ids = []
for ev, data in sse(5):
    if ev in ("ping", "sorting_group", "state"):
        obj = parse(ev, data)
        if obj is None:
            continue
        if ev == "ping" and set(obj) == {"uptime"}:
            continue
        if ev == "state" and "domain" not in obj:
            continue
        if ev == "ping":
            obj.pop("uptime", None)
            obj.pop("comment", None)
        if ev == "state":
            ids.append(obj["id"])
        docs.append(f"listing {ev} " + json.dumps(mask(obj), sort_keys=True))


def get(path, method="GET"):
    req = urllib.request.Request(
        f"http://{host}{path}", method=method, data=b"" if method == "POST" else None
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status, r.read().decode(errors="replace")
    except urllib.error.HTTPError as e:
        return e.code, ""
    except OSError as e:
        return -1, str(e)


rest = 0
for i in ids:
    dom, _, name = i.partition("/")
    for q in ("?detail=all", ""):
        st, body = get(f"/{dom}/{urllib.parse.quote(name)}{q}")
        if dom in {"button", "event"}:
            docs.append(f"rest {i}{q} status {st}")
            continue
        obj = parse(f"rest {i}{q}", body) if st == 200 else None
        rest += obj is not None
        docs.append(
            f"rest {i}{q} {st} "
            + (json.dumps(mask(obj), sort_keys=True) if obj else "")
        )

CMDS = [
    "/select/Big%20select/set?option=Option%2020%20with%20a%20fairly%20long%20label%20that%20pads%20the%20JSON%20document%20out",
    "/number/Level/set?value=42.5",
    "/text/Note/set?value=hello%20json%20arena",
    "/switch/Relay/turn_on",
    "/light/Lamp/turn_on?brightness=128",
    "/fan/Vent/turn_on?speed_level=2",
    "/cover/Shade/set?position=0.25",
    "/lock/Latch/unlock",
    "/button/Poke/press",
]
got = {}
STOP = threading.Event()


def stream():
    for ev, data in sse(120, STOP):
        if ev == "state":
            obj = parse("cmd state", data)
            if obj and "domain" not in obj and not varies(obj["id"]):
                got[obj["id"]] = obj


for c in (
    "/switch/Relay/turn_off",
    "/lock/Latch/lock",
    "/light/Lamp/turn_off",
    "/fan/Vent/turn_off",
    "/cover/Shade/set?position=1.0",
    "/number/Level/set?value=1",
    "/text/Note/set?value=x",
    "/select/Big%20select/set?option=Option%2001%20with%20a%20fairly%20long%20label%20that%20pads%20the%20JSON%20document%20out",
):
    get(c, "POST")
time.sleep(1.5)
t = threading.Thread(target=stream)
t.start()
time.sleep(3)
cmd_ok = 0
for c in CMDS:
    st, _ = get(c, "POST")
    cmd_ok += st == 200
    time.sleep(0.4)
time.sleep(3)
STOP.set()
t.join()
docs.extend(
    "after-cmd " + json.dumps(mask(got[k]), sort_keys=True) for k in sorted(got)
)

pathlib.Path(out).write_text("\n".join(docs) + "\n", encoding="utf-8")
print(
    f"listing entries {len(ids)}, rest docs {rest}, commands ok {cmd_ok}/{len(CMDS)}, state events after commands {len(got)}, bad json {bad}"
)
sys.exit(1 if bad or not ids or not got or cmd_ok != len(CMDS) else 0)
