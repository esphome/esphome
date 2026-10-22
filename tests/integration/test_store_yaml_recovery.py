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
GET_YAML_REQUEST = 161
GET_YAML_RESPONSE = 162
# DeviceCapabilitiesResponse.store_yaml and StoreYamlCapabilities.supported
CAPABILITIES_STORE_YAML_FIELD = 7
STORE_YAML_SUPPORTED_FIELD = 1
# GetYamlResponse fields
DATA_FIELD, DONE_FIELD, TOTAL_SIZE_FIELD, ENCODING_FIELD = 1, 2, 3, 4


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
        assert decode_fields(store_yaml).get(STORE_YAML_SUPPORTED_FIELD) == [1]

        await client.send_raw(GET_YAML_REQUEST, b"")
        responses: list[dict[int, list[int | bytes]]] = []
        while not responses or responses[-1].get(DONE_FIELD) != [1]:
            responses.append(decode_fields(await client.read_frame(GET_YAML_RESPONSE)))

    compressed = b"".join(r[DATA_FIELD][0] for r in responses if DATA_FIELD in r)
    assert responses[0][ENCODING_FIELD] == [b"zstd"]
    assert responses[0][TOTAL_SIZE_FIELD] == [len(compressed)]

    envelope = zstd_module().decompress(compressed)
    files = unpack_envelope(envelope)
    combined = b"\n".join(files.values())
    assert b"store-yaml-test" in combined
    assert b"store_yaml:" in combined
    # The inline OTA password comes back as a !secret listed in the skeleton.
    assert b"recoverme123" not in envelope
    assert "ota_password" in find_secret_references(combined.decode())
    assert b"ota_password: ''" in files["secrets.yaml"]
