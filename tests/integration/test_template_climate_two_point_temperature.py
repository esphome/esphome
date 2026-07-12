"""Integration tests for template climate: two-point target temperature + humidity.

Covers the CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE path (separate low/high
target temperatures instead of a single target_temperature) together with
current/target humidity, using the same device-echo pattern as the other
template climate tests: set_*_action lambdas write to globals, and the
corresponding state lambdas read them back.
"""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode
import pytest

from .state_utils import InitialStateHelper, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_template_climate_two_point_temperature(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Two-point target temperature + humidity: initial state and command round-trips.

    Verifies:
    - Initial target_temperature_low/high and current/target humidity come
      from the lambdas (device globals), not ESPHome defaults.
    - Commands for mode, target_temperature_low, target_temperature_high, and
      target_humidity are reflected once the device (lambda) confirms them.
    - Low and high can also be set together in a single command.
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
        assert test_climate.name == "Test Two-Point Heatpump"

        client.subscribe_states(
            initial_state_helper.on_state_wrapper(lambda state: None)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.HEAT_COOL  # ext_mode = 1
        assert initial.current_temperature == pytest.approx(21.0, abs=0.1)
        assert initial.target_temperature_low == pytest.approx(18.0, abs=0.1)
        assert initial.target_temperature_high == pytest.approx(24.0, abs=0.1)
        assert initial.current_humidity == pytest.approx(55.0, abs=0.1)
        assert initial.target_humidity == pytest.approx(50.0, abs=0.1)

        # Commands are confirmed by the device (lambda reads back the global
        # that the action wrote).
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT

        client.climate_command(test_climate.key, target_temperature_low=19.0)
        state = await wait_for_climate_state()
        assert state.target_temperature_low == pytest.approx(19.0, abs=0.1)

        client.climate_command(test_climate.key, target_temperature_high=23.0)
        state = await wait_for_climate_state()
        assert state.target_temperature_high == pytest.approx(23.0, abs=0.1)

        client.climate_command(test_climate.key, target_humidity=45.0)
        state = await wait_for_climate_state()
        assert state.target_humidity == pytest.approx(45.0, abs=0.1)

        # Low and high can also be commanded together in a single call.
        client.climate_command(
            test_climate.key, target_temperature_low=17.5, target_temperature_high=25.0
        )
        state = await wait_for_climate_state()
        assert state.target_temperature_low == pytest.approx(17.5, abs=0.1)
        assert state.target_temperature_high == pytest.approx(25.0, abs=0.1)

        # current_temperature/current_humidity are read from fixed lambdas and
        # never overridden by commands (no corresponding set actions).
        assert state.current_temperature == pytest.approx(21.0, abs=0.1)
        assert state.current_humidity == pytest.approx(55.0, abs=0.1)
