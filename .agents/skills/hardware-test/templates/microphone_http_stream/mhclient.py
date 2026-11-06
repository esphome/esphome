"""mhclient.py HOST STREAM_ID [--clients N] [--seconds S] [--pattern] [--stall K] [--stagger S]

Opens N concurrent GET /audio/microphones/STREAM_ID readers. Each reports its HTTP status, format headers, sample
count and rate, the longest gap between reads, and, with --pattern, every break in the fake microphone's uint16
counter. --stall K makes reader K stop reading after 2 s (the device should drop it, not the others).
Exits 1 when a reader other than the stalled one got no 200, no samples, an early EOF, a read error or (with
--pattern) a broken counter.
"""

import argparse
import http.client
import json
import struct
import sys
import threading
import time


def reader(
    host: str,
    path: str,
    secs: float,
    pattern: bool,
    stall: bool,
    res: list,
    idx: int,
    start_delay: float,
) -> None:
    time.sleep(start_delay)
    r = {
        "status": None,
        "samples": 0,
        "max_gap_ms": 0,
        "breaks": 0,
        "first_break": None,
        "eof": None,
        "rms": 0.0,
    }
    res[idx] = r
    c = http.client.HTTPConnection(host, 80, timeout=10)
    try:
        c.request("GET", path)
        resp = c.getresponse()
        r["status"] = resp.status
        r["headers"] = {
            k: v
            for k, v in resp.getheaders()
            if k.lower().startswith(("content-type", "x-", "transfer"))
        }
        if resp.status != 200:
            r["body"] = resp.read(300).decode(errors="replace")
            return
        t0 = time.time()
        last = t0
        prev = None
        carry = b""
        sq = 0
        while time.time() - t0 < secs:
            if stall and time.time() - t0 > 2:
                time.sleep(secs)  # stop reading, keep the socket open
                break
            data = resp.read1(8192)
            now = time.time()
            if not data:
                r["eof"] = round(now - t0, 2)
                break
            r["max_gap_ms"] = max(r["max_gap_ms"], round((now - last) * 1000))
            last = now
            data = carry + data
            n = len(data) // 2
            carry = data[n * 2 :]
            vals = struct.unpack(f"<{n}h", data[: n * 2])
            r["samples"] += n
            for v in vals:
                sq += v * v
                if pattern:
                    u = v & 0xFFFF
                    if prev is not None and u != (prev + 1) & 0xFFFF:
                        r["breaks"] += 1
                        if r["first_break"] is None:
                            r["first_break"] = (r["samples"], prev, u)
                    prev = u
        r["secs"] = round(time.time() - t0, 2)
        r["rate"] = round(r["samples"] / max(r["secs"], 0.001))
        r["rms"] = round((sq / max(r["samples"], 1)) ** 0.5, 1)
    except Exception as e:  # noqa: BLE001  report and keep the other readers going
        r["error"] = repr(e)
    finally:
        c.close()


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("host")
    p.add_argument("stream")
    p.add_argument("--clients", type=int, default=1)
    p.add_argument("--seconds", type=float, default=20)
    p.add_argument("--pattern", action="store_true")
    p.add_argument("--stall", type=int, default=-1)
    p.add_argument("--stagger", type=float, default=0.2)
    a = p.parse_args()
    c = http.client.HTTPConnection(a.host, 80, timeout=10)
    c.request("GET", "/audio/microphones")
    m = c.getresponse()
    print("META", m.status, m.read().decode(errors="replace")[:400])
    res: list[dict | None] = [None] * a.clients
    th = []
    for i in range(a.clients):
        t = threading.Thread(
            target=reader,
            args=(
                a.host,
                f"/audio/microphones/{a.stream}",
                a.seconds,
                a.pattern,
                i == a.stall,
                res,
                i,
                i * a.stagger,
            ),
        )
        t.start()
        th.append(t)
    for t in th:
        t.join()
    bad = False
    for i, r in enumerate(res):
        print(f"C{i}", json.dumps(r))
        if i != a.stall and (
            r["status"] != 200
            or not r["samples"]
            or r["eof"] is not None
            or "error" in r
            or (a.pattern and r["breaks"])
        ):
            bad = True
    sys.exit(1 if bad else 0)


main()
