"""Decode spitest captures, check every record against the expected wire data and diff two builds.

usage: analyze.py spi<board>.yaml CAPTURE [CAPTURE2]
With two captures (base, PR) also diffs every record, by the majority payload per record so ISR jitter does not
count, and prints the median time per operation. Failures seen in most passes are SYSTEMATIC, the rest sporadic.
"""

from collections import Counter, defaultdict
import pathlib
import re
import statistics
import sys

LINE = re.compile(r"\[spitest(?::\d+)?\]: (.*)")
TX0 = [0x01, 0x80, 0xA5, 0x3C, 0xF0, 0x0F, 0x55]
TX4 = [0xDE, 0xAD, 0xBE, 0xEF]
ANS = [0xC3, 0x5A, 0x81, 0x7E, 0x0F, 0xF0, 0x33, 0xCC]


def rev(b: int) -> int:
    return int(f"{b:08b}"[::-1], 2)


def parse_devs(yaml_path: str) -> dict:
    text = pathlib.Path(yaml_path).read_text(encoding="utf-8")
    devs = {}
    add = r'spitest::add\("(\w+)", id\(\w+\), (\d), (true|false), \d+, \d+, \d+, \d+, (true|false)\)'
    for m in re.finditer(add, text):
        name, mode, lsb, hw = m.groups()
        devs[name] = {
            "mode": int(mode),
            "lsb": lsb == "true",
            "hw": hw == "true",
            "idx": len(devs),
        }
    for d in devs.values():
        d["tag"] = 0x10 * (d["idx"] + 1)
    return devs


def bits_of(bytes_: list[int], lsb: bool) -> list[int]:
    out = []
    for b in bytes_:
        s = f"{b:08b}"
        out.extend(int(c) for c in (s[::-1] if lsb else s))
    return out


def sample(edges: str, idle: int, mode: int) -> list[int] | None:
    cpol, cpha = mode >> 1, mode & 1
    lvl = idle
    seen = False
    out = []
    for c in edges:
        if c == "-":
            continue
        new = 1 if c in "rR" else 0
        if new == lvl:
            return None  # an edge was lost
        lvl = new
        bit = 1 if c in "RF" else 0
        if new != cpol:  # leading edge
            seen = True
            if cpha == 0:
                out.append(bit)
        elif cpha == 1 and seen:
            out.append(bit)
    return out


def load(path: str) -> tuple[list[tuple[int, str]], int, list[dict]]:
    recs = []
    boots = 0
    heap = []
    for raw in pathlib.Path(path).read_bytes().decode(errors="replace").splitlines():
        m = LINE.search(raw)
        if not m:
            continue
        s = m.group(1).strip()
        if s.startswith("START"):
            boots += 1
        if s.startswith("HEAP boot") and recs and recs[-1][1].startswith("START"):
            heap.append(dict(kv.split("=") for kv in s.split()[2:]))
        recs.append((boots, s))
    return recs, boots, heap


def key_and_payload(s: str) -> tuple[tuple | None, str | None]:
    """Split a record into a key and a payload without timing fields."""
    p = s.split()
    if len(p) < 5 or not p[1].isdigit():
        return None, None
    kind = p[0]
    if kind in ("T", "S", "N"):
        key = (kind, p[1], p[2], p[3])
        rest = [x for x in p[4:] if not x.startswith(("us=", "per10=", "ev="))]
        return key, " ".join(rest)
    if kind == "F":
        key = (kind, p[1], p[2], p[3], p[4])
        rest = [x for x in p[5:] if not x.startswith(("per10=", "t1x10="))]
        return key, " ".join(rest)
    return None, None


def fields(s: str) -> dict[str, str]:
    return dict(x.split("=", 1) for x in s.split() if "=" in x)


