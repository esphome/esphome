"""Integration test for LightControlAction.

Tests that light.turn_on, light.turn_off, and light.control automation actions
work correctly with the compact per-field union storage. Exercises both constant
value and lambda paths.
"""

import asyncio
from typing import Any

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LightControlAction with constants and lambdas."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        client.subscribe_states(on_state)

        # Get entities
        entities = await client.list_entities_services()
        light = next(e for e in entities[0] if e.object_id == "test_light")
        buttons = {e.name: e for e in entities[0] if hasattr(e, "name")}

        async def wait_for_state(key: int, timeout: float = 5.0) -> Any:
            """Wait for a state change for the given entity key."""
            loop = asyncio.get_running_loop()
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        async def press_and_wait(button_name: str) -> Any:
            """Press a button and wait for light state change."""
            btn = buttons[button_name]
            client.button_command(btn.key)
            return await wait_for_state(light.key)

        # Test 1: light.turn_on with RGB constants
        state = await press_and_wait("Turn On RGB")
        assert state.state is True
        assert state.brightness == pytest.approx(1.0)
        assert state.red == pytest.approx(0.0, abs=0.01)
        assert state.green == pytest.approx(0.0, abs=0.01)
        assert state.blue == pytest.approx(1.0, abs=0.01)

        # Test 2: light.turn_off
        state = await press_and_wait("Turn Off")
        assert state.state is False

        # Test 3: light.turn_on with color_temperature
        state = await press_and_wait("Turn On CT")
        assert state.state is True
        assert state.brightness == pytest.approx(0.8)
        assert state.color_temperature == pytest.approx(250.0)  # 4000K = 250 mireds

        # Test 4: light.turn_on with effect
        state = await press_and_wait("Turn On Effect")
        assert state.effect == "Test Effect"

        # Test 5: Clear effect
        state = await press_and_wait("Clear Effect")
        assert state.effect == "None"

        # Test 6: light.control with cold/warm white
        state = await press_and_wait("Control CW")
        assert state.cold_white == pytest.approx(0.9, abs=0.1)
        assert state.warm_white == pytest.approx(0.1, abs=0.1)

        # Test 7: light.turn_on with lambda brightness
        # The global test_brightness is 0.75
        state = await press_and_wait("Lambda Brightness")
        assert state.state is True
        assert state.brightness == pytest.approx(0.75, abs=0.05)
        assert state.red == pytest.approx(1.0, abs=0.01)
        assert state.green == pytest.approx(0.0, abs=0.01)
        assert state.blue == pytest.approx(0.0, abs=0.01)

        # Test 8: light.turn_on with transition_length and brightness
        state = await press_and_wait("Turn On Transition")
        assert state.state is True
        assert state.brightness == pytest.approx(0.5)
