"""Integration tests for template climate: pull+optimistic device-correction flow.

The device's lambdas always return fixed values regardless of commands.  This
lets us verify the two-phase update behaviour of pull+optimistic:

  1. Optimistic preview  — control() applies the command and publishes it
                           immediately so HA sees the change at once.
  2. Device correction   — on the next loop iteration the lambda re-evaluates
                           to the fixed value, detects a mismatch, and
                           publishes the corrected state back.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode, EntityState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_template_climate_pull_optimistic_correction(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Pull+optimistic: optimistic preview is published, then device corrects it.

    The fixture's lambdas always return fixed values (mode=OFF, target=22.0°C)
    and the set_*_actions do nothing.  After each command we therefore expect
    two consecutive state updates:

      command HEAT        → first update:  mode == HEAT  (optimistic preview)
                          → second update: mode == OFF   (lambda correction)

      command target=25°C → first update:  target == 25  (optimistic preview)
                          → second update: target == 22  (lambda correction)
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

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Lambda always returns OFF / 22.0°C
        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF
        assert initial.target_temperature == pytest.approx(22.0, abs=0.1)

        # --- Mode correction ---
        # The action does nothing, so the lambda stays at OFF.
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)

        # First update: optimistic preview from control()
        preview = await wait_for_climate_state()
        assert preview.mode == ClimateMode.HEAT, (
            "Expected immediate optimistic HEAT preview before device correction"
        )

        # Second update: lambda re-evaluates to OFF and corrects the state
        corrected = await wait_for_climate_state()
        assert corrected.mode == ClimateMode.OFF, (
            "Expected device to correct mode back to OFF via lambda"
        )

        # --- Target temperature correction ---
        # The action does nothing, so the lambda stays at 22.0°C.
        client.climate_command(test_climate.key, target_temperature=25.0)

        # First update: optimistic preview from control()
        preview = await wait_for_climate_state()
        assert preview.target_temperature == pytest.approx(25.0, abs=0.1), (
            "Expected immediate optimistic 25°C preview before device correction"
        )

        # Second update: lambda re-evaluates to 22.0°C and corrects the state
        corrected = await wait_for_climate_state()
        assert corrected.target_temperature == pytest.approx(22.0, abs=0.1), (
            "Expected device to correct target temperature back to 22.0°C via lambda"
        )
