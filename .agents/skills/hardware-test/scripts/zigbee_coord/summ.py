"""summ.py NAME: per-build summary of one flow (the lines between the NAME start and NAME done marks), with the
reports grouped per endpoint, cluster and phase, so a PR and a dev run can be diffed."""

import collections
import pathlib
import re
import subprocess
import sys

D = pathlib.Path(__file__).resolve().parent


def main(n: str) -> None:
    out = subprocess.run(
        [sys.executable, str(D / "zbdrv.py"), "parse", f"{n} start", f"{n} done"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.splitlines()
    phase = ""
    reps: dict[tuple, list] = collections.defaultdict(list)
    rows = []
    for line in out:
        if "###" in line:
            phase = line.split("### ")[1].replace(n + " ", "")
            continue
        m = re.match(r"\S+ (\S+(?: a)?)\s+ep\s+(\S+) cl (\S+) attr (\S+) -> (.*)", line)
        if not m:
            continue
        kind, ep, cl, attr, v = m.groups()
        if kind.startswith("Report"):
            reps[(int(ep), cl, phase.split(" ")[0])].append(v)
        else:
            rows.append(f"[{phase[:26]:26}] {kind} ep {ep} cl {cl} attr {attr} -> {v}")
    print("\n".join(r for r in rows if "attr 0x0005" not in r))
    for k, v in sorted(reps.items()):
        print(f"reports ep {k[0]} cl {k[1]} during '{k[2]}': values={sorted(set(v))}")


main(sys.argv[1])
