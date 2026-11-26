"""summ.py NAME: per-build summary of one flow (the lines between the NAME start and NAME done marks), with the
reports grouped per endpoint, cluster and phase, so a PR and a dev run can be diffed."""

import collections
import sys

import zbdrv


def main(n: str) -> None:
    phase = ""
    reps: dict[tuple, list] = collections.defaultdict(list)
    for r in zbdrv.parse(zbdrv.between(zbdrv.lines(), f"{n} start", f"{n} done")):
        if r[0] == "MARK":
            phase = r[2].replace(n + " ", "")
            continue
        kind, _, ep, cl, attr, v = r
        if kind.startswith("Report"):
            reps[(int(ep) if ep.isdigit() else -1, cl, phase.split(" ")[0])].append(v)
        elif attr != "0x0005":
            print(f"[{phase[:26]:26}] {kind[:6]} ep {ep} cl {cl} attr {attr} -> {v}")
    for k, v in sorted(reps.items()):
        print(
            f"reports ep {k[0]} cl {k[1]} during '{k[2]}': values={sorted(set(v), key=str)}"
        )


main(sys.argv[1])
