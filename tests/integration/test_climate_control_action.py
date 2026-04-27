"""Integration test for climate ControlAction.

Tests that climate.control automation actions work correctly with the
per-instance bitmask field storage. Exercises multiple field combinations
to cover different bitmask variants and the lambda path.
"""

import asyncio
from typing import Any

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test climate ControlAction with constants and lambdas."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        client.subscribe_states(on_state)

        entities = await client.list_entities_services()
        climate = next(e for e in entities[0] if e.object_id == "test_climate")
        buttons = {e.name: e for e in entities[0] if hasattr(e, "name")}

        async def wait_for_state(key: int, timeout: float = 5.0) -> Any:
            loop = asyncio.get_running_loop()
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        async def press_and_wait(button_name: str) -> Any:
            btn = buttons[button_name]
            client.button_command(btn.key)
            return await wait_for_state(climate.key)

        # Test 1: mode only (mask 1) — set HEAT
        state = await press_and_wait("Set Mode Heat")
        # ClimateMode.CLIMATE_MODE_HEAT == 3
        assert state.mode == 3

        # Test 2: mode + low + high (mask 13) — HEAT_COOL with both temps
        state = await press_and_wait("Set Mode Temps")
        # CLIMATE_MODE_HEAT_COOL == 1
        assert state.mode == 1
        assert state.target_temperature_low == pytest.approx(19.0, abs=0.5)
        assert state.target_temperature_high == pytest.approx(23.0, abs=0.5)

        # Test 3: low only (mask 4)
        state = await press_and_wait("Set Low Only")
        assert state.target_temperature_low == pytest.approx(17.5, abs=0.5)

        # Test 4: lambda high — global is 21.5
        state = await press_and_wait("Lambda High")
        assert state.target_temperature_high == pytest.approx(21.5, abs=0.5)

        # Test 5: turn off via mode (mask 1)
        state = await press_and_wait("Set Off")
        # CLIMATE_MODE_OFF == 0
        assert state.mode == 0
