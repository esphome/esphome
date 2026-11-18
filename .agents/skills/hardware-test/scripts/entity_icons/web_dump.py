"""Dump the web_server view of every entity: the /events listing (detail all) and each REST endpoint with ?detail=all.

usage: web_dump.py HOST OUT
"""

import json
import pathlib
import socket
import sys
import time
import urllib.parse
import urllib.request

HOST, OUT = sys.argv[1], sys.argv[2]


def sse_listing(seconds: float) -> list[dict]:
    s = socket.create_connection((HOST, 80), timeout=5)
    s.sendall(
        b"GET /events HTTP/1.1\r\nHost: %s\r\nAccept: text/event-stream\r\n\r\n"
        % HOST.encode()
    )
    buf = b""
    deadline = time.monotonic() + seconds
    s.settimeout(1.0)
    while time.monotonic() < deadline:
        try:
            data = s.recv(65536)
        except TimeoutError:
            continue
        if not data:
            break
        buf += data
    s.close()
    head, _, body = buf.partition(b"\r\n\r\n")
    if b"chunked" in head.lower():
        out = b""
        while True:
            i = body.find(b"\r\n")
            if i < 0:
                break
            size = int(body[:i].split(b";")[0], 16)
            out += body[i + 2 : i + 2 + size]
            body = body[i + 2 + size + 2 :]
            if size == 0:
                break
        body = out
    rows = []
    for block in body.decode(errors="replace").split("\n\n"):
        ev, data = None, None
        for line in block.split("\n"):
            line = line.rstrip("\r")
            if line.startswith("event:"):
                ev = line[6:].strip()
            elif line.startswith("data:"):
                data = line[5:].strip()
        if ev in ("state", "state_detail_all") and data:
            try:
                obj = json.loads(data)
            except ValueError:
                rows.append({"event": ev, "invalid": data})
                continue
            # bare state updates that land inside the window carry no domain; only the listing rows are kept
            if "domain" in obj:
                rows.append({"event": ev, **obj})
    return rows


def get_json(url: str) -> dict:
    """One retry: the ESP8266 async server occasionally leaves a request unanswered."""
    for attempt in (1, 2):
        try:
            with urllib.request.urlopen(url, timeout=5) as resp:
                return json.loads(resp.read())
        except TimeoutError:
            if attempt == 2:
                raise
            print(f"retrying {url}", flush=True)
    return {}


rows = sse_listing(6)
ids = sorted({r["id"] for r in rows if "id" in r})
rest = []
for ident in ids:
    domain, _, obj = ident.partition("/")
    url = f"http://{HOST}/{domain}/{urllib.parse.quote(obj)}?detail=all"
    try:
        rest.append({"url": url.replace(HOST, "HOST"), **get_json(url)})
    except Exception as e:  # noqa: BLE001
        rest.append({"url": url.replace(HOST, "HOST"), "error": str(e)})
    time.sleep(0.3)
with pathlib.Path(OUT).open("w", encoding="utf-8") as f:
    f.writelines(
        json.dumps(r, sort_keys=True) + "\n"
        for r in sorted(rows, key=lambda r: (r.get("id", ""), r["event"]))
    )
    f.writelines(json.dumps(r, sort_keys=True) + "\n" for r in rest)
print(f"{len(rows)} sse rows, {len(ids)} ids, {len(rest)} rest rows")
