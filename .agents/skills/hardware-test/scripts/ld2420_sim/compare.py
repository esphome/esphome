"""compare.py BASE_RUN PR_RUN: PASS when two lddrive.py runs saw the same states after the steps button.

Also requires the fake radar to have answered every setup command (`cmds N acks N`), a Distance and a Presence state
in the sequence, and no ERROR log line. Compare runs with the same index: the second run of a build starts from the
first run's last states.
"""

import pathlib
import re
import sys


def load(path: str) -> tuple[list[str], list[str]]:
    lines = (
        pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    )
    seq, bad, started = [], [], False
    for line in lines:
        if re.search(r"PRESS (Energy|Simple) steps", line):
            started = True
        elif started and " STATE " in line:
            seq.append(line.split(" STATE ", 1)[1])
    if (
        not (m := re.search(r"cmds (\d+) acks (\d+)", "\n".join(lines)))
        or m[1] != m[2]
        or m[1] == "0"
    ):
        bad.append("the fake radar did not answer every setup command")
    if not any(s.startswith("Distance") for s in seq) or not any(
        s.startswith("Presence") for s in seq
    ):
        bad.append("no Distance or Presence state after the steps button")
    if any(re.search(r"\[E\]", line) for line in lines):
        bad.append("ERROR log line")
    return seq, bad


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    base, base_bad = load(sys.argv[1])
    pr, pr_bad = load(sys.argv[2])
    for label, seq in (("base", base), ("PR", pr)):
        print(f"{label}: " + "; ".join(seq))
    bad = [f"base: {b}" for b in base_bad] + [f"PR: {b}" for b in pr_bad]
    if base != pr:
        bad.append("the state sequences differ")
    print("FAIL: " + "; ".join(bad) if bad else "PASS")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
