"""Sum the bw12 edge test `meas` lines per counter: counted pulses against generated rising edges."""

from collections import defaultdict
import pathlib
import re
import sys

MEAS = re.compile(r"meas:\d+\]: (\w+) raw ([\d.]+) gen_rise (\d+)")
BS = re.compile(r"BS changes (\d+) gen_edges (\d+) state (\d)")


def summarize(path: str) -> None:
    counters: dict[str, list[float]] = defaultdict(lambda: [0.0, 0, 0])
    bs = [0, 0, 0]
    state = "?"
    seen: set[str] = set()
    with pathlib.Path(path).open(encoding="utf-8", errors="replace") as f:
        for line in f:
            if m := MEAS.search(line):
                key = m[1]
                if key not in seen:  # the first window is partial
                    seen.add(key)
                    continue
                c = counters[key]
                c[0] += float(m[2]) / 6  # pulses per minute over a 10 s window
                c[1] += int(m[3])
                c[2] += 1
            elif m := BS.search(line):
                if "BS" not in seen:
                    seen.add("BS")
                    continue
                bs[0] += int(m[1])
                bs[1] += int(m[2])
                bs[2] += 1
                state = m[3]
    print(path.rsplit("/", 1)[-1])
    for key, (counted, rises, windows) in sorted(counters.items()):
        ratio = f"{counted / rises:.3f}" if rises else "n/a (no generated edges)"
        print(
            f"  {key}: windows {windows} counted {counted:.0f} gen_rise {rises} ratio_to_rise {ratio}"
        )
    print(f"  BS: windows {bs[2]} changes {bs[0]} gen_edges {bs[1]} last_state {state}")


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        summarize(arg)
