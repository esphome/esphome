"""phase.py CAPTURE...: mean readings before calibrate_level, after it and after clear_calibration (xform.yaml)."""

import pathlib
import re
import statistics as st
import sys

for path in sys.argv[1:]:
    print("==", path)
    phase, data = "before", {}
    for line in (
        pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    ):
        if "STEP calibrate_level success" in line:
            phase = "calibrated"
            continue
        if "STEP clear_calibration" in line:
            phase = "cleared"
            continue
        if (m := re.search(r"VALUES (.*)", line)) and "nan" not in m.group(1):
            pairs = re.findall(r"(\w+)=(-?[\d.]+)", m.group(1))
            data.setdefault(phase, []).append({k: float(v) for k, v in pairs})
        if re.search(r"motion:01[0-2]|Level calibration", line):
            print("  ", line.strip().split("]: ", 1)[-1])
    for phase_name, rows in data.items():
        rows = rows[1:]  # the first sample after a switch is skipped
        if not rows:
            continue
        means = " ".join(
            f"{k}={st.mean(r[k] for r in rows):.4f}"
            for k in ("ax", "ay", "az", "gx", "gy", "gz")
        )
        print(f"  {phase_name:10s} n={len(rows)} {means}")
