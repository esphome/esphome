"""End-to-end test of `store_yaml` recovery over the native API.

Uses the raw client: released aioesphomeapi does not know GetYamlResponse yet.
"""

from __future__ import annotations

from aioesphomeapi import api_pb2
import pytest

from esphome.components.store_yaml import unpack_envelope
from esphome.helpers import zstd_module
from esphome.yaml_util import find_secret_references

from .raw_api_client import MESSAGE_TYPE_OF, RawApiClient, decode_fields
from .types import RunCompiledFunction

# Not in the released aioesphomeapi yet; see esphome/components/api/api.proto.
GET_YAML_REQUEST = 159
GET_YAML_RESPONSE = 160
# DeviceCapabilitiesResponse.store_yaml and StoreYamlCapabilities.supported
CAPABILITIES_STORE_YAML_FIELD = 6
STORE_YAML_SUPPORTED_FIELD = 1


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
        capabilities = decode_fields(
            await client.read_frame(MESSAGE_TYPE_OF[api_pb2.DeviceCapabilitiesResponse])
        )
        (store_yaml,) = capabilities[CAPABILITIES_STORE_YAML_FIELD]
        assert decode_fields(store_yaml).get(STORE_YAML_SUPPORTED_FIELD) == [1], (
            "expected DeviceCapabilitiesResponse to report store_yaml.supported"
        )

        await client.send_raw(GET_YAML_REQUEST, b"")
        chunks: list[bytes] = []
        advertised_total: int | None = None
        advertised_encoding: str | None = None
        done = False
        while not done:
            fields = decode_fields(await client.read_frame(GET_YAML_RESPONSE))
            if data := fields.get(1):
                chunks.append(data[0])
            done = fields.get(2) == [1]
            if total := fields.get(3):
                advertised_total = total[0]
            if encoding := fields.get(4):
                advertised_encoding = encoding[0].decode("utf-8")

    compressed = b"".join(chunks)
    assert advertised_encoding == "zstd", (
        f"expected encoding 'zstd', got {advertised_encoding!r}"
    )
    assert advertised_total == len(compressed), (
        f"server advertised {advertised_total} bytes but we received {len(compressed)}"
    )

    envelope = zstd_module().decompress(compressed)
    files = unpack_envelope(envelope)

    assert files, "envelope should contain at least one file"
    combined = b"\n".join(files.values())
    assert b"store-yaml-test" in combined, (
        "expected the fixture's device name to round-trip through the recovery blob"
    )
    assert b"store_yaml:" in combined, (
        "expected the store_yaml config line to be in the recovery blob"
    )

    # The inline cv.sensitive OTA password must be recovered as a `!secret`
    # reference, never as its raw value, and the synthetic secrets.yaml
    # skeleton must list the key so the recovered config is flashable.
    assert b"recoverme123" not in envelope, (
        "inline sensitive value leaked into the recovery blob"
    )
    assert "ota_password" in find_secret_references(combined.decode()), (
        "expected the inline OTA password to be recovered as a !secret reference"
    )
    assert b'ota_password: ""' in files["secrets.yaml"], (
        "expected the secrets.yaml skeleton to list the ota_password key"
    )
