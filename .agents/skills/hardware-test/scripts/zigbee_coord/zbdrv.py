"""zbdrv.py: helpers to drive the ESP32-H2 console coordinator through coord.fifo and parse coord.log.

The files live in ZBDIR (default: this directory); zbcon.py writes coord.log and reads coord.fifo.
ZBSRC=0xSHORT keeps only responses from one source address when two devices are on the network.

CLI:
  zbdrv.py mark TEXT
  zbdrv.py pos                                -> current size of coord.log (a SINCE for waitjoin)
  zbdrv.py waitjoin SINCE TIMEOUT             -> prints short address of the next commissioned device
  zbdrv.py interrogate LABEL SHORT            -> active_ep, simple_desc, read attrs per cluster; saves int-LABEL.json
  zbdrv.py eps LABEL HAS [NOT]                -> endpoints of that interrogate with cluster HAS and without NOT
  zbdrv.py send "cmd" [wait]
  zbdrv.py parse FROM_MARK [TO_MARK]          -> one line per response/report between marks
  zbdrv.py epbydesc FROM_MARK DESC...         -> per DESC, the endpoint whose Basic description (0x001C) read DESC
  zbdrv.py f32 VALUE                          -> VALUE as the wire bytes `zcl send_gen write -v` takes for a float
"""

import json
import os
import pathlib
import re
import struct
import sys
import time

Z = pathlib.Path(os.environ.get("ZBDIR") or pathlib.Path(__file__).resolve().parent)
LOG = Z / "coord.log"
FIFO = Z / "coord.fifo"

ATTRS = {
    0x000D: [0x001C, 0x0041, 0x0045, 0x006A, 0x0075, 0x0100, 0x0051, 0x006F, 0x0055],
    0x0010: [0x001C, 0x0055],
    0x000C: [0x001C, 0x0075, 0x0055],
    0x000F: [0x001C, 0x0055],
    0x0006: [0x0000],
    0x0008: [0x0000, 0x0002, 0x0003],
    0x0300: [0x0000, 0x0001, 0x0003, 0x0004, 0x0007, 0x0008, 0x400A, 0x400B, 0x400C],
}
FILT = os.environ.get("ZBSRC", "")
TYPES = {
    "0x39": "f",
    "0x10": "b",
    "0x20": "u8",
    "0x21": "u16",
    "0x23": "u32",
    "0x30": "e8",
    "0x31": "e16",
    "0x18": "m8",
    "0x19": "m16",
    "0x42": "s",
    "0xe2": "u32",
}


def lines():
    with LOG.open("rb") as f:
        return f.read().decode(errors="replace").splitlines()


def pos():
    return LOG.stat().st_size if LOG.exists() else 0


def send(cmd, wait=1.5):
    try:  # nonblocking: with no zbcon.py reading the fifo, fail instead of hanging
        fd = os.open(FIFO, os.O_WRONLY | os.O_NONBLOCK)
    except OSError as e:
        sys.exit(f"{FIFO}: {e} (is zbcon.py running?)")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(cmd + "\n")
    time.sleep(wait)


def mark(text):
    with LOG.open("a", encoding="utf-8") as f:
        f.write(time.strftime("%H:%M:%S") + " ### HWT " + text + "\n")


def wait_for(pattern, start, timeout):
    """First match of pattern in the log after byte offset start: (the line before it, match), or ("", None)."""
    end = time.time() + timeout
    rx = re.compile(pattern)
    while True:
        with LOG.open("rb") as f:
            f.seek(start)
            ls = f.read().decode(errors="replace").splitlines()
        for i, ln in enumerate(ls):
            if m := rx.search(ln):
                return (ls[i - 1] if i else ""), m
        if time.time() >= end:
            return "", None
        time.sleep(0.2)


def request(cmd, pattern, timeout):
    """Send cmd and wait until pattern shows up after it."""
    start = pos()
    send(cmd, 0)
    return wait_for(pattern, start, timeout)


def waitjoin(since, timeout):
    _, m = wait_for(
        r"New device commissioned or rejoined \(short: (0x[0-9a-f]{4})\)",
        since,
        timeout,
    )
    return m.group(1) if m else None


def interrogate(label, short):
    mark(f"{label} interrogate {short}")
    _, m = request(
        f"zdo request active_ep -d {short}", r"active ep: \[([0-9, ]*)\]", 15
    )
    if not m:
        print("NO ACTIVE EP")
        return {}
    eps = [int(x) for x in m.group(1).split(",") if x.strip()]
    print("active eps", eps)
    clusters = {}
    for ep in eps:
        dl, m = request(
            f"zdo request simple_desc -e {ep} -d {short}", r"in: \[([0-9a-fx, ]*)\]", 15
        )
        cl = [int(x, 16) for x in m.group(1).split(",") if x.strip()] if m else []
        clusters[ep] = cl
        print("ep", ep, dl.split(" ", 1)[-1], "in", [hex(c) for c in cl])
    mark(f"{label} reads")
    rx = "Read attribute response"
    for ep, cl in clusters.items():
        request(
            f"zcl send_gen read -d {short} --dst-ep {ep} -e 1 -c 0 -a 0x0005", rx, 2
        )
        for c in cl:
            src = 2 if c in (0x0006, 0x0008, 0x0300) else 1
            for a in ATTRS.get(c, []):
                request(
                    f"zcl send_gen read -d {short} --dst-ep {ep} -e {src} -c {c:#06x} -a {a:#06x}",
                    rx,
                    1.6,
                )
    mark(f"{label} reads done")
    return clusters


