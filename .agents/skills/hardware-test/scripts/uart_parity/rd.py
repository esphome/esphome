"""rd.py PORT SECONDS OUT n|e [ack] [feed]: raw termios reader at 115200 8N1 (n) or 8E1 (e), PARMRK on.

ack answers a lone 0x7F with 0x79 (a fake STM32 bootloader ACK, so stm32flash goes on to GVR 01 FE); feed writes
400 bytes of 0x55 after each RPT line (RX buffer probe). Writes go out in the reader's own framing.
OUT starts with "0.0000 TX framing=8E1 options=ack,feed" (analyze.py checks both), then "<seconds> RX <hex>" and
"<seconds> TX <what>" lines.
"""

import os
import pathlib
import select
import sys
import termios
import time

if (
    len(sys.argv) < 5
    or sys.argv[4] not in ("n", "e")
    or set(sys.argv[5:]) - {"ack", "feed"}
):
    sys.exit(__doc__)
port, secs, out, mode = sys.argv[1], float(sys.argv[2]), sys.argv[3], sys.argv[4]
ack, feed = "ack" in sys.argv[5:], "feed" in sys.argv[5:]


def write_all(fd: int, data: bytes, f, t0: float, limit: float = 2) -> int:
    """Write data to the non-blocking tty for up to limit seconds; returns the bytes written.

    A lost port writes the same HUP line to f as the read path and exits 1."""
    n, stop = 0, time.time() + limit
    while n < len(data) and time.time() < stop:
        try:
            n += os.write(fd, data[n:])
        except BlockingIOError:
            select.select([], [fd], [], 0.1)
        except OSError as e:
            f.write(f"{time.time() - t0:.4f} HUP {e}\n")
            sys.exit(1)
    return n


fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
# INPCK at 8N1 too: Linux marks framing errors only with it on
a[0] = termios.INPCK | termios.PARMRK
a[1] = 0
a[2] = (
    termios.CS8
    | termios.CREAD
    | termios.CLOCAL
    | (termios.PARENB if mode == "e" else 0)
)
a[3] = 0
a[4] = a[5] = termios.B115200
a[6][termios.VMIN] = 0
a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
# tcsetattr succeeds when any change applied: read it back so the header states the framing the port really has
b = termios.tcgetattr(fd)
want_par = termios.PARENB if mode == "e" else 0
if (
    (b[2] & termios.PARENB) != want_par
    or (b[2] & termios.CSIZE) != termios.CS8
    or b[4] != termios.B115200
    or b[5]
    != termios.B115200  # the output speed too, or acks and feeds go out at another baud
):
    os.close(fd)
    sys.exit("the port did not take 115200 8 data bits with the requested parity")
# without PARMRK and INPCK errors never become FF 00 X marks, and a zero mark count means nothing
want_i = termios.PARMRK | termios.INPCK
if b[0] & want_i != want_i:
    os.close(fd)
    sys.exit(
        "the port did not take PARMRK/INPCK, so parity and framing errors would not be marked"
    )
termios.tcflush(fd, termios.TCIOFLUSH)
t0 = time.time()
end = t0 + secs
tail = b""
try:
    with pathlib.Path(out).open("w", encoding="utf-8") as f:
        opts = ",".join(o for o, on in (("ack", ack), ("feed", feed)) if on) or "-"
        f.write(f"0.0000 TX framing={'8E1' if mode == 'e' else '8N1'} options={opts}\n")
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.02)
            if not r:
                continue
            try:
                b = os.read(fd, 4096)
            except BlockingIOError:
                continue
            except OSError as e:  # the adapter went away
                f.write(f"{time.time() - t0:.4f} HUP {e}\n")
                sys.exit(1)
            if not b:  # readable with no data: hangup
                f.write(f"{time.time() - t0:.4f} HUP port closed\n")
                sys.exit(1)
            f.write(f"{time.time() - t0:.4f} RX {b.hex(' ')}\n")
            f.flush()
            if ack and b == b"\x7f":
                n = write_all(fd, b"\x79", f, t0)
                f.write(
                    f"{time.time() - t0:.4f} TX 79*{n}{'' if n else ' SHORT 0/1'}\n"
                )
            tail = (tail + b)[-200:]
            # LF, not CRLF: a logger-only report (sdd.yaml) ends in \r ESC[0m \n
            if feed and b"RPT" in tail and tail.endswith(b"\n"):
                tail = b""
                n = write_all(fd, b"\x55" * 400, f, t0)
                f.write(
                    f"{time.time() - t0:.4f} TX 55*{n}{'' if n == 400 else f' SHORT {n}/400'}\n"
                )
finally:
    os.close(fd)
