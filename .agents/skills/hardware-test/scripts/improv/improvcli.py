"""Improv serial client that reads frames out of the log stream on the same port.
usage: improvcli.py PORT OUT --secrets FILE [--baud N] [--keep-dtr] [--settle S] [--skip-bad] [--allow-reset]
Runs: 300 stray bytes, state, info, network state, scan, provision with a bad password, provision with the real one,
state and network state again. Prints parsed frames; the raw stream overwrites OUT.
Exit 2 when the port does not open or the secrets file lacks a key. Exit 1 when any request goes unanswered, an
unexpected error or a malformed frame arrives, or the port resets or the log shows a reboot (unless --allow-reset).
Credentials come from the secrets file (wifi_ssid, wifi_password) and are never printed; the SSID is masked in scan
results."""

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


BOOT_MARKER = (
    b"Running through setup()"  # ESPHome logs it once per boot on every platform
)


def strings(p: bytes) -> list[str]:
    """Split length-prefixed strings; raise ValueError when a prefix runs past the end."""
    out = []
    while p:
        if 1 + p[0] > len(p):
            raise ValueError("string overruns the payload")
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
        self.reopen(setup=True)
        self.buf = bytearray()
        self.log = pathlib.Path(out).open("wb")  # noqa: SIM115
        self.t0 = time.time()
        self.events: list[tuple[float, int, bytes, bool]] = []
        self.loglines = 0
        self.reopens = 0
        self.boots = 0
        self.tail = b""
        self.ssid = ssid

    def reopen(self, setup: bool = False) -> None:
        # Native USB ports re-enumerate on reset; wait up to 10 s for them.
        err: Exception | None = None
        for _ in range(40):
            try:
                if self.ser.is_open:
                    self.ser.close()
                self.ser.open()
                return
            except (serial.SerialException, OSError) as exc:
                err = exc
                time.sleep(0.25)
        print(
            f"port did not {'open' if setup else 'come back'}: {err}", file=sys.stderr
        )
        sys.exit(2 if setup else 1)

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
                seen = self.tail + d
                self.boots += seen.count(BOOT_MARKER)
                self.tail = seen[-(len(BOOT_MARKER) - 1) :]
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
            if self.buf[6] != 1:  # not a version 1 header: "IMPROV" in log text
                del self.buf[:6]
                continue
            ln = self.buf[8]
            if len(self.buf) < 10 + ln:
                return
            body, ck = bytes(self.buf[: 9 + ln]), self.buf[9 + ln]
            del self.buf[: 10 + ln]
            typ, data = body[7], body[9:]
            ok = (sum(body) & 0xFF) == ck
            # A malformed frame is counted as bad, so the predicates never index past it.
            short = len(data) < {1: 1, 2: 1, 4: 2}.get(typ, 0)
            if typ == 4 and not short:
                try:
                    strings(data[2:])
                    short = data[1] != len(data) - 2
                except ValueError:
                    short = True
            self.events.append((time.time() - self.t0, typ, data, ok and not short))
            self.show(typ, data, ok, short)

    def show(self, typ: int, data: bytes, ok: bool, short: bool) -> None:
        t = f"{time.time() - self.t0:7.2f}"
        tag = "" if ok else " BADCHECKSUM"
        if short:
            print(f"{t} TYPE{typ} {data.hex()} MALFORMED{tag}")
        elif typ == 1:
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


def ask(
    d: Dev, label: str, cmd: int, pred, timeout: float = 1.5, payload: bytes = b""
) -> bool:
    """Send one request and require a reply matching pred; any other nonzero ERROR frame fails it."""
    print(f"== {label}")
    start = len(d.events)  # before sending, so an immediate reply counts
    t = time.time()
    d.send(cmd, payload, wait=0)
    e = d.wait_for(pred, timeout, start)
    print(
        f"reply after {time.time() - t:.1f}s"
        if e
        else f"NO reply within {timeout:.0f}s"
    )
    errs = [
        x for x in d.events[start:] if x[1] == 2 and x[3] and x[2][0] and not pred(x)
    ]
    if errs:
        print(f"unexpected errors: {[ERRORS.get(x[2][0], x[2][0]) for x in errs]}")
    return e is not None and not errs


