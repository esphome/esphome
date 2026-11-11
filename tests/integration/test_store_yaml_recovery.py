"""End-to-end test of `store_yaml` recovery over the native API.

Uses the raw client so a test controls when it reads; aioesphomeapi has no get_yaml helper.
"""

from __future__ import annotations

import asyncio
import random

from aioesphomeapi import api_pb2
import pytest

from esphome.components.store_yaml import unpack_envelope
from esphome.helpers import zstd_module
from esphome.yaml_util import find_secret_references

from .log_utils import LineWaiter
from .raw_api_client import MESSAGE_TYPE_OF, RawApiClient
from .types import RunCompiledFunction

GET_YAML_RESPONSE = MESSAGE_TYPE_OF[api_pb2.GetYamlResponse]


async def _read_yaml_stream(client: RawApiClient) -> list[api_pb2.GetYamlResponse]:
    """Read GetYamlResponse chunks up to and including the one marked done."""
    responses: list[api_pb2.GetYamlResponse] = []
    while not responses or not responses[-1].done:
        payload = await client.read_frame(GET_YAML_RESPONSE)
        responses.append(api_pb2.GetYamlResponse.FromString(payload))
    return responses


@pytest.mark.asyncio
async def test_store_yaml_recovery(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    """Stream the embedded YAML back from a host build and check the recovered files."""
    async with run_compiled(yaml_config), RawApiClient(unused_tcp_port) as client:
        await client.connect("store_yaml integration test")

        # Clients learn the device can answer get_yaml from its capabilities.
        await client.send_message(api_pb2.DeviceCapabilitiesRequest())
        capabilities = api_pb2.DeviceCapabilitiesResponse.FromString(
            await client.read_frame(MESSAGE_TYPE_OF[api_pb2.DeviceCapabilitiesResponse])
        )
        assert capabilities.store_yaml.supported

        await client.send_message(api_pb2.GetYamlRequest())
        responses = await _read_yaml_stream(client)

    compressed = b"".join(r.data for r in responses)
    assert responses[0].total_size == len(compressed)

    envelope = zstd_module().decompress(compressed)
    files = unpack_envelope(envelope)
    combined = b"\n".join(files.values())
    assert b"store-yaml-test" in combined
    assert b"store_yaml:" in combined
    # The inline OTA password comes back as a !secret listed in the skeleton.
    assert b"recoverme123" not in envelope
    assert "ota_password" in find_secret_references(combined.decode())
    assert b"ota_password: ''" in files["secrets.yaml"]


# Random hex barely compresses, so the blob spans many chunks and outgrows both buffers
SERVER_SNDBUF = 8192
RECV_BUFFER_SIZE = 4096
CHUNK_LIMIT = 1390


@pytest.mark.asyncio
async def test_store_yaml_recovery_backpressure(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    """A client that stops reading mid-stream still recovers every byte once it drains."""
    rng = random.Random(16445)
    values = {f"blob_{i}": rng.randbytes(60).hex() for i in range(1000)}
    config = yaml_config.replace("SERVER_SNDBUF", str(SERVER_SNDBUF)).replace(
        "# GENERATED_SUBSTITUTIONS",
        "\n".join(f"  {name}: {value}" for name, value in values.items()),
    )
    log = LineWaiter()
    async with (
        run_compiled(config, line_callback=log.callback),
        RawApiClient(unused_tcp_port, recv_buffer_size=RECV_BUFFER_SIZE) as client,
    ):
        # Without the pin the kernel buffers could swallow the stall
        await log.wait_for("SO_SNDBUF pinned to", timeout=10.0)
        await client.connect("store_yaml backpressure test")
        await client.send_message(api_pb2.GetYamlRequest())
        # Stop reading so the device fills both buffers and has to resume later
        await asyncio.sleep(0.5)
        responses = await _read_yaml_stream(client)

    chunks = [r.data for r in responses]
    compressed = b"".join(chunks)
    assert len(compressed) > 4 * (SERVER_SNDBUF + RECV_BUFFER_SIZE)
    assert len(responses) > 1 and all(len(c) <= CHUNK_LIMIT for c in chunks)
    assert not any(r.done for r in responses[:-1])
    assert responses[0].total_size == len(compressed)
    combined = b"\n".join(
        unpack_envelope(zstd_module().decompress(compressed)).values()
    )
    assert all(value.encode() in combined for value in values.values())
