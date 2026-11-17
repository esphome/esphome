"""Serial proxy client for spsim.hpp: subscribes over a raw plaintext API socket and checks every byte.

spclient.py HOST --baud B --mode reply|stream|ash [--pauses N --pause S --read S] [--duration S]
            [--interval MS] [--protocol]

Reads only when told to, with a 4 KB receive buffer set before connect, so a pause fills the TCP
pipe toward the device quickly. Prints per-phase counters and a RESULT line.
"""

from __future__ import annotations

import argparse
import asyncio
import socket
import time

from aioesphomeapi import api_pb2
from aioesphomeapi.core import MESSAGE_TYPE_TO_PROTO

TYPE_OF = {cls: num for num, cls in MESSAGE_TYPE_TO_PROTO.items()}
REPLY_SIZE = 88
DEBUG = False


def varint(v: int) -> bytes:
    out = bytearray()
    while True:
        b = v & 0x7F
        v >>= 7
        if v:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def read_varint(buf: bytearray, pos: int) -> tuple[int, int] | None:
    v = shift = 0
    while pos < len(buf):
        b = buf[pos]
        pos += 1
        v |= (b & 0x7F) << shift
        if not b & 0x80:
            return v, pos
        shift += 7
    return None


def reply(n: int) -> bytes:
    return (
        bytes([0xA5])
        + n.to_bytes(4, "big")
        + bytes((n + j) % 251 for j in range(REPLY_SIZE - 5))
    )


def crc_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (
                ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
            )
    return crc


class ReplyCheck:
    """Expects reply(0), reply(1), ... back to back; counts complete and broken replies."""

    def __init__(self) -> None:
        self.buf = bytearray()
        self.next = 0
        self.complete = 0
        self.broken = 0
        self.out_of_order = 0
        self.received = 0
        self.junk = 0  # leftovers of the baud switch before reply 0

    def feed(self, data: bytes) -> None:
        self.received += len(data)
        self.buf += data
        if (
            self.complete == 0
            and self.next == 0
            and not self.buf.startswith(reply(0)[: len(self.buf)])
        ):
            p = self.buf.find(b"\xa5\x00\x00\x00\x00")
            cut = p if p != -1 else max(len(self.buf) - 4, 0)
            self.junk += cut
            del self.buf[:cut]
        self._scan(False)

    def _scan(self, final: bool) -> None:
        buf = self.buf
        while len(buf) >= REPLY_SIZE:
            if buf[:REPLY_SIZE] == reply(self.next):
                del buf[:REPLY_SIZE]
                self.complete += 1
                self.next += 1
                continue
            found = None
            p = buf.find(0xA5, 1)
            while p != -1 and p + REPLY_SIZE <= len(buf):
                k = int.from_bytes(buf[p + 1 : p + 5], "big")
                if buf[p : p + REPLY_SIZE] == reply(k):
                    found = (p, k)
                    break
                p = buf.find(0xA5, p + 1)
            if found is None:
                if not final and len(buf) < REPLY_SIZE * 8:
                    return
                if not final:
                    return
                self.broken += 1
                del buf[:]
                return
            p, k = found
            if k < self.next:
                self.out_of_order += 1
                del buf[: p + REPLY_SIZE]
                continue
            self.broken += max(k - self.next, 1)
            del buf[:p]
            self.next = k

    def finish(self) -> None:
        self._scan(True)

    def summary(self) -> str:
        return (
            f"complete={self.complete} broken={self.broken} out_of_order={self.out_of_order} "
            f"received={self.received - self.junk} junk={self.junk} next={self.next}"
        )


