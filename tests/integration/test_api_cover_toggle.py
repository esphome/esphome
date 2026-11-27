"""The native API advertises cover toggle support and runs the toggle action on request."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import struct

from aioesphomeapi import CoverInfo, CoverState, EntityState, api_pb2
import pytest

from .log_utils import LineWaiter
from .raw_api_client import (
    MESSAGE_TYPE_OF,
    WIRE_FIXED32,
    WIRE_VARINT,
    RawApiClient,
    decode_fields,
    tag,
)
from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

COVER_COMMAND = MESSAGE_TYPE_OF[api_pb2.CoverCommandRequest]
LIST_COVER = MESSAGE_TYPE_OF[api_pb2.ListEntitiesCoverResponse]
LIST_DONE = MESSAGE_TYPE_OF[api_pb2.ListEntitiesDoneResponse]
# ListEntitiesCoverResponse fields; the installed client library does not know supports_toggle yet
FIELD_ENTITY_KEY, FIELD_SUPPORTS_TOGGLE = 2, 15
# CoverCommandRequest fields
FIELD_KEY, FIELD_STOP, FIELD_TOGGLE = 1, 8, 11


@pytest.mark.asyncio
async def test_api_cover_toggle(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port: int,
) -> None:
    lines = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
        RawApiClient(unused_tcp_port) as raw,
    ):
        entities, _ = await client.list_entities_services()
        toggle_cover = require_entity(entities, "toggle_cover", CoverInfo, exact=True)
        plain_cover = require_entity(entities, "plain_cover", CoverInfo, exact=True)

        positions: dict[int, list[float]] = {toggle_cover.key: [], plain_cover.key: []}

        def on_state(state: EntityState) -> None:
            if isinstance(state, CoverState) and state.key in positions:
                positions[state.key].append(state.position)

        def position_is(key: int, value: float) -> Callable[[EntityState], bool]:
            return lambda s: (
                isinstance(s, CoverState)
                and s.key == key
                and s.position == pytest.approx(value, abs=0.01)
            )

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)
        waiter = sub.waiter

        # Capability discovery: only the cover with a toggle_action advertises toggle
        await raw.connect()
        await raw.send_message(api_pb2.ListEntitiesRequest())
        await raw.read_until_frame(LIST_DONE)
        supports_toggle = {}
        for payload in raw.frames(LIST_COVER):
            fields = decode_fields(payload)
            key = fields[FIELD_ENTITY_KEY][0]
            supports_toggle[key] = bool(fields.get(FIELD_SUPPORTS_TOGGLE, [0])[0])
        assert supports_toggle == {toggle_cover.key: True, plain_cover.key: False}

        def key_field(key: int) -> bytes:
            return tag(FIELD_KEY, WIRE_FIXED32) + struct.pack("<I", key)

        toggle = tag(FIELD_TOGGLE, WIRE_VARINT) + b"\x01"
        stop = tag(FIELD_STOP, WIRE_VARINT) + b"\x01"

        # A toggle request runs the toggle action, which publishes 0.5
        toggled = waiter.expect(
            position_is(toggle_cover.key, 0.5), label="toggle action"
        )
        await raw.send_raw(COVER_COMMAND, key_field(toggle_cover.key) + toggle)
        await toggled

        # A cover without toggle_action drops the request with a warning; its state does not change
        await raw.send_raw(COVER_COMMAND, key_field(plain_cover.key) + toggle)
        await lines.wait_for("Plain Cover", "toggle unsupported", timeout=2.0)
        await asyncio.sleep(0.5)
        assert all(p == pytest.approx(1.0) for p in positions[plain_cover.key])

        # Stop and toggle in one request: stop wins and the toggle action does not run
        before = len(positions[toggle_cover.key])
        stopped = waiter.expect(
            position_is(toggle_cover.key, 0.25), label="stop action"
        )
        await raw.send_raw(COVER_COMMAND, key_field(toggle_cover.key) + stop + toggle)
        await stopped
        await asyncio.sleep(0.5)
        after = positions[toggle_cover.key][before:]
        assert all(p == pytest.approx(0.25, abs=0.01) for p in after), after
