"""Capture node A and node B, reset both, and hold B in reset during A's offline phase.

Usage: python drive.py PORT_A PORT_B OUTPREFIX SECONDS (at least 150 for nodea.yaml)
Writes OUTPREFIX-a.txt, OUTPREFIX-b.txt and OUTPREFIX-events.txt. On a USB-UART devkit RTS drives EN, so RTS on
with DTR off holds the board in reset.
"""

import pathlib
import re
import sys
import threading
import time

import serial

HOLD_AFTER, HOLD_FOR = 8.0, 15.0
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def stamp() -> str:
    return f"[{time.strftime('%H:%M:%S')}.{int(time.time() * 1000) % 1000:03d}]"


def open_port(port: str) -> serial.Serial:
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.dtr, s.rts = port, 115200, 0.2, False, False
    s.open()
    return s


def reset(s: serial.Serial) -> None:
    s.dtr, s.rts = False, True
    time.sleep(0.1)
    s.rts = False


def main() -> None:
    port_a, port_b, out, secs = (
        sys.argv[1],
        sys.argv[2],
        sys.argv[3],
        float(sys.argv[4]),
    )
    end = time.time() + secs
    offline = threading.Event()
    sa, sb = open_port(port_a), open_port(port_b)
    with (
        pathlib.Path(out + "-a.txt").open("w", encoding="utf-8") as fa,
        pathlib.Path(out + "-b.txt").open("w", encoding="utf-8") as fb,
        pathlib.Path(out + "-events.txt").open("w", encoding="utf-8") as fe,
    ):

        def event(msg: str) -> None:
            line = f"{stamp()} {msg}\n"
            fe.write(line)
            fe.flush()
            print(line, end="", flush=True)

        def reader(s: serial.Serial, f, tag: str) -> None:
            buf = b""
            while time.time() < end:
                buf += s.read(4096)
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    text = ANSI.sub("", raw.decode("utf-8", "replace").rstrip("\r"))
                    f.write(f"{stamp()}{text}\n")
                    f.flush()
                    if tag == "a" and "PHASE offline" in text:
                        offline.set()

        threads = [
            threading.Thread(target=reader, args=(sa, fa, "a")),
            threading.Thread(target=reader, args=(sb, fb, "b")),
        ]
        for t in threads:
            t.start()
        event("reset B")
        reset(sb)
        time.sleep(1.5)
        event("reset A")
        reset(sa)
        held = False
        if not offline.wait(timeout=secs):
            event("offline phase never seen, B NOT held")
        elif end - time.time() < HOLD_AFTER + HOLD_FOR + 10:
            event("offline phase too close to the end of the capture, B NOT held")
        else:
            held = True
            time.sleep(HOLD_AFTER)
            event("hold B in reset (EN low)")
            sb.rts = True
            time.sleep(HOLD_FOR)
            sb.rts = False
            event("release B")
        for t in threads:
            t.join()
        event("done")
    sys.exit(0 if held else 1)


if __name__ == "__main__":
    main()
