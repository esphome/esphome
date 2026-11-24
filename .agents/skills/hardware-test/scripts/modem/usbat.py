"""Send AT lines on a modem's own USB AT port and print the replies.

usage: usbat.py PORT WAIT CMD [CMD ...]  (waits WAIT seconds after each command)
"""

import sys
import time

import serial

port, wait = sys.argv[1], float(sys.argv[2])
with serial.Serial(port, 115200, timeout=0.2) as s:
    for cmd in sys.argv[3:]:
        s.reset_input_buffer()
        s.write((cmd + "\r").encode())
        end = time.time() + wait
        buf = b""
        while time.time() < end:
            buf += s.read(512)
        print(f"--- {cmd}\n{buf.decode(errors='replace').strip()}", flush=True)
