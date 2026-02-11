"""Integration test for template water heater component."""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import (
    WaterHeaterFeature,
    WaterHeaterInfo,
    WaterHeaterMode,
    WaterHeaterState,
    WaterHeaterStateFlag,
)
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_water_heater_template(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test template water heater basic state and mode changes."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        states: dict[int, aioesphomeapi.EntityState] = {}
        state_future: asyncio.Future[WaterHeaterState] | None = None

        def on_state(state: aioesphomeapi.EntityState) -> None:
            states[state.key] = state
            if (
                isinstance(state, WaterHeaterState)
                and state_future is not None
                and not state_future.done()
            ):
                state_future.set_result(state)

        async def wait_for_state(timeout: float = 5.0) -> WaterHeaterState:
            """Wait for next water heater state change."""
            nonlocal state_future
            state_future = loop.create_future()
            try:
                return await asyncio.wait_for(state_future, timeout)
            finally:
                state_future = None

        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        water_heater_infos = [e for e in entities if isinstance(e, WaterHeaterInfo)]
        assert len(water_heater_infos) == 1, (
            f"Expected exactly 1 water heater entity, got {len(water_heater_infos)}. Entity types: {[type(e).__name__ for e in entities]}"
        )

        test_water_heater = water_heater_infos[0]

        # Verify water heater entity info
        assert test_water_heater.object_id == "test_boiler"
        assert test_water_heater.name == "Test Boiler"
        assert test_water_heater.min_temperature == 30.0
        assert test_water_heater.max_temperature == 85.0
        assert test_water_heater.target_temperature_step == 0.5

        # Verify supported modes
        supported_modes = test_water_heater.supported_modes
        assert WaterHeaterMode.OFF in supported_modes, "Expected OFF in supported modes"
        assert WaterHeaterMode.ECO in supported_modes, "Expected ECO in supported modes"
        assert WaterHeaterMode.GAS in supported_modes, "Expected GAS in supported modes"
        assert WaterHeaterMode.PERFORMANCE in supported_modes, (
            "Expected PERFORMANCE in supported modes"
        )
        assert len(supported_modes) == 4, (
            f"Expected 4 supported modes, got {len(supported_modes)}: {supported_modes}"
        )

        # Subscribe with the wrapper that filters initial states
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for all initial states to be broadcast
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Get initial state and verify
        initial_state = initial_state_helper.initial_states.get(test_water_heater.key)
        assert initial_state is not None, "Water heater initial state not found"
        assert isinstance(initial_state, WaterHeaterState)
        # Initial mode is OFF (default) since we don't have a mode lambda
        # A mode lambda would override optimistic mode changes
        assert initial_state.mode == WaterHeaterMode.OFF, (
            f"Expected initial mode OFF, got {initial_state.mode}"
        )
        assert initial_state.current_temperature == 45.0, (
            f"Expected current temp 45.0, got {initial_state.current_temperature}"
        )
        assert initial_state.target_temperature == 60.0, (
            f"Expected target temp 60.0, got {initial_state.target_temperature}"
        )

        # Verify supported features: away mode and on/off (fixture has away + is_on lambdas)
        assert (
            test_water_heater.supported_features & WaterHeaterFeature.SUPPORTS_AWAY_MODE
        ) != 0, "Expected SUPPORTS_AWAY_MODE in supported_features"
        assert (
            test_water_heater.supported_features & WaterHeaterFeature.SUPPORTS_ON_OFF
        ) != 0, "Expected SUPPORTS_ON_OFF in supported_features"

        # Verify initial state: on (is_on lambda returns true), not away (away lambda returns false)
        assert (initial_state.state & WaterHeaterStateFlag.ON) != 0, (
            "Expected initial state to include ON flag"
        )
        assert (initial_state.state & WaterHeaterStateFlag.AWAY) == 0, (
            "Expected initial state to not include AWAY flag"
        )

        # Test turning on away mode
        client.water_heater_command(test_water_heater.key, away=True)
        away_on_state = await wait_for_state()
        assert (away_on_state.state & WaterHeaterStateFlag.AWAY) != 0
        # ON flag should still be set (is_on lambda returns true)
        assert (away_on_state.state & WaterHeaterStateFlag.ON) != 0

        # Test turning off away mode
        client.water_heater_command(test_water_heater.key, away=False)
        away_off_state = await wait_for_state()
        assert (away_off_state.state & WaterHeaterStateFlag.AWAY) == 0
        assert (away_off_state.state & WaterHeaterStateFlag.ON) != 0

        # Test turning off (on=False)
        client.water_heater_command(test_water_heater.key, on=False)
        off_state = await wait_for_state()
        assert (off_state.state & WaterHeaterStateFlag.ON) == 0
        assert (off_state.state & WaterHeaterStateFlag.AWAY) == 0

        # Test turning back on (on=True)
        client.water_heater_command(test_water_heater.key, on=True)
        on_state = await wait_for_state()
        assert (on_state.state & WaterHeaterStateFlag.ON) != 0

        # Test changing to GAS mode
        client.water_heater_command(test_water_heater.key, mode=WaterHeaterMode.GAS)
        gas_state = await wait_for_state()
        assert gas_state.mode == WaterHeaterMode.GAS

        # Test changing to ECO mode (from GAS)
        client.water_heater_command(test_water_heater.key, mode=WaterHeaterMode.ECO)
        eco_state = await wait_for_state()
        assert eco_state.mode == WaterHeaterMode.ECO
