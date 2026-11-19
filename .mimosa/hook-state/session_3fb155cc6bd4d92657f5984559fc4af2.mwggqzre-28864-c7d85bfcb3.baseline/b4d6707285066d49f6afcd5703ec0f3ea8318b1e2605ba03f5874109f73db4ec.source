"""Integration test for the light control apply actions.

Tests that light.turn_on, light.turn_off, and light.control automation actions
work correctly with the compact per-field union storage. Exercises both constant
value and lambda paths.
"""

from functools import partial

from aioesphomeapi import LightState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.turn_on, light.turn_off and light.control with constants and lambdas."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        light = require_entity(entities, "test_light", exact=True)
        sub = await subscribe_after_initial_states(client, entities)
        press = partial(
            press_and_wait,
            client,
            entities,
            sub.waiter,
            state_type=LightState,
            key=light.key,
        )

        # Test 1: light.turn_on with RGB constants
        state = await press("turn_on_rgb")
        assert state.state is True
        assert state.brightness == pytest.approx(1.0)
        assert state.red == pytest.approx(0.0, abs=0.01)
        assert state.green == pytest.approx(0.0, abs=0.01)
        assert state.blue == pytest.approx(1.0, abs=0.01)

        # Test 2: light.turn_off
        state = await press("turn_off")
        assert state.state is False

        # Test 3: light.turn_on with color_temperature
        state = await press("turn_on_ct")
        assert state.state is True
        assert state.brightness == pytest.approx(0.8)
        assert state.color_temperature == pytest.approx(250.0)  # 4000K = 250 mireds

        # Test 4: light.turn_on with effect
        state = await press("turn_on_effect")
        assert state.effect == "Test Effect"

        # Test 5: Clear effect
        state = await press("clear_effect")
        assert state.effect == "None"

        # Test 6: light.control with cold/warm white
        state = await press("control_cw")
        assert state.cold_white == pytest.approx(0.9, abs=0.1)
        assert state.warm_white == pytest.approx(0.1, abs=0.1)

        # Test 7: light.turn_on with lambda brightness
        # The global test_brightness is 0.75
        state = await press("lambda_brightness")
        assert state.state is True
        assert state.brightness == pytest.approx(0.75, abs=0.05)
        assert state.red == pytest.approx(1.0, abs=0.01)
        assert state.green == pytest.approx(0.0, abs=0.01)
        assert state.blue == pytest.approx(0.0, abs=0.01)

        # Test 8: light.turn_on with transition_length and brightness
        state = await press("turn_on_transition")
        assert state.state is True
        assert state.brightness == pytest.approx(0.5)
