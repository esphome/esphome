"""Integration tests for template climate: custom fan modes and presets with lambdas, optimistic=false.

The state lambdas read from globals that the set_custom_*_action lambdas write.
With optimistic=false, ESPHome does NOT speculatively apply commands; HA only
reflects the new state once the device confirms it through the lambda on the
next loop iteration.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, EntityState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_template_climate_custom_fan_mode_and_preset_lambdas_nonoptimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Custom fan/preset lambdas + non-optimistic: HA state is driven entirely by device lambdas.

    Each set_custom_*_action writes to a global; the corresponding state lambda
    reads it back.  This simulates an external device that is the single source
    of truth.  HA updates only after the lambda re-evaluates — there is no
    speculative/optimistic state applied on command receipt.

    Verifies:
    - supported_custom_fan_modes and supported_custom_presets are advertised
    - Initial state comes from the lambdas (device globals)
    - State only changes after the action fires and the lambda re-evaluates;
      the action writes the globals the lambda reads, so the sequence is:
      command → action → global update → lambda polls → state published
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
        assert test_climate.name == "Test Custom Mode Climate"

        # Traits must advertise all configured custom modes and presets.
        assert set(test_climate.supported_custom_fan_modes) == {
            "turbo",
            "silent",
            "eco",
        }
        assert set(test_climate.supported_custom_presets) == {
            "eco_plus",
            "power_save",
            "max",
        }

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Initial state must come from the lambdas (globals), not from ESPHome defaults.
        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.custom_fan_mode == "turbo"  # ext_custom_fan_mode initial
        assert initial.custom_preset == "eco_plus"  # ext_custom_preset initial

        # Commands write to globals via set_custom_*_action; the lambdas read them
        # back.  State only updates once the loop re-evaluates the lambda.
        client.climate_command(test_climate.key, custom_fan_mode="silent")
        state = await wait_for_climate_state()
        assert state.custom_fan_mode == "silent"

        client.climate_command(test_climate.key, custom_preset="power_save")
        state = await wait_for_climate_state()
        assert state.custom_preset == "power_save"

        # Verify a second round-trip to confirm the non-optimistic cycle is stable.
        client.climate_command(test_climate.key, custom_fan_mode="eco")
        state = await wait_for_climate_state()
        assert state.custom_fan_mode == "eco"

        client.climate_command(test_climate.key, custom_preset="max")
        state = await wait_for_climate_state()
        assert state.custom_preset == "max"
