"""an.py SERIAL: count readings, NaNs, probe results, crashes and reboots per PHASE of the vlpf.yaml capture."""

import pathlib
import re
import sys


def main() -> int:
    phases: list[tuple[str, dict[str, int]]] = []
    cur, c = "boot", {}
    fired = (0, 0)

    def bump(k: str) -> None:
        c[k] = c.get(k, 0) + 1

    for line in (
        pathlib.Path(sys.argv[1])
        .read_text(encoding="utf-8", errors="replace")
        .splitlines()
    ):
        if m := re.search(r"PHASE (\w+)", line):
            phases.append((cur, c))
            cur, c = m.group(1), {}
            continue
        if m := re.search(r"READ (\S+)", line):
            bump("nan" if m.group(1) == "nan" else "ok")
        if m := re.search(r"PROBE (\w+)", line):
            bump("probe_" + m.group(1))
        if m := re.search(r"fired a=(\d+) b=(\d+)", line):
            fired = (int(m.group(1)), int(m.group(2)))
        if "update called before prior reading" in line:
            bump("pending_warn")
        if "Running through setup" in line:
            bump("boot")
        if re.search(r"CRASH|Guru", line):
            bump("crash")
    phases.append((cur, c))
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
    # or reboot, and both faults must fire
    bad = [p for p, counts in phases if counts.get("crash")]
    base_ok = phases[1][1].get("ok", 0)
    if not base_ok:
        bad.append("first quiet phase had no readings")
    bad += [
        f"{p} {i} ok={counts.get('ok', 0)} of {base_ok}"
        for i, (p, counts) in enumerate(phases)
        if i > 1 and p == "quiet" and counts.get("ok", 0) * 2 < base_ok
    ]
    bad += [
        f"{p} reboot"
        for i, (p, counts) in enumerate(phases)
        if counts.get("boot", 0) > (1 if i == 0 else 0)
    ]
    bad += [f"fault {k} never fired" for k, n in zip("AB", fired, strict=True) if not n]
    if bad:
        print(f"FAIL: {bad}")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
