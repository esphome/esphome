"""Send AT lines on a modem's own USB AT port and print the replies.

usage: usbat.py PORT WAIT CMD [CMD ...]  (reads until a final result code, at most WAIT seconds per command)
"""

import re
import sys
import time

import serial

FINAL = re.compile(rb"\r\n(OK|ERROR|\+CME ERROR[^\r]*)\r\n$")

port, wait = sys.argv[1], float(sys.argv[2])
with serial.Serial(port, 115200, timeout=0.2) as s:
    for cmd in sys.argv[3:]:
        s.reset_input_buffer()
        s.write((cmd + "\r").encode())
        end = time.time() + wait
        buf = b""
        while time.time() < end:
            buf += s.read(512)
            if FINAL.search(buf):
                break
        print(f"--- {cmd}\n{buf.decode(errors='replace').strip()}", flush=True)