def rpc(cmd: int):
    return lambda e: e[1] == 4 and e[3] and e[2][0] == cmd


def state(e: tuple) -> bool:
    return e[1] == 1 and e[3]


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
    a.add_argument(
        "--allow-reset",
        action="store_true",
        help="a port reset or a reboot in the log does not fail the run",
    )
    o = a.parse_args()
    with pathlib.Path(o.secrets).open(encoding="utf-8") as f:
        sec = yaml.safe_load(f)
    try:
        ssid, pw = sec["wifi_ssid"], sec["wifi_password"]
    except (KeyError, TypeError) as exc:
        print(f"secrets file lacks {exc}", file=sys.stderr)
        sys.exit(2)
    if not all(
        isinstance(x, (str, int, float)) and not isinstance(x, bool) for x in (ssid, pw)
    ):
        print("wifi_ssid and wifi_password must be plain values", file=sys.stderr)
        sys.exit(2)
    ssid, pw = str(ssid), str(pw)  # YAML reads an unquoted number as int
    s, w = ssid.encode(), pw.encode()
    print(f"password length {len(w)} bytes")
    d = Dev(o.port, o.baud, o.out, o.keep_dtr, ssid)
    d.pump(o.settle)
    boots_at_start = d.boots
    before = d.loglines
    print("== stray bytes")
    j = len(d.events)
    junk = bytes(
        random.choice([b for b in range(256) if b != 0x49]) for _ in range(300)
    )
    d.ser.write(junk)
    d.ser.flush()
    d.pump(1.0)
    # Stray bytes must be ignored silently; the state reply below shows the parser recovered.
    ok = not any(e[1] == 2 and e[3] and e[2][0] for e in d.events[j:])
    # A saved network skips provisioning: the board must start unprovisioned.
    ok &= ask(d, "state", 2, lambda e: state(e) and e[2][0] != 4)
    if any(state(e) and e[2][0] == 4 for e in d.events[j:]):
        print(
            "board is already provisioned: erase the flash first (a new config name keeps the saved network)"
        )
    ok &= ask(d, "info", 3, rpc(3))
    ok &= ask(d, "netstate", 7, rpc(7))
    # The scan sends one reply per network and ends with an empty one.
    ok &= ask(d, "scan", 4, lambda e: rpc(4)(e) and e[2][1] == 0, timeout=10)
    seen = any(e[1] == 4 and e[3] and e[2][0] == 4 and s in e[2] for e in d.events)
    print(f"target network in scan: {seen}")
    creds = bytes([len(s)]) + s
    if not o.skip_bad:
        bad_pw = b"wrong-password-0123456789"
        ok &= ask(
            d,
            "provision bad password",
            1,
            lambda e: e[1] == 2 and e[3] and e[2][0] == 3,
            timeout=60,
            payload=creds + bytes([len(bad_pw)]) + bad_pw,
        )
        d.pump(2)
    # common.yaml sets next_url, so the provisioned reply must carry a URL.
    ok &= ask(
        d,
        "provision good",
        1,
        lambda e: rpc(1)(e) and any(x.startswith("http") for x in strings(e[2][2:])),
        timeout=60,
        payload=creds + bytes([len(w)]) + w,
    )
    d.pump(3)
    ok &= ask(d, "state is provisioned", 2, lambda e: state(e) and e[2][0] == 4)
    ok &= ask(d, "netstate", 7, rpc(7))
    d.pump(2)
    bad = sum(1 for e in d.events if not e[3])
    # Errors outside an ask() window, boot included: only the bad password case may report one.
    errors = sum(1 for e in d.events if e[1] == 2 and e[3] and e[2][0])
    if errors > (0 if o.skip_bad else 1):
        print(f"unexpected error frames: {errors}")
        ok = False
    reboots = d.boots - boots_at_start
    if (d.reopens or reboots) and not o.allow_reset:
        ok = False
    print(
        f"frames={len(d.events)} badframes={bad} reopens={d.reopens} reboots={reboots} loglines={d.loglines} "
        f"(before stray {before})"
    )
    print("PASS" if ok and not bad else "FAIL")
    sys.exit(0 if ok and not bad else 1)


if __name__ == "__main__":
    main()