def check(
    devs: dict, recs: list[tuple[int, str]], board: str
) -> tuple[int, list[str], dict, dict]:
    """Return (n_checks, failures, us per op, clock period x10 per op), both keyed by (device, step)."""
    fails = []
    n = 0
    timing = defaultdict(list)
    per = defaultdict(list)
    capture = any(s.startswith("S ") for _, s in recs)
    frames = defaultdict(list)
    for boot, s in recs:
        p = s.split()
        if p[0] == "F" and "UNTERMINATED" not in s:
            frames[(boot, p[1], p[2], p[3])].append(s)
    for boot, s in recs:
        p = s.split()
        if not p or p[0] not in ("S", "T", "N", "F"):
            continue
        f = fields(s)
        # the USB console can splice or truncate lines; a damaged record must not pass as clean
        if len(p) < 4 or not p[1].isdigit() or (p[0] == "T" and "us" not in f):
            fails.append(f"boot{boot}: malformed record {s}")
            continue
        if p[0] == "T":
            timing[(p[2], p[3])].append(int(f["us"]))
        if p[0] == "F" and "per10" in f and int(f["per10"]) > 0:
            per[(p[2], p[3])].append(int(f["per10"]))
        if "UNTERMINATED" in s:
            fails.append(f"boot{boot}: {s}")
        dev = p[2] if len(p) > 2 else None
        if dev not in devs and dev != "mx":
            continue
        step = int(p[3]) if len(p) > 3 and p[3].isdigit() else None
        d = devs.get(dev)
        if p[0] == "S" and dev != "mx":
            n += 1
            tag = d["tag"]
            lsb = d["lsb"]
            exp_bits = {0: 64, 5: 64, 6: 64, 8: 64, 9: 64, 1: 16, 2: 12, 3: 32, 4: 40}[
                step
            ]

            def fx(bs: list[int], lsb: bool = lsb) -> str:
                return "".join(f"{(rev(b) if lsb else b):02x}" for b in bs)

            errs = []
            if int(f["bits"]) != exp_bits:
                errs.append(f"bits {f['bits']} != {exp_bits}")
            if step in (0, 5, 6, 8, 9) and f["srx"] != fx(TX0 + [tag]):
                errs.append(f"capture got {f['srx']} want {fx(TX0 + [tag])}")
            if step == 4 and f["srx"] != fx(TX4 + [tag]):
                errs.append(f"capture got {f['srx']} want {fx(TX4 + [tag])}")
            if f["cs"] != "2/0":
                errs.append(f"cs edges {f['cs']} != 2/0")
            if errs:
                fails.append(f"boot{boot}: {s} :: {'; '.join(errs)}")
        if p[0] == "S" and dev == "mx":
            n += 1
            if f["bits"] != "128" or f["cs"] != "16/0":
                fails.append(f"boot{boot}: {s} :: want 128 bits and cs 16/0")
            else:
                regs = [int(f["srx"][i : i + 2], 16) for i in range(0, 32, 4)]
                if regs != list(range(8, 0, -1)):
                    fails.append(f"boot{boot}: {s} :: registers {regs}")
        if p[0] == "T" and capture and dev != "mx" and step in (8, 9):
            n += 1
            want = "".join(f"{b:02x}" for b in TX0 + [d["tag"]])
            if f["rx"] != want:
                fails.append(f"boot{boot}: {s} :: loopback read {f['rx']} want {want}")
        if p[0] == "T" and capture and dev != "mx" and step in (0, 3, 5, 6):
            n += 1
            tag = d["tag"]
            want = [(b ^ tag) for b in ANS][: 8 if step != 3 else 4]
            wanth = "".join(f"{(rev(b) if d['lsb'] else b):02x}" for b in want)
            if f["rx"] != wanth:
                fails.append(f"boot{boot}: {s} :: master read {f['rx']} want {wanth}")
        if p[0] == "N":
            n += 1
            nf = int(f["frames"])
            want = 8 if dev == "mx" else 1
            if nf != want or f["drop"] != "0":
                fails.append(f"boot{boot}: {s} :: frames {nf} want {want}")
            fl = frames.get((boot, p[1], p[2], p[3]), [])
            if dev == "mx":
                for fr in fl:
                    ff = fields(fr)
                    bits = sample(ff["e"], int(ff["idle"]), 0)
                    if bits is None or len(bits) != 16 or ff["ov"] != "0":
                        fails.append(f"boot{boot}: {fr} :: mx frame decode {bits}")
                continue
            if not fl and nf:
                fails.append(f"boot{boot}: {s} :: no F record")
            if len(fl) != 1:
                continue
            ff = fields(fl[0])
            if ff["ov"] != "0":
                fails.append(f"boot{boot}: {fl[0]} :: another CS was low")
            exp = None
            if step in (0, 5, 6):
                exp = TX0 + [d["tag"]]
            elif step == 4:
                exp = TX4 + [d["tag"]]
            elif step == 1:
                v = 0x1234 ^ d["tag"]
                exp = [v >> 8, v & 0xFF] if not d["lsb"] else None
            if board == "e66" and d["hw"]:
                continue  # HSPI pads cannot be read back; the T line carries the SPI registers instead
            bits = sample(ff["e"], int(ff["idle"]), d["mode"])
            if bits is None:
                fails.append(f"boot{boot}: {fl[0]} :: lost an edge")
            elif exp is not None:
                n += 1
                want = bits_of(exp, d["lsb"])
                if bits != want:
                    fails.append(f"boot{boot}: {fl[0]} :: decoded {bits} want {want}")
        if p[0] == "T" and not capture and dev != "mx" and step in (0, 5, 6):
            # software bus loopback and the C3 matrix loopback read back what was sent
            loop = (not d["hw"]) or board == "c3"
            if loop:
                n += 1
                want = "".join(f"{b:02x}" for b in TX0 + [d["tag"]])
                if f["rx"] != want:
                    fails.append(
                        f"boot{boot}: {s} :: loopback read {f['rx']} want {want}"
                    )
            elif board == "rp":
                n += 1
                if f["rx"] != "00" * 8:
                    fails.append(f"boot{boot}: {s} :: forced-low MISO read {f['rx']}")
        if (
            p[0] == "T"
            and not capture
            and dev != "mx"
            and step == 3
            and board == "rp"
            and d["hw"]
        ):
            n += 1
            if f["rx"] != "ff" * 4:
                fails.append(f"boot{boot}: {s} :: forced-high MISO read {f['rx']}")
    return n, fails, timing, per