def decode(hexs, t):
    b = bytes(int(x, 16) for x in hexs.split())
    k = TYPES.get(t, "raw")
    try:
        if k == "f":
            return round(struct.unpack("<f", b[:4])[0], 3)
        if k == "s":
            return repr(b[1 : 1 + b[0]].decode())
        if k in ("b", "u8", "e8", "m8"):
            return b[0]
        if k in ("u16", "e16", "m16"):
            return struct.unpack("<H", b[:2])[0]
        if k == "u32":
            return hex(struct.unpack("<I", b[:4])[0])
    except (struct.error, IndexError, UnicodeDecodeError):
        pass
    return hexs.strip()


def parse(ls):
    """Yields (kind, time, src ep, cluster, attr, value) per response or report, and ("MARK", time, text) per mark."""
    for i, ln in enumerate(ls):
        if "### HWT" in ln:
            yield ("MARK", ln[:8], ln.split("### HWT ", 1)[1])
            continue
        m = re.search(
            r"(Read attribute response|Report attribute|Write attribute response|Default response): "
            r"endpoint\((\d+)\) cluster\((0x[0-9a-f]+)\)",
            ln,
        )
        if not m:
            continue
        kind, lep, cl = m.groups()
        blk = " ".join(ls[i + 1 : i + 5])
        ms = re.search(r"src (0x[0-9a-f]+) ep (\d+)", blk)
        src = ms.group(2) if ms else "?"
        if FILT and (not ms or ms.group(1) != FILT):
            continue
        if kind == "Default response":
            mc = re.search(r"command\((0x[0-9a-f]+)\), status\((0x[0-9a-f]+)\)", blk)
            yield (
                kind,
                ln[:8],
                src,
                cl,
                mc.group(1) if mc else "?",
                f"status {mc.group(2) if mc else '?'}",
            )
            continue
        mt = re.search(
            r"attribute\((0x[0-9a-f]+)\), (type|status)\((0x[0-9a-f]+)\)", blk
        )
        if not mt:
            yield (kind, ln[:8], src, cl, "?", "?")
            continue
        if mt.group(2) == "status":
            v = f"status {mt.group(3)}"
        else:
            mv = re.search(r"0x4[0-9a-f]{7}   ((?:[0-9a-f]{2} {1,2})+)", blk)
            v = decode(mv.group(1), mt.group(3)) if mv else "?"
        yield (kind, ln[:8], src, cl, mt.group(1), v)


def between(ls, start, stop=None):
    """The log lines from the last `start` mark to the first `stop` mark after it (inclusive)."""
    marks = [i for i, ln in enumerate(ls) if "### HWT " + start in ln]
    if not marks:
        sys.exit(f"no mark {start!r} in {LOG}")
    s = marks[-1]
    e = next(
        (i for i in range(s, len(ls)) if stop and "### HWT " + stop in ls[i]),
        len(ls) - 1,
    )
    if stop and "### HWT " + stop not in ls[e]:
        print(
            f"warning: no mark {stop!r} after {start!r}, flow did not finish",
            file=sys.stderr,
        )
    return ls[s : e + 1]


def int_file(label):
    return Z / f"int-{label.replace(' ', '-')}.json"


def cli():
    a = sys.argv[1]
    if a == "mark":
        mark(" ".join(sys.argv[2:]))
    elif a == "pos":
        print(pos())
    elif a == "waitjoin":
        print(waitjoin(int(sys.argv[2]), float(sys.argv[3])) or "NOJOIN")
    elif a == "interrogate":
        clusters = interrogate(sys.argv[2], sys.argv[3])
        int_file(sys.argv[2]).write_text(json.dumps(clusters))
        if not clusters or not all(clusters.values()):
            sys.exit("interrogate incomplete (active_ep or simple_desc timed out)")
    elif a == "eps":
        has, nott = (
            int(sys.argv[3], 16),
            int(sys.argv[4], 16) if len(sys.argv) > 4 else None,
        )
        for ep, cl in json.loads(int_file(sys.argv[2]).read_text()).items():
            if has in cl and nott not in cl:
                print(ep)
    elif a == "send":
        send(sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 1.5)
    elif a == "parse":
        for r in parse(
            between(lines(), sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
        ):
            if r[0] == "MARK":
                print(f"{r[1]} ### {r[2]}")
            else:
                print(
                    f"{r[1]} {r[0][:6]:6} ep {r[2]:>3} cl {r[3]} attr {r[4]} -> {r[5]}"
                )
    elif a == "epbydesc":
        found = {}
        want = {repr(d): d for d in sys.argv[3:]}
        for r in parse(between(lines(), sys.argv[2])):
            if r[0].startswith("Read") and r[4] == "0x001c" and r[5] in want:
                found.setdefault(want[r[5]], r[2])
                if len(found) == len(want):
                    break
        print(" ".join(found.get(d, "?") for d in sys.argv[3:]))
    elif a == "f32":
        print("0x" + struct.pack("<f", float(sys.argv[2])).hex())


if __name__ == "__main__":
    cli()
