"""Integration test for user-defined action field metadata.

The metadata fields are read from the raw protobuf because the released
aioesphomeapi does not decode them yet; unknown fields survive parsing.
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import api_pb2
import pytest

from esphome.helpers import fnv1_hash

from .types import APIClientConnectedFactory, RunCompiledFunction

# Field numbers from api.proto
SERVICE_NAME = 1
SERVICE_ARGS = 3
SERVICE_DESCRIPTION = 5
ARG_NAME = 1
ARG_DESCRIPTION = 3
ARG_EXAMPLE = 4

WIRE_VARINT = 0
WIRE_FIXED32 = 5
WIRE_LENGTH = 2


def _decode_varint(data: bytes, pos: int) -> tuple[int, int]:
    result = shift = 0
    while True:
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return result, pos
        shift += 7


def _fields(data: bytes) -> dict[int, list[bytes]]:
    """Length delimited fields of a message keyed by field number."""
    fields: dict[int, list[bytes]] = {}
    pos = 0
    while pos < len(data):
        tag, pos = _decode_varint(data, pos)
        number, wire_type = tag >> 3, tag & 0x7
        if wire_type == WIRE_VARINT:
            _, pos = _decode_varint(data, pos)
        elif wire_type == WIRE_FIXED32:
            pos += 4
        elif wire_type == WIRE_LENGTH:
            length, pos = _decode_varint(data, pos)
            fields.setdefault(number, []).append(data[pos : pos + length])
            pos += length
        else:
            raise AssertionError(f"unexpected wire type {wire_type}")
    return fields


def _string(fields: dict[int, list[bytes]], number: int) -> str:
    return fields[number][0].decode() if number in fields else ""


@pytest.mark.asyncio
async def test_api_action_metadata(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Action and argument metadata reach the client and the actions still run."""
    loop = asyncio.get_running_loop()
    buzzer_called = loop.create_future()
    plain_called = loop.create_future()
    buzzer_pattern = re.compile(r"Buzzer: two_short")
    plain_pattern = re.compile(r"Plain action called")

    def check_output(line: str) -> None:
        if not buzzer_called.done() and buzzer_pattern.search(line):
            buzzer_called.set_result(True)
        elif not plain_called.done() and plain_pattern.search(line):
            plain_called.set_result(True)

    raw_services: list[bytes] = []

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        remove = client._connection.add_message_callback(
            lambda msg: raw_services.append(msg.SerializeToString()),
            (api_pb2.ListEntitiesServicesResponse,),
        )
        _, services = await client.list_entities_services()
        remove()

        by_name = {service.name: service for service in services}
        assert set(by_name) == {"play_buzzer", "plain_action"}
        # Keys are hashed at codegen time and must match what the client expects
        for name, service in by_name.items():
            assert service.key == fnv1_hash(name), name

        decoded = {
            _string(fields, SERVICE_NAME): fields
            for fields in map(_fields, raw_services)
        }
        buzzer = decoded["play_buzzer"]
        assert (
            _string(buzzer, SERVICE_DESCRIPTION) == "Play an RTTTL melody on the buzzer"
        )
        args = {
            _string(fields, ARG_NAME): fields
            for fields in map(_fields, buzzer[SERVICE_ARGS])
        }
        assert _string(args["song_str"], ARG_DESCRIPTION) == "RTTTL melody string"
        assert (
            _string(args["song_str"], ARG_EXAMPLE)
            == "two_short:d=4,o=5,b=100:16e6,16e6"
        )
        # An arg without metadata sends nothing for those fields
        assert ARG_DESCRIPTION not in args["volume"]
        assert ARG_EXAMPLE not in args["volume"]

        # An action without metadata sends nothing for those fields
        plain = decoded["plain_action"]
        assert SERVICE_DESCRIPTION not in plain
        assert ARG_DESCRIPTION not in _fields(plain[SERVICE_ARGS][0])

        await client.execute_service(
            by_name["play_buzzer"],
            {"song_str": "two_short:d=4,o=5,b=100:16e6,16e6", "volume": 3},
        )
        await client.execute_service(by_name["plain_action"], {"value": 1})
        await asyncio.wait_for(buzzer_called, timeout=5.0)
        await asyncio.wait_for(plain_called, timeout=5.0)
