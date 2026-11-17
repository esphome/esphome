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
import threading
import time

import serial

ANSI = re.compile(rb"\x1b\[[0-9;]*m")


def ts() -> str:
    t = time.time()
    return time.strftime("%H:%M:%S", time.localtime(t)) + f".{int(t * 1000) % 1000:03d}"


class Reader(threading.Thread):
    def __init__(self, ser: serial.Serial, path: str, marks) -> None:
        super().__init__(daemon=True)
        self.ser, self.f, self.marks = (
            ser,
            pathlib.Path(path).open("w", encoding="utf-8", buffering=1),  # noqa: SIM115
            marks,
        )
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

    def since(self, t0: float) -> list[str]:
        with self.lock:
            return [line for t, line in self.lines if t >= t0]


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
    rr = Reader(rtr, f"{a.label}-rtr.txt", marks)
    dr = Reader(dut, f"{a.label}-dut.txt", marks)
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

    for n in range(1, a.outages + 1):
        if not connected_for(a.stable, 400):
            mark(f"OUTAGE {n} SKIP dut never stable; releasing router and retrying")
            rtr.rts = False
            if not connected_for(a.stable, 300):
                mark("ABORT dut not connecting")
                break
        t0 = time.time()
        mark(f"OUTAGE {n} DOWN")
        rtr.dtr = False
        rtr.rts = True  # EN low: router held in reset
        ap_up = False
        hit = False
        while time.time() - t0 < a.max_outage:
            time.sleep(0.05)
            for ln in dr.since(t0):
                if "Starting fallback AP" in ln:
                    ap_up = True
            new = dr.since(t0)
            # the RESTARTING transition after the fallback AP started
            seen_ap = a.no_ap
            for ln in new:
                if "Starting fallback AP" in ln:
                    seen_ap = True
                if seen_ap and (
                    "Restarting adapter" in ln if a.no_ap else "→ RESTARTING" in ln
                ):
                    hit = True
                    break
            if hit:
                break
        if not hit:
            mark(f"OUTAGE {n} NO_RESTARTING ap_up={ap_up}; releasing")
            rtr.rts = False
            continue
        mark(f"OUTAGE {n} RESTARTING_SEEN ap_up={ap_up}")
        time.sleep(a.release_delay)
        rtr.rts = False
        mark(f"OUTAGE {n} UP")
        # let the cooldown and its outcome play out before the next stable check
        time.sleep(40)
    mark("END")
    rtr.rts = False
    time.sleep(2)
    rr.stop = dr.stop = True
    time.sleep(0.5)


if __name__ == "__main__":
    main()
