"""Run the ds248x fault phases against the DS2482-800 simulator and summarise them.

usage: SC=path/to/serial_capture.py drive.py TAG SIM_PORT DUT_PORT [PHASES]
PHASES is a comma list of c, cd, z, b, k, dead (default c,z,b,k,dead). Holds the simulator port for the whole run
(log and commands); the DUT log is captured per phase into TAG-<phase>.log, each phase starting with a DUT reset.
Exits 1 when the simulator never reports or a phase got no readings at all.
"""

import os
import pathlib
import re
import subprocess
import sys
import threading
import time

import serial

TAG, SIM, DUT = sys.argv[1:4]
PHASES = sys.argv[4].split(",") if len(sys.argv) > 4 else ["c", "z", "b", "k", "dead"]
CAP = [sys.executable, os.environ["SC"]]
EXPECT = {"t0": 21.5, "t1": 33.0, "t2a": -5.25, "t2b": 40.0}
ANSI = re.compile(r"\x1b\[[0-9;]*m")
COUNTERS = (
    "wrong_ch",
    "nacked",
    "drst",
    "ops_no_apu",
    "inj_c",
    "inj_z",
    "inj_b",
    "inj_k",
    "readsp",
)

sim = serial.Serial(SIM, 115200, timeout=0.2)
sim_lines: list[tuple[float, str]] = []
stop = threading.Event()
failed: list[str] = []


def sim_reader() -> None:
    buf = b""
    while not stop.is_set():
        buf += sim.read(4096)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            sim_lines.append(
                (time.time(), ANSI.sub("", line.decode(errors="replace")).strip())
            )


def send(c: str) -> None:
    sim.write(c.encode())
    sim.flush()


def last_cnt(before: float) -> dict[str, int]:
    for t, line in reversed(sim_lines):
        if t <= before and "CNT " in line:
            body = (
                line.split("CNT ", 1)[1]
                .replace("inj c=", "inj_c=")
                .replace(" z=", " inj_z=")
            )
            body = body.replace(" b=", " inj_b=").replace(" k=", " inj_k=")
            return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", body)}
    return {}


def phase(
    name: str, seconds: int, actions: list[tuple[float, str]], pre: str = ""
) -> None:
    if pre:
        send(pre)
        time.sleep(0.3)
    out = f"{TAG}-{name}.log"
    p = subprocess.Popen(
        [*CAP, DUT, str(seconds), out, "--reset"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    t0 = time.time()
    for at, c in actions:
        while time.time() - t0 < at:
            time.sleep(0.05)
        send(c)
    p.wait()
    time.sleep(5.5)  # one more simulator report
    a, b = last_cnt(t0), last_cnt(time.time())
    delta = {k: b.get(k, 0) - a.get(k, 0) for k in COUNTERS}
    log = ANSI.sub("", pathlib.Path(out).read_text(encoding="utf-8", errors="replace"))
    temps = re.findall(r"'(\w+)': Got Temperature=(-?[\d.]+)", log)
    per = dict.fromkeys(EXPECT, 0)
    wrong = []
    for s, v in temps:
        per[s] = per.get(s, 0) + 1
        if abs(float(v) - EXPECT.get(s, 999)) > 0.01:
            wrong.append(f"{s}={v}")
    counts = {
        "crc": len(re.findall(r"[Cc]hecksum|CRC", log)),
        "held_low": log.count("held low"),
        "busy_to": log.count("busy timeout"),
        "recov_fail": log.count("recovery failed"),
        "marked_failed": len(re.findall(r"was marked as failed", log)),
        "warn_set": len(re.findall(r"ds248x set Warning", log)),
        "warn_clear": len(re.findall(r"ds248x cleared Warning", log)),
        "hub_failed_bus": log.count("DS248x hub failed"),
    }
    more = "..." if len(wrong) > 6 else ""
    print(
        f"{TAG} {name}: readings={per} wrong={wrong[:6]}{more} n_wrong={len(wrong)} {counts} sim={delta}",
        flush=True,
    )
    if not temps:
        failed.append(name)


RUNS = {
    "c": lambda: phase("c", 90, [(12 + 6 * i, "c") for i in range(12)]),
    "cd": lambda: phase("cd", 100, [(10 + 3 * i, "c") for i in range(28)]),
    "z": lambda: phase("z", 50, [(12 + 5 * i, "z") for i in range(6)]),
    "b": lambda: phase("b", 40, [(12, "b")]),
    "k": lambda: phase("k", 40, [(12, "k")]),
    "dead": lambda: phase("dead", 45, [(25, "X")], pre="x"),
}


def main() -> int:
    threading.Thread(target=sim_reader, daemon=True).start()
    deadline = time.time() + 15
    while not any("CNT " in line for _, line in sim_lines):
        if time.time() > deadline:
            print(
                "FAIL: no CNT line from the simulator in 15 s (flashed dssim.yaml? SIM_PORT right?)"
            )
            return 1
        time.sleep(0.2)
    for name in PHASES:
        RUNS[name]()
    send("X")
    stop.set()
    if failed:
        print(f"FAIL: no readings at all in {failed}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
