"""mhclient.py HOST STREAM_ID [--clients N] [--seconds S] [--pattern] [--stall K] [--stagger S] [--expect-reject R]
                   [--rate HZ [--rate-tol F]] [--max-gap-ms MS] [--min-rms X]

Opens N concurrent GET /audio/microphones/STREAM_ID readers. Each reports its HTTP status, format headers, sample
count and rate, the longest gap between reads, and, with --pattern, every break in the fake microphone's uint16
counter. --stall K makes reader K stop reading after 2 s (the device should drop it, not the others).
--expect-reject R says the last R readers are past max_clients and must be refused: a non-200 status, or the
connection closed or reset before any samples. A timeout, any other error, or a 200 with samples fails those readers.
Exits 1 when the listing is not a 200 naming the stream, the stalled reader was not dropped, a reader past the limit
was not refused, or any other reader got no 200, no samples, an early EOF, a read error, (with --pattern) a broken counter, or a rate,
longest gap or rms outside the --rate, --max-gap-ms or --min-rms limits.
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
                r["dropped"] = dropped(c, resp)
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
        if isinstance(e, ConnectionError) and not r["samples"]:
            r["closed"] = True  # closed or reset before any audio: a refusal
    finally:
        c.close()


def dropped(c: http.client.HTTPConnection, resp: http.client.HTTPResponse) -> bool:
    """Drain what the device sent before it gave up; True when it then closed or reset the socket."""
    c.sock.settimeout(2)
    end = time.time() + 10
    try:
        while time.time() < end:
            if not resp.read1(65536):
                return True
    except TimeoutError:
        return False
    except (OSError, http.client.HTTPException):
        return True  # reset, or closed mid-chunk (IncompleteRead)
    return False


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("host")
    p.add_argument("stream")
    p.add_argument("--clients", type=int, default=1)
    p.add_argument("--seconds", type=float, default=20)
    p.add_argument("--pattern", action="store_true")
    p.add_argument("--stall", type=int, default=-1)
    p.add_argument("--stagger", type=float, default=0.2)
    p.add_argument("--expect-reject", type=int, default=0)
    p.add_argument(
        "--rate",
        type=float,
        help="expected samples per second (fail outside --rate-tol)",
    )
    p.add_argument("--rate-tol", type=float, default=0.05)
    p.add_argument(
        "--max-gap-ms",
        type=float,
        help="fail when the longest gap between reads is larger",
    )
    p.add_argument(
        "--min-rms",
        type=float,
        help="fail when the rms is lower (0 catches an all zero stream)",
    )
    a = p.parse_args()
    if a.stall >= a.clients:
        p.error("--stall must name one of the readers (0 to clients - 1)")
    if not 0 <= a.expect_reject < a.clients - (a.stall >= 0):
        p.error("--expect-reject must leave at least one normal reader")
    if a.stall >= a.clients - a.expect_reject:
        p.error("--stall must not name a reader that is expected to be refused")
    c = http.client.HTTPConnection(a.host, 80, timeout=10)
    c.request("GET", "/audio/microphones")
    m = c.getresponse()
    body = m.read().decode(errors="replace")
    c.close()
    print("META", m.status, body[:400])
    bad = m.status != 200 or a.stream not in body
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
    for i, r in enumerate(res):
        print(f"C{i}", json.dumps(r))
        failed = (
            r["status"] != 200
            or not r["samples"]
            or r["eof"] is not None
            or "error" in r
            or (a.pattern and r["breaks"])
            or (a.rate and abs(r.get("rate", 0) - a.rate) > a.rate * a.rate_tol)
            or (a.max_gap_ms is not None and r.get("max_gap_ms", 0) > a.max_gap_ms)
            or (a.min_rms is not None and r.get("rms", 0.0) < a.min_rms)
        )
        if i == a.stall:
            # judged on the drop, plus anything that went wrong before it stopped reading
            bad |= not r.get("dropped") or r["status"] != 200 or not r["samples"]
            bad |= bool(a.pattern and r["breaks"])
        elif i >= a.clients - a.expect_reject:
            refused = (r["status"] is not None and r["status"] != 200) or (
                not r["samples"] and (r["eof"] is not None or r.get("closed"))
            )
            if not refused:
                why = r.get("error") or f"status {r['status']}, {r['samples']} samples"
                print(f"C{i} is past max_clients but was not refused: {why}")
            bad |= not refused
        else:
            bad |= bool(failed)
    sys.exit(1 if bad else 0)


main()
