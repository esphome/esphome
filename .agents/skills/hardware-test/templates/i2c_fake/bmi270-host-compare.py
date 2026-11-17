"""bmi270-host-compare.py BASE.report PR.report: compare every scenario of the harness between base and PR.

Streams: base's split INIT_ADDR writes (W 5b xx / W 5c yy) are merged before comparing. NACK scenarios are matched by
the normalized transaction they hit. LOG lines are compared separately and every difference is printed."""

import pathlib
import re
import sys

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def parse(path):
    scen, cur = {}, None
    with pathlib.Path(path).open(encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = ANSI.sub("", line.rstrip("\n"))
            if line.startswith("SCENARIO "):
                cur = scen.setdefault(
                    line.split()[1], {"head": "", "log": [], "tx": [], "other": []}
                )
            elif line.startswith("NACK_TOTAL"):
                continue
            elif line.startswith("  failed="):
                cur["head"] = re.sub(r" setup_tx=\d+", "", line.strip())
            elif line.startswith("  LOG "):
                cur["log"].append(line[6:])
            elif line.startswith("  TX "):
                cur["tx"].append(line[5:])
            else:
                cur["other"].append(
                    re.sub(r" (aux_w|aux_r|status_reads)=\d+", "", line.strip())
                )
    return scen


def merge(tx):
    """Returns the merged list and, per original index (0 based), the merged index."""
    out, idx, i = [], [], 0
    while i < len(tx):
        a = tx[i]
        if (
            a.startswith("W 5b ")
            and len(a.split()) == 3
            and i + 1 < len(tx)
            and tx[i + 1].startswith("W 5c ")
            and len(tx[i + 1].split()) == 3
        ):
            out.append(a + " " + tx[i + 1].split()[2])
            idx += [len(out) - 1, len(out) - 1]
            i += 2
            continue
        out.append(a)
        idx.append(len(out) - 1)
        i += 1
    return out, idx


b, p = parse(sys.argv[1]), parse(sys.argv[2])
problems = 0
logdiffs = {}
for name in sorted(n for n in p if not n.startswith(("nack_", "step_"))):
    sb, sp = b[name], p[name]
    same_head = sb["head"] == sp["head"]
    same_other = sb["other"] == sp["other"]
    line = f"{name:20s} head={'same' if same_head else 'DIFF'} regs/values/upload={'same' if same_other else 'DIFF'}"
    if sp["tx"]:
        mb, _ = merge(
            [t for t in sb["tx"] if not t.startswith("A ")] if False else sb["tx"]
        )
        line += f" stream base={len(sb['tx'])}->{len(mb)} pr={len(sp['tx'])} {'same' if mb == sp['tx'] else 'DIFF'}"
        if mb != sp["tx"]:
            problems += 1
    if not (same_head and same_other):
        problems += 1
        line += f"\n    base: {sb['head']} {sb['other']}\n    pr:   {sp['head']} {sp['other']}"
    print(line)
    if sb["log"] != sp["log"]:
        key = (
            tuple(x for x in sb["log"] if x not in sp["log"]),
            tuple(x for x in sp["log"] if x not in sb["log"]),
        )
        logdiffs.setdefault(key, []).append(name)

# NACK sweep, matched through the merged aux stream
mb, idx = merge(b["cfg_aux"]["tx"])
mb_main = [t for t in b["cfg_aux"]["tx"] if not t.startswith("A ")]
# indexes counted over main-bus transactions only (the fake numbers those)
main_b = [t for t in b["cfg_aux"]["tx"] if not t.startswith("A ")]
main_p = [t for t in p["cfg_aux"]["tx"] if not t.startswith("A ")]
mmb, midx = merge(main_b)
assert mmb == main_p, "main streams differ"
nack_outcomes = {}
checked = 0
nack_total = int(
    [
        line
        for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
        if line.startswith("NACK_TOTAL")
    ][0].split()[1]
)
for i in range(nack_total):
    k = midx[i]
    sb, sp = b[f"nack_{i + 1}"], p[f"nack_{k + 1}"]
    checked += 1
    hb = (sb["head"], tuple(sb["other"]))
    hp = (sp["head"], tuple(sp["other"]))
    if hb != hp:
        problems += 1
        print(
            f"NACK base tx {i + 1} / pr tx {k + 1} ({main_p[k][:20]}): DIFF\n    base {hb}\n    pr   {hp}"
        )
    wb = [x for x in sb["log"] if x[:1] in "EW"]
    wp = [x for x in sp["log"] if x[:1] in "EW"]
    nack_outcomes.setdefault((sp["head"].split(" status")[0], tuple(wp)), []).append(
        k + 1
    )
    if wb != wp:
        key = (tuple(x for x in wb if x not in wp), tuple(x for x in wp if x not in wb))
        logdiffs.setdefault(key, []).append(f"nack_pr{k + 1}")
print(
    f"NACK scenarios checked: {checked} base transactions -> {len(main_p)} pr transactions"
)
print("PR NACK outcome classes (head, E/W log lines): pr transaction numbers")
for (h, w), ks in sorted(nack_outcomes.items(), key=lambda x: x[1][0]):
    ks = sorted(set(ks))
    print(f"  {h} | {' / '.join(w)} | tx {ks[0]}..{ks[-1]} ({len(ks)})")
print("BMM150 step failures (failed/mag_ready, values, W lines): base vs PR")
for name in sorted(n for n in p if n.startswith("step_")):
    sb, sp = b[name], p[name]
    vb = [x for x in sb["other"] if x.startswith(("REGS", "VALUES"))]
    vp = [x for x in sp["other"] if x.startswith(("REGS", "VALUES"))]
    if sb["head"] != sp["head"] or vb != vp:
        problems += 1
        print(
            f"  {name}: DIFF head/regs/values\n    base {sb['head']} {vb}\n    pr   {sp['head']} {vp}"
        )
    print(
        f"  {name:32s} {sp['head'].split(' status')[0]} | mag {[x for x in vp if x.startswith('VALUES')][0].split(' t=')[1] if vp else '-'}"
    )
    print(f"      base W: {[x for x in sb['log'] if x[:1] in 'EW']}")
    print(f"      pr   W: {[x for x in sp['log'] if x[:1] in 'EW']}")
print("LOG differences (only in base | only in PR): scenarios")
for (ob, op), names in logdiffs.items():
    names = sorted(set(names))
    print(
        f"  base-only {list(ob)} | pr-only {list(op)} : {len(names)} e.g. {names[:4]}"
    )
print("RESULT", "SAME" if problems == 0 else f"DIFFERENT ({problems})")
