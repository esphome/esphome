"""Improv serial client that reads frames out of the log stream on the same port.
usage: improvcli.py PORT OUT --secrets FILE [--baud N] [--keep-dtr] [--settle S] [--skip-bad]
Runs: 300 stray bytes, state, info, network state, scan, provision with a bad password, provision with the real one,
state and network state again. Prints parsed frames; the raw stream goes to OUT.
Exit 1 when a provisioning reply is missing or a frame fails its checksum. Credentials come from the secrets
file (wifi_ssid, wifi_password) and are never printed; the SSID is masked in scan results."""

import argparse
import pathlib
import random
import sys
import time

import serial
import yaml

STATES = {
    0: "stopped",
    1: "awaiting_auth",
    2: "authorized",
    3: "provisioning",
    4: "provisioned",
}
ERRORS = {
    0: "none",
    1: "invalid_rpc",
    2: "unknown_rpc",
    3: "unable_to_connect",
    4: "not_authorized",
    0xFF: "unknown",
}


def frame(cmd: int, payload: bytes = b"") -> bytes:
    data = bytes([cmd, len(payload)]) + payload
    body = b"IMPROV" + bytes([1, 3, len(data)]) + data
    return body + bytes([sum(body) & 0xFF])


def strings(p: bytes) -> list[str]:
    out = []
    while p:
        out.append(p[1 : 1 + p[0]].decode(errors="replace"))
        p = p[1 + p[0] :]
    return out


class Dev:
    def __init__(
        self, port: str, baud: int, out: str, keep_dtr: bool, ssid: str
    ) -> None:
        self.ser = serial.Serial()
        self.ser.port, self.ser.baudrate, self.ser.timeout = port, baud, 0.05
        if not keep_dtr:
            self.ser.dtr = False
            self.ser.rts = False
        self.reopen()
        self.buf = bytearray()
        self.log = pathlib.Path(out).open("ab")  # noqa: SIM115
        self.t0 = time.time()
        self.events: list[tuple[float, int, bytes, bool]] = []
        self.loglines = 0
        self.reopens = 0
        self.ssid = ssid

    def reopen(self) -> None:
        # Native USB ports re-enumerate on reset; wait up to 10 s for them.
        for _ in range(40):
            try:
                if self.ser.is_open:
                    self.ser.close()
                self.ser.open()
                return
            except (serial.SerialException, OSError):
                time.sleep(0.25)
        raise SystemExit("port did not come back")

    def pump(self, dur: float) -> None:
        end = time.time() + dur
        while time.time() < end:
            try:
                d = self.ser.read(4096)
            except serial.SerialException:
                # A dropped native USB port usually means the board reset; make that visible.
                self.reopens += 1
                print(
                    f"{time.time() - self.t0:7.2f} PORT RESET (reopen {self.reopens})",
                    flush=True,
                )
                self.reopen()
                continue
            if d:
                self.log.write(d)
                self.log.flush()
                self.buf += d
                self.parse()

    def parse(self) -> None:
        while True:
            i = self.buf.find(b"IMPROV")
            if i < 0:
                keep = max(0, len(self.buf) - 6)
                self.loglines += self.buf[:keep].count(b"\n")
                del self.buf[:keep]
                return
            self.loglines += self.buf[:i].count(b"\n")
            del self.buf[:i]
            if len(self.buf) < 9:
                return
            ln = self.buf[8]
            if len(self.buf) < 10 + ln:
                return
            body, ck = bytes(self.buf[: 9 + ln]), self.buf[9 + ln]
            del self.buf[: 10 + ln]
            typ, data = body[7], body[9:]
            ok = (sum(body) & 0xFF) == ck
            self.events.append((time.time() - self.t0, typ, data, ok))
            self.show(typ, data, ok)

    def show(self, typ: int, data: bytes, ok: bool) -> None:
        t = f"{time.time() - self.t0:7.2f}"
        tag = "" if ok else " BADCHECKSUM"
        if typ == 1:
            print(f"{t} STATE {STATES.get(data[0], data[0])}{tag}")
        elif typ == 2:
            print(f"{t} ERROR {ERRORS.get(data[0], data[0])}{tag}")
        elif typ == 4:
            strs = ["<ssid>" if x == self.ssid else x for x in strings(data[2:])]
            print(f"{t} RPC cmd={data[0]} len={data[1]} {strs}{tag}")
        else:
            print(f"{t} TYPE{typ} {data.hex()}{tag}")

    def send(self, cmd: int, payload: bytes = b"", wait: float = 2.0) -> None:
        self.ser.write(frame(cmd, payload))
        self.ser.flush()
        self.pump(wait)

    def wait_for(self, pred, timeout: float, start: int) -> tuple | None:
        end = time.time() + timeout
        while time.time() < end:
            self.pump(0.2)
            for e in self.events[start:]:
                if pred(e):
                    return e
        return None


