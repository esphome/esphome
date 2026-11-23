"""Bounded capture at 9600 that also feeds PMS5003T frames on the same port (esphome#23038).

  pmsfeed.py PORT SECONDS OUT [--reset] [--start S] [--period 1.0] [--expect-pm] [--expect-light-sleep]
             [--expect-all N] [--selftest]

--expect-pm (pms-dfs.yaml and pms-manual-pm.yaml runs): exit 1 unless the capture proves power management was
on (an esp_pm_configure() result of 0 from pmh.hpp, or the esp32_power_management config block). Any
esp_pm_configure() failure, or the component marked FAILED, exits 1 with or without it.
--expect-light-sleep (light sleep runs) also needs proof light sleep was on: "Light Sleep Enabled" or ls=1 = 0.
--expect-all N (the plain pms-dut.yaml baseline): exit 1 when the device parsed more than N fewer frames than were
fed between its first and last cnt line.
Exits 1 too when no frame was fed, none was parsed, or fewer than two device cnt lines were captured.

Frames go out from the first device cnt line until 22 s before the end, so two counts follow the last one; they
must agree, which catches a garbled final count.
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


DRAIN = 22  # s without feeding at the end: over two cnt periods (10 s), so two counts follow the last frame


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("seconds", type=float)
    ap.add_argument("out")
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--start", type=float, default=5.0)
    ap.add_argument("--period", type=float, default=1.0)
    ap.add_argument("--expect-pm", action="store_true")
    ap.add_argument("--expect-light-sleep", action="store_true")
    ap.add_argument("--expect-all", type=int, metavar="N")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if not a.selftest and a.seconds <= a.start + DRAIN:
        sys.exit(f"SECONDS must be longer than --start + {DRAIN}, or no frame is fed")
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
    # feeding starts after the first cnt line and stops DRAIN s before the end, so every frame is fed and
    # parsed (or lost) between two device counts and the two edges cannot split a frame
    counting = False
    n = 0
    buf = b""
    with pathlib.Path(a.out).open("wb") as out:
        while time.time() < deadline:
            if counting and next_feed <= time.time() < deadline - DRAIN:
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
                if not counting and b"[cnt" in line and b"parses=" in line:
                    counting = True
                    next_feed = max(next_feed, time.time() + 0.5)
        if buf.strip():  # a last line cut off at the deadline
            serial_capture.write_line(out, b"[partial] ", buf.rstrip(b"\r"), True)
        serial_capture.write_line(out, b"[feed] ", f"total={n}".encode(), True)
    s.close()
    log = pathlib.Path(a.out).read_bytes()
    parses = log.count(b"Got PM1.0 Standard")
    # the device counter, which a garbled log line cannot lose; it starts again at 0 on boot. A new boot shows as
    # the uptime or the counter going back, or the uptime gaining less than the host clock (lost cnt lines around
    # the restart). Each boot counts from its first cnt line; frames fed across a restart are left out of both.
    cnt = []
    for m in re.finditer(
        rb"^\[(\d\d):(\d\d):(\d\d(?:\.\d+)?)\].*?\[cnt\S*: parses=(\d+) uptime=(\d+)",
        log,
        re.MULTILINE,
    ):
        wall = int(m[1]) * 3600 + int(m[2]) * 60 + float(m[3])
        if cnt and wall < cnt[-1][2]:
            wall += 86400  # midnight
        cnt.append((int(m[4]), int(m[5]), wall, m.start()))
    dev, boots, restarts = None, 0, []
    if len(cnt) > 1:
        dev, boots, base, prev = 0, 1, cnt[0][0], cnt[0]
        for c in cnt[1:]:
            p, u, w, _ = c
            if u <= prev[1] or p < prev[0] or u - prev[1] < w - prev[2] - 5:
                dev += prev[0] - base
                boots, base = (
                    boots + 1,
                    p,
                )  # from its first count, matching the feeds left out below
                # parses after the old boot's last count and frames fed while booting are lost to the restart
                restarts.append((prev[3], c[3]))
            prev = c
        dev += prev[0] - base
    # builds with pmh.hpp log the esp_pm_configure() result; anything but 0 means no power management
    pm = re.findall(rb"esp_pm_configure\(ls=\d\) = (-?\d+)", log)
    # esp32_power_management (pms-dfs.yaml) logs a failure, and its config block when it is set up
    # its config block is printed even after a failed setup, so the core's FAILED line counts as a failure
    pm_failed = (
        log.count(b"esp_pm_configure failed")
        # it registers through power_management, so the core names it that
        + log.count(b"power_management is marked FAILED")
        + sum(x != b"0" for x in pm)
    )
    pm_ok = not pm_failed and (bool(pm) or b"Power Management:" in log)
    ls_ok = pm_ok and (
        b"Light Sleep Enabled" in log or b"esp_pm_configure(ls=1) = 0" in log
    )
    feeds = log.count(b"[feed] n=")
    # frames fed between the first and last cnt line (all of them when feeding and draining ran as planned)
    # a garbled digit in a cnt line also looks like a restart: a real one leaves a boot line between the counts
    suspect = sum(
        not re.search(rb"Running through setup|ESPHome version|rst:", log[a:b])
        for a, b in restarts
    )
    fed = [m.start() for m in re.finditer(rb"\[feed\] n=", log)]
    across = sum(any(a < f < b for a, b in restarts) for f in fed)  # not UART RX losses
    win = sum(cnt[0][3] < f < cnt[-1][3] for f in fed) - across if len(cnt) > 1 else 0
    print(
        f"pm_configure={','.join(x.decode() for x in pm) or ('MISSING' if a.expect_pm else '-')} "
        f"pm_failed={pm_failed} cnt_lines={len(cnt)} "
        f"feeds={feeds} feeds_between_cnt_lines={win} feeds_across_restarts={across} parses={parses} "
        f"dev_parses_between_cnt_lines={dev} "
        f"boots={boots} restarts_without_boot_line={suspect} "
        f"start_char={log.count(b'Start character')} payload_len={log.count(b'Payload length')} "
        f"checksum={log.count(b'checksum mismatch')}"
    )
    if feeds == 0:
        sys.exit("no frame was fed: the run tested nothing")
    if len(cnt) < 2:
        sys.exit("fewer than two cnt lines: the device parse count is unknown")
    # nothing is fed after the last frame, so the counts after it must agree; a garbled digit makes them differ
    fed_end = max((m.start() for m in re.finditer(rb"\[feed\] n=", log)), default=0)
    tail = [c[0] for c in cnt if c[3] > fed_end]
    if len(tail) < 2:
        sys.exit(
            "fewer than two cnt lines after the last frame: the final count cannot be checked"
        )
    if len(set(tail[-2:])) > 1:
        sys.exit(
            f"the last two cnt lines disagree ({tail[-2]} and {tail[-1]}): a cnt line is garbled"
        )
    if suspect:
        sys.exit(
            f"{suspect} restarts with no boot line between the counts: likely a garbled cnt line"
        )
    # dead when neither the counter nor a parse line shows any frame (parse lines cover frames past the window)
    if not dev and not parses:
        sys.exit(
            "frames were fed but the device parsed none: check the GPIO3 wiring and the second UART"
        )
    if not win:
        sys.exit(
            "no frame was fed inside the device counter window: the run measured nothing"
        )
    if pm_failed:
        sys.exit("esp_pm_configure() failed: the run is not under power management")
    if a.expect_pm and not pm_ok:
        sys.exit("--expect-pm: no proof in the capture that power management was on")
    # the device cannot parse more frames than were fed: more means a garbled cnt line that read too high
    if dev is not None and dev > win:
        sys.exit(f"{dev} parses counted for {win} frames fed: a cnt line is garbled")
    if a.expect_all is not None and dev is not None and win - dev > a.expect_all:
        sys.exit(f"--expect-all: {win - dev} of {win} frames not parsed")
    if a.expect_light_sleep and not ls_ok:
        sys.exit(
            "--expect-light-sleep: no proof in the capture that light sleep was on"
        )


if __name__ == "__main__":
    main()