class AshCheck:
    """Parses ASH DATA frames from spsim and checks the sequence numbers."""

    def __init__(self) -> None:
        self.frame = bytearray()
        self.esc = False
        self.valid = 0
        self.bad = 0
        self.gaps = 0
        self.missing_frames = 0
        self.out_of_order = 0
        self.last = -1
        self.received = 0

    def feed(self, data: bytes) -> None:
        self.received += len(data)
        for b in data:
            if b == 0x7E:
                self._frame()
                self.frame.clear()
                self.esc = False
            elif b == 0x7D:
                self.esc = True
            else:
                self.frame.append(b ^ 0x20 if self.esc else b)
                self.esc = False

    def _frame(self) -> None:
        f = self.frame
        if not f:
            return
        if len(f) != 27 or crc_ccitt(bytes(f[:25])) != int.from_bytes(f[25:27], "big"):
            self.bad += 1
            return
        seq = int.from_bytes(f[1:5], "big")
        self.valid += 1
        if self.last >= 0:
            if seq <= self.last:
                self.out_of_order += 1
            elif seq != self.last + 1:
                self.gaps += 1
                self.missing_frames += seq - self.last - 1
        self.last = seq

    def finish(self) -> None:
        pass

    def summary(self) -> str:
        return (
            f"valid_frames={self.valid} bad_frames={self.bad} gaps={self.gaps} "
            f"missing_frames={self.missing_frames} out_of_order={self.out_of_order} "
            f"received={self.received} last_seq={self.last}"
        )


class Client:
    def __init__(self, host: str, check) -> None:
        self.host = host
        self.check = check
        self.buf = bytearray()
        self.reading = asyncio.Event()
        self.reading.set()
        self.last_data = time.monotonic()
        self.responses: list[str] = []
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        s.setblocking(False)
        self.sock = s
        self.collect = True

    async def send(self, msg) -> None:
        payload = msg.SerializeToString()
        frame = b"\x00" + varint(len(payload)) + varint(TYPE_OF[type(msg)]) + payload
        await asyncio.get_running_loop().sock_sendall(self.sock, frame)

    async def write(self, data: bytes) -> None:
        await self.send(api_pb2.SerialProxyWriteRequest(instance=0, data=data))

    async def reader(self) -> None:
        loop = asyncio.get_running_loop()
        while True:
            await self.reading.wait()
            data = await loop.sock_recv(self.sock, 4096)
            if not data:
                print("CONNECTION CLOSED by device", flush=True)
                return
            self.buf += data
            while (m := self._parse()) is not None:
                await self._handle(*m)

    def _parse(self):
        buf = self.buf
        if not buf:
            return None
        assert buf[0] == 0, f"indicator {buf[0]}"
        if (a := read_varint(buf, 1)) is None:
            return None
        size, pos = a
        if (b := read_varint(buf, pos)) is None:
            return None
        mtype, pos = b
        if len(buf) - pos < size:
            return None
        payload = bytes(buf[pos : pos + size])
        del buf[: pos + size]
        return mtype, payload

    async def _handle(self, mtype: int, payload: bytes) -> None:
        if mtype == TYPE_OF[api_pb2.PingRequest]:
            await self.send(api_pb2.PingResponse())
        elif mtype == TYPE_OF[api_pb2.SerialProxyDataReceived]:
            data = api_pb2.SerialProxyDataReceived.FromString(payload).data
            self.last_data = time.monotonic()
            if self.collect:
                if DEBUG:
                    print(
                        f"{time.monotonic():.2f} RX {len(data)} {data.hex()}",
                        flush=True,
                    )
                self.check.feed(data)
        elif mtype == TYPE_OF[api_pb2.SerialProxyRequestResponse]:
            r = api_pb2.SerialProxyRequestResponse.FromString(payload)
            self.responses.append(f"type={r.type} status={r.status} {r.error_message}")
        elif mtype == TYPE_OF[api_pb2.HelloResponse]:
            self.responses.append("hello")
        elif mtype == TYPE_OF[api_pb2.DisconnectRequest]:
            print("DISCONNECT REQUEST from device", flush=True)


async def configure(c: Client, baud: int) -> None:
    await c.send(
        api_pb2.SerialProxyConfigureRequest(
            instance=0, baudrate=baud, stop_bits=1, data_size=8, parity=0
        )
    )


