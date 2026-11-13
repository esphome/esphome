"""check.py CAP...: match every published hx711 value against the values the fake chip served.

Exits non-zero when a capture published a value the fake never served, published nothing, or crashed.
"""

import pathlib
import re
import sys


def check(path: str) -> bool:
    with pathlib.Path(path).open(encoding="utf-8", errors="replace") as f:
        t = f.read()
    served = {int(v) for v in re.findall(r"HX read \S+ value (-?\d+)", t)}
    got = [int(v) for v in re.findall(r"Got value (-?\d+)", t)]
    bad = [v for v in got if v not in served]
    nan = len(re.findall(r"ENT [AB] nan", t))
    short = len(re.findall(r"short transfer", t))
    crash = len(re.findall(r"Guru|abort\(\)|Backtrace:|task_wdt: Task", t))
    print(
        f"{path} published {len(got)} matching {len(got) - len(bad)} unmatched {bad[:5]} nan {nan} short {short}"
        f" pd {t.count('HX power down')} pu {t.count('HX power up')} crash {crash}"
    )
    return bool(got) and not bad and not crash


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    ok = [check(p) for p in sys.argv[1:]]
    return 0 if all(ok) else 1


if __name__ == "__main__":
    sys.exit(main())
