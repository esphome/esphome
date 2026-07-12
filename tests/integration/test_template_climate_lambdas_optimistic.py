"""Integration tests for template climate: state-readback lambdas, optimistic=true.

State lambdas are the source of truth.  With optimistic=true, commands are
also reflected in HA immediately as a preview — the device then confirms (or
corrects) the state via the lambda on the next loop iteration.
"""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import (
    ClimateAction,
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)
import pytest

from .state_utils import InitialStateHelper, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_template_climate_lambdas_optimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """State-readback lambdas + optimistic: initial state from lambdas, commands shown immediately.

    State lambdas are the source of truth, but HA also reflects commands at
    once (optimistic).  Because the set_*_action lambdas write back to the
    same globals the state lambdas read from, the device always confirms the
    command, so the final state matches what was commanded.

    This verifies that state lambdas drive the initial state while optimistic
    mode is also active (commands are reflected in HA immediately).
    """
    async with run_compiled(yaml_config), api_client_connected() as client:

        async def wait_for_climate_state(
            timeout: float = 5.0,
        ) -> aioesphomeapi.ClimateState:
            return await wait_for_state(
                client, lambda s: isinstance(s, aioesphomeapi.ClimateState), timeout
            )

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"

        test_climate = climate_infos[0]
        assert test_climate.name == "Test External Heatpump"

        client.subscribe_states(
            initial_state_helper.on_state_wrapper(lambda state: None)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Initial state must come from the lambdas, not from ESPHome defaults,
        # even though optimistic=true.
        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF  # ext_mode = 0
        assert initial.action == ClimateAction.IDLE  # ext_action = 4
        assert initial.current_temperature == pytest.approx(22.5, abs=0.1)
        assert initial.target_temperature == pytest.approx(21.0, abs=0.1)
        assert initial.fan_mode == ClimateFanMode.AUTO  # ext_fan_mode = 2
        assert initial.swing_mode == ClimateSwingMode.OFF  # ext_swing_mode = 0
        assert initial.preset == ClimatePreset.ECO  # ext_preset = 5

        # Commands are reflected immediately (optimistic) and then confirmed by
        # the device (lambda reads back the global that the action wrote).
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT

        client.climate_command(test_climate.key, target_temperature=23.0)
        state = await wait_for_climate_state()
        assert state.target_temperature == pytest.approx(23.0, abs=0.1)

        client.climate_command(test_climate.key, fan_mode=ClimateFanMode.HIGH)
        state = await wait_for_climate_state()
        assert state.fan_mode == ClimateFanMode.HIGH

        client.climate_command(test_climate.key, swing_mode=ClimateSwingMode.VERTICAL)
        state = await wait_for_climate_state()
        assert state.swing_mode == ClimateSwingMode.VERTICAL

        client.climate_command(test_climate.key, preset=ClimatePreset.AWAY)
        state = await wait_for_climate_state()
        assert state.preset == ClimatePreset.AWAY

        # current_temperature is read from the lambda (fixed 22.5°C) and never
        # overridden by commands, since there is no set_current_temperature_action.
        assert state.current_temperature == pytest.approx(22.5, abs=0.1)
