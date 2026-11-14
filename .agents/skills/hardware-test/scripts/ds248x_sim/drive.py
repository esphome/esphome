"""Run the ds248x fault phases against the DS2482-800 simulator and summarise them.

usage: SC=path/to/serial_capture.py drive.py TAG SIM_PORT DUT_PORT [PHASES]
PHASES is a comma list of c, cd, z, b, k, dead (default c,z,b,k,dead). Holds the simulator port for the whole run
(log and commands); the DUT log is captured per phase into TAG-<phase>.log, each phase starting with a DUT reset.
Exits 1 when the simulator never reports, the simulator port fails, the DUT capture fails, a phase ends without a
fresh simulator report, a sensor got no correct reading after the last fault of the phase had SETTLE seconds to clear
(the recovery window), the DUT logged a CRC, held low, busy timeout or no response line in the recovery window, any
temperature is wrong, a phase other than dead logged a failed recovery, a failed hub or a component marked failed, a
ds248x warning was still set at the end of a phase, a fault phase moved its injection counter (counted when the
DUT meets the fault) by other than the number of faults sent (arm_lost counts arms merged into a pending one) or,
for a NACK fault, never moved the nacked counter, a write delivered
data while a NACK was armed (nack_miss, including a write followed by a repeated start into a read), the DUT sent an address-only write (empty_wr, which would make
nacked ambiguous), a write was cut off by a repeated start into another write and never processed
(dropped), a fault is still pending at the end of a phase, the simulator counted a config write with a wrong complement (cfg_bad) or an unknown
command or argument (bad_cmd), a counter is missing from the simulator CNT line, the simulator aborted a read reply
(tx_abrt, not gated in the dead phase), a phase that does not reset the chip (all but b and k) counted a ROM match on
the wrong channel (wrong_ch) or a 1-Wire command without the active pull-up bit (ops_no_apu), the simulator never
confirmed going dead, or the DUT log shows no sign of the dead hub (no response, warning, failed hub or failed
component) in the dead phase. DUT capture stderr goes to TAG-<phase>.err. The whole phase CRC, held low, busy
timeout and no response counts are printed for information only: the faults cause them.
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
# seconds after the last fault action before the recovery window opens
SETTLE = 3.0
STAMP = re.compile(r"^\[(\d\d):(\d\d):(\d\d)(?:\.(\d{3}))?\]")
# every sensor dspd.yaml configures, with the temperature the simulator serves
EXPECT = {"t0": 21.5, "t1": 33.0, "t2a": -5.25, "t2b": 40.0}
ANSI = re.compile(r"\x1b\[[0-9;]*m")
COUNTERS = (
    "wrong_ch",
    "nacked",
    "nack_miss",
    "empty_wr",
    "dropped",
    "arm_lost",
    "drst",
    "ops_no_apu",
    "inj_c",
    "inj_z",
    "inj_b",
    "inj_k",
    "readsp",
    "cfg_bad",
    "bad_cmd",
    "tx_abrt",
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


def after(log: str, when: float) -> str:
    """Lines of a serial_capture --ms log stamped after the wall clock time `when`."""
    lt = time.localtime(when)
    ref = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec + when % 1
    keep = []
    for line in log.splitlines():
        if m := STAMP.match(line):
            sod = int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4] or 0) / 1000
            if 0 < (sod - ref) % 86400 < 43200:  # survives midnight
                keep.append(line)
    return "\n".join(keep)


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
            [*CAP, DUT, str(seconds), str(out), "--reset", "--ms"],
            stdout=subprocess.DEVNULL,
            stderr=err,
        )
        t0 = last_fault = time.time()
        for at, c in actions:
            time.sleep(max(0.0, t0 + at - time.time()))
            send(c)
            last_fault = time.time()
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
    if missing := [k for k in (*COUNTERS, "pend") if k not in a or k not in b]:
        failed.append(f"{name} (missing counters {missing} on the simulator CNT line)")
    delta = {k: b.get(k, 0) - a.get(k, 0) for k in COUNTERS}
    log = out.read_text(encoding="utf-8", errors="replace") if out.exists() else ""
    temps = re.findall(r"'(\w+)': Got Temperature=(-?[\d.]+)", log)
    per = dict.fromkeys(EXPECT, 0)
    wrong = []
    for s, v in temps:
        per[s] = per.get(s, 0) + 1
        if abs(float(v) - EXPECT.get(s, 999)) > 0.01:
            wrong.append(f"{s}={v}")

    def errors(text: str) -> dict[str, int]:
        return {
            "crc": len(re.findall(r"[Cc]hecksum|CRC", text)),
            "held_low": text.count("held low"),
            "busy_to": text.count("busy timeout"),
            "no_resp": text.count("did not respond"),
        }

    counts = errors(log) | {
        "recov_fail": log.count("recovery failed"),
        "marked_failed": len(re.findall(r"was marked as failed", log)),
        "warn_set": len(re.findall(r"ds248x set Warning", log)),
        "warn_clear": len(re.findall(r"ds248x cleared Warning", log)),
        "hub_failed_bus": log.count("DS248x hub failed"),
    }
    # the recovery window: from SETTLE seconds after the last fault action to the end of the capture
    tail = after(log, last_fault + SETTLE)
    late_ok = dict.fromkeys(EXPECT, 0)
    for s, v in re.findall(r"'(\w+)': Got Temperature=(-?[\d.]+)", tail):
        if s in EXPECT and abs(float(v) - EXPECT[s]) <= 0.01:
            late_ok[s] += 1
    late_err = {k: n for k, n in errors(tail).items() if n}
    more = "..." if len(wrong) > 6 else ""
    print(
        f"{TAG} {name}: readings={per} recovered={late_ok} wrong={wrong[:6]}{more} n_wrong={len(wrong)} {counts} "
        f"late_errors={late_err} sim={delta}",
        flush=True,
    )
    if silent := [s for s, n in late_ok.items() if not n]:
        failed.append(
            f"{name} (no correct reading from {silent} in the recovery window)"
        )
    if late_err:
        failed.append(f"{name} (errors in the recovery window {late_err})")
    if name not in DUT_SAW and (
        hard := {
            k: counts[k]
            for k in ("recov_fail", "marked_failed", "hub_failed_bus")
            if counts[k]
        }
    ):
        failed.append(f"{name} (failed recovery, hub or component {hard})")
    warn = re.findall(r"ds248x (set|cleared) Warning", log)
    if warn and warn[-1] == "set":
        failed.append(f"{name} (ds248x warning still set at the end of the phase)")
    if wrong:
        failed.append(f"{name} ({len(wrong)} wrong temperatures)")
    if (inj := INJECTED_BY.get(name)) and delta[inj] != (
        sent := sum(c == inj[-1] for _, c in actions)
    ):
        failed.append(
            f"{name} ({inj} moved {delta[inj]} for {sent} faults sent, arm_lost={delta['arm_lost']})"
        )
    if name in NACK_PHASES and not delta["nacked"]:
        failed.append(f"{name} (nacked never moved, the DUT never got the NACK)")
    if name in DUT_SAW and not any(counts[k] for k in DUT_SAW[name]):
        failed.append(
            f"{name} (no {'/'.join(DUT_SAW[name])} in the DUT log, the DUT never saw the fault)"
        )
    if delta["nack_miss"]:
        failed.append(
            f"{name} ({delta['nack_miss']} writes delivered data while a NACK was armed)"
        )
    if delta["empty_wr"]:
        failed.append(
            f"{name} ({delta['empty_wr']} address-only writes, nacked cannot tell them from NACKs)"
        )
    if delta["dropped"]:
        failed.append(
            f"{name} ({delta['dropped']} writes cut off by a repeated start, never processed)"
        )
    if malformed := {k: delta[k] for k in ("cfg_bad", "bad_cmd") if delta[k]}:
        failed.append(f"{name} (malformed writes {malformed})")
    if name not in RESET_PHASES and (
        stale := {k: delta[k] for k in ("wrong_ch", "ops_no_apu") if delta[k]}
    ):
        failed.append(f"{name} (stale channel or lost active pull-up {stale})")
    if name not in ABORT_OK_PHASES and delta["tx_abrt"]:
        failed.append(f"{name} (simulator aborted {delta['tx_abrt']} read replies)")
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
# phases that switch the controller off, which may abort a read reply in flight
ABORT_OK_PHASES = {"dead"}
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
