"""Shared helpers for summary.py and pool.py: quantiles and device-side times from devtime.py."""

import json
import pathlib
import re
import statistics
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent


def quantile(v: list[float], q: float) -> float:
    return sorted(v)[int(round(q * (len(v) - 1)))]


def fmt(v: list[float]) -> str:
    if not v:
        return "none"
    return f"{min(v):.2f}/{statistics.median(v):.2f}/{quantile(v, 0.95):.2f}/{max(v):.2f} (n={len(v)})"


def rows(tag: str, sc: str) -> list[dict]:
    with pathlib.Path(f"{tag}-{sc}.jsonl").open(encoding="utf-8") as f:
        return [json.loads(line) for line in f]


def api_times(rs: list[dict]) -> list[float]:
    return [a["t_svc"] for r in rs for a in r.get("detail", []) if a.get("t_svc")]


def devtime(tag: str, rs: list[dict]) -> str:
    """devtime.py output over the scenario's window in cap-TAG.txt."""

    def hms(t: float) -> str:
        return time.strftime("%H:%M:%S", time.localtime(t))

    args = [f"cap-{tag}.txt", hms(rs[0]["ts"] - 60), hms(rs[-1]["ts"] + 5)]
    return subprocess.run(
        [sys.executable, str(HERE / "devtime.py"), *args],
        capture_output=True,
        text=True,
        check=False,
    ).stdout


def link_and_discovery(
    out: str, slot: str | None = None
) -> tuple[list[float], list[float]]:
    """Link-up and discovery-only seconds of resolved attempts (discovery over 0.5 s, so cached ones drop out)."""
    link, disc = [], []
    for line in out.splitlines():
        if slot and f"slot{slot}" not in line:
            continue
        if (m := re.search(r"link=(\d+)ms discovery=(\d+)ms", line)) and int(
            m[2]
        ) > 500:
            link.append(int(m[1]) / 1000)
            disc.append(int(m[2]) / 1000)
    return link, disc
