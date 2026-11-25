"""encheck.py LOG [LOG...]: summarise an entity_transport link test capture.

Reads ETRX <link> <field>=<value>, SESS <link> 0/1, STAT, CHAOS and warning lines; ETRX lines it cannot parse are
counted. Per link: cnt updates, gaps in the received count (a gap is a step other than +1), values never sent (flt not
equal to the sender's formula), binary transitions, long sensor updates, session edges. Prints one block per log.
Exits 1 on a reboot or crash during a capture (more than one boot), a value never sent, a checked stream with bad
payloads, an ETRX line it cannot parse (other than the cut last line), no STAT line, or no counter value on any link.
"""

import collections
import pathlib
import re
import struct
import sys


def f32(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def expect(t):  # 12345.678f + t * 0.001f - (t % 7) * 1000.5f in single precision
    return f32(f32(f32(12345.678) + f32(f32(t) * f32(0.001))) - f32((t % 7) * 1000.5))


VALID = {f"{expect(t):.3f}" for t in range(20000)}
TS = re.compile(r"^\[(\d\d):(\d\d):(\d\d)")


def secs(line):
    m = TS.match(line)
    return int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) if m else None


CRASH = re.compile(r"Guru Meditation|Exception \(\d+\)|abort\(\)|panic|Backtrace:")
failures = []
for path in sys.argv[1:]:
    crashes = 0
    links = collections.defaultdict(
        lambda: {"cnt": [], "flt": [], "bin": [], "long": [], "sess": []}
    )
    warns = collections.Counter()
    stats, chaos, boots, unparsed = [], [], 0, 0
    with pathlib.Path(path).open(encoding="utf-8", errors="replace") as fh:
        for line in fh:
            t = secs(line)
            if "ETRX" in line:
                m = re.search(r"ETRX (\S+) (\w+)=(\S+)", line)
                if m:
                    links[m[1]][m[2]].append((t, m[3]))
                elif "[partial]" not in line:  # the capture cuts a line at its end
                    unparsed += 1
            elif "SESS" in line:
                m = re.search(r"SESS (\S+) (\d)", line)
                if m:
                    links[m[1]]["sess"].append((t, m[2]))
            elif "STAT" in line:
                stats.append(line.strip())
            elif "CHAOS" in line:
                chaos.append((line[1:9], line.strip().split("CHAOS")[1]))
            if "Running through setup" in line:
                boots += 1
            if CRASH.search(line):
                crashes += 1
            if re.search(r"\]\[[WE]\]", line):
                msg = re.sub(r"^\[[^\]]*\]", "", line.strip())
                msg = re.sub(r"\d+", "N", msg)
                warns[msg] += 1
    print(f"== {path}  boots={boots} crashes={crashes} unparsed_etrx={unparsed}")
    if boots > 1 or crashes:
        failures.append(f"{path}: {boots} boots, {crashes} crash lines")
    if unparsed:
        failures.append(f"{path}: {unparsed} ETRX lines that could not be parsed")
    if not stats:
        failures.append(f"{path}: no STAT line")
    if not any(d["cnt"] for d in links.values()):
        failures.append(f"{path}: no link received a counter value")
    for c in chaos:
        print("  chaos", c)
    for name, d in sorted(links.items()):
        tc = [(t, int(float(v))) for t, v in d["cnt"] if v != "nan"]
        cnts = [c for _, c in tc]
        gaps = [
            (tc[i][0], cnts[i - 1], cnts[i])
            for i in range(1, len(cnts))
            if cnts[i] not in (cnts[i - 1], cnts[i - 1] + 1)
        ]
        bad_flt = [v for _, v in d["flt"] if v not in VALID and v != "nan"]
        bins = [v for _, v in d["bin"]]
        trans = sum(1 for i in range(1, len(bins)) if bins[i] != bins[i - 1])
        print(
            f"  {name}: cnt updates={len(cnts)} first={cnts[:1]} last={cnts[-1:]} gaps={len(gaps)} "
            f"flt={len(d['flt'])} bad_flt={len(bad_flt)} bin_trans={trans} long={len(d['long'])} "
            f"sess={[s for _, s in d['sess']]}"
        )
        for g in gaps[:12]:
            at = (
                "?"
                if g[0] is None
                else f"{g[0] // 3600:02d}:{g[0] // 60 % 60:02d}:{g[0] % 60:02d}"
            )
            print(f"     gap at {at}: {g[1]} -> {g[2]}")
        if bad_flt:
            print("     bad flt", bad_flt[:5])
            failures.append(f"{path}: {name} received {len(bad_flt)} values never sent")
    last = {}
    for s in stats:
        body = s[s.find("STAT") :]
        last[body.split()[1]] = body
    for body in last.values():
        print("  " + body)
        if (m := re.search(r" bad=(\d+)", body)) and int(m[1]):
            failures.append(f"{path}: {body.split()[1]} bad={m[1]}")
    for w, n in warns.most_common(15):
        print(f"  W x{n}: {w[:170]}")
for f in failures:
    print("FAIL", f)
sys.exit(1 if failures else 0)