def completeness(recs: list[tuple[int, str]]) -> list[str]:
    """Every boot but the last must reach DONE with one S (wire capture) or N record per scheduled item."""
    kind = "S" if any(s.startswith("S ") for _, s in recs) else "N"
    items: dict[int, int] = {}
    done: set[int] = set()
    count: Counter = Counter()
    for boot, s in recs:
        p = s.split()
        if p and p[0] == "START":
            items[boot] = int(fields(s).get("items", -1))
        elif p and p[0] == "DONE":
            done.add(boot)
        elif len(p) > 1 and p[0] == kind and p[1].isdigit():
            count[boot] += 1
    fails = []
    last = max(items, default=0)
    for boot, want in items.items():
        if boot in done and count[boot] != want:
            fails.append(f"boot{boot}: {count[boot]} {kind} records for {want} items")
        elif boot not in done and boot != last:
            fails.append(
                f"boot{boot}: restarted before DONE after {count[boot]} of {want} items"
            )
    if not done:
        fails.append("no boot reached DONE")
    return fails


def canon(devs: dict, k: tuple | None, pl: str) -> str:
    """F records: compare what a device on the wire would latch, not ISR timing."""
    if k is None or k[0] != "F":
        return pl
    f = fields(pl)
    mode = 0 if k[2] == "mx" else devs[k[2]]["mode"]
    e = f.get("e", "-").replace("-", "")
    bits = sample(e, int(f["idle"]), mode)
    b = "lost" if bits is None else "".join(map(str, bits))
    return f"idle={f['idle']} edges={len(e)} ov={f['ov']} bits={b}"


def variants(devs: dict, recs: list[tuple[int, str]]) -> dict[tuple, Counter]:
    v = defaultdict(Counter)
    for _, s in recs:
        k, pl = key_and_payload(s)
        if k is not None:
            v[k][canon(devs, k, pl)] += 1
    return v


def majority(c: Counter | None) -> str | None:
    return c.most_common(1)[0][0] if c else None


def main() -> None:
    yaml_path = sys.argv[1]
    board = re.search(r"spi(e32|c3|e66|rp)\.yaml$", yaml_path).group(1)
    devs = parse_devs(yaml_path)
    caps = sys.argv[2:]
    results = []
    failed = False
    for c in caps:
        recs, boots, heap = load(c)
        n, fails, timing, per = check(devs, recs, board)
        fails += completeness(recs)
        failed |= n == 0 or bool(fails)
        v = variants(devs, recs)
        occ = Counter()
        for _, s in recs:
            k, _pl = key_and_payload(s)
            if k is not None:
                occ[k] += 1
        fk = defaultdict(list)
        for fl in fails:
            body = fl.split(": ", 1)[-1].split(" :: ")[0]
            k, _pl = key_and_payload(body)
            fk[k].append(fl)
        systematic = {k: f for k, f in fk.items() if k is None or len(f) * 2 > occ[k]}
        sporadic = sum(len(f) for k, f in fk.items() if k not in systematic)
        unstable = {k: pl for k, pl in v.items() if len(pl) > 1}
        done = sum(1 for _, s in recs if s.startswith("DONE"))
        print(
            f"== {c}\n boots={boots} done={done} records={len(recs)} checks={n} failed={len(fails)} "
            f"systematic_keys={len(systematic)} sporadic_fails={sporadic} keys_with_minority_variants={len(unstable)}"
        )
        for k, f in list(systematic.items())[:12]:
            print("  SYSTEMATIC", len(f), "of", occ[k], f[0][:400])
        print(" heap at boot:", [(h.get("free"), h.get("used")) for h in heap])
        results.append((v, timing, per, fails))
    if len(results) == 2:
        (va, ta, pa, _), (vb, tb, pb, _) = results
        keys = set(va) | set(vb)
        diff = [k for k in sorted(keys) if majority(va.get(k)) != majority(vb.get(k))]
        print(
            f"== diff base vs PR (majority payload per key): {len(keys)} keys, {len(diff)} differ"
        )
        for k in diff[:20]:
            print(
                "  DIFF",
                k,
                "base:",
                dict(va.get(k) or {}),
                "pr:",
                dict(vb.get(k) or {}),
            )
        print("== timing (median us per op, base -> PR; clock period from edges)")
        for k in sorted(set(ta) | set(tb)):
            a = statistics.median(ta[k]) if ta.get(k) else None
            b = statistics.median(tb[k]) if tb.get(k) else None
            pa_ = statistics.median(pa[k]) / 10 if pa.get(k) else None
            pb_ = statistics.median(pb[k]) / 10 if pb.get(k) else None
            if k[1] in ("0", "7"):
                print(f"  {k[0]} step{k[1]}: {a} -> {b} us   period {pa_} -> {pb_} us")

    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
