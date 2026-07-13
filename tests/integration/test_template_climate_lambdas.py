"""Integration tests for template climate: state-readback lambdas, optimistic vs non-optimistic.

Both scenarios use an external-device echo pattern: set_*_action lambdas write
to globals, and the state lambdas read them back, simulating a device (e.g. a
heatpump) that is the single source of truth. The fixtures are identical
except for the `optimistic:` flag.

With optimistic=true, HA also reflects commands immediately as a preview; the
device then confirms (or corrects) the state via the lambda on the next loop
iteration. With optimistic=false, HA does not speculatively apply commands --
it only reflects the new state once the device confirms it through the
lambda. In both cases the loop() poll is effectively immediate on host,
so the two scenarios converge to the same observed final state; what differs
is the documented intent, not the assertions, so both tests share one body.
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

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction


async def _run_lambdas_scenario(client: aioesphomeapi.APIClient) -> None:
    """Shared body: initial state from lambdas, then command/confirm round-trips."""

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

    client.subscribe_states(initial_state_helper.on_state_wrapper(lambda state: None))

    try:
        await initial_state_helper.wait_for_initial_states()
    except TimeoutError:
        pytest.fail("Timeout waiting for initial states")

    # Initial state must come from the lambdas, not from ESPHome defaults.
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

    # Each command is confirmed by the device (lambda reads back the global
    # that the action wrote).
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


@pytest.mark.asyncio
async def test_template_climate_lambdas_optimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """State-readback lambdas + optimistic=true: commands are also shown immediately as a preview."""
    clear_host_prefs("tmpl-clim-lam-opt")
    async with run_compiled(yaml_config), api_client_connected() as client:
        await _run_lambdas_scenario(client)


@pytest.mark.asyncio
async def test_template_climate_lambdas_nonoptimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """State-readback lambdas + optimistic=false: HA state is driven entirely by device lambdas."""
    clear_host_prefs("tmpl-clim-lam-nopt")
    async with run_compiled(yaml_config), api_client_connected() as client:
        await _run_lambdas_scenario(client)
