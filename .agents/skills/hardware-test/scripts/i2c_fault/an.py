"""an.py SERIAL: count readings, NaNs, probe results, crashes and reboots per PHASE of the vlpf.yaml capture."""

import pathlib
import re
import sys


def main() -> int:
    phases: list[tuple[str, dict[str, int]]] = []
    cur, c = "boot", {}
    # fired totals at the start of the current phase and the latest seen; PHASE lines carry the totals so far
    start = fired = (0, 0)

    def close(totals: tuple[int, int]) -> None:
        c["fired_a"], c["fired_b"] = (n - s for n, s in zip(totals, start, strict=True))
        phases.append((cur, c))

    def bump(k: str) -> None:
        c[k] = c.get(k, 0) + 1

    for line in (
        pathlib.Path(sys.argv[1])
        .read_text(encoding="utf-8", errors="replace")
        .splitlines()
    ):
        if m := re.search(r"PHASE (\w+)(?: fired a=(\d+) b=(\d+))?", line):
            if m.group(2) is not None:
                fired = (int(m.group(2)), int(m.group(3)))
            close(fired)
            start = fired
            cur, c = m.group(1), {}
            continue
        if m := re.search(r"READ (\S+)", line):
            # newlib prints a NaN as "nan" or "-nan"
            bump("nan" if "nan" in m.group(1).lower() else "ok")
        if m := re.search(r"PROBE (\w+) 0x([0-9A-Fa-f]{2})", line):
            bump("probe_" + m.group(1))
            if m.group(1) == "ok" and m.group(2).upper() != "EE":
                bump("probe_bad_id")
        if m := re.search(r"fired a=(\d+) b=(\d+)", line):
            fired = (int(m.group(1)), int(m.group(2)))
        if "update called before prior reading" in line:
            bump("pending_warn")
        if "Running through setup" in line:
            bump("boot")
        if re.search(r"CRASH|Guru", line):
            bump("crash")
    close(fired)
    for i, (p, counts) in enumerate(phases):
        print(i, p, counts)
    print(f"fired a={fired[0]} b={fired[1]}")
    seq = [p for p, _ in phases[1:]]
    if seq != ["quiet", "faultA", "quiet", "faultB", "quiet"]:
        print(
            f"FAIL: phase sequence {seq}, the capture ended early or the board restarted"
        )
        return 1
    # quiet phases after a fault phase must read at least half as often as the first quiet phase, nothing may crash
    # or reboot, and each fault must fire in its own phase and nowhere else
    bad = [p for p, counts in phases if counts.get("crash")]
    base_ok = phases[1][1].get("ok", 0)
    if not base_ok:
        bad.append("first quiet phase had no readings")
    bad += [
        f"{p} {i} ok={counts.get('ok', 0)} of {base_ok}"
        for i, (p, counts) in enumerate(phases)
        if i > 1 and p == "quiet" and counts.get("ok", 0) * 2 < base_ok
    ]
    # a target out of range reads NaN in every phase; when the first quiet phase had none, a NaN later means the
    # driver had not recovered from the fault
    if not phases[1][1].get("nan"):
        bad += [
            f"{p} {i} nan={counts['nan']}"
            for i, (p, counts) in enumerate(phases)
            if i > 1 and p == "quiet" and counts.get("nan")
        ]
    # same for the driver's "update called before prior reading" warning: a later quiet phase means a stall
    if not phases[1][1].get("pending_warn"):
        bad += [
            f"{p} {i} pending_warn={counts['pending_warn']}"
            for i, (p, counts) in enumerate(phases)
            if i > 1 and p == "quiet" and counts.get("pending_warn")
        ]
    bad += [
        f"{p} reboot"
        for i, (p, counts) in enumerate(phases)
        if counts.get("boot", 0) > (1 if i == 0 else 0)
    ]
    # the model ID probe tells a dead chip from a stalled driver: quiet phases must answer with 0xEE, and the 10 s
    # probe interval gives every quiet phase at least one PROBE line
    bad += [
        f"{p} {i} probe_ok={counts.get('probe_ok', 0)} probe_err={counts.get('probe_err', 0)} "
        f"probe_bad_id={counts.get('probe_bad_id', 0)}"
        for i, (p, counts) in enumerate(phases)
        if p == "quiet"
        and (
            counts.get("probe_err")
            or counts.get("probe_bad_id")
            or not counts.get("probe_ok")
        )
    ]
    for i, (p, counts) in enumerate(phases):
        for k in "ab":
            n = counts.get(f"fired_{k}", 0)
            if p == f"fault{k.upper()}" and not n:
                bad.append(f"fault {k.upper()} never fired in its phase {i}")
            elif p != f"fault{k.upper()}" and n:
                bad.append(f"fault {k.upper()} fired {n} times in {p} {i}")
    if bad:
        print(f"FAIL: {bad}")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
