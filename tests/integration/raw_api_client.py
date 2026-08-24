"""Minimal plaintext native-api client over a raw socket.

Reads only when told to, so tests control when the TCP pipe backs up toward
the device; payloads are skipped and only message types are counted.
"""

from __future__ import annotations

import asyncio
from collections import Counter
import socket
from typing import Self

from aioesphomeapi import api_pb2
import aioesphomeapi.core as api_core
from google.protobuf import message

from .const import LOCALHOST

# Message type ids are protocol constants; derive them from aioesphomeapi so
# they cannot drift from the client library in use.
MESSAGE_TYPE_OF = {cls: num for num, cls in api_core.MESSAGE_TYPE_TO_PROTO.items()}

_READ_CHUNK = 4096


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


def encode_frame(msg_type: int, payload: bytes) -> bytes:
    """Encode one plaintext api frame: 0x00, payload length, message type."""
    return b"\x00" + encode_varint(len(payload)) + encode_varint(msg_type) + payload


class FrameParser:
    """Incremental parser for the plaintext api frame stream."""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> list[int]:
        self._buf.extend(data)
        types: list[int] = []
        while (msg_type := self._try_parse()) is not None:
            types.append(msg_type)
        return types

    def _try_parse(self) -> int | None:
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
        del buf[: pos + size]
        return msg_type


class RawApiClient:
    """Plaintext api client whose reads happen only on request."""

    def __init__(self, port: int, recv_buffer_size: int | None = None) -> None:
        self._port = port
        self._parser = FrameParser()
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
        loop = asyncio.get_running_loop()
        await loop.sock_sendall(
            self._sock,
            encode_frame(MESSAGE_TYPE_OF[type(msg)], msg.SerializeToString()),
        )

    async def read_until_frame(self, msg_type: int, timeout: float = 10.0) -> None:
        """Read until at least one frame of msg_type has been received."""
        loop = asyncio.get_running_loop()

        async def _read_loop() -> None:
            while not self.frame_counts[msg_type]:
                data = await loop.sock_recv(self._sock, _READ_CHUNK)
                assert data, "server closed the connection unexpectedly"
                self.bytes_received += len(data)
                self.frame_counts.update(self._parser.feed(data))

        await asyncio.wait_for(_read_loop(), timeout)

    def close(self) -> None:
        self._sock.close()
