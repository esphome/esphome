"""Run the ds248x fault phases against the DS2482-800 simulator and summarise them.

usage: SC=path/to/serial_capture.py drive.py TAG SIM_PORT DUT_PORT [PHASES]
PHASES is a comma list of c, cd, z, b, k, dead (default c,z,b,k,dead). Holds the simulator port for the whole run
(log and commands); the DUT log is captured per phase into TAG-<phase>.log, each phase starting with a DUT reset.
Exits 1 when the simulator never reports, the simulator port fails, the DUT capture fails, a phase ends without a
fresh simulator report, a sensor got no readings in a phase, any temperature is wrong, a fault phase never moved its
injection counter (counted when the DUT meets the fault) or, for a NACK fault, the nacked counter, a fault is still
pending at the end of a phase, the simulator counted a config write with a wrong complement (cfg_bad) or an unknown
command or argument (bad_cmd), a phase that does not reset the chip (all but b and k) counted a ROM match on the wrong
channel (wrong_ch) or a 1-Wire command without the active pull-up bit (ops_no_apu), the simulator never confirmed
going dead, or the DUT log shows no sign of the dead hub (no response, warning, failed hub or failed component) in the
dead phase. DUT capture stderr goes to TAG-<phase>.err.
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
# every sensor dspd.yaml configures, with the temperature the simulator serves
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
    "cfg_bad",
    "bad_cmd",
)

sim = serial.Serial(SIM, 115200, timeout=0.2)
sim_lines: list[tuple[float, str]] = []
stop = threading.Event()
failed: list[str] = []
reader_error: list[BaseException] = []


def sim_reader() -> None:
    buf = bytearray()
    while not stop.is_set():
        try:
            buf += sim.read(4096)
        except Exception as err:  # noqa: BLE001 - reported by main() and per phase
            reader_error.append(err)
            print(f"FAIL: simulator port read failed: {err!r}", flush=True)
            return
        *lines, rest = buf.split(b"\n")
        buf = bytearray(rest)
        now = time.time()
        sim_lines.extend(
            (now, ANSI.sub("", line.decode(errors="replace")).strip()) for line in lines
        )


def send(c: str) -> None:
    sim.write(c.encode())
    sim.flush()


def last_cnt(before: float) -> dict[str, int]:
    for t, line in reversed(sim_lines):
        if t <= before and "CNT " in line:
            body = line.split("CNT ", 1)[1]
            return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", body)}
    return {}


def phase(
    name: str, seconds: int, actions: list[tuple[float, str]], pre: str = ""
) -> None:
    if pre:
        n_pre = len(sim_lines)
        send(pre)
        time.sleep(0.3)
        if want := CONFIRMED_BY.get(pre):
            deadline = time.time() + 3
            while not any(line.endswith(want) for _, line in sim_lines[n_pre:]):
                if time.time() > deadline:
                    failed.append(
                        f"{name} (simulator never logged {want} after {pre!r})"
                    )
                    break
                time.sleep(0.05)
    out = pathlib.Path(f"{TAG}-{name}.log")
    out.unlink(missing_ok=True)
    with pathlib.Path(f"{TAG}-{name}.err").open("wb") as err:
        p = subprocess.Popen(
            [*CAP, DUT, str(seconds), str(out), "--reset"],
            stdout=subprocess.DEVNULL,
            stderr=err,
        )
        t0 = time.time()
        for at, c in actions:
            time.sleep(max(0.0, t0 + at - time.time()))
            send(c)
        if rc := p.wait():
            failed.append(f"{name} (DUT capture exited {rc}, see {TAG}-{name}.err)")
    n0 = len(sim_lines)
    send("s")  # one more simulator report
    deadline = time.time() + 6
    while time.time() < deadline and not any(
        "CNT " in line for _, line in sim_lines[n0:]
    ):
        time.sleep(0.05)
    fresh = any("CNT " in line for _, line in sim_lines[n0:])
    a, b = last_cnt(t0), last_cnt(time.time())
    delta = {k: b.get(k, 0) - a.get(k, 0) for k in COUNTERS}
    log = out.read_text(encoding="utf-8", errors="replace") if out.exists() else ""
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
        "no_resp": log.count("did not respond"),
    }
    more = "..." if len(wrong) > 6 else ""
    print(
        f"{TAG} {name}: readings={per} wrong={wrong[:6]}{more} n_wrong={len(wrong)} {counts} sim={delta}",
        flush=True,
    )
    if silent := [s for s, n in per.items() if s in EXPECT and not n]:
        failed.append(f"{name} (no readings from {silent})")
    if wrong:
        failed.append(f"{name} ({len(wrong)} wrong temperatures)")
    if (inj := INJECTED_BY.get(name)) and not delta[inj]:
        failed.append(f"{name} ({inj} never moved, the fault was not injected)")
    if name in NACK_PHASES and not delta["nacked"]:
        failed.append(f"{name} (nacked never moved, the DUT never got the NACK)")
    if name in DUT_SAW and not any(counts[k] for k in DUT_SAW[name]):
        failed.append(
            f"{name} (no {'/'.join(DUT_SAW[name])} in the DUT log, the DUT never saw the fault)"
        )
    if malformed := {k: delta[k] for k in ("cfg_bad", "bad_cmd") if delta[k]}:
        failed.append(f"{name} (malformed writes {malformed})")
    if name not in RESET_PHASES and (
        stale := {k: delta[k] for k in ("wrong_ch", "ops_no_apu") if delta[k]}
    ):
        failed.append(f"{name} (stale channel or lost active pull-up {stale})")
    if b.get("pend"):
        failed.append(f"{name} (a fault was still pending at the end of the phase)")
    if not fresh or not a or reader_error:
        print(f"{TAG} {name}: no fresh simulator report, sim counters are not valid")
        failed.append(f"{name} (no simulator report)")


# fault phase -> simulator counter that must move during it
INJECTED_BY = {"c": "inj_c", "cd": "inj_c", "z": "inj_z", "b": "inj_b", "k": "inj_k"}
# fault phases whose fault is a NACK
NACK_PHASES = {"c", "cd", "z", "k"}
# fault phases that reset the chip (channel 0, config cleared) under the driver; the 1-Wire commands left in the
# interrupted transaction may then run on the wrong channel or without APU, so wrong_ch and ops_no_apu are not gated
RESET_PHASES = {"b", "k"}
# phase -> DUT log counts of which at least one must be non-zero
DUT_SAW = {"dead": ("no_resp", "warn_set", "hub_failed_bus", "marked_failed")}
# pre command -> simulator log line that confirms it took effect
CONFIRMED_BY = {"x": "DEAD"}

# name: (seconds, actions, pre)
RUNS: dict[str, tuple[int, list[tuple[float, str]], str]] = {
    "c": (90, [(12 + 6 * i, "c") for i in range(12)], ""),
    "cd": (100, [(10 + 3 * i, "c") for i in range(28)], ""),
    "z": (50, [(12 + 5 * i, "z") for i in range(6)], ""),
    "b": (40, [(12, "b")], ""),
    "k": (40, [(12, "k")], ""),
    "dead": (45, [(25, "X")], "x"),
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
        phase(name, *RUNS[name])
    send("X")
    stop.set()
    if failed:
        print(f"FAIL: {failed}")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
