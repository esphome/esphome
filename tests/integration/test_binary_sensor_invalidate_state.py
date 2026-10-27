"""Integration test for binary_sensor.invalidate_state() functionality.

This tests the fix in PR #12296 where invalidate_state() was not properly
reporting the 'unknown' state to the API. The binary sensor should report
missing_state=True when invalidated.

Regression test for: https://github.com/esphome/esphome/issues/12252
"""

from __future__ import annotations

from aioesphomeapi import BinarySensorInfo, BinarySensorState
import pytest

from .state_utils import (
    assert_device_name,
    require_entity,
    subscribe_after_initial_states,
)
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
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-binary-sensor-invalidate")

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

        # Wait for initial states
        sub = await subscribe_after_initial_states(client, entities)

        # Check initial state - should be missing (unknown)
        initial_state = sub.initial_states.get(binary_sensor.key)
        assert initial_state is not None, "No initial state received for binary sensor"
        assert isinstance(initial_state, BinarySensorState)
        assert initial_state.missing_state is True, (
            f"Initial state should have missing_state=True, got {initial_state}"
        )

        # Test 1: Set state to true
        next_state = sub.waiter.next_state(binary_sensor.key, BinarySensorState)
        client.button_command(set_true_button.key)
        state = await next_state

        assert state.missing_state is False, (
            f"After setting true, missing_state should be False, got {state}"
        )
        assert state.state is True, f"Expected state=True, got {state}"

        # Test 2: Set state to false
        next_state = sub.waiter.next_state(binary_sensor.key, BinarySensorState)
        client.button_command(set_false_button.key)
        state = await next_state

        assert state.missing_state is False, (
            f"After setting false, missing_state should be False, got {state}"
        )
        assert state.state is False, f"Expected state=False, got {state}"

        # Test 3: Invalidate state (set to unknown)
        # This is the critical test for the bug fix
        # A timeout here means invalidate_state() is not reported to the API (issue #12252)
        next_state = sub.waiter.next_state(binary_sensor.key, BinarySensorState)
        client.button_command(invalidate_button.key)
        state = await next_state

        assert state.missing_state is True, (
            f"After invalidate_state(), missing_state should be True (unknown), "
            f"got {state}. This is the regression from issue #12252."
        )
