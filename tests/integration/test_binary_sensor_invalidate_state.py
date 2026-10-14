"""Integration test for binary_sensor.invalidate_state() functionality.

This tests the fix in PR #12296 where invalidate_state() was not properly
reporting the 'unknown' state to the API. The binary sensor should report
missing_state=True when invalidated.

Regression test for: https://github.com/esphome/esphome/issues/12252
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorInfo, BinarySensorState, EntityState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_binary_sensor_invalidate_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that binary_sensor.invalidate_state() reports unknown to the API.

    This verifies that:
    1. Binary sensor starts with missing_state=True (no initial state)
    2. Publishing true sets missing_state=False and state=True
    3. Publishing false sets missing_state=False and state=False
    4. Invalidating state sets missing_state=True (unknown state)
    """
    loop = asyncio.get_running_loop()

    # Track state changes
    states_received: list[BinarySensorState] = []
    state_future: asyncio.Future[BinarySensorState] = loop.create_future()

    def on_state(state: EntityState) -> None:
        """Track binary sensor state changes."""
        if isinstance(state, BinarySensorState):
            states_received.append(state)
            if not state_future.done():
                state_future.set_result(state)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "test-binary-sensor-invalidate"

        # Get entities
        entities, _ = await client.list_entities_services()

        # Find our binary sensor and buttons using helper
        binary_sensor = require_entity(entities, "test_binary_sensor", BinarySensorInfo)
        set_true_button = require_entity(
            entities, "set_true", description="Set True button"
        )
        set_false_button = require_entity(
            entities, "set_false", description="Set False button"
        )
        invalidate_button = require_entity(
            entities, "invalidate", description="Invalidate button"
        )

        # Set up initial state helper to handle the initial state broadcast
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Check initial state - should be missing (unknown)
        initial_state = initial_state_helper.initial_states.get(binary_sensor.key)
        assert initial_state is not None, "No initial state received for binary sensor"
        assert isinstance(initial_state, BinarySensorState)
        assert initial_state.missing_state is True, (
            f"Initial state should have missing_state=True, got {initial_state}"
        )

        # Test 1: Set state to true
        states_received.clear()
        state_future = loop.create_future()
        client.button_command(set_true_button.key)

        try:
            state = await asyncio.wait_for(state_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for state=true")

        assert state.missing_state is False, (
            f"After setting true, missing_state should be False, got {state}"
        )
        assert state.state is True, f"Expected state=True, got {state}"

        # Test 2: Set state to false
        states_received.clear()
        state_future = loop.create_future()
        client.button_command(set_false_button.key)

        try:
            state = await asyncio.wait_for(state_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for state=false")

        assert state.missing_state is False, (
            f"After setting false, missing_state should be False, got {state}"
        )
        assert state.state is False, f"Expected state=False, got {state}"

        # Test 3: Invalidate state (set to unknown)
        # This is the critical test for the bug fix
        states_received.clear()
        state_future = loop.create_future()
        client.button_command(invalidate_button.key)

        try:
            state = await asyncio.wait_for(state_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for invalidated state - "
                "binary_sensor.invalidate_state() may not be reporting to the API. "
                "See issue #12252."
            )

        assert state.missing_state is True, (
            f"After invalidate_state(), missing_state should be True (unknown), "
            f"got {state}. This is the regression from issue #12252."
        )
