"""Bounded, timestamped serial capture that always exits and closes the port.

  serial_capture.py PORT SECONDS OUTFILE [--baud 115200] [--released] [--reset]

  default      open with DTR/RTS as pyserial leaves them (asserted). Fine for ESP devkits and the Pico.
  --released   drive DTR and RTS low before opening. Required for the BW15 and BK7238 CH340 kits: their
               CH340 straps the boot pin (DTR) and reset (RTS), so a plain open holds the chip in reset.
  --reset      after opening, pulse reset (DTR low, RTS high 100 ms, RTS low) to capture from a fresh boot.
  --baud       115200 by default; 74880 shows the ESP8266 ROM boot messages.

Exit status 1 when nothing was received (wrong port name, or a port another capture still holds).
Lines are written as [HH:MM:SS]text with ANSI colour stripped and flushed one by one, so a shell
loop can grep the file while the capture runs. If the port is missing or disappears (USB CDC boards
re-enumerate on reboot) the script retries the open until the deadline instead of dying.

Never run an open-ended serial session: every capture has a deadline, and a flash on the same port
needs the capture stopped first (pkill -f "[s]erial_capture.py" does not match the killing shell).
"""

import argparse
import contextlib
import pathlib
import re
import sys
import time

import serial

ANSI = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]")
PORT_ERRORS = (serial.SerialException, OSError)


def open_port(args: argparse.Namespace) -> serial.Serial:
    s = serial.Serial(port=None, baudrate=args.baud, timeout=0.2)
    s.port = args.port
    if args.released:
        s.dtr = False
        s.rts = False
    s.open()
    if args.reset:
        try:
            s.dtr = False
            s.rts = True
            time.sleep(0.1)
            s.rts = False
            s.reset_input_buffer()
        except PORT_ERRORS:
            s.close()
            raise
    return s


def note(out, text: str) -> None:
    out.write(f"[{time.strftime('%H:%M:%S')}][capture] {text}\n".encode())
    out.flush()


def capture(args: argparse.Namespace, out) -> int:
    """Returns the number of bytes received."""
    deadline = time.time() + args.seconds
    buf = b""
    s = None
    received = 0
    while time.time() < deadline:
        if s is None:
            try:
                s = open_port(args)
                args.reset = False  # only reset on the first open
            except PORT_ERRORS as err:
                note(out, f"open failed: {err}")
                time.sleep(1)
                continue
        try:
            data = s.read(4096)
        except PORT_ERRORS as err:
            note(out, f"port lost: {err}")
            with contextlib.suppress(*PORT_ERRORS):
                s.close()
            s = None
            time.sleep(1)
            continue
        if not data:
            continue
        received += len(data)
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = ANSI.sub(b"", line).rstrip(b"\r")
            out.write(b"[" + time.strftime("%H:%M:%S").encode() + b"]" + line + b"\n")
            out.flush()
    if s is not None:
        s.close()
    return received


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("port")
    ap.add_argument("seconds", type=float)
    ap.add_argument("outfile")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--released", action="store_true")
    ap.add_argument("--reset", action="store_true")
    args = ap.parse_args()
    with pathlib.Path(args.outfile).open("ab") as out:
        received = capture(args, out)
    if received == 0:
        print(f"[capture] nothing received from {args.port}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
