"""check.py CAPTURE: judge one sun sweep capture (one full up and down sweep).

Each of the five triggers must fire exactly once: the fires counter logged by the sweep must reach 5, and no trigger
may fire twice. Lines can be garbled on some serial adapters, so the counter is the authority. Also prints the
seconds past the minute of the fire lines: with one shared elevation poll they are all the same second.
"""

from collections import Counter
import pathlib
import re
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    fires = [int(n) for n in re.findall(r"fires (\d+)", text)]
    names = re.findall(r"\[fire[^\]]*\]: (sun\w+ [-\w]+) at (-?[\d.]+)", text)
    secs = Counter(m for m in re.findall(r"\[\d\d:\d\d:(\d\d)\]\[I\]\[fire", text))
    starts = len(re.findall(r"start lon", text))
    print(
        f"sweep starts {starts}, final fires counter {fires[-1] if fires else 'none'}"
    )
    for name, elev in names:
        print(f"  {name} at {elev}")
    print(f"seconds past the minute of the fire lines: {dict(sorted(secs.items()))}")
    bad = []
    if starts != 1:
        bad.append(f"{starts} sweep starts (reset during the run?)")
    if not fires or fires[-1] != 5:
        bad.append(f"fires counter {fires[-1] if fires else 'missing'}, want 5")
    dup = [n for n, c in Counter(n for n, _ in names).items() if c > 1]
    if dup:
        bad.append(f"fired more than once: {dup}")
    print("FAIL: " + "; ".join(bad) if bad else "PASS")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
