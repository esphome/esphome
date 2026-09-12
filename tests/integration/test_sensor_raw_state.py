"""Integration tests for Sensor::get_raw_state().

Raw state storage only exists when filters are compiled in (USE_SENSOR_FILTER).
Without it, get_raw_state() returns state, so both build configurations are covered:
one fixture with a filtered sensor and one with no filters at all.
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import APIClient, EntityInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

NO_FILTER_PATTERN = re.compile(r"NO_FILTER: state=([\d.]+) raw_state=([\d.]+)")
WITH_FILTER_PATTERN = re.compile(r"WITH_FILTER: state=([\d.]+) raw_state=([\d.]+)")


async def _press_and_read(
    client: APIClient,
    entities: list[EntityInfo],
    button_object_id: str,
    future: asyncio.Future[tuple[float, float]],
    label: str,
) -> tuple[float, float]:
    button = next(
        (e for e in entities if button_object_id in e.object_id.lower()), None
    )
    assert button is not None, f"{button_object_id} not found"
    client.button_command(button.key)
    try:
        return await asyncio.wait_for(future, timeout=5.0)
    except TimeoutError:
        pytest.fail(f"Timeout waiting for {label} log message")


@pytest.mark.asyncio
async def test_sensor_raw_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """With filters compiled in, raw state is stored separately from state."""
    loop = asyncio.get_running_loop()
    no_filter_future: asyncio.Future[tuple[float, float]] = loop.create_future()
    with_filter_future: asyncio.Future[tuple[float, float]] = loop.create_future()

    def check_output(line: str) -> None:
        if not no_filter_future.done() and (match := NO_FILTER_PATTERN.search(line)):
            no_filter_future.set_result((float(match.group(1)), float(match.group(2))))
        if not with_filter_future.done() and (
            match := WITH_FILTER_PATTERN.search(line)
        ):
            with_filter_future.set_result(
                (float(match.group(1)), float(match.group(2)))
            )

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        state, raw_state = await _press_and_read(
            client, entities, "test_no_filter_button", no_filter_future, "NO_FILTER"
        )
        assert state == 21.5
        assert raw_state == 21.5

        state, raw_state = await _press_and_read(
            client,
            entities,
            "test_with_filter_button",
            with_filter_future,
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
    loop = asyncio.get_running_loop()
    no_filter_future: asyncio.Future[tuple[float, float]] = loop.create_future()

    def check_output(line: str) -> None:
        if not no_filter_future.done() and (match := NO_FILTER_PATTERN.search(line)):
            no_filter_future.set_result((float(match.group(1)), float(match.group(2))))

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        state, raw_state = await _press_and_read(
            client, entities, "test_no_filter_button", no_filter_future, "NO_FILTER"
        )
        assert state == 21.5
        assert raw_state == 21.5
