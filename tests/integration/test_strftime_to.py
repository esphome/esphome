"""Integration test for ESPTime::strftime_to() method."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, TextSensorState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_strftime_to(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test ESPTime::strftime_to() formats time correctly."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "strftime-to-test"

        # Get entities
        entities, _ = await client.list_entities_services()

        # Find our text sensors
        format_test = require_entity(
            entities, "time_format_test", description="Time Format Test sensor"
        )
        short_format = require_entity(
            entities, "time_short_format", description="Time Short Format sensor"
        )
        string_format = require_entity(
            entities, "time_string_format", description="Time String Format sensor"
        )
        error_format = require_entity(
            entities, "time_error_format", description="Time Error Format sensor"
        )

        # Set up state tracking with InitialStateHelper
        loop = asyncio.get_running_loop()
        states: dict[int, TextSensorState] = {}
        all_received = loop.create_future()
        expected_keys = {
            format_test.key,
            short_format.key,
            string_format.key,
            error_format.key,
        }
        initial_state_helper = InitialStateHelper(entities)

        def on_state(state: EntityState) -> None:
            if isinstance(state, TextSensorState) and not state.missing_state:
                states[state.key] = state
                if expected_keys <= states.keys() and not all_received.done():
                    all_received.set_result(True)

        # Subscribe with the wrapper that filters initial states
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states to be broadcast
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Wait for all expected states
        try:
            await asyncio.wait_for(all_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for text sensor states. Got: {list(states.keys())}"
            )

        # Validate strftime_to with full format
        # Note: The exact output depends on timezone, but should contain date components
        format_test_state = states[format_test.key].state
        assert "2024" in format_test_state or "2023" in format_test_state, (
            f"Expected year in format test output, got: {format_test_state}"
        )
        # Should have format like "YYYY-MM-DD HH:MM:SS"
        assert len(format_test_state) == 19, (
            f"Expected 19 chars for datetime format, got {len(format_test_state)}: {format_test_state}"
        )

        # Validate short format (HH:MM)
        short_format_state = states[short_format.key].state
        assert len(short_format_state) == 5, (
            f"Expected 5 chars for HH:MM format, got {len(short_format_state)}: {short_format_state}"
        )
        assert ":" in short_format_state, (
            f"Expected colon in HH:MM format, got: {short_format_state}"
        )

        # Validate string format (the std::string returning version)
        string_format_state = states[string_format.key].state
        assert len(string_format_state) == 10, (
            f"Expected 10 chars for YYYY-MM-DD format, got {len(string_format_state)}: {string_format_state}"
        )
        assert string_format_state.count("-") == 2, (
            f"Expected two dashes in YYYY-MM-DD format, got: {string_format_state}"
        )

        # Validate error format returns "ERROR"
        error_format_state = states[error_format.key].state
        assert error_format_state == "ERROR", (
            f"Expected 'ERROR' for empty format string, got: {error_format_state}"
        )
