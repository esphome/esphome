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

from .state_utils import require_only_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_water_heater_template(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test template water heater basic state and mode changes."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        test_water_heater = require_only_entity(entities, WaterHeaterInfo)

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

        # Wait for all initial states to be broadcast
        sub = await subscribe_after_initial_states(client, entities)

        # Get initial state and verify
        initial_state = sub.initial_states.get(test_water_heater.key)
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

        # Verify supported features: operation mode, away mode, and on/off
        assert (
            test_water_heater.supported_features
            & WaterHeaterFeature.SUPPORTS_OPERATION_MODE
        ) != 0, "Expected SUPPORTS_OPERATION_MODE in supported_features"
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
        next_state = sub.waiter.next_state(test_water_heater.key, WaterHeaterState)
        client.water_heater_command(test_water_heater.key, away=True)
        away_on_state = await next_state
        assert (away_on_state.state & WaterHeaterStateFlag.AWAY) != 0
        # ON flag should still be set (is_on lambda returns true)
        assert (away_on_state.state & WaterHeaterStateFlag.ON) != 0

        # Test turning off away mode
        next_state = sub.waiter.next_state(test_water_heater.key, WaterHeaterState)
        client.water_heater_command(test_water_heater.key, away=False)
        away_off_state = await next_state
        assert (away_off_state.state & WaterHeaterStateFlag.AWAY) == 0
        assert (away_off_state.state & WaterHeaterStateFlag.ON) != 0

        # Test turning off (on=False)
        next_state = sub.waiter.next_state(test_water_heater.key, WaterHeaterState)
        client.water_heater_command(test_water_heater.key, on=False)
        off_state = await next_state
        assert (off_state.state & WaterHeaterStateFlag.ON) == 0
        assert (off_state.state & WaterHeaterStateFlag.AWAY) == 0

        # Test turning back on (on=True)
        next_state = sub.waiter.next_state(test_water_heater.key, WaterHeaterState)
        client.water_heater_command(test_water_heater.key, on=True)
        on_state = await next_state
        assert (on_state.state & WaterHeaterStateFlag.ON) != 0

        # Test changing to GAS mode
        next_state = sub.waiter.next_state(test_water_heater.key, WaterHeaterState)
        client.water_heater_command(test_water_heater.key, mode=WaterHeaterMode.GAS)
        gas_state = await next_state
        assert gas_state.mode == WaterHeaterMode.GAS

        # Test changing to ECO mode (from GAS)
        next_state = sub.waiter.next_state(test_water_heater.key, WaterHeaterState)
        client.water_heater_command(test_water_heater.key, mode=WaterHeaterMode.ECO)
        eco_state = await next_state
        assert eco_state.mode == WaterHeaterMode.ECO


@pytest.mark.asyncio
async def test_water_heater_template_unknown_temperature(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a template water heater whose temperature lambdas stay unknown.

    NAN never compares equal to itself, so a lambda that keeps returning NAN must not be
    mistaken for a changed value and republish the state on every loop iteration.
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        state_count = 0

        def on_state(state: aioesphomeapi.EntityState) -> None:
            nonlocal state_count
            if isinstance(state, WaterHeaterState):
                state_count += 1

        entities, _ = await client.list_entities_services()
        require_only_entity(entities, WaterHeaterInfo)

        client.subscribe_states(on_state)

        # Let the device run for a while; only the single initial state may arrive.
        await asyncio.sleep(1.0)
        assert state_count <= 1, (
            f"Expected at most 1 state publish, got {state_count} - "
            "an unknown (NAN) temperature is republishing every loop"
        )
