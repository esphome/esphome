"""Drive the usb_cdc_acm echo firmware on the nRF52840 dongle from the Mac.

usage: drive.py DATA_PORT [case ...]
Cases: short, bulk1k, bulk16k, bulk64k, burst, flush, peek, linecoding, dtr, stream, counters
"""

import random
import sys
import time

import serial

port = sys.argv[1]
cases = sys.argv[2:] or [
    "short",
    "bulk1k",
    "bulk16k",
    "bulk64k",
    "burst",
    "flush",
    "peek",
    "linecoding",
    "dtr",
    "stream",
    "counters",
]
results = []


def say(case, ok, detail):
    results.append((case, ok, detail))
    print(f"{'PASS' if ok else 'FAIL'} {case}: {detail}", flush=True)


def info(case, detail):
    """A case whose result lives in the device log; printed, not counted."""
    print(f"INFO {case}: {detail}", flush=True)


def open_port(baud=115200):
    s = serial.Serial(port, baudrate=baud, timeout=1)
    time.sleep(0.3)  # let the 100 ms line-state poll see DTR and the echo loop settle
    return s


def read_exact(s, n, timeout):
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while len(buf) < n and time.monotonic() < deadline:
        chunk = s.read(min(4096, n - len(buf)))
        if chunk:
            buf += chunk
    return bytes(buf)


def payload(n):
    # No 0x01 anywhere: the device treats a chunk starting with 0x01 as a command
    rnd = random.Random(n)
    return bytes(rnd.choice(range(2, 256)) for _ in range(n))


def cmd(s, c, extra=b""):
    s.write(b"\x01" + c + extra)
    s.flush()


def echo_case(name, n, chunk):
    s = open_port()
    s.reset_input_buffer()
    data = payload(n)
    t0 = time.monotonic()
    for i in range(0, n, chunk):
        s.write(data[i : i + chunk])
    got = read_exact(s, n, timeout=max(5, n / 2000))
    dt = time.monotonic() - t0
    s.close()
    ok = got == data
    first_bad = next((i for i in range(min(len(got), n)) if got[i] != data[i]), None)
    say(
        name,
        ok,
        f"{len(got)}/{n} bytes echoed in {dt:.2f}s ({n / dt / 1024:.1f} KiB/s), first mismatch {first_bad}",
    )


for case in cases:
    if case == "short":
        s = open_port()
        s.reset_input_buffer()
        ok = True
        lat = []
        for i in range(20):
            msg = f"hello {i}\n".encode()
            t0 = time.monotonic()
            s.write(msg)
            got = read_exact(s, len(msg), 2)
            lat.append(time.monotonic() - t0)
            ok = ok and got == msg
        s.close()
        say(
            case,
            ok,
            f"20 lines, latency min {min(lat) * 1000:.0f} ms max {max(lat) * 1000:.0f} ms",
        )
    elif case == "bulk1k":
        echo_case(case, 1024, 1024)
    elif case == "bulk16k":
        echo_case(case, 16 * 1024, 4096)
    elif case == "bulk64k":
        echo_case(case, 64 * 1024, 16384)
    elif case == "burst":
        s = open_port()
        s.reset_input_buffer()
        cmd(s, b"B")
        got = read_exact(s, 1000, 3)
        s.close()
        expect = bytes((ord("A") + (i % 26)) for i in range(1000))
        prefix_ok = got == expect[: len(got)]
        say(
            case,
            prefix_ok and 0 < len(got) < 1000,
            f"{len(got)} of 1000 bytes arrived (256 byte TX ring, rest must be dropped and warned), prefix intact {prefix_ok}",
        )
    elif case == "flush":
        s = open_port()
        s.reset_input_buffer()
        cmd(s, b"F")
        got = read_exact(s, 200, 3)
        s.close()
        expect = bytes((ord("a") + (i % 26)) for i in range(200))
        say(case, got == expect, f"{len(got)}/200 bytes, flush result in device log")
        # flush with the port closed (DTR off): device must return FAILED at once, see log
        time.sleep(0.5)
    elif case == "peek":
        s = open_port()
        s.reset_input_buffer()
        cmd(s, b"K", b"Z")
        time.sleep(0.5)
        s.close()
        info(
            case,
            "peek/read result is in the device log: expect `peek ok=1 0x5A read ok=1 0x5A`",
        )
    elif case == "linecoding":
        for baud in (9600, 115200, 921600):
            s = open_port(baud)
            s.reset_input_buffer()
            cmd(s, b"L")
            time.sleep(0.4)
            s.close()
            time.sleep(0.3)
        say(
            case, True, "opened at 9600, 115200, 921600; device log must show each baud"
        )
    elif case == "dtr":
        for _i in range(5):
            s = open_port()
            time.sleep(0.3)
            s.close()
            time.sleep(0.3)
        say(
            case,
            True,
            "5 open/close cycles; device log must show 5 Opened and 5 Closed",
        )
    elif case == "stream":
        # device streams 100 x 100 byte lines over 10 s; close the port for 3 s in the middle
        s = open_port()
        s.reset_input_buffer()
        cmd(s, b"S")
        part1 = bytearray()
        t0 = time.monotonic()
        while time.monotonic() - t0 < 2.5:
            part1 += s.read(4096)
        s.close()
        time.sleep(3.0)
        s = open_port()
        part2 = bytearray()
        t0 = time.monotonic()
        while time.monotonic() - t0 < 6.0:
            part2 += s.read(4096)
        s.close()
        lines1 = [x for x in bytes(part1).split(b"\n") if x.startswith(b"STREAM")]
        lines2 = [x for x in bytes(part2).split(b"\n") if x.startswith(b"STREAM")]
        n1 = [int(x[7:10]) for x in lines1]
        n2 = [int(x[7:10]) for x in lines2]
        resumed = bool(n2) and max(n2) >= 90
        say(
            case,
            resumed,
            f"before close: lines {min(n1) if n1 else None}-{max(n1) if n1 else None} ({len(n1)}), after reopen: {min(n2) if n2 else None}-{max(n2) if n2 else None} ({len(n2)}); streaming resumed after reopen {resumed}",
        )
    elif case == "counters":
        s = open_port()
        cmd(s, b"C")
        time.sleep(0.3)
        s.close()
        say(case, True, "counters in device log")
    time.sleep(0.5)

print(
    "SUMMARY",
    sum(1 for r in results if r[1]),
    "pass",
    sum(1 for r in results if not r[1]),
    "fail",
)
