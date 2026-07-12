"""Integration tests for template climate: mixed lambda/no-lambda fields, optimistic=false.

Regression test for a bug where a single component-wide "any state lambda
configured" flag gated immediate application for every settable field. If a
climate entity had a state lambda for one field (e.g. mode) but not another
(e.g. swing_mode) and optimistic was false, the field without a lambda would
never be updated by control(), since nothing else was ever going to read it
back either. The fix decides immediate application per field.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode, ClimateSwingMode, EntityState
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-mix-nopt"


@pytest.mark.asyncio
async def test_template_climate_mixed_lambdas_nonoptimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A field with no state lambda must still update immediately when optimistic=false.

    `mode` has a state lambda (device-echo pattern) and only updates once the
    lambda re-reads the global the set_mode_action wrote. `swing_mode` has no
    state lambda at all, so it must be applied directly by control() even
    though the entity as a whole is non-optimistic.
    """
    # swing_mode has no state lambda, so restored preference state (from a
    # previous run of this same binary name) would otherwise persist across
    # runs and make the initial-state assertion below order-dependent.
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
        assert test_climate.name == "Test Mixed Climate"

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF
        assert initial.swing_mode == ClimateSwingMode.OFF

        # swing_mode has no state lambda: it must be applied directly, even
        # though the entity is non-optimistic.
        client.climate_command(test_climate.key, swing_mode=ClimateSwingMode.VERTICAL)
        state = await wait_for_climate_state()
        assert state.swing_mode == ClimateSwingMode.VERTICAL

        # mode has a state lambda (device echo via set_mode_action -> global
        # -> lambda readback): it should still confirm correctly.
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT
