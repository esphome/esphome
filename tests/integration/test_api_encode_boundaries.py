"""Encode paths at their branch boundaries: zero skipped float, fixed32 state, negative int32,
length prefixes of two varint bytes and two byte field tags."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    NumberState,
    SelectInfo,
    SensorInfo,
    SensorState,
    TextSensorState,
)
import pytest

from .state_utils import InitialStateHelper, StateWaiter, require_entity
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
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info, (entities, _) = await asyncio.gather(
            client.device_info(), client.list_entities_services()
        )
        assert device_info.suggested_area == "Kitchen"

        sensor = require_entity(entities, "zero_then_value", SensorInfo)
        assert sensor.accuracy_decimals == -2
        select = require_entity(entities, "long_option_select", SelectInfo)
        assert len(LONG_OPTION) >= 128
        assert select.options == ["short", LONG_OPTION]
        text = require_entity(entities, "long_text")
        number = require_entity(entities, "negative_number")
        button = require_entity(entities, "publish_values")

        initial = InitialStateHelper(entities)
        waiter = StateWaiter()
        client.subscribe_states(initial.on_state_wrapper(waiter.on_state))
        await initial.wait_for_initial_states()

        # A float of exactly zero is skipped on the wire and must still read as 0.0, not missing
        first = initial.initial_states[sensor.key]
        assert isinstance(first, SensorState)
        assert first.state == 0.0 and not first.missing_state
        first_number = initial.initial_states[number.key]
        assert isinstance(first_number, NumberState)
        assert first_number.state == -123.5

        client.button_command(button.key)
        await asyncio.gather(
            waiter.expect(
                lambda s: (
                    isinstance(s, SensorState)
                    and s.key == sensor.key
                    and s.state == 12.5
                )
            ),
            waiter.expect(
                lambda s: (
                    isinstance(s, TextSensorState)
                    and s.key == text.key
                    and s.state == "y" * 200
                )
            ),
        )
