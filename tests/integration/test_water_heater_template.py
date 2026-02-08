"""Integration test for template water heater component."""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import WaterHeaterInfo, WaterHeaterMode, WaterHeaterState
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
        gas_mode_future: asyncio.Future[WaterHeaterState] = loop.create_future()
        eco_mode_future: asyncio.Future[WaterHeaterState] = loop.create_future()

        def on_state(state: aioesphomeapi.EntityState) -> None:
            states[state.key] = state
            if isinstance(state, WaterHeaterState):
                # Wait for GAS mode
                if state.mode == WaterHeaterMode.GAS and not gas_mode_future.done():
                    gas_mode_future.set_result(state)
                # Wait for ECO mode (we start at OFF, so test transitioning to ECO)
                elif state.mode == WaterHeaterMode.ECO and not eco_mode_future.done():
                    eco_mode_future.set_result(state)

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

        # Test changing to GAS mode
        client.water_heater_command(test_water_heater.key, mode=WaterHeaterMode.GAS)

        try:
            gas_state = await asyncio.wait_for(gas_mode_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("GAS mode change not received within 5 seconds")

        assert isinstance(gas_state, WaterHeaterState)
        assert gas_state.mode == WaterHeaterMode.GAS

        # Test changing to ECO mode (from GAS)
        client.water_heater_command(test_water_heater.key, mode=WaterHeaterMode.ECO)

        try:
            eco_state = await asyncio.wait_for(eco_mode_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("ECO mode change not received within 5 seconds")

        assert isinstance(eco_state, WaterHeaterState)
        assert eco_state.mode == WaterHeaterMode.ECO
