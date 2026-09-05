"""Integration tests for the api outgoing_connection option.

The device dials out to the test's listener when no dial-back target client is
connected. The listener plays the Home Assistant side over the accepted socket
using aioesphomeapi's sans-IO Noise handshake: the device sends its server
hello first so the listener could pick the right key, and the NNpsk0 handshake
then verifies both sides. Protocol roles stay unchanged, so the client speaks
exactly the same frames as over a normal connection. A client becomes the
remembered dial-back target by setting the outgoing_connection_target flag in
its hello.
"""

from __future__ import annotations

import asyncio
import socket
from typing import Any

from aioesphomeapi import api_pb2
import pytest

from .raw_api_client import MESSAGE_TYPE_OF
from .types import RunCompiledFunction

KEY = "bOFFzzvfpg5DB94DuBGLXD/hMnhpDKgP9UQyBulwWVU="
DEVICE_NAME = "outgoing-conn-test"
HA_CLIENT_INFO = "Home Assistant 2026.8.0"
# HelloRequest field 4 (outgoing_connection_target) as raw protobuf bytes; the
# installed aioesphomeapi's api_pb2 predates the field, so append it manually.
HELLO_TARGET_FLAG = b"\x20\x01"


# Every run must start with no saved peer
pytestmark = pytest.mark.usefixtures("isolated_preferences")


def _frame(payload: bytes) -> bytes:
    return bytes((0x01, len(payload) >> 8, len(payload) & 0xFF)) + payload


async def _read_frame(reader: asyncio.StreamReader, timeout: float = 10.0) -> bytes:
    header = await asyncio.wait_for(reader.readexactly(3), timeout)
    assert header[0] == 0x01, f"Bad frame indicator: {header[0]}"
    return await asyncio.wait_for(
        reader.readexactly((header[1] << 8) | header[2]), timeout
    )


def _check_server_hello(server_hello: bytes) -> None:
    assert server_hello[0] == 0x01, "Bad chosen proto in server hello"
    name, mac, _rest = server_hello[1:].split(b"\x00", 2)
    assert name.decode() == DEVICE_NAME
    assert len(mac) == 12, f"Expected bare MAC, got {mac!r}"


async def _run_ha_session(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    *,
    device_dialed_out: bool,
) -> None:
    """Handshake and exchange the usual first messages as Home Assistant would."""
    # Lazy import per the module's own contract (pulls in the noise stack)
    from aioesphomeapi.noise import NoiseHandshake

    if device_dialed_out:
        # On an outgoing connection the device announces itself first so the
        # peer can pick the matching key before its PSK-mixed first message.
        _check_server_hello(await _read_frame(reader))

    handshake = NoiseHandshake(KEY, b"NoiseAPIInit\x00\x00")
    writer.write(b"\x01\x00\x00" + _frame(b"\x00" + handshake.write_message()))
    await writer.drain()

    if not device_dialed_out:
        _check_server_hello(await _read_frame(reader))

    reply = await _read_frame(reader)
    assert reply[0] == 0, f"Handshake rejected: {reply[1:].decode(errors='replace')}"
    handshake.read_message(reply[1:])
    encrypt_cipher, decrypt_cipher = handshake.get_ciphers()

    async def transact(msg: Any, response_cls: Any, extra_payload: bytes = b"") -> Any:
        msg_type = MESSAGE_TYPE_OF[type(msg)]
        payload = msg.SerializeToString() + extra_payload
        plaintext = (
            bytes(
                (msg_type >> 8, msg_type & 0xFF, len(payload) >> 8, len(payload) & 0xFF)
            )
            + payload
        )
        writer.write(_frame(encrypt_cipher.encrypt(plaintext)))
        await writer.drain()
        want = MESSAGE_TYPE_OF[response_cls]
        while True:
            plain = decrypt_cipher.decrypt(await _read_frame(reader))
            if ((plain[0] << 8) | plain[1]) == want:
                response = response_cls()
                response.ParseFromString(bytes(plain[4:]))
                return response

    # Declare this client a dial-back target in the hello
    await transact(
        api_pb2.HelloRequest(client_info=HA_CLIENT_INFO),
        api_pb2.HelloResponse,
        extra_payload=HELLO_TARGET_FLAG,
    )
    device_info = await transact(
        api_pb2.DeviceInfoRequest(), api_pb2.DeviceInfoResponse
    )
    assert device_info.name == DEVICE_NAME


async def _serve_home_assistant(listener: socket.socket) -> None:
    """Accept one dial-in from the device and run the client side over it."""
    loop = asyncio.get_running_loop()
    conn, _ = await asyncio.wait_for(loop.sock_accept(listener), timeout=30)
    reader, writer = await asyncio.open_connection(sock=conn)
    try:
        await _run_ha_session(reader, writer, device_dialed_out=True)
    finally:
        writer.close()


@pytest.mark.asyncio
async def test_api_outgoing_connection(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """With a configured host the device dials out and speaks the normal API."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(2)
    listener.setblocking(False)
    port = listener.getsockname()[1]

    try:
        yaml = yaml_config.replace("OUTGOING_PORT", str(port))
        async with run_compiled(yaml):
            await _serve_home_assistant(listener)
    finally:
        listener.close()


@pytest.mark.asyncio
async def test_api_outgoing_connection_remembered(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    """No host configured: the device remembers the client whose hello carried
    the dial-back flag and dials that address after a restart."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Bound but not yet listening so first-phase dials cannot queue stale
    # connections; whether the device attempts any dial before the restart
    # is timing dependent and not asserted here.
    listener.bind(("127.0.0.1", 0))
    port = listener.getsockname()[1]

    try:
        yaml = yaml_config.replace("OUTGOING_PORT", str(port))

        async with run_compiled(yaml):
            # Connect inbound with the dial-back flag; the device persists the
            # peer address during the hello.
            reader, writer = await asyncio.open_connection("127.0.0.1", unused_tcp_port)
            try:
                await _run_ha_session(reader, writer, device_dialed_out=False)
            finally:
                writer.close()

        # Restart with the same preferences: the device now dials the
        # remembered address on its own.
        listener.listen(2)
        listener.setblocking(False)
        async with run_compiled(yaml):
            await _serve_home_assistant(listener)
    finally:
        listener.close()
