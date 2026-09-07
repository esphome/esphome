"""Messages without fields go through the shared ProtoMessage entry points on both directions."""

from __future__ import annotations

from aioesphomeapi import api_pb2
import pytest

from .raw_api_client import MESSAGE_TYPE_OF, RawApiClient
from .types import RunCompiledFunction


@pytest.mark.asyncio
async def test_api_empty_message_roundtrip(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    unused_tcp_port: int,
) -> None:
    async with run_compiled(yaml_config), RawApiClient(unused_tcp_port) as client:
        await client.connect()

        # Field free request and reply on the plain send path
        await client.send_message(api_pb2.PingRequest())
        await client.read_until_frame(MESSAGE_TYPE_OF[api_pb2.PingResponse])

        # Field free request answered by a message with fields, and a list that ends with
        # the field free ListEntitiesDoneResponse through the batching path
        await client.send_message(api_pb2.DeviceInfoRequest())
        await client.read_until_frame(MESSAGE_TYPE_OF[api_pb2.DeviceInfoResponse])
        await client.send_message(api_pb2.ListEntitiesRequest())
        await client.read_until_frame(
            MESSAGE_TYPE_OF[api_pb2.ListEntitiesSwitchResponse]
        )
        await client.read_until_frame(MESSAGE_TYPE_OF[api_pb2.ListEntitiesDoneResponse])

        await client.send_message(api_pb2.DisconnectRequest())
        await client.read_until_frame(MESSAGE_TYPE_OF[api_pb2.DisconnectResponse])
