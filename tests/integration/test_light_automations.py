"""Integration test for light automation triggers.

Tests that on_turn_on, on_turn_off, and on_state triggers work correctly
with the listener interface pattern.
"""

import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_automations(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light on_turn_on, on_turn_off, and on_state triggers."""
    waiter = LineWaiter()
    on_turn_on_future = waiter.expect("TRIGGER: on_turn_on fired")
    on_turn_off_future = waiter.expect("TRIGGER: on_turn_off fired")
    on_state_text = "TRIGGER: on_state fired"

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Get entities
        entities = await client.list_entities_services()
        light = require_entity(entities[0], "test_light", exact=True)

        # Count on_state events from now on
        on_state_before = waiter.count(on_state_text)

        # Test 1: Turn light on - should trigger on_turn_on and on_state
        on_state_future_1 = waiter.expect(on_state_text)
        client.light_command(key=light.key, state=True)

        # Wait for on_turn_on trigger
        await waiter.wait(on_turn_on_future, 5.0, "on_turn_on trigger did not fire")

        # Wait for on_state trigger
        await waiter.wait(
            on_state_future_1, 5.0, "on_state trigger did not fire after turn on"
        )

        # Test 2: Turn light off - should trigger on_turn_off and on_state
        on_state_future_2 = waiter.expect(on_state_text)
        client.light_command(key=light.key, state=False)

        # Wait for on_turn_off trigger
        await waiter.wait(on_turn_off_future, 5.0, "on_turn_off trigger did not fire")

        # Wait for on_state trigger
        await waiter.wait(
            on_state_future_2, 5.0, "on_state trigger did not fire after turn off"
        )

        # Verify on_state fired exactly twice (once for on, once for off)
        on_state_count = waiter.count(on_state_text) - on_state_before
        assert on_state_count == 2, (
            f"on_state should have triggered exactly twice, got {on_state_count}"
        )