async def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=6053)
    ap.add_argument("--baud", type=int, default=1920)
    ap.add_argument("--mode", choices=["reply", "stream", "ash"], default="reply")
    ap.add_argument(
        "--interval",
        type=float,
        default=600,
        help="request interval in ms (reply mode)",
    )
    ap.add_argument(
        "--duration", type=float, default=0, help="plain reading time in s, no pauses"
    )
    ap.add_argument("--pauses", type=int, default=0)
    ap.add_argument("--pause", type=float, default=10)
    ap.add_argument("--read", type=float, default=15)
    ap.add_argument(
        "--protocol",
        action="store_true",
        help="put the port in PROTOCOL mode (tap active)",
    )
    ap.add_argument("--debug", action="store_true")
    a = ap.parse_args()
    global DEBUG  # noqa: PLW0603
    DEBUG = a.debug

    check = AshCheck() if a.mode == "ash" else ReplyCheck()
    c = Client(a.host, check)
    loop = asyncio.get_running_loop()
    await loop.sock_connect(c.sock, (a.host, a.port))
    print(
        f"SO_RCVBUF={c.sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)}",
        flush=True,
    )
    rtask = asyncio.create_task(c.reader())
    await c.send(
        api_pb2.HelloRequest(
            client_info="spclient", api_version_major=1, api_version_minor=10
        )
    )
    await asyncio.sleep(0.5)
    await c.send(
        api_pb2.SerialProxyRequest(
            instance=0, type=api_pb2.SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE
        )
    )
    await asyncio.sleep(0.3)
    # Bring the simulator to the target baud from whichever rate it is on, then the proxy
    c.collect = False
    for b in (1920, 115200):
        await configure(c, b)
        await asyncio.sleep(0.3)
        await c.write(b"\x7eB%d\x7e" % a.baud)
        await asyncio.sleep(0.6)
    await configure(c, a.baud)
    await asyncio.sleep(0.3)
    await c.write(b"\x7eX\x7e")
    await asyncio.sleep(0.3)
    await c.send(
        api_pb2.SerialProxySetModeRequest(
            instance=0,
            mode=api_pb2.SERIAL_PROXY_MODE_PROTOCOL
            if a.protocol
            else api_pb2.SERIAL_PROXY_MODE_RAW,
        )
    )
    await asyncio.sleep(1.5)
    c.collect = True
    print(f"setup responses: {c.responses}", flush=True)

    stop = asyncio.Event()
    requests = 0

    async def requester() -> None:
        nonlocal requests
        while not stop.is_set():
            await c.write(b"\x7eR\x7e")
            requests += 1
            await asyncio.sleep(a.interval / 1000)

    if a.mode == "reply":
        wtask = asyncio.create_task(requester())
    else:
        await c.write(b"\x7eS\x7e" if a.mode == "stream" else b"\x7eA\x7e")
        wtask = None

    t0 = time.monotonic()
    if a.duration:
        end = t0 + a.duration
        while time.monotonic() < end:
            await asyncio.sleep(min(30, end - time.monotonic()))
            print(f"t={time.monotonic() - t0:.0f}s {check.summary()}", flush=True)
    for i in range(a.pauses):
        await asyncio.sleep(a.read)
        print(
            f"t={time.monotonic() - t0:.0f}s before pause {i + 1}: {check.summary()}",
            flush=True,
        )
        c.reading.clear()
        await asyncio.sleep(a.pause)
        c.reading.set()
    if a.pauses:
        await asyncio.sleep(a.read)
    stop.set()
    if wtask:
        await wtask
    await asyncio.sleep(1.0)
    await c.write(b"\x7eX\x7e")
    await asyncio.sleep(3)
    # Drain: until nothing arrives for 3 s
    while time.monotonic() - c.last_data < 3:
        await asyncio.sleep(0.5)
    check.finish()
    print(
        f"RESULT mode={a.mode} baud={a.baud} requests={requests} {check.summary()}",
        flush=True,
    )
    rtask.cancel()
    c.sock.close()


if __name__ == "__main__":
    asyncio.run(main())