def provision(d: Dev, ssid: str, pw: str, pred, label: str) -> bool:
    s, w = ssid.encode(), pw.encode()
    p = bytes([len(s)]) + s + bytes([len(w)]) + w
    start = len(d.events)  # before sending, so an immediate reply counts
    t = time.time()
    d.send(1, p, wait=0)
    e = d.wait_for(pred, 60, start)
    print(f"{label} after {time.time() - t:.1f}s" if e else f"NO {label} within 60s")
    return e is not None


def main() -> None:
    a = argparse.ArgumentParser()
    a.add_argument("port")
    a.add_argument("out")
    a.add_argument("--baud", type=int, default=115200)
    a.add_argument(
        "--keep-dtr",
        action="store_true",
        help="leave DTR asserted (Pico 2 W CDC sends nothing without)",
    )
    a.add_argument("--secrets", required=True)
    a.add_argument("--settle", type=float, default=3)
    a.add_argument("--skip-bad", action="store_true")
    o = a.parse_args()
    with pathlib.Path(o.secrets).open(encoding="utf-8") as f:
        sec = yaml.safe_load(f)
    ssid, pw = sec["wifi_ssid"], sec["wifi_password"]
    print(f"password length {len(pw.encode())} bytes")
    d = Dev(o.port, o.baud, o.out, o.keep_dtr, ssid)
    d.pump(o.settle)
    before = d.loglines
    print("== stray bytes")
    junk = bytes(
        random.choice([b for b in range(256) if b != 0x49]) for _ in range(300)
    )
    d.ser.write(junk)
    d.ser.flush()
    d.pump(1.0)
    print("== state")
    d.send(2, wait=1.5)
    print("== info")
    d.send(3, wait=1.5)
    print("== netstate")
    d.send(7, wait=1.5)
    print("== scan")
    d.send(4, wait=6)
    seen = any(e[1] == 4 and e[2][0] == 4 and ssid.encode() in e[2] for e in d.events)
    print(f"target network in scan: {seen}")
    ok = True
    if not o.skip_bad:
        print("== provision bad password")
        ok &= provision(
            d,
            ssid,
            "wrong-password-0123456789",
            lambda e: e[1] == 2 and e[2][0] == 3 and e[3],
            "unable_to_connect",
        )
        d.pump(2)
    print("== provision good")
    ok &= provision(
        d,
        ssid,
        pw,
        lambda e: e[1] == 4 and e[2][0] == 1 and e[3],
        "provisioned response",
    )
    d.pump(3)
    print("== state")
    d.send(2, wait=1.5)
    print("== netstate")
    d.send(7, wait=1.5)
    d.pump(2)
    bad = sum(1 for e in d.events if not e[3])
    print(
        f"frames={len(d.events)} badchecksum={bad} reopens={d.reopens} loglines={d.loglines} "
        f"(before stray {before})"
    )
    print("PASS" if ok and not bad else "FAIL")
    sys.exit(0 if ok and not bad else 1)


if __name__ == "__main__":
    main()
