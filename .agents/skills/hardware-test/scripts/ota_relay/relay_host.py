"""Host side of the two channel relay in relay.hpp.
Usage: relay_host.py PORT SPEC [SPEC]   |   relay_host.py PORT --ping
Each SPEC is LPORT:TPORT:RATE_BPS; the first SPEC is channel 0. A local client on 127.0.0.1:LPORT is relayed to
TPORT on the relay's target, paced to RATE_BPS. Flag files in this script's directory:
  drop-ms     holds a number of ms; the AP relay (RELAY_AP) renames its AP for that long, then the file is removed
  abort-chN   closes channel N on the device side mid upload
env: RELAY_BAUD (default 115200; 921600 for a RELAY_UART build)."""

import contextlib
import os
from pathlib import Path
import socket
import struct
import sys
import threading
import time

import serial

D = Path(__file__).resolve().parent
# Largest frame payload; relay.hpp's MAX_FRAME must match
MAX_FRAME = 2100
ser = serial.Serial()
ser.port = sys.argv[1]
ser.baudrate = int(os.environ.get("RELAY_BAUD", "115200"))
ser.timeout = 0.05
ser.dtr = False
ser.rts = False
ser.open()
wlock = threading.Lock()
T0 = time.time()


def log(m: str) -> None:
    print(f"{time.strftime('%H:%M:%S')} {time.time() - T0:8.2f} {m}", flush=True)


class Ch:
    def __init__(self, n: int) -> None:
        self.n = n
        self.conn = None
        self.rx = 0
        self.seq = 0
        self.err = None
        self.pong = None
        self.credit = threading.Semaphore(4)
        self.ev_pong = threading.Event()
        self.ev_open = threading.Event()
        self.ev_closed = threading.Event()


chs = [Ch(0), Ch(1)]


def send(t: int, p: bytes = b"") -> None:
    with wlock:
        ser.write(b"\xa5\x5a" + bytes([t]) + struct.pack("<H", len(p)) + p)


def on_frame(t: int, p: bytes) -> None:
    c = chs[t >> 4]
    op = t & 15
    if op == 0:
        c.pong = p[0] if p else None
        c.ev_pong.set()
    elif op == 1:
        c.err = p[0] if p else 255
        c.ev_open.set()
    elif op == 2:
        c.rx += len(p)
        if c.conn:
            with contextlib.suppress(OSError):
                c.conn.sendall(p)
    elif op == 4:
        c.credit.release()
    elif op == 5:
        log("AP back up (drop done)")
    elif op == 3:
        for _ in range(8):
            c.credit.release()
        log(f"ch{c.n} device closed TCP reason={p[0] if p else '?'}")
        c.ev_closed.set()
        if c.conn:
            with contextlib.suppress(OSError):
                c.conn.shutdown(socket.SHUT_WR)


def reader() -> None:
    buf = bytearray()
    while True:
        try:
            buf += ser.read(4096)
        except serial.SerialException as e:
            log(f"relay serial port lost: {e}")
            os._exit(1)
        pos = 0
        while True:
            i = buf.find(b"\xa5\x5a", pos)
            if i < 0:
                pos = max(pos, len(buf) - 1)
                break
            pos = i
            if len(buf) < i + 5:
                break
            t = buf[i + 2]
            n = struct.unpack_from("<H", buf, i + 3)[0]
            if n > MAX_FRAME or (t >> 4) > 1:
                pos = i + 2
                continue
            if len(buf) < i + 5 + n:
                break
            pos = i + 5 + n
            on_frame(t, bytes(buf[i + 5 : pos]))
        del buf[:pos]


def dropper() -> None:
    flag = D / "drop-ms"
    while True:
        if flag.exists():
            text = flag.read_text(encoding="utf-8").strip()
            flag.unlink()
            try:
                ms = int(text or "5000")
                arg = struct.pack("<H", ms)
            except (ValueError, struct.error):
                log(f"bad drop-ms content {text!r}, no drop sent")
                continue
            log(f"DROP: AP down for {ms} ms")
            send(5, arg)
        time.sleep(0.1)


def wait_ready(c: Ch, limit: float = 90) -> bool:
    end = time.time() + limit
    while time.time() < end:
        c.ev_pong.clear()
        send(0 | (c.n << 4))
        if c.ev_pong.wait(1) and c.pong == 1:
            return True
        time.sleep(0.5)
    log(f"ch{c.n} relay not ready after {limit:.0f}s (last pong={c.pong})")
    return False


def pump(c: Ch, conn: socket.socket, rate: int) -> int:
    tx = 0
    start = time.time()
    flag = D / f"abort-ch{c.n}"
    conn.settimeout(0.2)
    while not c.ev_closed.is_set():
        if flag.exists():
            flag.unlink()
            log(f"ch{c.n} ABORT: closing the device side at tx={tx}")
            send(3 | (c.n << 4))
            c.ev_closed.wait(5)
            break
        try:
            d = conn.recv(1024)
        except TimeoutError:
            continue
        if not d:
            break
        while not c.credit.acquire(timeout=0.5):
            if c.ev_closed.is_set():
                return tx
        d = bytes([c.seq & 255, sum(d) & 255]) + d
        c.seq += 1
        send(2 | (c.n << 4), d)
        tx += len(d)
        if (ahead := tx / rate - (time.time() - start)) > 0:
            time.sleep(ahead)
    return tx


def serve(c: Ch, lport: int, tport: int, rate: int) -> None:
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", lport))
    srv.listen(1)
    log(f"ch{c.n} listening {lport} -> target:{tport} at {rate} B/s")
    while True:
        conn, _ = srv.accept()
        t = time.time()
        if not wait_ready(c):
            conn.close()
            continue
        c.ev_open.clear()
        c.ev_closed.clear()
        c.rx = 0
        c.seq = 0
        c.credit = threading.Semaphore(4)
        send(1 | (c.n << 4), struct.pack("<H", tport))
        if not c.ev_open.wait(15) or c.err:
            log(f"ch{c.n} open failed err={c.err}")
            conn.close()
            continue
        c.conn = conn
        start = time.time()
        log(f"ch{c.n} open ok (relay ready {start - t:.1f}s)")
        tx = 0
        try:
            tx = pump(c, conn, rate)
        except OSError as e:
            log(f"ch{c.n} local err {e}")
        if not c.ev_closed.is_set():
            # Let the device reply before closing our side
            c.ev_closed.wait(3)
            if not c.ev_closed.is_set():
                send(3 | (c.n << 4))
                c.ev_closed.wait(5)
        log(f"ch{c.n} session end tx={tx} rx={c.rx} dur={time.time() - start:.1f}s")
        c.conn = None
        with contextlib.suppress(OSError):
            conn.close()


threading.Thread(target=reader, daemon=True).start()
if sys.argv[2] == "--ping":
    t = time.time()
    if not wait_ready(chs[0]):
        sys.exit(1)
    log(f"relay ready after {time.time() - t:.1f}s")
    sys.exit(0)
threading.Thread(target=dropper, daemon=True).start()
for k, spec in enumerate(sys.argv[2:4]):
    lp, tp, r = (int(x) for x in spec.split(":"))
    threading.Thread(target=serve, args=(chs[k], lp, tp, r), daemon=True).start()
while True:
    time.sleep(3600)
