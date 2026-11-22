"""rd.py PORT SECONDS OUT n|e [ack] [feed]: raw termios reader at 115200 8N1 (n) or 8E1 (e), PARMRK on.

ack answers a lone 0x7F with 0x79 (a fake STM32 bootloader ACK, so stm32flash goes on to GVR 01 FE); feed writes
400 bytes of 0x55 after each RPT line (RX buffer probe). Writes go out in the reader's own framing.
OUT gets "<seconds> RX <hex>" and "<seconds> TX <what>" lines.
"""

import os
import pathlib
import select
import sys
import termios
import time

port, secs, out, mode = sys.argv[1], float(sys.argv[2]), sys.argv[3], sys.argv[4]
ack, feed = "ack" in sys.argv[5:], "feed" in sys.argv[5:]
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = (termios.INPCK | termios.PARMRK) if mode == "e" else termios.PARMRK
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
termios.tcflush(fd, termios.TCIOFLUSH)
t0 = time.time()
end = t0 + secs
tail = b""
with pathlib.Path(out).open("w", encoding="utf-8") as f:
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.02)
        if not r:
            continue
        try:
            b = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not b:
            continue
        f.write(f"{time.time() - t0:.4f} RX {b.hex(' ')}\n")
        f.flush()
        if ack and b == b"\x7f":
            os.write(fd, b"\x79")
            f.write(f"{time.time() - t0:.4f} TX 79\n")
        tail = (tail + b)[-200:]
        if feed and b"RPT" in tail and tail.endswith(b"\r\n"):
            tail = b""
            os.write(fd, b"\x55" * 400)
            f.write(f"{time.time() - t0:.4f} TX 55*400\n")
os.close(fd)
