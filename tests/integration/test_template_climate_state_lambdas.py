"""Integration test for template climate component with state-readback lambdas.

Tests the non-optimistic mode where the external device owns its state and
ESPHome reads it back via lambdas (mode, fan_mode, swing_mode, preset,
target_temperature, current_temperature).
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import (
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
    EntityState,
)
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_template_climate_state_lambdas(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test template climate with non-optimistic mode and state-readback lambdas.

    The set_*_action writes to globals; the state lambdas read from those globals.
    This simulates an external device (e.g. a heatpump) that owns its own state.
    """
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        state_future: asyncio.Future[aioesphomeapi.ClimateState] = loop.create_future()

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, aioesphomeapi.ClimateState)
                and not state_future.done()
            ):
                state_future.set_result(state)

        async def wait_for_climate_state(
            timeout: float = 5.0,
        ) -> aioesphomeapi.ClimateState:
            nonlocal state_future
            state_future = loop.create_future()
            try:
                return await asyncio.wait_for(state_future, timeout)
            finally:
                state_future = loop.create_future()

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"

        test_climate = climate_infos[0]
        assert test_climate.name == "Test External Heatpump"

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Verify initial state is read from lambdas (not optimistic defaults)
        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF  # ext_mode = 0
        assert initial.current_temperature == pytest.approx(22.5, abs=0.1)
        assert initial.target_temperature == pytest.approx(21.0, abs=0.1)
        assert initial.fan_mode == ClimateFanMode.AUTO  # ext_fan_mode = 2
        assert initial.swing_mode == ClimateSwingMode.OFF  # ext_swing_mode = 0
        assert initial.preset == ClimatePreset.ECO  # ext_preset = 5

        # Send HEAT mode command. The set_mode_action writes to ext_mode global.
        # Since optimistic=false, state only updates when the lambda is re-evaluated.
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        heat_state = await wait_for_climate_state()
        assert heat_state.mode == ClimateMode.HEAT

        # Send target temperature — set_target_temperature_action writes to ext_target_temp
        client.climate_command(test_climate.key, target_temperature=23.0)
        temp_state = await wait_for_climate_state()
        assert temp_state.target_temperature == pytest.approx(23.0, abs=0.1)

        # Send fan mode HIGH — set_fan_mode_action writes to ext_fan_mode
        client.climate_command(test_climate.key, fan_mode=ClimateFanMode.HIGH)
        fan_state = await wait_for_climate_state()
        assert fan_state.fan_mode == ClimateFanMode.HIGH

        # Send swing mode VERTICAL — set_swing_mode_action writes to ext_swing_mode
        client.climate_command(test_climate.key, swing_mode=ClimateSwingMode.VERTICAL)
        swing_state = await wait_for_climate_state()
        assert swing_state.swing_mode == ClimateSwingMode.VERTICAL

        # Send preset AWAY — set_preset_action writes to ext_preset
        client.climate_command(test_climate.key, preset=ClimatePreset.AWAY)
        preset_state = await wait_for_climate_state()
        assert preset_state.preset == ClimatePreset.AWAY

        # Verify current_temperature is always read from the lambda (22.5f constant)
        assert preset_state.current_temperature == pytest.approx(22.5, abs=0.1)
