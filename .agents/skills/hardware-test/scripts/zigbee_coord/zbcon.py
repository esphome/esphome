"""zbcon.py PORT LOG FIFO SECONDS: bounded console bridge; lines written to FIFO are sent to the H2 console.

Every console line goes to LOG with a timestamp. Commands go out 0.3 s apart. Opening the port resets the H2; when it
comes back under another device path the bridge follows it by its USB serial number.
"""

import collections
import contextlib
import os
import pathlib
import select
import sys
import time

import serial

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import serial_capture as sc  # noqa: E402


def open_port(path: str) -> serial.Serial:
    s = serial.Serial(baudrate=115200, timeout=0)
    s.port = path
    s.dtr = False
    s.rts = False
    s.open()
    return s


port, log, fifo, secs = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
if not pathlib.Path(fifo).exists():
    os.mkfifo(fifo)
fd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
wfd = os.open(fifo, os.O_WRONLY)  # keep the fifo open so EOF never fires
serial_number, index = sc.identify(port)
s = open_port(port)
end = time.time() + secs
buf = b""
cmdbuf = b""
pending: collections.deque[bytes] = collections.deque()
next_send = 0.0
with pathlib.Path(log).open("ab", buffering=0) as out:

    def emit(line: bytes) -> None:
        out.write(f"{time.strftime('%H:%M:%S')} ".encode() + line + b"\n")

    while time.time() < end:
        wait = max(0.0, min(0.2, next_send - time.time())) if pending else 0.2
        r, _, _ = select.select([s.fileno(), fd], [], [], wait)
        if s.fileno() in r:
            try:
                d = s.read(4096)
            except sc.PORT_ERRORS as e:
                emit(b"## read error " + str(e).encode())
                with contextlib.suppress(*sc.PORT_ERRORS):
                    s.close()
                while time.time() < end:
                    time.sleep(1)
                    try:
                        s = open_port(sc.current_path(port, serial_number, index))
                    except sc.PORT_ERRORS:
                        continue
                    emit(b"## reopened " + s.port.encode())
                    break
                continue
            buf += sc.ANSI.sub(b"", d)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                emit(line.rstrip(b"\r"))
        if fd in r:
            cmdbuf += os.read(fd, 4096)
            while b"\n" in cmdbuf:
                c, cmdbuf = cmdbuf.split(b"\n", 1)
                pending.append(c)
        if pending and time.time() >= next_send:
            c = pending.popleft()
            emit(b">>> " + c)
            s.write(c + b"\n")
            s.flush()
            next_send = time.time() + 0.3
