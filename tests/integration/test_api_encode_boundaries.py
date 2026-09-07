"""Encode paths at their branch boundaries: zero skipped float, fixed32 state, negative int32,
length prefixes of two varint bytes, two byte field tags and a clean disconnect."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    EntityState,
    NumberState,
    SelectInfo,
    SensorInfo,
    SensorState,
    TextSensorState,
)
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

LONG_OPTION = (
    "option-with-a-name-long-enough-that-its-length-prefix-needs-two-varint-bytes-"
    "when-the-list-entities-response-is-encoded-xxxxxxxxxx"
)


@pytest.mark.asyncio
async def test_api_encode_boundaries(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info.suggested_area == "Kitchen"

        entities, _ = await client.list_entities_services()
        sensor = require_entity(entities, "zero_then_value", SensorInfo)
        assert sensor.accuracy_decimals == -2
        select = require_entity(entities, "long_option_select", SelectInfo)
        assert len(LONG_OPTION) >= 128
        assert select.options == ["short", LONG_OPTION]
        text = require_entity(entities, "long_text")
        number = require_entity(entities, "negative_number")
        button = require_entity(entities, "publish_values")

        sensor_value: asyncio.Future[float] = loop.create_future()
        text_value: asyncio.Future[str] = loop.create_future()
        initial = InitialStateHelper(entities)

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, SensorState)
                and state.key == sensor.key
                and not sensor_value.done()
            ):
                sensor_value.set_result(state.state)
            elif (
                isinstance(state, TextSensorState)
                and state.key == text.key
                and not text_value.done()
            ):
                text_value.set_result(state.state)

        client.subscribe_states(initial.on_state_wrapper(on_state))
        await initial.wait_for_initial_states()

        # A float of exactly zero is skipped on the wire and must still read as 0.0, not missing
        first = initial.initial_states[sensor.key]
        assert isinstance(first, SensorState)
        assert first.state == 0.0 and not first.missing_state
        first_number = initial.initial_states[number.key]
        assert isinstance(first_number, NumberState)
        assert first_number.state == -123.5

        client.button_command(button.key)
        assert await asyncio.wait_for(sensor_value, 5.0) == 12.5
        assert await asyncio.wait_for(text_value, 5.0) == "y" * 200

        # DisconnectRequest and DisconnectResponse carry no fields
        await client.disconnect()
