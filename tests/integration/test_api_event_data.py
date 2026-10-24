"""The native API sends the variables of an event once and the values with each event."""

from __future__ import annotations

import asyncio
import struct

from aioesphomeapi import ButtonInfo, EventInfo, api_pb2
import pytest

from .log_utils import LineWaiter
from .raw_api_client import MESSAGE_TYPE_OF, RawApiClient, decode_fields
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

LIST_EVENT = MESSAGE_TYPE_OF[api_pb2.ListEntitiesEventResponse]
LIST_DONE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesDoneResponse]
EVENT = MESSAGE_TYPE_OF[api_pb2.EventResponse]

# The installed client library does not know the new fields yet, so they are read by number
INFO_KEY, INFO_VARIABLES = 2, 11
EVENT_KEY, EVENT_TYPE, EVENT_VALUES = 1, 2, 4
# EventVariableType, and the field of EventVariableValue that holds each type
BOOL, INT, FLOAT, STRING = 0, 1, 2, 3
VALUE_FIELD = {BOOL: 2, INT: 3, FLOAT: 4, STRING: 5}

DIAL_VARIABLES = [("count", INT)]
# The Turn Dial button fires the dial this often. At about 20 bytes per event that is 200 KB;
# the pinned socket blocks after about 5 KB on Linux and 65 KB on macOS
DIAL_EVENTS = 10000
RECV_BUFFER_SIZE = 4096
# Logged by the sndbuf_pin_component fixture when it pins a socket
SNDBUF_PIN_LOG = "SO_SNDBUF pinned to"
# Logged when queued events lose their values
STALE_LOG = "queued events lose their values to newer ones"
# Time for the device to fire the events while the client does not read
STALL_SECONDS = 0.5


def variables_of(payload: bytes) -> list[tuple[str, int]]:
    """Name and type of each variable in a ListEntitiesEventResponse."""
    variables = []
    for raw in decode_fields(payload).get(INFO_VARIABLES, []):
        fields = decode_fields(raw)
        # Type bool is 0, which protobuf does not send
        variables.append((fields[1][0].decode(), fields.get(2, [BOOL])[0]))
    return variables


def values_of(
    payload: bytes, variables: list[tuple[str, int]]
) -> dict[str, bool | int | float | str]:
    """The values of an EventResponse by variable name, as Home Assistant shows them."""
    values = {}
    for raw in decode_fields(payload).get(EVENT_VALUES, []):
        fields = decode_fields(raw)
        # Index 0 is not sent either
        name, type_ = variables[fields.get(1, [0])[0]]
        # A value equal to the protobuf default is sent as its index only
        value = fields.get(VALUE_FIELD[type_], [b"" if type_ == STRING else 0])[0]
        if type_ == BOOL:
            value = bool(value)
        elif type_ == INT:
            value = (value >> 1) ^ -(value & 1)  # sint32: -3 is sent as 5
        elif type_ == FLOAT:
            value = struct.unpack("<f", struct.pack("<I", value))[0]
        else:
            value = value.decode()
        values[name] = value
    return values


@pytest.mark.shared_yaml("api_event_data")
@pytest.mark.asyncio
async def test_api_event_data(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port: int,
) -> None:
    """Each event carries the values it was triggered with, and only those."""
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
        RawApiClient(unused_tcp_port) as raw,
    ):
        entities, _ = await client.list_entities_services()
        dimmer = require_entity(entities, "dimmer", EventInfo, exact=True)
        doorbell = require_entity(entities, "doorbell", EventInfo, exact=True)
        buttons = [
            require_entity(entities, name, ButtonInfo, exact=True)
            for name in ("press_twice", "turn_left", "ring", "press_zero")
        ]

        await raw.connect()
        await raw.send_message(api_pb2.ListEntitiesRequest())
        await raw.read_until_frame(LIST_DONE)
        variables = {
            decode_fields(payload)[INFO_KEY][0]: variables_of(payload)
            for payload in raw.frames(LIST_EVENT)
        }
        assert variables[dimmer.key] == [
            ("number_of_presses", INT),
            ("steps", INT),
            ("fast", BOOL),
            ("speed", FLOAT),
            ("action", STRING),
        ]
        assert variables[doorbell.key] == []

        await raw.send_message(api_pb2.SubscribeStatesRequest())
        for count, button in enumerate(buttons, start=1):
            # Same connection as the subscription, so the press cannot overtake it
            await raw.send_message(api_pb2.ButtonCommandRequest(key=button.key))
            await raw.read_until_frame(EVENT, count=count)

        received = []
        for payload in raw.frames(EVENT):
            fields = decode_fields(payload)
            event_variables = variables[fields[EVENT_KEY][0]]
            received.append(
                (fields[EVENT_TYPE][0].decode(), values_of(payload, event_variables))
            )
        assert received == [
            ("press", {"number_of_presses": 2}),
            (
                "rotate_left",
                {"steps": -3, "fast": True, "speed": 1.5, "action": "start"},
            ),
            ("ring", {}),
            ("press", {"number_of_presses": 0, "fast": False}),
        ]


@pytest.mark.shared_yaml("api_event_data")
@pytest.mark.asyncio
async def test_api_event_data_backlog(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port: int,
) -> None:
    """An event that waits in the queue while the same event fires again goes out without values.

    The device keeps the values of the last event only. The client stops reading, the dial
    fires 10000 times, and the events the socket cannot take wait in the queue. Only the newest
    of them still owns its values, so the older ones arrive with their type only.
    """
    waiter = LineWaiter()
    async with run_compiled(yaml_config, line_callback=waiter.callback):
        # Fails loudly if the pin never applied
        await waiter.wait_for(SNDBUF_PIN_LOG, timeout=10)
        async with api_client_connected() as client:
            entities, _ = await client.list_entities_services()
        turn_dial = require_entity(entities, "turn_dial", ButtonInfo, exact=True)

        async with RawApiClient(
            unused_tcp_port, recv_buffer_size=RECV_BUFFER_SIZE
        ) as raw:
            await raw.connect()
            await raw.send_message(api_pb2.SubscribeStatesRequest())
            await raw.send_message(api_pb2.ButtonCommandRequest(key=turn_dial.key))
            await asyncio.sleep(STALL_SECONDS)
            await raw.read_until_frame(EVENT, count=DIAL_EVENTS, timeout=30)
            counts = [
                values_of(payload, DIAL_VARIABLES).get("count")
                for payload in raw.frames(EVENT)
            ]

    assert len(counts) == DIAL_EVENTS
    wrong = [
        (number, count)
        for number, count in enumerate(counts, start=1)
        if count not in (None, number)
    ]
    assert not wrong, f"events with the count of another event: {wrong[:3]}"
    assert None in counts, "no event waited in the queue"
    # Sent while the socket took them: each with its count.
    # Waited in the queue: without values, except the newest.
    sent = counts.index(None)
    queued = DIAL_EVENTS - sent
    assert counts == [*range(1, sent + 1), *[None] * (queued - 1), DIAL_EVENTS]
    # One warning for the whole backlog, not one for each of the thousands of events
    assert sum(STALE_LOG in line for line in waiter.lines) == 1
