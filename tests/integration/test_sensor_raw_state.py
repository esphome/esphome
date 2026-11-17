"""Integration tests for Sensor::get_raw_state().

Raw state storage only exists when filters are compiled in (USE_SENSOR_FILTER).
Without it, get_raw_state() returns state, so both build configurations are covered:
one fixture with a filtered sensor and one with no filters at all.
"""

from __future__ import annotations

import re

from aioesphomeapi import APIClient, EntityInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

STATE_PATTERN = re.compile(r"state=([\d.]+) raw_state=([\d.]+)")


async def _press_and_read(
    client: APIClient,
    entities: list[EntityInfo],
    button_object_id: str,
    waiter: LineWaiter,
    label: str,
) -> tuple[float, float]:
    button = require_entity(entities, button_object_id)
    client.button_command(button.key)
    line = await waiter.wait_for(f"{label}: state=", timeout=5.0)
    match = STATE_PATTERN.search(line)
    assert match is not None, f"Unexpected {label} line: {line}"
    return float(match.group(1)), float(match.group(2))


@pytest.mark.asyncio
async def test_sensor_raw_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """With filters compiled in, raw state is stored separately from state."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        state, raw_state = await _press_and_read(
            client, entities, "test_no_filter_button", waiter, "NO_FILTER"
        )
        assert state == 21.5
        assert raw_state == 21.5

        state, raw_state = await _press_and_read(
            client,
            entities,
            "test_with_filter_button",
            waiter,
            "WITH_FILTER",
        )
        assert state == 43.0
        assert raw_state == 21.5


@pytest.mark.asyncio
async def test_sensor_raw_state_no_filter(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Without filters compiled in, get_raw_state() returns state."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        state, raw_state = await _press_and_read(
            client, entities, "test_no_filter_button", waiter, "NO_FILTER"
        )
        assert state == 21.5
        assert raw_state == 21.5
