"""Integration test for climate ControlAction.

Tests that climate.control automation actions work correctly with the
single stateless apply lambda/function pointer implementation. Exercises
multiple field combinations and the lambda path.
"""

from __future__ import annotations

from functools import partial

from aioesphomeapi import ClimateInfo, ClimateMode, ClimateState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test climate ControlAction with constants and lambdas."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_climate", ClimateInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=ClimateState
        )

        # mode only — set HEAT
        state = await press("set_mode_heat")
        assert state.mode == ClimateMode.HEAT

        # mode + target_temperature_low + target_temperature_high
        state = await press("set_mode_temps")
        assert state.mode == ClimateMode.HEAT_COOL
        assert state.target_temperature_low == pytest.approx(19.0, abs=0.5)
        assert state.target_temperature_high == pytest.approx(23.0, abs=0.5)

        # target_temperature_low only
        state = await press("set_low_only")
        assert state.target_temperature_low == pytest.approx(17.5, abs=0.5)

        # lambda path: target_temperature_high computed at runtime
        state = await press("lambda_high")
        assert state.target_temperature_high == pytest.approx(21.5, abs=0.5)

        # mode only — turn off via mode
        state = await press("set_off")
        assert state.mode == ClimateMode.OFF
