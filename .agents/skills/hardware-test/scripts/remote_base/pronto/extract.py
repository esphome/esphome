"""extract.py SERIAL OUT: keep MARK, pronto dump, trigger, binary sensor and heap lines from the first MARK on,
with timestamps and source line numbers removed (lognorm.py). Also holds the trigger block parser that consist.py
and jit.py share."""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
from lognorm import normalize  # noqa: E402

TRIG_LEN = "trig: TRIG len="
TRIG = "trig: TRIG "


def trigger_blocks(lines):
    """Yield (n, text, i) per on_pronto trigger: the length it logged, its chunks joined, and the index of the first
    line after the block."""
    i = 0
    while i < len(lines):
        if not lines[i].startswith(TRIG_LEN):
            i += 1
            continue
        n = int(lines[i][len(TRIG_LEN) :])
        i += 1
        text = ""
        while (
            i < len(lines)
            and lines[i].startswith(TRIG)
            and not lines[i].startswith(TRIG_LEN)
        ):
            text += lines[i][len(TRIG) :]
            i += 1
        yield n, text, i


def read_lines(path):
    return pathlib.Path(path).read_text(encoding="utf-8").splitlines()


def main() -> None:
    out, on = [], False
    with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            m = re.match(r"\[(\w)\]\[([\w.]+)\]: ?(.*)$", normalize(raw.rstrip("\n")))
            if not m:
                continue
            _lvl, tag, msg = m.groups()
            if "MARK tx" in msg:
                on = True
            if not on:
                continue
            if (
                tag in ("remote.pronto", "trig", "heap")
                or "MARK" in msg
                or "BS pronto" in msg
                or tag == "remote.nec"
            ):
                out.append(f"{tag}: {msg}")
    pathlib.Path(sys.argv[2]).write_text("\n".join(out) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
