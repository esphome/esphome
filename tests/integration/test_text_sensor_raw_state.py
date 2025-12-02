"""Integration test for TextSensor get_raw_state() functionality.

This tests the optimization in PR #12205 where raw_state is only stored
when filters are configured. When no filters exist, get_raw_state() should
return state directly.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_text_sensor_raw_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that get_raw_state() works correctly with and without filters.

    Without filters: get_raw_state() should return the same value as state
    With filters: get_raw_state() should return the original (unfiltered) value
    """
    loop = asyncio.get_running_loop()

    # Futures to track log messages
    no_filter_future: asyncio.Future[tuple[str, str]] = loop.create_future()
    with_filter_future: asyncio.Future[tuple[str, str]] = loop.create_future()

    # Patterns to match log output
    # NO_FILTER: state='hello world' raw_state='hello world'
    no_filter_pattern = re.compile(r"NO_FILTER: state='([^']*)' raw_state='([^']*)'")
    # WITH_FILTER: state='HELLO WORLD' raw_state='hello world'
    with_filter_pattern = re.compile(
        r"WITH_FILTER: state='([^']*)' raw_state='([^']*)'"
    )

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not no_filter_future.done():
            match = no_filter_pattern.search(line)
            if match:
                no_filter_future.set_result((match.group(1), match.group(2)))

        if not with_filter_future.done():
            match = with_filter_pattern.search(line)
            if match:
                with_filter_future.set_result((match.group(1), match.group(2)))

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "test-text-sensor-raw-state"

        # Get entities to find our buttons
        entities, _ = await client.list_entities_services()

        # Find the test buttons
        no_filter_button = next(
            (e for e in entities if "test_no_filter_button" in e.object_id.lower()),
            None,
        )
        assert no_filter_button is not None, "Test No Filter Button not found"

        with_filter_button = next(
            (e for e in entities if "test_with_filter_button" in e.object_id.lower()),
            None,
        )
        assert with_filter_button is not None, "Test With Filter Button not found"

        # Test 1: Text sensor without filters
        # get_raw_state() should return the same as state
        client.button_command(no_filter_button.key)

        try:
            state, raw_state = await asyncio.wait_for(no_filter_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for NO_FILTER log message")

        assert state == "hello world", f"Expected state='hello world', got '{state}'"
        assert raw_state == "hello world", (
            f"Expected raw_state='hello world', got '{raw_state}'"
        )
        assert state == raw_state, (
            f"Without filters, state and raw_state should be equal. "
            f"state='{state}', raw_state='{raw_state}'"
        )

        # Test 2: Text sensor with to_upper filter
        # state should be filtered (uppercase), raw_state should be original
        client.button_command(with_filter_button.key)

        try:
            state, raw_state = await asyncio.wait_for(with_filter_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for WITH_FILTER log message")

        assert state == "HELLO WORLD", f"Expected state='HELLO WORLD', got '{state}'"
        assert raw_state == "hello world", (
            f"Expected raw_state='hello world', got '{raw_state}'"
        )
        assert state != raw_state, (
            f"With filters, state and raw_state should differ. "
            f"state='{state}', raw_state='{raw_state}'"
        )
