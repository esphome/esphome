"""Capture node A and node B, reset both, and hold B in reset during A's offline phase.

Usage: python drive.py PORT_A PORT_B OUTPREFIX SECONDS (at least 150 for nodea.yaml)
Writes OUTPREFIX-a.txt, OUTPREFIX-b.txt and OUTPREFIX-events.txt through serial_capture.write_line(). On a USB-UART
devkit RTS drives EN, so RTS on with DTR off holds the board in reset. Exits 1 when B was not held or a port failed;
the test results are the SUMMARY lines in OUTPREFIX-a.txt.
"""

import argparse
import pathlib
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import serial_capture  # noqa: E402

HOLD_AFTER, HOLD_FOR = 8.0, 15.0
PORT_ARGS = argparse.Namespace(baud=115200, released=True, reset=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port_a")
    parser.add_argument("port_b")
    parser.add_argument("out")
    parser.add_argument("seconds", type=float)
    args = parser.parse_args()
    end = time.time() + args.seconds
    offline = threading.Event()
    failed: list[str] = []
    with (
        pathlib.Path(args.out + "-a.txt").open("wb") as fa,
        pathlib.Path(args.out + "-b.txt").open("wb") as fb,
        pathlib.Path(args.out + "-events.txt").open("wb") as fe,
    ):

        def event(msg: str) -> None:
            serial_capture.note(fe, msg, True)
            print(msg, flush=True)

        def reader(s, f, phase_seen: threading.Event | None) -> None:
            buf = b""
            try:
                while time.time() < end:
                    *lines, buf = (buf + s.read(4096)).split(b"\n")
                    for raw in lines:
                        serial_capture.write_line(f, b"", raw.rstrip(b"\r"), True)
                        if phase_seen is not None and b"PHASE offline" in raw:
                            phase_seen.set()
            except OSError as e:  # serial.SerialException included
                failed.append(s.port)
                event(f"reader on {s.port} failed: {e}")

        event("reset B")
        sb = serial_capture.open_port(PORT_ARGS, args.port_b)
        threads = [threading.Thread(target=reader, args=(sb, fb, None))]
        threads[0].start()
        time.sleep(1.5)
        event("reset A")
        sa = serial_capture.open_port(PORT_ARGS, args.port_a)
        threads.append(threading.Thread(target=reader, args=(sa, fa, offline)))
        threads[1].start()
        held = False
        if not offline.wait(timeout=max(0.0, end - time.time())):
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
        sa.close()
        sb.close()
        event("done")
    sys.exit(0 if held and not failed else 1)


if __name__ == "__main__":
    main()
