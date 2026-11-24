"""zbdrv.py: helpers to drive the ESP32-H2 console coordinator through coord.fifo and parse coord.log.

The files live in ZBDIR (default: this directory); zbcon.py writes coord.log and reads coord.fifo.
ZBSRC=0xSHORT keeps only responses from one source address when two devices are on the network.

CLI:
  zbdrv.py mark TEXT
  zbdrv.py waitjoin SINCE_LINE TIMEOUT        -> prints short address of the next commissioned device
  zbdrv.py interrogate LABEL SHORT            -> active_ep, simple_desc, read attrs per cluster
  zbdrv.py send "cmd" [wait]
  zbdrv.py parse FROM_MARK [TO_MARK]          -> one line per response/report between marks
  zbdrv.py epbydesc FROM_MARK DESCRIPTION     -> endpoint whose Basic description (0x001C) read DESCRIPTION
  zbdrv.py lines                              -> current line count of coord.log (a SINCE_LINE for waitjoin)
"""

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


def send(cmd, wait=1.5):
    with FIFO.open("w", encoding="utf-8") as f:
        f.write(cmd + "\n")
    time.sleep(wait)


def mark(text):
    with LOG.open("a", encoding="utf-8") as f:
        f.write(time.strftime("%H:%M:%S") + " ### HWT " + text + "\n")


def wait_for(pattern, start, timeout):
    end = time.time() + timeout
    rx = re.compile(pattern)
    while time.time() < end:
        ls = lines()
        for i in range(start, len(ls)):
            m = rx.search(ls[i])
            if m:
                return i, m
        time.sleep(0.5)
    return None, None


def waitjoin(since, timeout):
    i, m = wait_for(
        r"New device commissioned or rejoined \(short: (0x[0-9a-f]{4})\)",
        since,
        timeout,
    )
    return m.group(1) if m else None


def interrogate(label, short, extra_eps=()):
    mark(f"{label} interrogate {short}")
    n = len(lines())
    send(f"zdo request active_ep -d {short}", 3)
    i, m = wait_for(r"active ep: \[([0-9, ]*)\]", n, 15)
    if not m:
        print("NO ACTIVE EP")
        return {}
    eps = [int(x) for x in m.group(1).split(",") if x.strip()]
    print("active eps", eps)
    clusters = {}
    for ep in eps:
        n = len(lines())
        send(f"zdo request simple_desc -e {ep} -d {short}", 3)
        i, m = wait_for(r"in: \[([0-9a-fx, ]*)\]", n, 15)
        cl = [int(x, 16) for x in m.group(1).split(",") if x.strip()] if m else []
        clusters[ep] = cl
        dl = lines()[i - 1] if i else ""
        print("ep", ep, dl.split(" ", 1)[-1], "in", [hex(c) for c in cl])
    mark(f"{label} reads")
    for ep, cl in clusters.items():
        send(f"zcl send_gen read -d {short} --dst-ep {ep} -e 1 -c 0 -a 0x0005", 2)
        for c in cl:
            src = 2 if c in (0x0006, 0x0008, 0x0300) else 1
            for a in ATTRS.get(c, []):
                send(
                    f"zcl send_gen read -d {short} --dst-ep {ep} -e {src} -c {c:#06x} -a {a:#06x}",
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
    out = []
    phase = ""
    for i, ln in enumerate(ls):
        if "### HWT" in ln:
            phase = ln.split("### HWT ", 1)[1]
            out.append(("MARK", ln[:8], phase))
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
        if FILT and ms and ms.group(1) != FILT:
            continue
        if kind == "Default response":
            mc = re.search(r"command\((0x[0-9a-f]+)\), status\((0x[0-9a-f]+)\)", blk)
            out.append(
                (
                    kind,
                    ln[:8],
                    src,
                    cl,
                    mc.group(1) if mc else "?",
                    f"status {mc.group(2) if mc else '?'}",
                )
            )
            continue
        mt = re.search(
            r"attribute\((0x[0-9a-f]+)\), (type|status)\((0x[0-9a-f]+)\)", blk
        )
        if not mt:
            out.append((kind, ln[:8], src, cl, "?", "?"))
            continue
        if mt.group(2) == "status":
            v = f"status {mt.group(3)}"
        else:
            mv = re.search(r"0x4[0-9a-f]{7}   ((?:[0-9a-f]{2} {1,2})+)", blk)
            v = decode(mv.group(1), mt.group(3)) if mv else "?"
        out.append((kind, ln[:8], src, cl, mt.group(1), v))
    return out


def cli():
    a = sys.argv[1]
    if a == "mark":
        mark(" ".join(sys.argv[2:]))
    elif a == "waitjoin":
        s = waitjoin(int(sys.argv[2]), float(sys.argv[3]))
        print(s or "NOJOIN")
    elif a == "interrogate":
        interrogate(sys.argv[2], sys.argv[3])
    elif a == "send":
        send(sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 1.5)
    elif a == "parse":
        ls = lines()
        s = max(i for i, ln in enumerate(ls) if "### HWT " + sys.argv[2] in ln)
        e = len(ls)
        if len(sys.argv) > 3:
            e = next(
                (i for i in range(s, len(ls)) if "### HWT " + sys.argv[3] in ls[i]),
                len(ls),
            )
        for r in parse(ls[s : e + 1]):
            if r[0] == "MARK":
                print(f"{r[1]} ### {r[2]}")
            else:
                print(
                    f"{r[1]} {r[0][:6]:6} ep {r[2]:>3} cl {r[3]} attr {r[4]} -> {r[5]}"
                )
    elif a == "epbydesc":
        ls = lines()
        s = max(i for i, ln in enumerate(ls) if "### HWT " + sys.argv[2] in ln)
        for r in parse(ls[s:]):
            if (
                r[0].startswith("Read")
                and r[4] == "0x001c"
                and r[5] == repr(sys.argv[3])
            ):
                print(r[2])
                break
    elif a == "lines":
        print(len(lines()))


if __name__ == "__main__":
    cli()
