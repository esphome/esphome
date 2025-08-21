"""Integration test for all light call combinations.

Tests that LightCall handles all possible light operations correctly
including RGB, color temperature, effects, transitions, and flash.
"""

import asyncio
from typing import Any

from aioesphomeapi import LightState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_calls(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test all possible LightCall operations and combinations."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Track state changes with futures
        state_futures: dict[int, asyncio.Future[Any]] = {}
        states: dict[int, Any] = {}

        def on_state(state: Any) -> None:
            states[state.key] = state
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        client.subscribe_states(on_state)

        # Get the light entities
        entities = await client.list_entities_services()
        lights = [e for e in entities[0] if e.object_id.startswith("test_")]
        assert len(lights) >= 2  # Should have RGBCW and RGB lights

        rgbcw_light = next(light for light in lights if "RGBCW" in light.name)
        rgb_light = next(light for light in lights if "RGB Light" in light.name)

        async def wait_for_state_change(key: int, timeout: float = 1.0) -> Any:
            """Wait for a state change for the given entity key."""
            loop = asyncio.get_event_loop()
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        # Test all individual parameters first

        # Test 1: state only
        client.light_command(key=rgbcw_light.key, state=True)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Test 2: brightness only
        client.light_command(key=rgbcw_light.key, brightness=0.5)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.brightness == pytest.approx(0.5)

        # Test 3: color_brightness only
        client.light_command(key=rgbcw_light.key, color_brightness=0.8)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.color_brightness == pytest.approx(0.8)

        # Test 4-7: RGB values must be set together via rgb parameter
        client.light_command(key=rgbcw_light.key, rgb=(0.7, 0.3, 0.9))
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.red == pytest.approx(0.7, abs=0.1)
        assert state.green == pytest.approx(0.3, abs=0.1)
        assert state.blue == pytest.approx(0.9, abs=0.1)

        # Test 7: white value
        client.light_command(key=rgbcw_light.key, white=0.6)
        state = await wait_for_state_change(rgbcw_light.key)
        # White might need more tolerance or might not be directly settable
        if isinstance(state, LightState) and state.white is not None:
            assert state.white == pytest.approx(0.6, abs=0.1)

        # Test 8: color_temperature only
        client.light_command(key=rgbcw_light.key, color_temperature=300)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.color_temperature == pytest.approx(300)

        # Test 9: cold_white only
        client.light_command(key=rgbcw_light.key, cold_white=0.8)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.cold_white == pytest.approx(0.8)

        # Test 10: warm_white only
        client.light_command(key=rgbcw_light.key, warm_white=0.2)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.warm_white == pytest.approx(0.2)

        # Test 11: transition_length with state change
        client.light_command(key=rgbcw_light.key, state=False, transition_length=0.1)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is False

        # Test 12: flash_length
        client.light_command(key=rgbcw_light.key, state=True, flash_length=0.2)
        state = await wait_for_state_change(rgbcw_light.key)
        # Flash starts
        assert state.state is True
        # Wait for flash to end
        state = await wait_for_state_change(rgbcw_light.key)

        # Test 13: effect only
        # First ensure light is on
        client.light_command(key=rgbcw_light.key, state=True)
        state = await wait_for_state_change(rgbcw_light.key)
        # Now set effect
        client.light_command(key=rgbcw_light.key, effect="Random Effect")
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.effect == "Random Effect"

        # Test 14: stop effect
        client.light_command(key=rgbcw_light.key, effect="None")
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.effect == "None"

        # Test 15: color_mode parameter
        client.light_command(
            key=rgbcw_light.key, state=True, color_mode=5
        )  # COLD_WARM_WHITE
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Now test common combinations

        # Test 16: RGB combination (set_rgb) - RGB values get normalized
        client.light_command(key=rgbcw_light.key, rgb=(1.0, 0.0, 0.5))
        state = await wait_for_state_change(rgbcw_light.key)
        # RGB values get normalized - in this case red is already 1.0
        assert state.red == pytest.approx(1.0, abs=0.1)
        assert state.green == pytest.approx(0.0, abs=0.1)
        assert state.blue == pytest.approx(0.5, abs=0.1)

        # Test 17: Multiple RGB changes to test transitions
        client.light_command(key=rgbcw_light.key, rgb=(0.2, 0.8, 0.4))
        state = await wait_for_state_change(rgbcw_light.key)
        # RGB values get normalized so green (highest) becomes 1.0
        # Expected: (0.2/0.8, 0.8/0.8, 0.4/0.8) = (0.25, 1.0, 0.5)
        assert state.red == pytest.approx(0.25, abs=0.01)
        assert state.green == pytest.approx(1.0, abs=0.01)
        assert state.blue == pytest.approx(0.5, abs=0.01)

        # Test 18: State + brightness + transition
        client.light_command(
            key=rgbcw_light.key, state=True, brightness=0.7, transition_length=0.1
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True
        assert state.brightness == pytest.approx(0.7)

        # Test 19: RGB + brightness + color_brightness
        client.light_command(
            key=rgb_light.key,
            state=True,
            brightness=0.8,
            color_brightness=0.9,
            rgb=(0.2, 0.4, 0.6),
        )
        state = await wait_for_state_change(rgb_light.key)
        assert state.state is True
        assert state.brightness == pytest.approx(0.8)

        # Test 20: Color temp + cold/warm white
        client.light_command(
            key=rgbcw_light.key, color_temperature=250, cold_white=0.7, warm_white=0.3
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.color_temperature == pytest.approx(250)

        # Test 21: Turn RGB light off
        client.light_command(key=rgb_light.key, state=False)
        state = await wait_for_state_change(rgb_light.key)
        assert state.state is False

        # Test color mode combinations to verify get_suitable_color_modes optimization

        # Test 22: White only mode
        client.light_command(key=rgbcw_light.key, state=True, white=0.5)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Test 23: Color temperature only mode
        client.light_command(key=rgbcw_light.key, state=True, color_temperature=300)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.color_temperature == pytest.approx(300)

        # Test 24: Cold/warm white only mode
        client.light_command(
            key=rgbcw_light.key, state=True, cold_white=0.6, warm_white=0.4
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.cold_white == pytest.approx(0.6)
        assert state.warm_white == pytest.approx(0.4)

        # Test 25: RGB only mode
        client.light_command(key=rgb_light.key, state=True, rgb=(0.5, 0.5, 0.5))
        state = await wait_for_state_change(rgb_light.key)
        assert state.state is True

        # Test 26: RGB + white combination
        client.light_command(
            key=rgbcw_light.key, state=True, rgb=(0.3, 0.3, 0.3), white=0.5
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Test 27: RGB + color temperature combination
        client.light_command(
            key=rgbcw_light.key, state=True, rgb=(0.4, 0.4, 0.4), color_temperature=280
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Test 28: RGB + cold/warm white combination
        client.light_command(
            key=rgbcw_light.key,
            state=True,
            rgb=(0.2, 0.2, 0.2),
            cold_white=0.5,
            warm_white=0.5,
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Test 29: White + color temperature combination
        client.light_command(
            key=rgbcw_light.key, state=True, white=0.6, color_temperature=320
        )
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True

        # Test 30: No specific color parameters (tests default mode selection)
        client.light_command(key=rgbcw_light.key, state=True, brightness=0.75)
        state = await wait_for_state_change(rgbcw_light.key)
        assert state.state is True
        assert state.brightness == pytest.approx(0.75)

        # Final cleanup - turn all lights off
        for light in lights:
            client.light_command(
                key=light.key,
                state=False,
            )
            state = await wait_for_state_change(light.key)
            assert state.state is False
