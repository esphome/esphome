"""zbcon.py PORT LOG FIFO SECONDS: bounded console bridge; lines written to FIFO are sent to the H2 console.

Every console line goes to LOG with a timestamp; ZBRESET=1 pulses RTS on open to reset the H2.
"""

import contextlib
import os
import pathlib
import re
import select
import sys
import time

import serial

port, log, fifo, secs = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
if not pathlib.Path(fifo).exists():
    os.mkfifo(fifo)
fd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
wfd = os.open(fifo, os.O_WRONLY)  # keep the fifo open so EOF never fires
s = serial.Serial()
s.port = port
s.baudrate = 115200
s.timeout = 0
s.dtr = False
s.rts = False
s.open()
if os.environ.get("ZBRESET"):
    s.dtr = False
    s.rts = True
    time.sleep(0.2)
    s.rts = False
ansi = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]")
end = time.time() + secs
buf = b""
cmdbuf = b""
with pathlib.Path(log).open("ab", buffering=0) as out:

    def emit(line: bytes) -> None:
        out.write(f"{time.strftime('%H:%M:%S')} ".encode() + line + b"\n")

    while time.time() < end:
        r, _, _ = select.select([s.fileno(), fd], [], [], 0.2)
        if s.fileno() in r:
            try:
                d = s.read(4096)
            except (serial.SerialException, OSError) as e:
                emit(b"## read error " + str(e).encode())
                with contextlib.suppress(serial.SerialException, OSError):
                    s.close()
                while time.time() < end:
                    time.sleep(1)
                    try:
                        s.open()
                    except (serial.SerialException, OSError):
                        continue
                    emit(b"## reopened")
                    break
                continue
            buf += ansi.sub(b"", d)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                emit(line.rstrip(b"\r"))
        if fd in r:
            cmdbuf += os.read(fd, 4096)
            while b"\n" in cmdbuf:
                c, cmdbuf = cmdbuf.split(b"\n", 1)
                emit(b">>> " + c)
                s.write(c + b"\n")
                s.flush()
                time.sleep(0.3)
