"""an.py SERIAL: count readings, NaNs, probe results, crashes and reboots per PHASE of the vlpf.yaml capture."""

import pathlib
import re
import sys


def main() -> int:
    phases: list[tuple[str, dict[str, int]]] = []
    cur, c = "boot", {}

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
        if "update called before prior reading" in line:
            bump("pending_warn")
        if "Running through setup" in line:
            bump("boot")
        if re.search(r"CRASH|Guru", line):
            bump("crash")
    phases.append((cur, c))
    for i, (p, counts) in enumerate(phases):
        print(i, p, counts)
    # quiet phases after a fault phase must read again, and nothing may crash
    bad = [p for p, counts in phases if counts.get("crash")]
    bad += [p for p, counts in phases[1:] if p == "quiet" and not counts.get("ok")]
    if len(phases) < 5:
        print(f"FAIL: only {len(phases) - 1} PHASE lines, the capture ended early")
        return 1
    if bad:
        print(f"FAIL: {bad}")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
