"""End-to-end test for the `store_yaml` recovery flow over the native API.

Talks plaintext API to a host build directly via asyncio sockets rather than
through aioesphomeapi: the released aioesphomeapi shipped with this PR does
not yet know about `GetYamlRequest` / `GetYamlResponse`, so the high-level
client would silently drop the streamed bytes as "unknown message type".

The raw client implements just enough of the plaintext framing
(``0x00 | varint(size) | varint(msg_type) | payload``, see
``api_frame_helper_plaintext.cpp``) to send the empty `GetYamlRequest`
(message type 149) and accumulate every `GetYamlResponse` (message type 150)
until ``done=true``.
"""

from __future__ import annotations

import asyncio
import contextlib
import struct

import pytest

try:
    from compression import zstd  # type: ignore[import-not-found]
except ImportError:
    from backports import zstd  # type: ignore[import-not-found, no-redef]

from .types import RunCompiledFunction

# Message IDs from esphome/components/api/api.proto.
HELLO_REQUEST = 1
HELLO_RESPONSE = 2
GET_YAML_REQUEST = 149
GET_YAML_RESPONSE = 150

ENVELOPE_MAGIC = b"EHY1"


def _encode_varint(value: int) -> bytes:
    """Encode an unsigned integer as a protobuf varint."""
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def _read_varint(buf: bytes, pos: int) -> tuple[int, int]:
    result = 0
    shift = 0
    while True:
        b = buf[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result, pos
        shift += 7


def _parse_get_yaml_response(payload: bytes) -> tuple[bytes, bool, int, str]:
    """Hand-rolled parser for `GetYamlResponse`.

    Returns ``(data, done, total_size, encoding)``.
    """
    data = b""
    done = False
    total_size = 0
    encoding = ""
    pos = 0
    while pos < len(payload):
        tag, pos = _read_varint(payload, pos)
        field_number = tag >> 3
        wire_type = tag & 0x07
        if wire_type == 0:  # varint
            value, pos = _read_varint(payload, pos)
            if field_number == 2:
                done = bool(value)
            elif field_number == 3:
                total_size = value
        elif wire_type == 2:  # length-delimited
            length, pos = _read_varint(payload, pos)
            chunk = payload[pos : pos + length]
            pos += length
            if field_number == 1:
                data = chunk
            elif field_number == 4:
                encoding = chunk.decode("utf-8")
        else:
            raise AssertionError(f"unexpected wire type {wire_type}")
    return data, done, total_size, encoding


def _unpack_envelope(blob: bytes) -> dict[str, bytes]:
    """Inverse of `_pack_envelope` in `esphome/components/store_yaml/__init__.py`."""
    assert blob[:4] == ENVELOPE_MAGIC, "envelope must start with EHY1 magic"
    pos = 4
    (count,) = struct.unpack_from("<I", blob, pos)
    pos += 4
    files: dict[str, bytes] = {}
    for _ in range(count):
        (path_len,) = struct.unpack_from("<H", blob, pos)
        pos += 2
        path = blob[pos : pos + path_len].decode("utf-8")
        pos += path_len
        (content_len,) = struct.unpack_from("<I", blob, pos)
        pos += 4
        content = blob[pos : pos + content_len]
        pos += content_len
        files[path] = content
    assert pos == len(blob), "envelope must consume all bytes"
    return files


class _PlaintextClient:
    """Just-enough plaintext API client for one short streaming exchange."""

    def __init__(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        self._reader = reader
        self._writer = writer

    async def send(self, msg_type: int, payload: bytes = b"") -> None:
        # Frame: 0x00 | varint(payload_size) | varint(message_id) | payload
        frame = (
            b"\x00" + _encode_varint(len(payload)) + _encode_varint(msg_type) + payload
        )
        self._writer.write(frame)
        await self._writer.drain()

    async def recv(self) -> tuple[int, bytes]:
        # Read preamble byte (must be 0x00 for plaintext).
        preamble = await self._reader.readexactly(1)
        assert preamble == b"\x00", f"unexpected preamble {preamble!r}"

        async def _read_varint_stream() -> int:
            result = 0
            shift = 0
            while True:
                byte = (await self._reader.readexactly(1))[0]
                result |= (byte & 0x7F) << shift
                if not (byte & 0x80):
                    return result
                shift += 7

        payload_size = await _read_varint_stream()
        msg_type = await _read_varint_stream()
        payload = await self._reader.readexactly(payload_size) if payload_size else b""
        return msg_type, payload


@pytest.mark.asyncio
async def test_store_yaml_recovery(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    """Compile a host build with `store_yaml`, ask it to stream the YAML back,
    decompress, and verify the recovered file tree matches the source fixture."""
    async with run_compiled(yaml_config):
        # Open a raw TCP connection to the API server.
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection("127.0.0.1", unused_tcp_port),
            timeout=10.0,
        )
        client = _PlaintextClient(reader, writer)
        try:
            # HelloRequest: client_info (field 1, length-delimited string).
            # Password auth (the old ConnectRequest/Response exchange at message
            # IDs 3/4) was removed in 2026.1.0, so a successful HelloResponse is
            # all the handshake we need before issuing application requests.
            client_info = b"store_yaml integration test"
            api_version = b"\x10\x01\x18\x0e"  # api_version_major=1, minor=14
            hello_payload = (
                b"\x0a" + _encode_varint(len(client_info)) + client_info + api_version
            )
            await client.send(HELLO_REQUEST, hello_payload)
            msg_type, _ = await asyncio.wait_for(client.recv(), timeout=5.0)
            assert msg_type == HELLO_RESPONSE, f"expected HelloResponse, got {msg_type}"

            # The actual request under test.
            await client.send(GET_YAML_REQUEST, b"")

            chunks: list[bytes] = []
            advertised_total: int | None = None
            advertised_encoding: str | None = None
            done = False
            while not done:
                msg_type, payload = await asyncio.wait_for(client.recv(), timeout=5.0)
                if msg_type != GET_YAML_RESPONSE:
                    # Tolerate intervening server messages (e.g. pings).
                    continue
                chunk, done, total_size, encoding = _parse_get_yaml_response(payload)
                if encoding:
                    advertised_encoding = encoding
                if total_size and advertised_total is None:
                    advertised_total = total_size
                if chunk:
                    chunks.append(chunk)
        finally:
            writer.close()
            with contextlib.suppress(ConnectionError, OSError):
                await writer.wait_closed()

    compressed = b"".join(chunks)
    assert advertised_encoding == "zstd", (
        f"expected encoding 'zstd', got {advertised_encoding!r}"
    )
    assert advertised_total == len(compressed), (
        f"server advertised {advertised_total} bytes but we received {len(compressed)}"
    )

    envelope = zstd.decompress(compressed)
    files = _unpack_envelope(envelope)

    assert files, "envelope should contain at least one file"
    combined = b"\n".join(files.values())
    assert b"store-yaml-test" in combined, (
        "expected the fixture's device name to round-trip through the recovery blob"
    )
    assert b"store_yaml:" in combined, (
        "expected the store_yaml config line to be in the recovery blob"
    )
