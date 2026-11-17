"""Router outage harness: a D1 mini soft AP is the DUT's only network; hold it in reset to make outages.

usage: outage.py DUT_PORT RTR_PORT LABEL OUTAGES [--release-delay S] [--stable S]

Each outage: wait until the DUT has been connected for --stable seconds, hold the router in reset (RTS on EN),
wait until the DUT enters RESTARTING with its fallback AP already up (the portal is active, so this is the
skip-restart branch and a 30 s COOLDOWN follows), wait --release-delay seconds and let the router boot again,
so the AP returns inside the cooldown. Writes LABEL-dut.txt, LABEL-rtr.txt and LABEL-marks.txt (all lines
prefixed with a wall clock timestamp) for an.py.
"""

import argparse
import pathlib
import re
import sys
import threading
import time

import serial

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from an import ends_cooldown  # noqa: E402

ANSI = re.compile(rb"\x1b\[[0-9;]*m")


def ts() -> str:
    t = time.time()
    return time.strftime("%H:%M:%S", time.localtime(t)) + f".{int(t * 1000) % 1000:03d}"


class Reader(threading.Thread):
    def __init__(self, ser: serial.Serial, path: str) -> None:
        super().__init__(daemon=True)
        self.ser = ser
        self.f = pathlib.Path(path).open("w", encoding="utf-8", buffering=1)  # noqa: SIM115
        self.lines: list[tuple[float, str]] = []
        self.lock = threading.Lock()
        self.stop = False

    def run(self) -> None:
        buf = b""
        while not self.stop:
            try:
                chunk = self.ser.read(512)
            except Exception as e:  # noqa: BLE001
                self.f.write(f"{ts()} READ ERROR {e}\n")
                time.sleep(0.5)
                continue
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = ANSI.sub(b"", raw).decode("utf-8", "replace").rstrip("\r")
                now = time.time()
                self.f.write(f"{ts()} {line}\n")
                with self.lock:
                    self.lines.append((now, line))

    def after(self, idx: int) -> tuple[list[str], int]:
        """Lines from index idx on, and the index to pass next time."""
        with self.lock:
            return [line for _, line in self.lines[idx:]], len(self.lines)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dut")
    ap.add_argument("rtr")
    ap.add_argument("label")
    ap.add_argument("outages", type=int)
    ap.add_argument("--release-delay", type=float, default=1.0)
    ap.add_argument("--stable", type=float, default=20.0)
    ap.add_argument("--max-outage", type=float, default=300.0)
    ap.add_argument(
        "--no-ap",
        action="store_true",
        help="DUT has no fallback AP: react to 'Restarting adapter'",
    )
    a = ap.parse_args()

    marks = pathlib.Path(f"{a.label}-marks.txt").open(  # noqa: SIM115
        "w", encoding="utf-8", buffering=1
    )

    def mark(msg: str) -> None:
        line = f"{ts()} MARK {msg}"
        marks.write(line + "\n")
        print(line, flush=True)

    # Router first: open with both lines released so it runs; opening may reset it once
    rtr = serial.Serial()
    rtr.port, rtr.baudrate, rtr.timeout = a.rtr, 115200, 0.2
    rtr.dtr = False
    rtr.rts = False
    rtr.open()
    rtr.dtr = False
    rtr.rts = False
    dut = serial.Serial(a.dut, 115200, timeout=0.2)  # opening resets the BK7238
    rr = Reader(rtr, f"{a.label}-rtr.txt")
    dr = Reader(dut, f"{a.label}-dut.txt")
    rr.start()
    dr.start()
    mark(f"START label={a.label} outages={a.outages} release_delay={a.release_delay}")

    def connected_for(seconds: float, cap: float) -> bool:
        """Wait until the last `seconds` of ALIVE lines all read conn=1."""
        t_end = time.time() + cap
        run_start = None
        while time.time() < t_end:
            time.sleep(0.5)
            with dr.lock:
                alive = [(t, ln) for t, ln in dr.lines[-40:] if "ALIVE" in ln]
            if not alive:
                continue
            t, ln = alive[-1]
            if "conn=1" in ln:
                if run_start is None:
                    run_start = t
                if t - run_start >= seconds:
                    return True
            else:
                run_start = None
        return False

    failed = False
    for n in range(1, a.outages + 1):
        if not connected_for(a.stable, 400):
            mark(f"OUTAGE {n} SKIP dut never stable; releasing router and retrying")
            rtr.rts = False
            if not connected_for(a.stable, 300):
                mark("ABORT dut not connecting")
                failed = True
                break
        t0 = time.time()
        _, idx = dr.after(0)
        mark(f"OUTAGE {n} DOWN")
        rtr.rts = True  # EN low: router held in reset
        ap_up = False
        hit = False
        # the RESTARTING transition after the fallback AP started
        while not hit and time.time() - t0 < a.max_outage:
            time.sleep(0.05)
            new, idx = dr.after(idx)
            for ln in new:
                if "Starting fallback AP" in ln:
                    ap_up = True
                if (ap_up or a.no_ap) and (
                    "Restarting adapter" in ln if a.no_ap else "→ RESTARTING" in ln
                ):
                    hit = True
                    break
        if not hit:
            mark(f"OUTAGE {n} NO_RESTARTING ap_up={ap_up}; releasing")
            failed = True
            rtr.rts = False
            continue
        mark(f"OUTAGE {n} RESTARTING_SEEN ap_up={ap_up}")
        time.sleep(a.release_delay)
        rtr.rts = False
        mark(f"OUTAGE {n} UP")
        # let the cooldown end (an.py's end of cooldown lines) before the next stable check
        t_up = time.time()
        ended = False
        while not ended and time.time() - t_up < 40:
            time.sleep(0.2)
            new, idx = dr.after(idx)
            ended = any(ends_cooldown(ln) for ln in new)
    mark("END")
    rtr.rts = False
    time.sleep(2)
    rr.stop = dr.stop = True
    time.sleep(0.5)
    # an aborted run or an outage that never reached RESTARTING has fewer outages than asked for
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
