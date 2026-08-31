"""Integration tests for the api outgoing_connection option.

The device dials out to the test's listener when no client with a state
subscription is connected. The listener plays the Home Assistant side over the
accepted socket using aioesphomeapi's sans-IO Noise handshake: the device
sends its server hello first so the listener could pick the right key, and the
NNpsk0 handshake then verifies both sides. Protocol roles stay unchanged, so
the client speaks exactly the same frames as over a normal connection.
"""

from __future__ import annotations

import asyncio
import socket
from typing import Any

from aioesphomeapi import APIClient, api_pb2
import pytest

from .raw_api_client import MESSAGE_TYPE_OF
from .types import APIClientConnectedFactory, RunCompiledFunction

KEY = "bOFFzzvfpg5DB94DuBGLXD/hMnhpDKgP9UQyBulwWVU="
DEVICE_NAME = "outgoing-conn-test"
HA_CLIENT_INFO = "Home Assistant 2026.8.0"


@pytest.fixture(autouse=True)
def isolated_preferences(monkeypatch: pytest.MonkeyPatch, tmp_path) -> None:
    """Keep host preferences per-test so every run starts with no saved peer."""
    monkeypatch.setenv("ESPHOME_PREFDIR", str(tmp_path / "prefs"))


def _frame(payload: bytes) -> bytes:
    return bytes((0x01, len(payload) >> 8, len(payload) & 0xFF)) + payload


async def _read_frame(reader: asyncio.StreamReader, timeout: float = 10.0) -> bytes:
    header = await asyncio.wait_for(reader.readexactly(3), timeout)
    assert header[0] == 0x01, f"Bad frame indicator: {header[0]}"
    return await asyncio.wait_for(
        reader.readexactly((header[1] << 8) | header[2]), timeout
    )


async def _serve_home_assistant(
    listener: socket.socket, *, subscribe_states: bool = False
) -> None:
    """Accept one dial-in from the device and run the client side over it."""
    # Lazy import per the module's own contract (pulls in the noise stack)
    from aioesphomeapi.noise import NoiseHandshake

    loop = asyncio.get_running_loop()
    conn, _ = await asyncio.wait_for(loop.sock_accept(listener), timeout=30)
    reader, writer = await asyncio.open_connection(sock=conn)
    try:
        # On an outgoing connection the device announces itself first so the
        # peer can pick the matching key before its PSK-mixed first message.
        server_hello = await _read_frame(reader)
        assert server_hello[0] == 0x01, "Bad chosen proto in server hello"
        name, mac, _rest = server_hello[1:].split(b"\x00", 2)
        assert name.decode() == DEVICE_NAME
        assert len(mac) == 12, f"Expected bare MAC, got {mac!r}"

        # Normal NNpsk0 handshake: client hello plus PSK-mixed message one,
        # then the device's response completes it and proves the key matches.
        handshake = NoiseHandshake(KEY, b"NoiseAPIInit\x00\x00")
        writer.write(b"\x01\x00\x00" + _frame(b"\x00" + handshake.write_message()))
        await writer.drain()
        reply = await _read_frame(reader)
        assert reply[0] == 0, (
            f"Handshake rejected: {reply[1:].decode(errors='replace')}"
        )
        handshake.read_message(reply[1:])
        encrypt_cipher, decrypt_cipher = handshake.get_ciphers()

        async def transact(msg: Any, response_cls: Any | None) -> Any:
            msg_type = MESSAGE_TYPE_OF[type(msg)]
            payload = msg.SerializeToString()
            plaintext = (
                bytes(
                    (
                        msg_type >> 8,
                        msg_type & 0xFF,
                        len(payload) >> 8,
                        len(payload) & 0xFF,
                    )
                )
                + payload
            )
            writer.write(_frame(encrypt_cipher.encrypt(plaintext)))
            await writer.drain()
            if response_cls is None:
                return None
            want = MESSAGE_TYPE_OF[response_cls]
            while True:
                plain = decrypt_cipher.decrypt(await _read_frame(reader))
                if ((plain[0] << 8) | plain[1]) == want:
                    response = response_cls()
                    response.ParseFromString(bytes(plain[4:]))
                    return response

        await transact(
            api_pb2.HelloRequest(client_info=HA_CLIENT_INFO), api_pb2.HelloResponse
        )
        device_info = await transact(
            api_pb2.DeviceInfoRequest(), api_pb2.DeviceInfoResponse
        )
        assert device_info.name == DEVICE_NAME

        if subscribe_states:
            await transact(api_pb2.SubscribeStatesRequest(), None)
            # No entities are configured, so there is nothing to wait for;
            # give the device a moment to process the subscription.
            await asyncio.sleep(0.5)
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
            await _serve_home_assistant(listener, subscribe_states=True)
    finally:
        listener.close()


@pytest.mark.asyncio
async def test_api_outgoing_connection_remembered(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """No host configured: the device remembers the Home Assistant client that
    connected inbound and dials that address after a restart."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Bound but not yet listening: dial attempts in the first phase are
    # refused, exercising the retry path without queueing stale connections.
    listener.bind(("127.0.0.1", 0))
    port = listener.getsockname()[1]

    try:
        yaml = yaml_config.replace("OUTGOING_PORT", str(port))

        async with (
            run_compiled(yaml),
            api_client_connected(noise_psk=KEY, client_info=HA_CLIENT_INFO) as client,
        ):
            client: APIClient
            device_info = await client.device_info()
            assert device_info.name == DEVICE_NAME
            # Subscribing to states marks this client as Home Assistant;
            # the device persists the peer address for dial-back.
            client.subscribe_states(lambda state: None)
            await asyncio.sleep(1.0)

        # Restart with the same preferences: the device now dials the
        # remembered address on its own.
        listener.listen(2)
        listener.setblocking(False)
        async with run_compiled(yaml):
            await _serve_home_assistant(listener)
    finally:
        listener.close()
