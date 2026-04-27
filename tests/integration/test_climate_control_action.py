"""Integration test for climate ControlAction.

Tests that climate.control automation actions work correctly with the
per-instance bitmask field storage. Exercises multiple field combinations
to cover different bitmask variants and the lambda path.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ButtonInfo,
    ClimateInfo,
    ClimateMode,
    ClimateState,
    EntityState,
)
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test climate ControlAction with constants and lambdas."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        climate_state_future: asyncio.Future[ClimateState] | None = None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, ClimateState)
                and climate_state_future is not None
                and not climate_state_future.done()
            ):
                climate_state_future.set_result(state)

        async def wait_for_climate_state(timeout: float = 5.0) -> ClimateState:
            nonlocal climate_state_future
            climate_state_future = loop.create_future()
            try:
                return await asyncio.wait_for(climate_state_future, timeout)
            finally:
                climate_state_future = None

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        require_entity(entities, "test_climate", ClimateInfo)

        async def press_and_wait(name: str) -> ClimateState:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            return await wait_for_climate_state()

        # Test 1: mode only (mask 1) — set HEAT
        state = await press_and_wait("Set Mode Heat")
        assert state.mode == ClimateMode.HEAT

        # Test 2: mode + low + high (mask 13) — HEAT_COOL with both temps
        state = await press_and_wait("Set Mode Temps")
        assert state.mode == ClimateMode.HEAT_COOL
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
        assert state.mode == ClimateMode.OFF
