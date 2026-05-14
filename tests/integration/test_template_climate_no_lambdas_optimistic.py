"""Integration tests for template climate: no state-readback lambdas, optimistic=true.

When no state-readback lambdas are present ESPHome owns the internal state.
Commands apply immediately — no external confirmation is needed.
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

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-no-lam-opt"


@pytest.mark.asyncio
async def test_template_climate_no_lambdas_optimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """No state-readback lambdas + optimistic: every command is reflected in HA immediately.

    ESPHome owns the state — no lambdas are needed to confirm commands.
    """
    clear_host_prefs(DEVICE_NAME)
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

        # Verify advertised capabilities
        assert ClimateMode.OFF in test_climate.supported_modes
        assert ClimateMode.HEAT in test_climate.supported_modes
        assert ClimateMode.COOL in test_climate.supported_modes
        assert ClimateMode.FAN_ONLY in test_climate.supported_modes

        assert ClimateFanMode.AUTO in test_climate.supported_fan_modes
        assert ClimateFanMode.LOW in test_climate.supported_fan_modes
        assert ClimateFanMode.HIGH in test_climate.supported_fan_modes

        assert ClimateSwingMode.OFF in test_climate.supported_swing_modes
        assert ClimateSwingMode.VERTICAL in test_climate.supported_swing_modes

        assert ClimatePreset.NONE in test_climate.supported_presets
        assert ClimatePreset.ECO in test_climate.supported_presets
        assert ClimatePreset.AWAY in test_climate.supported_presets

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF

        # Each command updates ESPHome's internal state immediately.
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT

        client.climate_command(test_climate.key, target_temperature=22.5)
        state = await wait_for_climate_state()
        assert state.target_temperature == pytest.approx(22.5, abs=0.1)

        client.climate_command(test_climate.key, fan_mode=ClimateFanMode.HIGH)
        state = await wait_for_climate_state()
        assert state.fan_mode == ClimateFanMode.HIGH

        client.climate_command(test_climate.key, swing_mode=ClimateSwingMode.VERTICAL)
        state = await wait_for_climate_state()
        assert state.swing_mode == ClimateSwingMode.VERTICAL

        client.climate_command(test_climate.key, preset=ClimatePreset.AWAY)
        state = await wait_for_climate_state()
        assert state.preset == ClimatePreset.AWAY
