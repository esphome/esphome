"""decode_field() must take fields that match their declared wire type, drop the ones that do
not, skip unknown fields, and handle two byte tags, varints and length prefixes."""

from __future__ import annotations

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
from .state_utils import InitialStateHelper, StateWaiter, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

SWITCH_COMMAND = MESSAGE_TYPE_OF[api_pb2.SwitchCommandRequest]
WIRE_VARINT, WIRE_LENGTH, WIRE_FIXED32 = 0, 2, 5


def tag(field: int, wire_type: int) -> bytes:
    return encode_varint((field << 3) | wire_type)


@pytest.mark.asyncio
async def test_api_decode_wire_types(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port: int,
) -> None:
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
        key = tag(1, WIRE_FIXED32) + struct.pack("<I", switch.key)
        on, off = tag(2, WIRE_VARINT) + b"\x01", tag(2, WIRE_VARINT) + b"\x00"

        switch_states: list[bool] = []
        waiter = StateWaiter()

        def on_state(state: EntityState) -> None:
            if isinstance(state, SwitchState) and state.key == switch.key:
                switch_states.append(state.state)
            waiter.on_state(state)

        def switch_is(value: bool) -> Callable[[EntityState], bool]:
            return lambda s: (
                isinstance(s, SwitchState) and s.key == switch.key and s.state is value
            )

        def number_is(value: float) -> Callable[[EntityState], bool]:
            return lambda s: (
                isinstance(s, NumberState) and s.key == number.key and s.state == value
            )

        initial = InitialStateHelper(entities)
        client.subscribe_states(initial.on_state_wrapper(on_state))
        await initial.wait_for_initial_states()
        await raw.connect()

        # A well formed command: fixed32 key, varint state
        await raw.send_raw(SWITCH_COMMAND, key + on)
        await waiter.expect(switch_is(True))
        await raw.send_raw(SWITCH_COMMAND, key + off)
        await waiter.expect(switch_is(False))

        # The same field with the wrong wire type is dropped, and a varint key never matches an
        # entity; each of these would turn the switch on if the payload were read as a varint
        seen = len(switch_states)
        await raw.send_raw(SWITCH_COMMAND, key + tag(2, WIRE_LENGTH) + b"\x01\x01")
        await raw.send_raw(
            SWITCH_COMMAND, key + tag(2, WIRE_FIXED32) + b"\x01\x00\x00\x00"
        )
        await raw.send_raw(
            SWITCH_COMMAND, tag(1, WIRE_VARINT) + encode_varint(switch.key) + on
        )
        # A later command on the same connection proves the bad frames were fully consumed;
        # the number state arriving means any switch state from them would already be here
        client.number_command(number.key, -77.5)
        await waiter.expect(number_is(-77.5))
        assert len(switch_states) == seen

        # Truncated bodies stop the decode loop without taking the connection down: a tag with its
        # continuation bit set and nothing after it, a length prefix past the end of the payload,
        # and a fixed32 with two of its four bytes
        await raw.send_raw(SWITCH_COMMAND, key + b"\x80")
        await raw.send_raw(SWITCH_COMMAND, key + tag(2, WIRE_LENGTH) + b"\x7f" + b"ab")
        await raw.send_raw(SWITCH_COMMAND, tag(1, WIRE_FIXED32) + b"\x01\x02")
        await raw.send_raw(SWITCH_COMMAND, key + on)
        await waiter.expect(switch_is(True), label="switch on after truncated frames")
        await raw.send_raw(SWITCH_COMMAND, key + off)
        await waiter.expect(switch_is(False))

        # An unknown field ahead of the known ones is skipped; field 200 needs a two byte tag
        await raw.send_raw(
            SWITCH_COMMAND, tag(200, WIRE_VARINT) + encode_varint(300) + key + on
        )
        await waiter.expect(switch_is(True))

        # Two byte tags (effect fields 18 and 19) and a two byte varint (300 ms transition)
        client.light_command(
            light.key, state=True, brightness=0.5, transition_length=0.3, effect="Pulse"
        )
        await waiter.expect(
            lambda s: (
                isinstance(s, LightState) and s.key == light.key and s.effect == "Pulse"
            )
        )
        client.light_command(light.key, effect="None", state=False)
        await waiter.expect(
            lambda s: isinstance(s, LightState) and s.key == light.key and not s.state
        )

        # A string whose length prefix needs two varint bytes
        long_text = "w" * 200
        client.text_command(text.key, long_text)
        await waiter.expect(
            lambda s: (
                isinstance(s, TextState) and s.key == text.key and s.state == long_text
            )
        )
