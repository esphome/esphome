"""Minimal plaintext native-api client over a raw socket.

Reads only when told to, so tests control when the TCP pipe backs up toward
the device; message types are counted and each frame is kept for inspection,
and read_frame() hands them out in arrival order.
"""

from __future__ import annotations

import asyncio
from collections import Counter
import socket
import struct
from typing import Self

from aioesphomeapi import api_pb2
import aioesphomeapi.core as api_core
from google.protobuf import message

from .const import LOCALHOST

# Message type ids are protocol constants; derive them from aioesphomeapi so
# they cannot drift from the client library in use.
MESSAGE_TYPE_OF = {cls: num for num, cls in api_core.MESSAGE_TYPE_TO_PROTO.items()}

_READ_CHUNK = 4096

# Backpressure tests pin both ends of the pipe small; only tens of KB fit in the kernel
RECV_BUFFER_SIZE = 4096
SERVER_SNDBUF = (
    8192  # substituted for SERVER_SNDBUF in fixtures using sndbuf_pin_component
)
# Logged by the sndbuf_pin_component fixture when it pins a socket
SNDBUF_PIN_LOG = "SO_SNDBUF pinned to"


def pin_server_sndbuf(yaml_config: str) -> str:
    """Substitute the pinned server send buffer size into a fixture."""
    return yaml_config.replace("SERVER_SNDBUF", str(SERVER_SNDBUF))


def encode_varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def decode_varint(buf: bytearray, pos: int) -> tuple[int, int] | None:
    """Decode one varint at pos; return (value, new_pos) or None if short."""
    value = shift = 0
    while pos < len(buf):
        byte = buf[pos]
        pos += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, pos
        shift += 7
    return None


WIRE_VARINT, WIRE_LENGTH, WIRE_FIXED32 = 0, 2, 5


def tag(field: int, wire_type: int) -> bytes:
    return encode_varint((field << 3) | wire_type)


def decode_fields(payload: bytes) -> dict[int, list[int | bytes]]:
    """Split a protobuf payload into field number -> raw values; a test reads fields the client lacks."""
    fields: dict[int, list[int | bytes]] = {}
    buf = bytearray(payload)
    pos = 0
    while pos < len(buf):
        key, pos = decode_varint(buf, pos)
        field, wire_type = key >> 3, key & 0x07
        if wire_type == WIRE_VARINT:
            value, pos = decode_varint(buf, pos)
        elif wire_type == WIRE_LENGTH:
            length, pos = decode_varint(buf, pos)
            value = bytes(buf[pos : pos + length])
            pos += length
        elif wire_type == WIRE_FIXED32:
            value = struct.unpack_from("<I", buf, pos)[0]
            pos += 4
        else:
            raise AssertionError(f"unexpected wire type {wire_type} for field {field}")
        fields.setdefault(field, []).append(value)
    return fields


def encode_frame(msg_type: int, payload: bytes) -> bytes:
    """Encode one plaintext api frame: 0x00, payload length, message type."""
    return b"\x00" + encode_varint(len(payload)) + encode_varint(msg_type) + payload


class FrameParser:
    """Incremental parser for the plaintext api frame stream."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self.frames: list[tuple[int, bytes]] = []

    def feed(self, data: bytes) -> list[int]:
        self._buf.extend(data)
        types: list[int] = []
        while (frame := self._try_parse()) is not None:
            self.frames.append(frame)
            types.append(frame[0])
        return types

    def _try_parse(self) -> tuple[int, bytes] | None:
        buf = self._buf
        if not buf:
            return None
        assert buf[0] == 0, f"expected plaintext frame, got indicator {buf[0]}"
        if (size_decoded := decode_varint(buf, 1)) is None:
            return None
        size, pos = size_decoded
        if (type_decoded := decode_varint(buf, pos)) is None:
            return None
        msg_type, pos = type_decoded
        if len(buf) - pos < size:
            return None
        payload = bytes(buf[pos : pos + size])
        del buf[: pos + size]
        return msg_type, payload


class RawApiClient:
    """Plaintext api client whose reads happen only on request."""

    def __init__(self, port: int, recv_buffer_size: int | None = None) -> None:
        self._port = port
        self._parser = FrameParser()
        self._read_pos = 0  # frames before this index were consumed by read_frame()
        self.bytes_received = 0
        self.frame_counts: Counter[int] = Counter()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if recv_buffer_size is not None:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, recv_buffer_size)
                # Kernels may round up (Linux doubles) but must not clamp below
                applied = sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
                assert applied >= recv_buffer_size, (
                    f"SO_RCVBUF clamped to {applied}, requested {recv_buffer_size}"
                )
            sock.setblocking(False)
        except Exception:
            sock.close()
            raise
        self._sock = sock

    async def __aenter__(self) -> Self:
        return self

    async def __aexit__(self, *exc_info: object) -> None:
        self.close()

    async def connect(self, client_info: str = "raw-api-client") -> None:
        """Connect and complete the Hello handshake (no auth step since 2026.1.0)."""
        loop = asyncio.get_running_loop()
        await loop.sock_connect(self._sock, (LOCALHOST, self._port))
        hello = api_pb2.HelloRequest()
        hello.client_info = client_info
        hello.api_version_major = 1
        hello.api_version_minor = 10
        await self.send_message(hello)
        await self.read_until_frame(MESSAGE_TYPE_OF[api_pb2.HelloResponse])

    async def send_message(self, msg: message.Message) -> None:
        await self.send_raw(MESSAGE_TYPE_OF[type(msg)], msg.SerializeToString())

    async def send_raw(self, msg_type: int, payload: bytes) -> None:
        """Send a frame with a hand built payload, for shapes protobuf will not serialize."""
        loop = asyncio.get_running_loop()
        await loop.sock_sendall(self._sock, encode_frame(msg_type, payload))

    async def _recv_once(self) -> None:
        data = await asyncio.get_running_loop().sock_recv(self._sock, _READ_CHUNK)
        assert data, "server closed the connection unexpectedly"
        self.bytes_received += len(data)
        self.frame_counts.update(self._parser.feed(data))

    async def read_until_frame(
        self, msg_type: int, timeout: float = 10.0, count: int = 1
    ) -> None:
        """Read until at least count frames of msg_type have been received."""

        async def _read_loop() -> None:
            while self.frame_counts[msg_type] < count:
                await self._recv_once()

        await asyncio.wait_for(_read_loop(), timeout)

    async def read_frame(self, msg_type: int, timeout: float = 10.0) -> bytes:
        """Return the oldest unconsumed payload of msg_type, skipping earlier frames of other types."""
        frames = self._parser.frames

        async def _read_loop() -> bytes:
            while True:
                while self._read_pos < len(frames):
                    frame_type, payload = frames[self._read_pos]
                    self._read_pos += 1
                    if frame_type == msg_type:
                        return payload
                await self._recv_once()

        return await asyncio.wait_for(_read_loop(), timeout)

    def frames(self, msg_type: int) -> list[bytes]:
        """Payloads of every frame of msg_type received so far."""
        return [payload for kind, payload in self._parser.frames if kind == msg_type]

    def close(self) -> None:
        self._sock.close()
