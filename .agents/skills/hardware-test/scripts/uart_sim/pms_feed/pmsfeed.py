"""Bounded capture at 9600 that also feeds PMS5003T frames on the same port (esphome#23038).

  pmsfeed.py PORT SECONDS OUT [--reset] [--start S] [--period 1.0] [--expect-pm] [--selftest]

--expect-pm (pms-manual-pm.yaml runs): exit 1 unless an esp_pm_configure() result of 0 was captured.
Exits 1 too when fewer than two device cnt lines were captured, so the device count is unknown.

Frame n: PM1.0 std and atm = n, PM2.5 = n+1000, PM10 = n+2000, temp = 20.0+n%10, hum = 40.0+n%10.
Writes "[feed] n=..." lines into OUT so parses can be matched to frames; device lines go through
serial_capture.write_line() (millisecond stamps, colours stripped, secrets masked).
"""

import argparse
import pathlib
import re
import struct
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
import serial_capture  # noqa: E402


def frame(n: int) -> bytes:
    n &= 0x3FF
    words = [
        n,
        n + 1000,
        n + 2000,  # std PM1.0/2.5/10
        n,
        n + 1000,
        n + 2000,  # atm
        3000 + n,
        900 + n,
        200 + n,
        20 + n % 50,  # counts
        200 + (n % 10) * 10,  # temperature x10
        400 + (n % 10) * 10,  # humidity x10
        0x9100,  # version/error
    ]
    body = b"\x42\x4d" + struct.pack(">H", 28) + struct.pack(">13H", *words)
    return body + struct.pack(">H", sum(body) & 0xFFFF)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("seconds", type=float)
    ap.add_argument("out")
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--start", type=float, default=5.0)
    ap.add_argument("--period", type=float, default=1.0)
    ap.add_argument("--expect-pm", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        f = frame(7)
        assert len(f) == 32 and f[:4] == b"\x42\x4d\x00\x1c"
        assert struct.unpack(">H", f[30:])[0] == sum(f[:30])
        print(f.hex(" "))
        return
    s = serial_capture.open_port(
        argparse.Namespace(baud=9600, released=False, reset=a.reset), a.port
    )
    s.timeout = 0.05
    t0 = time.time()
    deadline = t0 + a.seconds
    next_feed = t0 + a.start
    n = 0
    buf = b""
    with pathlib.Path(a.out).open("wb") as out:
        while time.time() < deadline:
            if time.time() >= next_feed:
                s.write(frame(n))
                serial_capture.write_line(
                    out, b"[feed] ", f"n={n & 0x3FF}".encode(), True
                )
                n += 1
                next_feed += a.period
            data = s.read(4096)
            if not data:
                continue
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                serial_capture.write_line(out, b"", line.rstrip(b"\r"), True)
        if buf.strip():  # a last line cut off at the deadline
            serial_capture.write_line(out, b"[partial] ", buf.rstrip(b"\r"), True)
        serial_capture.write_line(out, b"[feed] ", f"total={n}".encode(), True)
    s.close()
    log = pathlib.Path(a.out).read_bytes()
    parses = log.count(b"Got PM1.0 Standard")
    # the device counter, which a garbled log line cannot lose; it starts again at 0 on boot. A new boot shows as
    # the uptime or the counter going back, or the uptime gaining less than the host clock (lost cnt lines around
    # the restart). The first boot counts from its first cnt line, a later one (started in the capture) from 0.
    cnt = []
    for m in re.finditer(
        rb"^\[(\d\d):(\d\d):(\d\d(?:\.\d+)?)\].*?\[cnt\S*: parses=(\d+) uptime=(\d+)",
        log,
        re.MULTILINE,
    ):
        wall = int(m[1]) * 3600 + int(m[2]) * 60 + float(m[3])
        if cnt and wall < cnt[-1][2]:
            wall += 86400  # midnight
        cnt.append((int(m[4]), int(m[5]), wall))
    dev, boots = None, 0
    if len(cnt) > 1:
        dev, boots, base, prev = 0, 1, cnt[0][0], cnt[0]
        for p, u, w in cnt[1:]:
            if u <= prev[1] or p < prev[0] or u - prev[1] < w - prev[2] - 5:
                dev += prev[0] - base
                boots, base = boots + 1, 0
            prev = (p, u, w)
        dev += prev[0] - base
    # builds with pmh.hpp log the esp_pm_configure() result; anything but 0 means no power management
    pm = re.findall(rb"esp_pm_configure\(ls=\d\) = (-?\d+)", log)
    pm_ok = bool(pm) and all(x == b"0" for x in pm)
    print(
        f"pm_configure={','.join(x.decode() for x in pm) or ('MISSING' if a.expect_pm else '-')} "
        f"cnt_lines={len(cnt)} "
        f"feeds={log.count(b'[feed] n=')} parses={parses} dev_parses_between_cnt_lines={dev} boots={boots} "
        f"start_char={log.count(b'Start character')} payload_len={log.count(b'Payload length')} "
        f"checksum={log.count(b'checksum mismatch')}"
    )
    if len(cnt) < 2:
        sys.exit("fewer than two cnt lines: the device parse count is unknown")
    if a.expect_pm and not pm_ok:
        sys.exit(
            "--expect-pm: no successful esp_pm_configure() line, power management not proven"
        )


if __name__ == "__main__":
    main()
