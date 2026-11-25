"""summ.py CAPTURE...: setup, heap and register lines, then reading means, spread and acceleration magnitude."""

import pathlib
import re
import statistics as st
import sys

PATTERNS = (
    "SETUP ",
    "HEAP [a-z]+[0-9]",
    "REGS after_setup",
    "Config load|upload failed|FAILED|BMM150|Level calibration|STEP ",
    "rst cause|Exception|Soft WDT",
)
KEYS = ("ax", "ay", "az", "gx", "gy", "gz", "t")


def rows_of(text: str, tag: str) -> list[dict[str, float]]:
    found = re.findall(rf"{tag} (.*)", text)
    return [
        {k: float(v) for k, v in re.findall(r"(\w+)=(-?[\d.]+|nan)", line)}
        for line in found
    ]


for path in sys.argv[1:]:
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    print("==", path.replace("-cap.txt", ""))
    for pat in PATTERNS:
        for line in re.findall(rf".*(?:{pat}).*", text):
            print("  ", line.split("]: ", 1)[-1][:230])
    rows = [
        r for r in rows_of(text, "VALUES") if r.get("ax") == r.get("ax")
    ]  # drop NaN rows
    if rows:
        spread = " ".join(
            f"{k}={st.mean(r[k] for r in rows):.4f}±{st.pstdev(r[k] for r in rows):.4f}"
            for k in KEYS
        )
        print(f"   n={len(rows)} {spread}")
        mag = st.mean((r["ax"] ** 2 + r["ay"] ** 2 + r["az"] ** 2) ** 0.5 for r in rows)
        print(f"   |a| mean={mag:.4f} g")
    if mag_rows := rows_of(text, "MAG"):
        print("   mag first/last:", mag_rows[0], mag_rows[-1])
