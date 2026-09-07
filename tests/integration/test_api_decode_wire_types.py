"""decode_field() must take fields that match their declared wire type, drop the ones that do
not, skip unknown fields, and handle two byte tags, varints and length prefixes."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import struct

from aioesphomeapi import (
    EntityState,
    LightState,
    NumberState,
    SwitchState,
    TextState,
    api_pb2,
)
import pytest

from .raw_api_client import MESSAGE_TYPE_OF, RawApiClient, encode_varint
from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

SWITCH_COMMAND = MESSAGE_TYPE_OF[api_pb2.SwitchCommandRequest]
WIRE_VARINT, WIRE_LENGTH, WIRE_FIXED32 = 0, 2, 5


def tag(field: int, wire_type: int) -> bytes:
    return encode_varint((field << 3) | wire_type)


def key_field(key: int) -> bytes:
    return tag(1, WIRE_FIXED32) + struct.pack("<I", key)


@pytest.mark.asyncio
async def test_api_decode_wire_types(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port: int,
) -> None:
    loop = asyncio.get_running_loop()
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
        RawApiClient(unused_tcp_port) as raw,
    ):
        entities, _ = await client.list_entities_services()
        switch = require_entity(entities, "wire_switch")
        light = require_entity(entities, "wire_light")
        text = require_entity(entities, "wire_text")
        number = require_entity(entities, "wire_number")

        waiters: list[
            tuple[Callable[[EntityState], bool], asyncio.Future[EntityState]]
        ] = []
        initial = InitialStateHelper(entities)

        def on_state(state: EntityState) -> None:
            for pred, fut in waiters:
                if not fut.done() and pred(state):
                    fut.set_result(state)

        async def expect(pred: Callable[[EntityState], bool]) -> EntityState:
            fut: asyncio.Future[EntityState] = loop.create_future()
            waiters.append((pred, fut))
            try:
                return await asyncio.wait_for(fut, 5.0)
            finally:
                waiters.remove((pred, fut))

        def switch_is(value: bool) -> Callable[[EntityState], bool]:
            return lambda s: (
                isinstance(s, SwitchState) and s.key == switch.key and s.state is value
            )

        client.subscribe_states(initial.on_state_wrapper(on_state))
        await initial.wait_for_initial_states()
        await raw.connect()

        # A well formed command: fixed32 key, varint state
        await raw.send_raw(
            SWITCH_COMMAND, key_field(switch.key) + tag(2, WIRE_VARINT) + b"\x01"
        )
        await expect(switch_is(True))
        await raw.send_raw(
            SWITCH_COMMAND, key_field(switch.key) + tag(2, WIRE_VARINT) + b"\x00"
        )
        await expect(switch_is(False))

        # The same field with the wrong wire type is dropped: a length delimited or fixed32
        # "state" must not turn the switch on, and a varint key never matches an entity
        await raw.send_raw(
            SWITCH_COMMAND, key_field(switch.key) + tag(2, WIRE_LENGTH) + b"\x01\x01"
        )
        await raw.send_raw(
            SWITCH_COMMAND,
            key_field(switch.key) + tag(2, WIRE_FIXED32) + b"\x01\x00\x00\x00",
        )
        await raw.send_raw(
            SWITCH_COMMAND,
            tag(1, WIRE_VARINT)
            + encode_varint(switch.key)
            + tag(2, WIRE_VARINT)
            + b"\x01",
        )
        # An unknown field ahead of the known ones is skipped and the rest still decodes;
        # field 200 needs a two byte tag
        await raw.send_raw(
            SWITCH_COMMAND,
            tag(200, WIRE_VARINT)
            + encode_varint(300)
            + key_field(switch.key)
            + tag(2, WIRE_VARINT)
            + b"\x01",
        )
        state = await expect(switch_is(True))
        assert state.state is True
        await raw.send_raw(
            SWITCH_COMMAND, key_field(switch.key) + tag(2, WIRE_VARINT) + b"\x00"
        )
        await expect(switch_is(False))

        # Two byte tags (effect fields 18 and 19) and a two byte varint (300 ms transition)
        client.light_command(
            light.key, state=True, brightness=0.5, transition_length=0.3, effect="Pulse"
        )
        await expect(
            lambda s: (
                isinstance(s, LightState) and s.key == light.key and s.effect == "Pulse"
            )
        )
        client.light_command(light.key, effect="None", state=False)
        await expect(
            lambda s: isinstance(s, LightState) and s.key == light.key and not s.state
        )

        # A string whose length prefix needs two varint bytes
        long_text = "w" * 200
        client.text_command(text.key, long_text)
        await expect(
            lambda s: (
                isinstance(s, TextState) and s.key == text.key and s.state == long_text
            )
        )

        # A negative fixed32 float
        client.number_command(number.key, -77.5)
        await expect(
            lambda s: (
                isinstance(s, NumberState) and s.key == number.key and s.state == -77.5
            )
        )
