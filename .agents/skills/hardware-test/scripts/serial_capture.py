"""Bounded, timestamped serial capture that always exits and closes the port.

  serial_capture.py PORT SECONDS OUTFILE [--baud 115200] [--released] [--reset] [--append]

  default      open with DTR/RTS as pyserial leaves them (asserted); fine for ESP devkits and the Pico.
  --released   drive DTR and RTS low before opening: the BW15 and BK7238 CH340 kits strap the boot pin
               (DTR) and reset (RTS), so a plain open holds the chip in reset.
  --reset      pulse reset after opening (DTR low, RTS high 100 ms, RTS low) to capture a fresh boot.
  --baud       115200 by default; 74880 shows the ESP8266 ROM boot messages.
  --append     keep what OUTFILE already holds; by default it is truncated so counts cover this run only.

Exit status 1 when nothing was received (wrong port name, or a port another capture still holds).
Text ESPHome marks as secret (LOG_SECRET, the Wi-Fi password) is written as <secret>.
Lines are written as [HH:MM:SS]text with ANSI colour stripped and flushed one by one, so a shell
loop can grep the file while the capture runs. If the port is missing or disappears (USB CDC boards
re-enumerate on reboot) the script retries until the deadline, and follows the board by its USB serial
number when it comes back under another device path.

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
from serial.tools import list_ports

ANSI = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]")
# ESPHome's LOG_SECRET span (a Wi-Fi password); a span cut off before its end marker is secret to the end
SECRET = re.compile(rb"\x1b\[8m.*?(?:\x1b\[28m|$)", re.DOTALL)
PORT_ERRORS = (serial.SerialException, OSError)


def identify(port: str) -> tuple[str | None, int]:
    """USB serial number of the port and its position among that device's ports (a dongle has two)."""
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    serial_number = next((p.serial_number for p in ports if p.device == port), None)
    siblings = [
        p.device for p in ports if serial_number and p.serial_number == serial_number
    ]
    return serial_number, siblings.index(port) if port in siblings else 0


def current_path(port: str, serial_number: str | None, index: int) -> str:
    """The port's current device path: a USB CDC board can come back under a new name after a reboot."""
    if serial_number:
        ports = sorted(list_ports.comports(), key=lambda p: p.device)
        siblings = [p.device for p in ports if p.serial_number == serial_number]
        if index < len(siblings):
            return siblings[index]
    return port


def open_port(args: argparse.Namespace, path: str) -> serial.Serial:
    s = serial.Serial(port=None, baudrate=args.baud, timeout=0.2)
    s.port = path
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


def write_line(out, tag: bytes, data: bytes) -> None:
    stamp = time.strftime("[%H:%M:%S]").encode()
    out.write(stamp + tag + ANSI.sub(b"", SECRET.sub(b"<secret>", data)) + b"\n")
    out.flush()


def note(out, text: str) -> None:
    write_line(out, b"[capture] ", text.encode())


def capture(args: argparse.Namespace, out) -> int:
    """Returns the number of bytes received."""
    deadline = time.time() + args.seconds
    buf = b""
    s = None
    received = 0
    serial_number, index = None, 0
    while time.time() < deadline:
        if s is None:
            path = current_path(args.port, serial_number, index)
            try:
                s = open_port(args, path)
                args.reset = False  # only reset on the first open
                if serial_number is None:
                    serial_number, index = identify(path)
                if path != args.port:
                    note(out, f"reopened as {path}")
            except PORT_ERRORS as err:
                note(out, f"open failed: {err}")
                time.sleep(1)
                continue
        try:
            data = s.read(4096)
        except PORT_ERRORS as err:
            if buf:  # keep the cut-off line apart from the next boot's first line
                write_line(out, b"[partial]", buf)
                buf = b""
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
            write_line(out, b"", line.rstrip(b"\r"))
    if buf:
        write_line(out, b"[partial]", buf)
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
    ap.add_argument("--append", action="store_true")
    args = ap.parse_args()
    with pathlib.Path(args.outfile).open("ab" if args.append else "wb") as out:
        received = capture(args, out)
    if received == 0:
        print(f"[capture] nothing received from {args.port}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
